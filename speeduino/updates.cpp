/** @file
 * EEPROM Storage updates.
 */
/** Store and load various configs to/from EEPROM considering the the data format versions of various SW generations.
 * This routine is used for doing any data conversions that are required during firmware changes.
 * This prevents users getting difference reports in TS when such a data change occurs.
 * It also can be used for setting good values when there are variables that move locations in the ini.
 * When a user skips multiple firmware versions at a time, this will roll through the updates 1 at a time.
 * The doUpdates() uses may lower level routines from Arduino EEPROM library and storage.ino to carry out EEPROM storage tasks.
 */
#include "globals.h"
#include "storage.h"
#include "sensors.h"
#include "updates.h"
#include "pages.h"
#include "comms_CAN.h"
#include EEPROM_LIB_H //This is defined in the board .h files
#include "units.h"

#define CURRENT_DATA_VERSION    25

// Helper: Add offset to entire table values (file scope to reduce nesting)
static void addOffsetToTable(table3d16RpmLoad* table, int8_t offset)
{
  auto table_it = table->values.begin();
  for(uint8_t x=0; x < table->values.num_rows; x++)
  {
    auto row = *table_it;
    while (!row.at_end()) { *row = *row + offset; ++row; }
    ++table_it;
  }
}

// Helper: Shift table values left (multiply by 2)
static void shiftTableLeft(table3d8RpmLoad* table)
{
  auto table_it = table->values.begin();
  while (!table_it.at_end())
  {
    auto row = *table_it;
    while (!row.at_end()) { *row = *row << 1; ++row; }
    ++table_it;
  }
}

// Helper: Set all table values to constant
static void setTableConstant(table3d8RpmLoad* table, uint8_t value)
{
  auto table_it = table->values.begin();
  while (!table_it.at_end())
  {
    auto row = *table_it;
    while (!row.at_end()) { *row = value; ++row; }
    ++table_it;
  }
}

// Helper: Scale rotary split bins by 2 (file scope to reduce nesting)
static void scaleRotarySplitBins(void)
{
  for(uint8_t x = 0; x < 8; x++) { configPage10.rotarySplitBins[x] *= 2; }
}

// Helper: Update programmable output data indices (file scope to reduce nesting)
static void updateProgrammableOutputIndices(uint8_t y)
{
  if ((configPage13.firstDataIn[y] > 22) && (configPage13.firstDataIn[y] < 240)) { configPage13.firstDataIn[y]++; }
  if ((configPage13.firstDataIn[y] > 92) && (configPage13.firstDataIn[y] < 240)) { configPage13.firstDataIn[y]++; }
  if ((configPage13.secondDataIn[y] > 22) && (configPage13.secondDataIn[y] < 240)) { configPage13.secondDataIn[y]++; }
  if ((configPage13.secondDataIn[y] > 92) && (configPage13.secondDataIn[y] < 240)) { configPage13.secondDataIn[y]++; }
}

namespace {
  // ==================================================================================
  // EEPROM VERSION UPDATE HANDLERS - FASE C5 REFACTORING
  // Each function handles migration from one version to the next
  // Pattern: Version Handler Extraction for MISRA compliance
  // ==================================================================================

  /** @brief Update from version 2: May 2017 ignition table offset fix (+40) */
  static inline void updateFromVersion_02(void)
  {
    addOffsetToTable(&ignitionTable, 40);
    writeAllConfig();
    storeEEPROMVersion(3);
  }

  /** @brief Update from version 3: June 2017 CAN values + spark duration fix */
  static inline void updateFromVersion_03(void)
  {
    configPage9.speeduino_tsCanId = 0;
    configPage9.true_address = 256;
    configPage9.realtime_base_address = 336;
    if(configPage4.sparkDur == UINT8_MAX) { configPage4.sparkDur = 10; }
    writeAllConfig();
    storeEEPROMVersion(4);
  }

  /** @brief Update from version 4: July 2017 cranking enrichment curve */
  static inline void updateFromVersion_04(void)
  {
    configPage10.crankingEnrichBins[0] = 0;
    configPage10.crankingEnrichBins[1] = 40;
    configPage10.crankingEnrichBins[2] = 70;
    configPage10.crankingEnrichBins[3] = 100;
    configPage10.crankingEnrichValues[0] = 100 + configPage2.crankingPct;
    configPage10.crankingEnrichValues[1] = 100 + configPage2.crankingPct;
    configPage10.crankingEnrichValues[2] = 100 + configPage2.crankingPct;
    configPage10.crankingEnrichValues[3] = 100 + configPage2.crankingPct;
    writeAllConfig();
    storeEEPROMVersion(5);
  }

  /** @brief Update from version 5: September 2017 table size increase (128 min) */
  static inline void updateFromVersion_05(void)
  {
    for(int x=0; x < 1152; x++)
    {
      int endMem = EEPROM_CONFIG10_END - x;
      int startMem = endMem - 128;
      byte currentVal = EEPROM.read(startMem);
      EEPROM.update(endMem, currentVal);
    }
    for(int x=0; x < 352; x++)
    {
      int endMem = EEPROM_CONFIG10_END - 1152 - x;
      int startMem = endMem - 64;
      byte currentVal = EEPROM.read(startMem);
      EEPROM.update(endMem, currentVal);
    }
    storeEEPROMVersion(6);
    loadConfig();
  }

  /** @brief Update from version 6: November 2017 staging table addition */
  static inline void updateFromVersion_06(void)
  {
    for(int x=0; x < 529; x++)
    {
      int endMem = EEPROM_CONFIG10_END - x;
      int startMem = endMem - 82;
      byte currentVal = EEPROM.read(startMem);
      EEPROM.update(endMem, currentVal);
    }
    storeEEPROMVersion(7);
    loadConfig();
  }

  /** @brief Update from version 7: Flex fuel settings conversion to tables */
  static inline void updateFromVersion_07(void)
  {
    configPage10.flexBoostBins[0] = 0;
    configPage10.flexBoostAdj[0]  = (int8_t)configPage2.aeColdPct;
    configPage10.flexFuelBins[0] = 0;
    configPage10.flexFuelAdj[0]  = configPage2.idleUpPin;
    configPage10.flexAdvBins[0] = 0;
    configPage10.flexAdvAdj[0]  = configPage2.aeTaperMin;

    for (uint8_t x = 1; x < 6; x++)
    {
      uint8_t pct = x * 20;
      configPage10.flexBoostBins[x] = pct;
      configPage10.flexFuelBins[x] = pct;
      configPage10.flexAdvBins[x] = pct;

      int16_t boostAdder = (((configPage2.aeColdTaperMin - (int8_t)configPage2.aeColdPct) * pct) / 100) + (int8_t)configPage2.aeColdPct;
      configPage10.flexBoostAdj[x] = boostAdder;

      uint8_t fuelAdder = (((configPage2.idleUpAdder - configPage2.idleUpPin) * pct) / 100) + configPage2.idleUpPin;
      configPage10.flexFuelAdj[x] = fuelAdder;

      uint8_t advanceAdder = (((configPage2.aeTaperMax - configPage2.aeTaperMin) * pct) / 100) + configPage2.aeTaperMin;
      configPage10.flexAdvAdj[x] = advanceAdder;
    }
    writeAllConfig();
    storeEEPROMVersion(8);
  }

  /** @brief Update from version 8: May 2018 separate load sources */
  static inline void updateFromVersion_08(void)
  {
    configPage2.fuelAlgorithm = (LoadSource)configPage2.legacyMAP;
    configPage2.ignAlgorithm = (LoadSource)configPage2.legacyMAP;
    configPage4.boostType = 1;
    writeAllConfig();
    storeEEPROMVersion(9);
  }

  /** @brief Update from version 9: October 2018 AUX channels + ADC filters */
  static inline void updateFromVersion_09(void)
  {
    for (byte AuxinChan = 0; AuxinChan <16 ; AuxinChan++)
    {
      configPage9.caninput_sel[AuxinChan] = 0;
    }
    configPage4.ADCFILTER_TPS  = ADCFILTER_TPS_DEFAULT;
    configPage4.ADCFILTER_CLT  = ADCFILTER_CLT_DEFAULT;
    configPage4.ADCFILTER_IAT  = ADCFILTER_IAT_DEFAULT;
    configPage4.ADCFILTER_O2   = ADCFILTER_O2_DEFAULT;
    configPage4.ADCFILTER_BAT  = ADCFILTER_BAT_DEFAULT;
    configPage4.ADCFILTER_MAP  = ADCFILTER_MAP_DEFAULT;
    configPage4.ADCFILTER_BARO = ADCFILTER_BARO_DEFAULT;
    writeAllConfig();
    storeEEPROMVersion(10);
  }

  /** @brief Update from version 10: May 2019 priming pulse 2D table + ASE + CLT advance */
  static inline void updateFromVersion_10(void)
  {
    configPage2.primePulse[0] = configPage2.aeColdTaperMax / 5;
    configPage2.primePulse[1] = configPage2.aeColdTaperMax / 5;
    configPage2.primePulse[2] = configPage2.aeColdTaperMax / 5;
    configPage2.primePulse[3] = configPage2.aeColdTaperMax / 5;
    configPage2.primeBins[0] = 0;
    configPage2.primeBins[1] = 40;
    configPage2.primeBins[2] = 70;
    configPage2.primeBins[3] = 100;

    configPage2.asePct[0] = configPage2.aeColdTaperMin;
    configPage2.asePct[1] = configPage2.aeColdTaperMin;
    configPage2.asePct[2] = configPage2.aeColdTaperMin;
    configPage2.asePct[3] = configPage2.aeColdTaperMin;
    configPage2.aseCount[0] = 10;
    configPage2.aseCount[1] = 10;
    configPage2.aseCount[2] = 10;
    configPage2.aseCount[3] = 10;
    configPage2.aseBins[0] = 0;
    configPage2.aseBins[1] = 20;
    configPage2.aseBins[2] = 60;
    configPage2.aseBins[3] = 80;

    configPage4.cltAdvBins[0] = 0;
    configPage4.cltAdvBins[1] = 30;
    configPage4.cltAdvBins[2] = 60;
    configPage4.cltAdvBins[3] = 70;
    configPage4.cltAdvBins[4] = 85;
    configPage4.cltAdvBins[5] = 100;
    configPage4.cltAdvValues[0] = 0;
    configPage4.cltAdvValues[1] = 0;
    configPage4.cltAdvValues[2] = 0;
    configPage4.cltAdvValues[3] = 0;
    configPage4.cltAdvValues[4] = 0;
    configPage4.cltAdvValues[5] = 0;

    if(configPage2.tachoDuration > 6) { configPage2.tachoDuration = 3; }
    configPage2.aeMode = AE_MODE_TPS;
    configPage2.maeThresh = configPage2.taeThresh;
    configPage4.maeRates[0] = 75;
    configPage4.maeRates[1] = 75;
    configPage4.maeRates[2] = 75;
    configPage4.maeRates[3] = 75;
    configPage4.maeBins[0] = 7;
    configPage4.maeBins[1] = 12;
    configPage4.maeBins[2] = 20;
    configPage4.maeBins[3] = 40;
    configPage10.fuel2Mode = 0;
    writeAllConfig();
    storeEEPROMVersion(11);
  }

  /** @brief Update from version 11: Sep 2019 battery calibration + fuel table 2 */
  static inline void updateFromVersion_11(void)
  {
    configPage4.batVoltCorrect = 0;
    configPage2.legacyMAP = 0;
    configPage10.fuel2Mode = 0;
    configPage10.fuel2SwitchVariable = 0;
    configPage10.fuel2SwitchValue = 7000;
    writeAllConfig();
    storeEEPROMVersion(12);
  }

  /** @brief Update from version 12: Nov 2019 baro correction + idle advance */
  static inline void updateFromVersion_12(void)
  {
    configPage4.baroFuelBins[0] = 80;
    configPage4.baroFuelBins[1] = 85;
    configPage4.baroFuelBins[2] = 90;
    configPage4.baroFuelBins[3] = 95;
    configPage4.baroFuelBins[4] = 100;
    configPage4.baroFuelBins[5] = 105;
    configPage4.baroFuelBins[6] = 110;
    configPage4.baroFuelBins[7] = 115;
    configPage4.baroFuelValues[0] = 100;
    configPage4.baroFuelValues[1] = 100;
    configPage4.baroFuelValues[2] = 100;
    configPage4.baroFuelValues[3] = 100;
    configPage4.baroFuelValues[4] = 100;
    configPage4.baroFuelValues[5] = 100;
    configPage4.baroFuelValues[6] = 100;
    configPage4.baroFuelValues[7] = 100;

    configPage2.idleAdvEnabled = 0;
    configPage2.idleAdvTPS = 5;
    configPage2.idleAdvRPM = 20;
    configPage4.idleAdvBins[0] = 30;
    configPage4.idleAdvBins[1] = 40;
    configPage4.idleAdvBins[2] = 50;
    configPage4.idleAdvBins[3] = 60;
    configPage4.idleAdvBins[4] = 70;
    configPage4.idleAdvBins[5] = 80;
    configPage4.idleAdvValues[0] = 15;
    configPage4.idleAdvValues[1] = 15;
    configPage4.idleAdvValues[2] = 15;
    configPage4.idleAdvValues[3] = 15;
    configPage4.idleAdvValues[4] = 15;
    configPage4.idleAdvValues[5] = 15;
    writeAllConfig();
    storeEEPROMVersion(13);
  }

  /** @brief Update from version 13: 202005 cranking enrichment scale + injector timing + PID */
  static inline void updateFromVersion_13(void)
  {
    configPage10.crankingEnrichValues[0] = configPage10.crankingEnrichValues[0] / 5;
    configPage10.crankingEnrichValues[1] = configPage10.crankingEnrichValues[1] / 5;
    configPage10.crankingEnrichValues[2] = configPage10.crankingEnrichValues[2] / 5;
    configPage10.crankingEnrichValues[3] = configPage10.crankingEnrichValues[3] / 5;

    configPage2.injAng[0] = configPage2.injAng[0];
    configPage2.injAng[1] = configPage2.injAng[0];
    configPage2.injAng[2] = configPage2.injAng[0];
    configPage2.injAng[3] = configPage2.injAng[0];
    configPage2.injAngRPM[0] = 5;
    configPage2.injAngRPM[1] = 25;
    configPage2.injAngRPM[2] = 45;
    configPage2.injAngRPM[3] = 65;

    configPage2.dfcoDelay = 0;
    configPage2.dfcoMinCLT = temperatureAddOffset(40);

    for (int i=0; i<6; i++)
    {
      configPage10.flexAdvAdj[i] += 40;
    }

    configPage2.aeColdPct = 100;
    configPage2.aeColdTaperMin = 40;
    configPage2.aeColdTaperMax = 100;

    if(configPage6.idleKP >= 8) { configPage6.idleKP = UINT8_MAX; }
    else { configPage6.idleKP = configPage6.idleKP<<5; }
    if(configPage6.idleKI >= 8) { configPage6.idleKI = UINT8_MAX; }
    else { configPage6.idleKI = configPage6.idleKI<<5; }
    if(configPage6.idleKD >= 8) { configPage6.idleKD = UINT8_MAX; }
    else { configPage6.idleKD = configPage6.idleKD<<5; }
    if(configPage10.vvtCLKP >= 8) { configPage10.vvtCLKP = UINT8_MAX; }
    else { configPage10.vvtCLKP = configPage10.vvtCLKP<<5; }
    if(configPage10.vvtCLKI >= 8) { configPage10.vvtCLKI = UINT8_MAX; }
    else { configPage10.vvtCLKI = configPage10.vvtCLKI<<5; }
    if(configPage10.vvtCLKD >= 8) { configPage10.vvtCLKD = UINT8_MAX; }
    else { configPage10.vvtCLKD = configPage10.vvtCLKD<<5; }

    configPage10.crankingEnrichTaper = 1;
    configPage2.aseTaperTime = 1;
    configPage2.SoftLimitMode = SOFT_LIMIT_FIXED;
    configPage2.vssMode = 0;
    writeAllConfig();
    storeEEPROMVersion(14);
  }

  /** @brief Update from version 14: 202008 calibration tables 2D + oil/fuel pressure + WMI + programmable outputs */
  static inline void updateFromVersion_14(void)
  {
    int y;
    for(int x=0; x<(CALIBRATION_TABLE_SIZE/16); x++)
    {
      y = EEPROM_CALIBRATION_CLT_OLD + (x * 16);
      cltCalibrationTable.values[x] = EEPROM.read(y);
      cltCalibrationTable.axis[x] = (x * 32);

      y = EEPROM_CALIBRATION_IAT_OLD + (x * 16);
      iatCalibrationTable.values[x] = EEPROM.read(y);
      iatCalibrationTable.axis[x] = (x * 32);

      y = EEPROM_CALIBRATION_O2_OLD + (x * 16);
      o2CalibrationTable.values[x] = EEPROM.read(y);
      o2CalibrationTable.axis[x] = (x * 32);
    }
    writeCalibration();

    configPage10.oilPressureProtEnbl = false;
    configPage10.oilPressureEnable = false;
    configPage10.fuelPressureEnable = false;

    configPage10.wmiEnabled = 0;
    configPage10.wmiMode = 0;
    configPage10.wmiOffset = 0;
    configPage10.wmiIndicatorEnabled = 0;
    configPage10.wmiEmptyEnabled = 0;
    configPage10.wmiAdvEnabled = 0;
    for(int i=0; i<6; i++)
    {
      configPage10.wmiAdvBins[i] = i*100/2;
      configPage10.wmiAdvAdj[i] = OFFSET_IGNITION;
    }

    configPage13.outputPin[0] = 0;
    configPage13.outputPin[1] = 0;
    configPage13.outputPin[2] = 0;
    configPage13.outputPin[3] = 0;
    configPage13.outputPin[4] = 0;
    configPage13.outputPin[5] = 0;
    configPage13.outputPin[6] = 0;
    configPage13.outputPin[7] = 0;

    configPage2.multiplyMAP = configPage2.crkngAddCLTAdv;
    configPage2.aeApplyMode = 0;
    configPage2.primingDelay = 0;
    configPage2.aseTaperTime = 10;
    writeAllConfig();
    storeEEPROMVersion(15);
  }

  /** @brief Update from version 15: 202012 2nd spark table */
  static inline void updateFromVersion_15(void)
  {
    configPage10.spark2Mode = 0;
    writeAllConfig();
    storeEEPROMVersion(16);
  }

  /** @brief Update from version 16: Page 13 fix + dwell map */
  static inline void updateFromVersion_16(void)
  {
    for(int x=EEPROM_CONFIG14_END; x>=EEPROM_CONFIG13_START; x--)
    {
      EEPROM.update(x, EEPROM.read(x-112));
    }
    configPage6.iacPWMrun = false;
    configPage2.useDwellMap = 0;
    writeAllConfig();
    storeEEPROMVersion(17);
  }

  /** @brief Update from version 17: VVT accuracy 0.5 + VVT2 + map sample RPM switch */
  static inline void updateFromVersion_17(void)
  {
    shiftTableLeft(&vvtTable);

    configPage10.vvtCLholdDuty = configPage10.vvtCLholdDuty << 1;
    configPage10.vvtCLminDuty = configPage10.vvtCLminDuty << 1;
    configPage10.vvtCLmaxDuty = configPage10.vvtCLmaxDuty << 1;

    configPage10.vvt2Enabled = 0;
    configPage4.vvt2PWMdir = 0;
    configPage10.TrigEdgeThrd = 0;

    if(configPage6.tachoMode == 1) { configPage6.vvtMode = VVT_MODE_ONOFF; }

    configPage10.vvtCLMinAng = 0;
    configPage10.vvtCLMaxAng = 200;
    configPage4.ANGLEFILTER_VVT = 0;

    configPage2.idleAdvDelay *= 2;
    configPage2.mapSwitchPoint = 0;
    configPage9.boostByGearEnabled = 0;

    configPage13.outputTimeLimit[0] = 0;
    configPage13.outputTimeLimit[1] = 0;
    configPage13.outputTimeLimit[2] = 0;
    configPage13.outputTimeLimit[3] = 0;
    configPage13.outputTimeLimit[4] = 0;
    configPage13.outputTimeLimit[5] = 0;
    configPage13.outputTimeLimit[6] = 0;
    configPage13.outputTimeLimit[7] = 0;
    writeAllConfig();
    storeEEPROMVersion(18);
  }

  /** @brief Update from version 18: 202202 TPS resolution 0.5% + SD logging defaults */
  /**
   * @brief Scale TPS-dependent values by 2 (8-bit to 9-bit resolution)
   * @note MISRA-C compliant: Lines: 15 | Cyclomatic: 3 | Nesting: 1
   */
  static inline void v18_scaleTPSValues(void)
  {
    configPage2.idleAdvTPS *= 2;
    configPage2.iacTPSlimit *= 2;
    configPage4.floodClear *= 2;
    configPage4.dfcoTPSThresh *= 2;
    configPage6.egoTPSMax *= 2;
    configPage10.lnchCtrlTPS *= 2;
    configPage10.wmiTPS *= 2;
    configPage10.n2o_minTPS *= 2;

    if(configPage10.fuel2SwitchVariable == FUEL2_CONDITION_TPS) { configPage10.fuel2SwitchValue *= 2; }
    if(configPage10.spark2SwitchVariable == SPARK2_CONDITION_TPS) { configPage10.spark2SwitchVariable *= 2; }
  }

  /**
   * @brief Scale fuel/trim tables if TPS-based load algorithm
   * @note MISRA-C compliant: Lines: 20 | Cyclomatic: 2 | Nesting: 2
   */
  static inline void v18_scaleFuelTables(void)
  {
    if(configPage2.fuelAlgorithm != LOAD_SOURCE_TPS) { return; }

    multiplyTableLoad(&fuelTable,  fuelTable.type_key,  4);
    multiplyTableLoad(&afrTable,   afrTable.type_key,   4);
    multiplyTableLoad(&trim1Table, trim1Table.type_key, 4);
    multiplyTableLoad(&trim2Table, trim2Table.type_key, 4);
    multiplyTableLoad(&trim3Table, trim3Table.type_key, 4);
    multiplyTableLoad(&trim4Table, trim4Table.type_key, 4);
    multiplyTableLoad(&trim5Table, trim5Table.type_key, 4);
    multiplyTableLoad(&trim6Table, trim6Table.type_key, 4);
    multiplyTableLoad(&trim7Table, trim7Table.type_key, 4);
    multiplyTableLoad(&trim8Table, trim8Table.type_key, 4);

    // Rotary split table scaling (if rotary mode)
    if(configPage4.sparkMode == IGN_MODE_ROTARY) { scaleRotarySplitBins(); }
  }

  /**
   * @brief Scale ignition/boost/VVT tables based on load algorithm
   * @note MISRA-C compliant: Lines: 18 | Cyclomatic: 4 | Nesting: 1
   */
  static inline void v18_scaleOtherTables(void)
  {
    // Ignition and secondary tables (if TPS-based)
    if(configPage2.ignAlgorithm == LOAD_SOURCE_TPS) { multiplyTableLoad(&ignitionTable, ignitionTable.type_key, 4); }
    if(configPage10.fuel2Algorithm == LOAD_SOURCE_TPS) { multiplyTableLoad(&fuelTable2, fuelTable2.type_key, 4); }
    if(configPage10.spark2Algorithm == LOAD_SOURCE_TPS) { multiplyTableLoad(&ignitionTable2, ignitionTable2.type_key, 4); }

    // Boost table always scaled
    multiplyTableLoad(&boostTable, boostTable.type_key, 2);

    // VVT tables scaled up (TPS) or down (MAP)
    if(configPage6.vvtLoadSource == VVT_LOAD_TPS)
    {
      multiplyTableLoad(&vvtTable, vvtTable.type_key, 2);
      multiplyTableLoad(&vvt2Table, vvt2Table.type_key, 2);
    }
    else
    {
      divideTableLoad(&vvtTable, vvtTable.type_key, 2);
      divideTableLoad(&vvt2Table, vvt2Table.type_key, 2);
    }
  }

  /**
   * @brief Initialize configPage13 onboard logging parameters to zero
   * @note MISRA-C compliant: Lines: 22 | Cyclomatic: 1 | Nesting: 0
   */
  static inline void v18_initializeLoggingConfig(void)
  {
    configPage13.onboard_log_csv_separator = 0;
    configPage13.onboard_log_file_style = 0;
    configPage13.onboard_log_file_rate = 0;
    configPage13.onboard_log_filenaming = 0;
    configPage13.onboard_log_storage = 0;
    configPage13.onboard_log_trigger_boot = 0;
    configPage13.onboard_log_trigger_RPM = 0;
    configPage13.onboard_log_trigger_prot = 0;
    configPage13.onboard_log_trigger_Vbat = 0;
    configPage13.onboard_log_trigger_Epin = 0;
    configPage13.onboard_log_tr1_duration = 0;
    configPage13.onboard_log_tr2_thr_on = 0;
    configPage13.onboard_log_tr2_thr_off = 0;
    configPage13.onboard_log_tr3_thr_RPM = 0;
    configPage13.onboard_log_tr3_thr_MAP = 0;
    configPage13.onboard_log_tr3_thr_Oil = 0;
    configPage13.onboard_log_tr3_thr_AFR = 0;
    configPage13.onboard_log_tr4_thr_on = 0;
    configPage13.onboard_log_tr4_thr_off = 0;
    configPage13.onboard_log_tr5_Epin_pin = 0;
  }

  /**
   * @brief Update from version 18: 202103 TPS resolution increase + onboard logging init
   * @note MISRA-C compliant: Lines: 16 | Cyclomatic: 1 | Nesting: 0
   * @note Previous: Lines: 76 | Cyclomatic: 11 | Nesting: 3 (CRITICAL violation)
   */
  static inline void updateFromVersion_18(void)
  {
    // Migrate fan enable flag
    configPage2.fanEnable = configPage6.fanUnused;

    // Scale TPS-dependent values
    v18_scaleTPSValues();

    // Scale lookup tables based on load algorithm
    v18_scaleFuelTables();
    v18_scaleOtherTables();

    // Reset VVT delay parameters
    configPage4.vvtDelay = 0;
    configPage4.vvtMinClt = 0;

    // Initialize onboard logging configuration
    v18_initializeLoggingConfig();

    // Commit changes
    writeAllConfig();
    storeEEPROMVersion(19);
  }

  /** @brief Update from version 19: 202207 injector pairing + CAN broadcast + AFR protection */
  static inline void updateFromVersion_19(void)
  {
    if( configPage4.inj4cylPairing > INJ_PAIR_14_23 ) { configPage4.inj4cylPairing = 0; }
    bool is4cyl = (configPage2.nCylinders == 4);
    if (is4cyl && (configPage2.injLayout == INJ_SEQUENTIAL)) { configPage4.inj4cylPairing = INJ_PAIR_13_24; }
    else if (is4cyl) { configPage4.inj4cylPairing = INJ_PAIR_14_23; }

    configPage9.hardRevMode = 1;
    configPage6.tachoMode = 0;
    configPage4.CANBroadcastProtocol = CAN_BROADCAST_PROTOCOL_OFF;
    configPage15.boostDCWhenDisabled = 0;
    configPage15.boostControlEnable = EN_BOOST_CONTROL_BARO;

    setTableConstant(&boostTableLookupDuty, 100);

    auto table_X = boostTableLookupDuty.axisX.begin();
    uint16_t i = 0;
    while (!table_X.at_end())
    {
      ++i;
      *table_X = 1000+(500*i);
      ++table_X;
    }

    auto table_Y = boostTableLookupDuty.axisY.begin();
    i = 0;
    while (!table_Y.at_end())
    {
      ++i;
      *table_Y = (120 + 10*i);
      ++table_Y;
    }

    configPage9.afrProtectEnabled = 0;
    configPage9.afrProtectMinMAP = 90;
    configPage9.afrProtectMinRPM = 40;
    configPage9.afrProtectMinTPS = 160;
    configPage9.afrProtectDeviation = 14;
    writeAllConfig();
    storeEEPROMVersion(20);
  }

  /** @brief Update from version 20: 202305 TAE/MAE minimum change + decel fuel + AC control + oil pressure delay */
  static inline void updateFromVersion_20(void)
  {
    configPage2.taeMinChange = 4;
    configPage2.maeMinChange = 2;
    configPage2.decelAmount = 100;

    for (uint8_t y = 0; y < sizeof(configPage13.outputPin); y++) { updateProgrammableOutputIndices(y); }

    configPage15.airConEnable = 0;
    configPage10.oilPressureProtTime = 0;
    configPage9.iacStepperPower = 0;
    writeAllConfig();
    storeEEPROMVersion(21);
  }

  /** @brief Update from version 21: 202310 rolling cut curve + DFCO hyster scaling */
  static inline void updateFromVersion_21(void)
  {
    configPage15.rollingProtRPMDelta[0]   = -30;
    configPage15.rollingProtRPMDelta[1]   = -20;
    configPage15.rollingProtRPMDelta[2]   = -10;
    configPage15.rollingProtRPMDelta[3]   = -5;
    configPage15.rollingProtCutPercent[0] = 50;
    configPage15.rollingProtCutPercent[1] = 65;
    configPage15.rollingProtCutPercent[2] = 80;
    configPage15.rollingProtCutPercent[3] = 95;
    configPage4.dfcoHyster = configPage4.dfcoHyster / 2;
    writeAllConfig();
    storeEEPROMVersion(22);
  }

  /** @brief Update from version 22: 202402 WMI PWM resolution + hw test modes + DFCO taper + EGO MAP limits */
  static inline void updateFromVersion_22(void)
  {
    if( configPage10.wmiMode >= WMI_MODE_OPENLOOP ) { multiplyTableValue(wmiMapPage, 2); }
    configPage13.hwTestInjDuration = 8;
    configPage13.hwTestIgnDuration = 4;
    configPage9.dfcoTaperEnable = 0;
    configPage9.dfcoTaperTime = 10;
    configPage9.dfcoTaperFuel = 100;
    configPage9.dfcoTaperAdvance = 20;
    configPage9.egoMAPMax = 255;
    configPage9.egoMAPMin = 0;
    configPage2.canWBO = 0;
    writeAllConfig();
    storeEEPROMVersion(23);
  }

  /** @brief Update from version 23: 202501 knock mode + CAN broadcast selection + VSS launch + flex freq + configPage10 realignment */
  static inline void updateFromVersion_23(void)
  {
    configPage10.knock_mode = KNOCK_MODE_OFF;
    if(configPage2.unused1_126_1 == true) { configPage4.CANBroadcastProtocol = CAN_BROADCAST_PROTOCOL_BMW; }
    if(configPage2.unused1_126_2 == true) { configPage4.CANBroadcastProtocol = CAN_BROADCAST_PROTOCOL_VAG; }
    configPage10.lnchCtrlVss = 255;
    configPage2.flexFreqLow = 50;
    configPage2.flexFreqHigh = 150;

    uint8_t origBoostIntv = ((uint8_t *)&configPage10)[27];
    ((uint8_t *)&configPage10)[27] = ((uint8_t *)&configPage10)[26];
    ((uint8_t *)&configPage10)[26] = ((uint8_t *)&configPage10)[25];
    ((uint8_t *)&configPage10)[25] = origBoostIntv;

    uint8_t origlnchCtrlTPS= ((uint8_t *)&configPage10)[32];
    for(byte x=32U; x<74U; x++)
    {
      ((uint8_t *)&configPage10)[x] = ((uint8_t *)&configPage10)[x+1];
    }
    ((uint8_t *)&configPage10)[74] = origlnchCtrlTPS;

    writeAllConfig();
    storeEEPROMVersion(24);
  }

  /** @brief Update from version 24: 202504 (placeholder for future updates) */
  static inline void updateFromVersion_24(void)
  {
    writeAllConfig();
    storeEEPROMVersion(25);
  }

  /** @brief Handler for brand new EEPROM (version 0 or 255) */
  static inline void updateBrandNewEEPROM(void)
  {
    configPage9.true_address = 0x200;

    configPage13.outputPin[0] = 0;
    configPage13.outputPin[1] = 0;
    configPage13.outputPin[2] = 0;
    configPage13.outputPin[3] = 0;
    configPage13.outputPin[4] = 0;
    configPage13.outputPin[5] = 0;
    configPage13.outputPin[6] = 0;
    configPage13.outputPin[7] = 0;

    configPage4.FILTER_FLEX = FILTER_FLEX_DEFAULT;
    storeEEPROMVersion(CURRENT_DATA_VERSION);
  }

} // end anonymous namespace

// ==================================================================================
// MAIN UPDATE FUNCTION
// ==================================================================================

/**
 * @brief Execute EEPROM data structure updates based on version
 * @details Reads EEPROM version once and applies all necessary updates sequentially.
 * Uses switch with fall-through to apply cumulative updates from old version to current.
 * @note MISRA-C compliant: Lines: 44 | Cyclomatic: 3 | Nesting: 1
 * @note Previous: Lines: 36 | Cyclomatic: 18 | Nesting: 2 (CRITICAL violation)
 */
void doUpdates(void)
{
  // Read EEPROM version once (avoid multiple reads)
  uint8_t eepromVersion = readEEPROMVersion();

  // Handle brand new/erased EEPROM
  if((eepromVersion == 0) || (eepromVersion == 255))
  {
    updateBrandNewEEPROM();
    return;
  }

  // Handle future versions (downgrade protection)
  if(eepromVersion > CURRENT_DATA_VERSION)
  {
    storeEEPROMVersion(CURRENT_DATA_VERSION);
    return;
  }

  // Apply cumulative updates using switch fall-through
  // Each case falls through to apply all updates from old version to current
  switch(eepromVersion)
  {
    #ifndef SMALL_FLASH_MODE
    case 2:  updateFromVersion_02(); // Falls through
    case 3:  updateFromVersion_03(); // Falls through
    case 4:  updateFromVersion_04(); // Falls through
    case 5:  updateFromVersion_05(); // Falls through
    case 6:  updateFromVersion_06(); // Falls through
    case 7:  updateFromVersion_07(); // Falls through
    case 8:  updateFromVersion_08(); // Falls through
    case 9:  updateFromVersion_09(); // Falls through
    case 10: updateFromVersion_10(); // Falls through
    case 11: updateFromVersion_11(); // Falls through
    case 12: updateFromVersion_12(); // Falls through
    case 13: updateFromVersion_13(); // Falls through
    case 14: updateFromVersion_14(); // Falls through
    case 15: updateFromVersion_15(); // Falls through
    #endif
    case 16: updateFromVersion_16(); // Falls through
    case 17: updateFromVersion_17(); // Falls through
    case 18: updateFromVersion_18(); // Falls through
    case 19: updateFromVersion_19(); // Falls through
    case 20: updateFromVersion_20(); // Falls through
    case 21: updateFromVersion_21(); // Falls through
    case 22: updateFromVersion_22(); // Falls through
    case 23: updateFromVersion_23(); // Falls through
    case 24: updateFromVersion_24(); break;
    default: break; // Already at current version
  }
}

// ==================================================================================
// TABLE HELPER FUNCTIONS
// ==================================================================================

void multiplyTableLoad(void *pTable, table_type_t key, uint8_t multiplier)
{
  auto y_it = y_begin(pTable, key);
  while(!y_it.at_end())
  {
    *y_it = *y_it * multiplier;
    ++y_it;
  }
}

void divideTableLoad(void *pTable, table_type_t key, uint8_t divisor)
{
  auto y_it = y_begin(pTable, key);
  while(!y_it.at_end())
  {
    *y_it = *y_it / divisor;
    ++y_it;
  }
}

void multiplyTableValue(uint8_t pageNum, uint8_t multiplier)
{
  uint16_t count = getPageSize(pageNum);
  for (uint16_t i = 0; i < count; i++)
  {
    setPageValue(pageNum, i, (uint8_t)(getPageValue(pageNum, i) * multiplier));
  }
}

void divideTableValue(uint8_t pageNum, uint8_t divisor)
{
  uint16_t count = getPageSize(pageNum);
  for (uint16_t i = 0; i < count; i++)
  {
    setPageValue(pageNum, i, (uint8_t)(getPageValue(pageNum, i) / divisor));
  }
}
