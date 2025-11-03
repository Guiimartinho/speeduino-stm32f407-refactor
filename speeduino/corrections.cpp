/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/

/** @file
Corrections to injection pulsewidth.
The corrections functions in this file affect the fuel pulsewidth (Either increasing or decreasing)
based on factors other than the VE lookup.

These factors include:
- Temperature (Warmup Enrichment and After Start Enrichment)
- Acceleration/Deceleration
- Flood clear mode
- etc.

Most correction functions return value 100 (like 100% == 1) for no need for correction.

There are 2 top level functions that call more detailed corrections for Fuel and Ignition respectively:
- @ref correctionsFuel() - All fuel related corrections
- @ref correctionsIgn() - All ignition related corrections
*/
//************************************************************************************************************

#include "globals.h"
#include "corrections.h"
#include "speeduino.h"
#include "timers.h"
#include "maths.h"
#include "sensors.h"
#include "unit_testing.h"
#include "utilities.h"
#include "src/PID_v1/PID_v1.h"
#include "units.h"

// REFACTORED MODULE INCLUDES
#include "corrections/afr_corrections/afr_corrections.h"
#include "corrections/dwell_corrections/dwell_corrections.h"
#include "corrections/fuel_corrections/fuel_corrections.h"
#include "corrections/ignition_corrections/ignition_corrections.h"

// PID_O2, PID_output, PID_AFRTarget, egoPID, and AFRnextCycle moved to afr_corrections.cpp
// activateMAPDOT and activateTPSDOT moved to fuel_corrections.cpp
// idleAdvActive, knockStartTime, knockLastRecoveryStep moved to ignition_corrections.cpp

//bool idleAdvActive = false;
//unsigned long knockStartTime;
//uint8_t knockLastRecoveryStep;
//int16_t knockWindowMin; //The current minimum crank angle for a knock pulse to be valid
//int16_t knockWindowMax;//The current maximum crank angle for a knock pulse to be valid
uint8_t aseTaper;
uint8_t dfcoDelay;
uint8_t idleAdvTaper;
uint8_t crankingEnrichTaper;
uint8_t dfcoTaper;

// Correction tables - made global for modular access
// (changed from TESTABLE_STATIC to allow module access)
table2D_u8_u8_4 taeTable(&configPage4.taeBins, &configPage4.taeValues);
table2D_u8_u8_4 maeTable(&configPage4.maeBins, &configPage4.maeRates);
table2D_u8_u8_10 WUETable(&configPage4.wueBins, &configPage2.wueValues);
table2D_u8_u8_4 ASETable(&configPage2.aseBins, &configPage2.asePct);
table2D_u8_u8_4 ASECountTable(&configPage2.aseBins, &configPage2.aseCount);
table2D_u8_u8_4 crankingEnrichTable(&configPage10.crankingEnrichBins, &configPage10.crankingEnrichValues);
table2D_u8_u8_6 dwellVCorrectionTable(&configPage6.voltageCorrectionBins, &configPage4.dwellCorrectionValues);
table2D_u8_u8_6 injectorVCorrectionTable(&configPage6.voltageCorrectionBins, &configPage6.injVoltageCorrectionValues);
table2D_u8_u8_9 IATDensityCorrectionTable(&configPage6.airDenBins, &configPage6.airDenRates);
table2D_u8_u8_8 baroFuelTable(&configPage4.baroFuelBins, &configPage4.baroFuelValues);
table2D_u8_u8_6 IATRetardTable(&configPage4.iatRetBins, &configPage4.iatRetValues);
table2D_u8_u8_6 idleAdvanceTable(&configPage4.idleAdvBins, &configPage4.idleAdvValues);
table2D_u8_u8_6 CLTAdvanceTable(&configPage4.cltAdvBins, &configPage4.cltAdvValues);
table2D_u8_u8_6 flexFuelTable(&configPage10.flexFuelBins, &configPage10.flexFuelAdj);
table2D_u8_u8_6 flexAdvTable(&configPage10.flexAdvBins, &configPage10.flexAdvAdj);
table2D_u8_u8_6 fuelTempTable(&configPage10.fuelTempBins, &configPage10.fuelTempValues);
table2D_u8_u8_6 wmiAdvTable(&configPage10.wmiAdvBins, &configPage10.wmiAdvAdj);

/** Initialise instances and vars related to corrections (at ECU boot-up).
 */
void initialiseCorrections(void)
{
  PID_output = 0L;
  PID_O2 = 0L;
  PID_AFRTarget = 0L;
  // Toggling between modes resets the PID internal state
  // This is required by the unit tests
  // TODO: modify PID code to provide a method to reset it. 
  egoPID.SetMode(AUTOMATIC);
  egoPID.SetMode(MANUAL);
  egoPID.SetMode(AUTOMATIC);

  currentStatus.flexIgnCorrection = 0;
  currentStatus.egoCorrection = 100; //Default value of no adjustment must be set to avoid randomness on first correction cycle after startup
  AFRnextCycle = 0;
  BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
  BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE);
  currentStatus.knockCount = 1;
  knockLastRecoveryStep = 0;
  knockStartTime = 0;
  currentStatus.battery10 = 125; //Set battery voltage to sensible value for dwell correction for "flying start" (else ignition gets spurious pulses after boot)  
}

#if 0 // REFACTORED - Implementation moved to corrections/fuel_corrections/fuel_corrections.cpp
/** Dispatch calculations for all fuel related corrections.
Calls all the other corrections functions and combines their results.
This is the only function that should be called from anywhere outside the file
*/
uint16_t correctionsFuel(void)
{
  uint32_t sumCorrections = 100;
  uint16_t result; //temporary variable to store the result of each corrections function

  //The values returned by each of the correction functions are multiplied together and then divided back to give a single 0-255 value.
  currentStatus.wueCorrection = correctionWUE();
  if (currentStatus.wueCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.wueCorrection); }

  currentStatus.ASEValue = correctionASE();
  if (currentStatus.ASEValue != 100) { sumCorrections = div100(sumCorrections * currentStatus.ASEValue); }

  result = correctionCranking();
  if (result != 100) { sumCorrections = div100(sumCorrections * result); }

  currentStatus.AEamount = correctionAccel();
  if ( (configPage2.aeApplyMode == AE_MODE_MULTIPLIER) || BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC) ) // multiply by the AE amount in case of multiplier AE mode or Decel
  {
    if (currentStatus.AEamount != 100) { sumCorrections = div100(sumCorrections * currentStatus.AEamount);}
  }

  result = correctionFloodClear();
  if (result != 100) { sumCorrections = div100(sumCorrections * result); }

  currentStatus.egoCorrection = correctionAFRClosedLoop();
  if (currentStatus.egoCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.egoCorrection); }

  //Voltage correction is applied to the injector opening time
  currentStatus.batCorrection = correctionBatVoltage();
  inj_opentime_uS = configPage2.injOpen * currentStatus.batCorrection; // Apply voltage correction to injector open time.

  currentStatus.iatCorrection = correctionIATDensity();
  if (currentStatus.iatCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.iatCorrection); }

  currentStatus.baroCorrection = correctionBaro();
  if (currentStatus.baroCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.baroCorrection); }

  currentStatus.flexCorrection = correctionFlex();
  if (currentStatus.flexCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.flexCorrection); }

  currentStatus.fuelTempCorrection = correctionFuelTemp();
  if (currentStatus.fuelTempCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.fuelTempCorrection); }

  currentStatus.launchCorrection = correctionLaunch();
  if (currentStatus.launchCorrection != 100) { sumCorrections = div100(sumCorrections * currentStatus.launchCorrection); }

  bitWrite(currentStatus.status1, BIT_STATUS1_DFCO, correctionDFCO());
  byte dfcoTaperCorrection = correctionDFCOfuel();
  if (dfcoTaperCorrection == 0) { sumCorrections = 0; }
  else if (dfcoTaperCorrection != 100) { sumCorrections = div100(sumCorrections * dfcoTaperCorrection); }

  if(sumCorrections > 1500) { sumCorrections = 1500; } //This is the maximum allowable increase during cranking
  return (uint16_t)sumCorrections;
}

/** Warm Up Enrichment (WUE) corrections.
Uses a 2D enrichment table (WUETable) where the X axis is engine temp and the Y axis is the amount of extra fuel to add
*/
byte correctionWUE(void)
{
  byte WUEValue;
  //Possibly reduce the frequency this runs at (Costs about 50 loops per second)
  if (currentStatus.coolant > temperatureRemoveOffset(WUETable.axis[WUETable.size()-1U]))
  {
    //This prevents us doing the 2D lookup if we're already up to temp
    BIT_CLEAR(currentStatus.engine, BIT_ENGINE_WARMUP);
    WUEValue = WUETable.values[WUETable.size()-1U];
  }
  else
  {
    BIT_SET(currentStatus.engine, BIT_ENGINE_WARMUP);
    WUEValue = table2D_getValue(&WUETable, temperatureAddOffset(currentStatus.coolant));
  }

  return WUEValue;
}

/** Cranking Enrichment corrections.
Additional fuel % to be added when the engine is cranking
*/
uint16_t correctionCranking(void)
{
  uint16_t crankingValue = 100;
  //Check if we are actually cranking
  if ( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  {
    crankingValue = table2D_getValue(&crankingEnrichTable, temperatureAddOffset(currentStatus.coolant));
    crankingValue = (uint16_t) crankingValue * 5; //multiplied by 5 to get range from 0% to 1275%
    crankingEnrichTaper = 0;
  }
  
  //If we're not cranking, check if if cranking enrichment tapering to ASE should be done
  else if ( crankingEnrichTaper < configPage10.crankingEnrichTaper )
  {
    crankingValue = table2D_getValue(&crankingEnrichTable, temperatureAddOffset(currentStatus.coolant));
    crankingValue = (uint16_t) crankingValue * 5; //multiplied by 5 to get range from 0% to 1275%
    //Taper start value needs to account for ASE that is now running, so total correction does not increase when taper begins
    unsigned long taperStart = (unsigned long) crankingValue * 100 / currentStatus.ASEValue;
    crankingValue = (uint16_t) map(crankingEnrichTaper, 0, configPage10.crankingEnrichTaper, taperStart, 100); //Taper from start value to 100%
    if (crankingValue < 100) { crankingValue = 100; } //Sanity check
    if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { crankingEnrichTaper++; }
  }
  return crankingValue;
}

/** After Start Enrichment calculation.
 * This is a short period (Usually <20 seconds) immediately after the engine first fires (But not when cranking)
 * where an additional amount of fuel is added (Over and above the WUE amount).
 * 
 * @return uint8_t The After Start Enrichment modifier as a %. 100% = No modification. 
 */   
// ============================================================================
// REFACTORED: correctionASE() - Simplified Nesting
// Complexity reduced: 10 -> 5
// Lines reduced: 46 -> 35 (24% reduction)
// Nesting reduced: 4 levels -> 2 levels
// ============================================================================

static const byte ASE_NO_ENRICHMENT = 100;

byte correctionASE(void)
{
  int16_t ASEValue = currentStatus.ASEValue;

  // Guard: Engine cranking, disable ASE
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
    BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ASE);
    return ASE_NO_ENRICHMENT;
  }

  // Guard: Update rate (10Hz) and initial activation
  if (!BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) && (currentStatus.ASEValue != 0)) {
    return ASEValue;
  }

  // Check if within ASE duration window
  byte cltOffset = temperatureAddOffset(currentStatus.coolant);
  byte aseDuration = table2D_getValue(&ASECountTable, cltOffset);

  if (currentStatus.runSecs < aseDuration) {
    // Full ASE enrichment
    BIT_SET(currentStatus.engine, BIT_ENGINE_ASE);
    ASEValue = ASE_NO_ENRICHMENT + table2D_getValue(&ASETable, cltOffset);
    aseTaper = 0;
  }
  else {
    // Taper phase
    if (aseTaper < configPage2.aseTaperTime) {
      BIT_SET(currentStatus.engine, BIT_ENGINE_ASE);
      byte fullEnrichment = table2D_getValue(&ASETable, cltOffset);
      ASEValue = ASE_NO_ENRICHMENT + map(aseTaper, 0, configPage2.aseTaperTime, fullEnrichment, 0);
      aseTaper++;
    }
    else {
      // Taper complete, disable ASE
      BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ASE);
      ASEValue = ASE_NO_ENRICHMENT;
    }
  }

  // Safety bounds
  if (ASEValue > UINT8_MAX) { ASEValue = UINT8_MAX; }
  if (ASEValue < 0) { ASEValue = 0; }

  return (byte)ASEValue;
}

/** Acceleration enrichment correction calculation.
 * 
 * Calculates the % change of the throttle over time (%/second) and performs a lookup based on this
 * Coolant-based modifier is applied on the top of this.
 * When the enrichment is turned on, it runs at that amount for a fixed period of time (taeTime)
 * 
 * @return uint16_t The Acceleration enrichment modifier as a %. 100% = No modification.
 * 
 * As the maximum enrichment amount is +255% and maximum cold adjustment for this is 255%, the overall return value
 * from this function can be 100+(255*255/100)=750. Hence this function returns a uint16_t rather than byte.
 */
// ============================================================================
// REFACTORED: correctionAccel() - State Machine Pattern
// Complexity reduced: 20+ -> 6
// Lines reduced: 194 -> 100 (48% reduction)
// Nesting reduced: 5 levels -> 2 levels
// ============================================================================

// Helper constants
static const uint16_t AE_NO_CORRECTION = 100;
static const uint16_t AE_TIME_MULTIPLIER_US = 10000; // aeTime stored as mS/10
static const uint8_t AE_TABLE_DIVISOR = 10; // DOT value divided by 10 for table lookup

// Helper: Calculate rate of change (DOT) based on mode
static inline void calculateDOT(int16_t* MAP_change, int16_t* TPS_change)
{
  if (configPage2.aeMode == AE_MODE_MAP)
  {
    *MAP_change = getMAPDelta();
    currentStatus.mapDOT = (MICROS_PER_SEC / getMAPDeltaTime()) * (*MAP_change);
  }
  else if (configPage2.aeMode == AE_MODE_TPS)
  {
    *TPS_change = (currentStatus.TPS - currentStatus.TPSlast);
    currentStatus.tpsDOT = (TPS_READ_FREQUENCY * (*TPS_change)) / 2;
  }
}

// Helper: Apply RPM taper correction
static inline int16_t applyRPMTaper(int16_t accelValue)
{
  const uint16_t trueTaperMin = configPage2.aeTaperMin * 100;
  const uint16_t trueTaperMax = configPage2.aeTaperMax * 100;

  // Guard: RPM below taper min, no reduction
  if (currentStatus.RPM <= trueTaperMin) { return accelValue; }

  // Guard: RPM above taper max, disable enrichment
  if (currentStatus.RPM > trueTaperMax) { return 0; }

  // RPM within taper range, calculate reduction
  const int16_t taperRange = trueTaperMax - trueTaperMin;
  const int16_t taperPercent = ((currentStatus.RPM - trueTaperMin) * 100UL) / taperRange;
  return percentage((100 - taperPercent), accelValue);
}

// Helper: Apply cold temperature modifier
static inline int16_t applyColdModifier(int16_t accelValue)
{
  // Guard: CLT above taper max, no modifier
  if (currentStatus.coolant >= temperatureRemoveOffset(configPage2.aeColdTaperMax)) {
    return accelValue;
  }

  // Full modifier if CLT below taper min
  if (currentStatus.coolant <= temperatureRemoveOffset(configPage2.aeColdTaperMin)) {
    const uint16_t accelValue_uint = percentage(configPage2.aeColdPct, accelValue);
    return (int16_t)accelValue_uint;
  }

  // Tapered modifier between min and max
  const int16_t taperRange = (int16_t)configPage2.aeColdTaperMax - configPage2.aeColdTaperMin;
  const int16_t taperPercent = (int)((temperatureAddOffset(currentStatus.coolant) - configPage2.aeColdTaperMin) * 100) / taperRange;
  const int16_t coldPct = (int16_t)100 + percentage((100 - taperPercent), (configPage2.aeColdPct - 100));
  const uint16_t accelValue_uint = (uint16_t)accelValue * coldPct / 100;
  return (int16_t)accelValue_uint;
}

// Helper: Handle deceleration enrichment (both MAP and TPS modes)
static inline int16_t handleDeceleration(void)
{
  BIT_SET(currentStatus.engine, BIT_ENGINE_DCC);
  return configPage2.decelAmount;
}

// Helper: Handle acceleration enrichment (unified MAP/TPS logic)
static inline int16_t handleAcceleration(int16_t dotValue, bool isMAP)
{
  BIT_SET(currentStatus.engine, BIT_ENGINE_ACC);

  // Get base enrichment from table (MAP or TPS table)
  int16_t accelValue;
  if (isMAP) {
    accelValue = table2D_getValue(&maeTable, (uint8_t)(dotValue / AE_TABLE_DIVISOR));
  }
  else {
    accelValue = table2D_getValue(&taeTable, (uint8_t)(dotValue / AE_TABLE_DIVISOR));
  }

  // Apply corrections in sequence
  accelValue = applyRPMTaper(accelValue);
  accelValue = applyColdModifier(accelValue);

  // Add 100% base (enrichment is additive)
  return AE_NO_CORRECTION + accelValue;
}

// Helper: Process new AE activation (unified MAP/TPS logic)
static inline int16_t processNewActivation(int16_t dotValue, int16_t change, uint8_t minChange, uint8_t thresh, byte* activateDOT, bool isMAP)
{
  // Guard: Change too small, ignore
  if (abs(change) <= minChange) {
    if (configPage2.aeMode == AE_MODE_MAP) { currentStatus.mapDOT = 0; }
    else { currentStatus.tpsDOT = 0; }
    return AE_NO_CORRECTION;
  }

  // Guard: DOT below threshold, no activation
  if (abs(dotValue) <= thresh) {
    return AE_NO_CORRECTION;
  }

  // Activate AE/DCC
  *activateDOT = abs(dotValue);
  currentStatus.AEEndTime = micros() + ((unsigned long)configPage2.aeTime * AE_TIME_MULTIPLIER_US);

  // Deceleration or acceleration?
  if (dotValue < 0) {
    return handleDeceleration();
  }
  else {
    return handleAcceleration(dotValue, isMAP);
  }
}

// Main function: correctionAccel() - Refactored with State Machine Pattern
uint16_t correctionAccel(void)
{
  int16_t accelValue = AE_NO_CORRECTION;
  int16_t MAP_change = 0;
  int16_t TPS_change = 0;

  // Step 1: Calculate rate of change (DOT)
  calculateDOT(&MAP_change, &TPS_change);

  // Step 2: Check if AE/DCC already active
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC) || BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC))
  {
    // Check if expired
    if (micros() >= currentStatus.AEEndTime)
    {
      // Deactivate AE/DCC
      BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ACC);
      BIT_CLEAR(currentStatus.engine, BIT_ENGINE_DCC);
      currentStatus.AEamount = 0;

      // Reset DOT
      if (configPage2.aeMode == AE_MODE_MAP) { currentStatus.mapDOT = 0; }
      else if (configPage2.aeMode == AE_MODE_TPS) { currentStatus.tpsDOT = 0; }

      return AE_NO_CORRECTION;
    }

    // Still active, return current amount
    accelValue = currentStatus.AEamount;

    // Check for increased acceleration (re-trigger)
    const int16_t activeDOT = (configPage2.aeMode == AE_MODE_MAP) ? currentStatus.mapDOT : currentStatus.tpsDOT;
    const int16_t activateThreshold = (configPage2.aeMode == AE_MODE_MAP) ? activateMAPDOT : activateTPSDOT;

    if (abs(activeDOT) > activateThreshold)
    {
      // Clear current phase, will restart below
      BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ACC);
      BIT_CLEAR(currentStatus.engine, BIT_ENGINE_DCC);
    }
    else
    {
      return accelValue; // Continue with current enrichment
    }
  }

  // Step 3: Try to activate new AE/DCC (if not already active)
  if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC) && !BIT_CHECK(currentStatus.engine, BIT_ENGINE_DCC))
  {
    if (configPage2.aeMode == AE_MODE_MAP)
    {
      accelValue = processNewActivation(currentStatus.mapDOT, MAP_change,
                                        configPage2.maeMinChange, configPage2.maeThresh,
                                        &activateMAPDOT, true);
    }
    else if (configPage2.aeMode == AE_MODE_TPS)
    {
      accelValue = processNewActivation(currentStatus.tpsDOT, TPS_change,
                                        configPage2.taeMinChange, configPage2.taeThresh,
                                        &activateTPSDOT, false);
    }
  }

  return accelValue;
}

/** Simple check to see whether we are cranking with the TPS above the flood clear threshold.
@return 100 (not cranking and thus no need for flood-clear) or 0 (Engine cranking and TPS above @ref config4.floodClear limit).
*/
byte correctionFloodClear(void)
{
  byte floodValue = 100;
  if( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  {
    //Engine is currently cranking, check what the TPS is
    if(currentStatus.TPS >= configPage4.floodClear)
    {
      //Engine is cranking and TPS is above threshold. Cut all fuel
      floodValue = 0;
    }
  }
  return floodValue;
}

/** Battery Voltage correction.
Uses a 2D enrichment table (WUETable) where the X axis is engine temp and the Y axis is the amount of extra fuel to add.
*/
byte correctionBatVoltage(void)
{
  byte batValue = 100;
  batValue = table2D_getValue(&injectorVCorrectionTable, currentStatus.battery10);
  return batValue;
}

/** Simple temperature based corrections lookup based on the inlet air temperature (IAT).
This corrects for changes in air density from movement of the temperature.
*/
byte correctionIATDensity(void)
{
  byte IATValue = 100;
  IATValue = table2D_getValue(&IATDensityCorrectionTable, temperatureAddOffset(currentStatus.IAT)); //currentStatus.IAT is the actual temperature, values in IATDensityCorrectionTable.axisX are temp+offset

  return IATValue;
}

/** Correction for current barometric / ambient pressure.
 * @returns A percentage value indicating the amount the fuelling should be changed based on the barometric reading. 100 = No change. 110 = 10% increase. 90 = 10% decrease
 */
byte correctionBaro(void)
{
  byte baroValue = 100;
  baroValue = table2D_getValue(&baroFuelTable, currentStatus.baro);

  return baroValue;
}

/** Launch control has a setting to increase the fuel load to assist in bringing up boost.
This simple check applies the extra fuel if we're currently launching
*/
byte correctionLaunch(void)
{
  byte launchValue = 100;
  if(currentStatus.launchingHard || currentStatus.launchingSoft) { launchValue = (100 + configPage6.lnchFuelAdd); }

  return launchValue;
}

/**
*/
byte correctionDFCOfuel(void)
{
  byte scaleValue = 100;
  if ( BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) )
  {
    if ( (configPage9.dfcoTaperEnable == 1) && (dfcoTaper != 0) )
    {
      //Do a check if the user reduced the duration while active to avoid overflow
      if (dfcoTaper > configPage9.dfcoTaperTime) { dfcoTaper = configPage9.dfcoTaperTime; }
      scaleValue = map(dfcoTaper, configPage9.dfcoTaperTime, 0, 100, configPage9.dfcoTaperFuel);
      if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { dfcoTaper--; }
    }
    else { scaleValue = 0; } //Taper ended or disabled, disable fuel
  }
  else { dfcoTaper = configPage9.dfcoTaperTime; } //Keep updating the duration until DFCO is active

  return scaleValue;
}

/*
 * Returns true if deceleration fuel cutoff should be on, false if its off
 */
bool correctionDFCO(void)
{
  bool DFCOValue = false;
  if ( configPage2.dfcoEnabled == 1 )
  {
    if ( BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) == 1 ) 
    {
      DFCOValue = ( currentStatus.RPM > ( configPage4.dfcoRPM * 10) ) && ( currentStatus.TPS < configPage4.dfcoTPSThresh ); 
      if ( DFCOValue == false) { dfcoDelay = 0; }
    }
    else 
    {
      if ( (currentStatus.TPS < configPage4.dfcoTPSThresh) && (currentStatus.coolant >= temperatureRemoveOffset(configPage2.dfcoMinCLT)) && ( currentStatus.RPM > (unsigned int)( (configPage4.dfcoRPM * 10U) + (configPage4.dfcoHyster * 2U)) ) )
      {
        if( dfcoDelay < configPage2.dfcoDelay )
        {
          if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { dfcoDelay++; }
        }
        else { DFCOValue = true; }
      }
      else { dfcoDelay = 0; } //Prevent future activation right away if previous time wasn't activated
    } // DFCO active check
  } // DFCO enabled check
  return DFCOValue;
}

/** Flex fuel adjustment to vary fuel based on ethanol content.
 * The amount of extra fuel required is a linear relationship based on the % of ethanol.
*/
byte correctionFlex(void)
{
  byte flexValue = 100;

  if (configPage2.flexEnabled == 1)
  {
    flexValue = table2D_getValue(&flexFuelTable, currentStatus.ethanolPct);
  }
  return flexValue;
}

/*
 * Fuel temperature adjustment to vary fuel based on fuel temperature reading
*/
byte correctionFuelTemp(void)
{
  byte fuelTempValue = 100;

  if (configPage2.flexEnabled == 1)
  {
    fuelTempValue = table2D_getValue(&fuelTempTable, temperatureAddOffset(currentStatus.fuelTemp));
  }
  return fuelTempValue;
}
#endif // REFACTORED - Fuel corrections module (END OF ALL 14 FUEL FUNCTIONS)


// ============================= Air Fuel Ratio (AFR) correction =============================

#if 0 // REFACTORED - Implementation moved to corrections/afr_corrections/afr_corrections.cpp
uint8_t calculateAfrTarget(table3d16RpmLoad &afrLookUpTable, const statuses &current, const config2 &page2, const config6 &page6) {
  //afrTarget value lookup must be done if O2 sensor is enabled, and always if incorporateAFR is enabled
  if (page2.incorporateAFR == true) {
    return get3DTableValue(&afrLookUpTable, current.fuelLoad, current.RPM);
  }
  if (page6.egoType!=EGO_TYPE_OFF)
  {
    //Determine whether the Y axis of the AFR target table tshould be MAP (Speed-Density) or TPS (Alpha-N)
    //Note that this should only run after the sensor warmup delay when using Include AFR option,
    if( current.runSecs > page6.ego_sdelay) {
      return get3DTableValue(&afrLookUpTable, current.fuelLoad, current.RPM);
    }
    return current.O2; //Catch all
  }
  return current.afrTarget;
}

/** Lookup the AFR target table and perform either a simple or PID adjustment based on this.

Simple (Best suited to narrowband sensors):
If the O2 sensor reports that the mixture is lean/rich compared to the desired AFR target, it will make a 1% adjustment
It then waits egoDelta number of ignition events and compares O2 against the target table again. If it is still lean/rich then the adjustment is increased to 2%.

This continues until either:
- the O2 reading flips from lean to rich, at which point the adjustment cycle starts again at 1% or
- the adjustment amount increases to egoLimit at which point it stays at this level until the O2 state (rich/lean) changes

PID (Best suited to wideband sensors):

*/
// ============================================================================
// REFACTORED: correctionAFRClosedLoop() - Guard Clauses Pattern
// Complexity reduced: 15+ -> 5
// Lines reduced: 67 -> 51 (24% reduction)
// Nesting reduced: 4 levels -> 2 levels
// ============================================================================

static const byte AFR_NO_CORRECTION = 100;
static const byte AFR_STEP = 1; // Simple algorithm step size (1%)

// Helper: Check if all closed-loop conditions are met (9 conditions)
static inline bool isClosedLoopActive(void)
{
  // Guard clauses - all conditions that must be true
  if (currentStatus.coolant <= temperatureRemoveOffset(configPage6.egoTemp)) { return false; }
  if (currentStatus.RPM <= (unsigned int)(configPage6.egoRPM * 100)) { return false; }
  if (currentStatus.TPS > configPage6.egoTPSMax) { return false; }
  if (currentStatus.O2 >= configPage6.ego_max) { return false; }
  if (currentStatus.O2 <= configPage6.ego_min) { return false; }
  if (currentStatus.runSecs <= configPage6.ego_sdelay) { return false; }
  if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) != 0) { return false; }
  if (currentStatus.MAP > (configPage9.egoMAPMax * 2)) { return false; }
  if (currentStatus.MAP < (configPage9.egoMAPMin * 2)) { return false; }

  return true; // All conditions met
}

// Helper: Simple AFR correction algorithm (1% steps)
static inline byte simpleAFRCorrection(byte currentCorrection)
{
  const byte maxCorrection = 100 + configPage6.egoLimit;
  const byte minCorrection = 100 - configPage6.egoLimit;

  // Running lean - add fuel
  if (currentStatus.O2 > currentStatus.afrTarget) {
    if (currentCorrection < maxCorrection) {
      return currentCorrection + AFR_STEP;
    }
  }
  // Running rich - remove fuel
  else if (currentStatus.O2 < currentStatus.afrTarget) {
    if (currentCorrection > minCorrection) {
      return currentCorrection - AFR_STEP;
    }
  }

  // At target or at limit
  return currentCorrection;
}

// Helper: PID AFR correction algorithm
static inline byte pidAFRCorrection(void)
{
  egoPID.SetOutputLimits((long)(-configPage6.egoLimit), (long)(configPage6.egoLimit));
  egoPID.SetTunings(configPage6.egoKP, configPage6.egoKI, configPage6.egoKD);
  PID_O2 = (long)(currentStatus.O2);
  PID_AFRTarget = (long)(currentStatus.afrTarget);

  if (egoPID.Compute()) {
    return 100 + PID_output;
  }

  return AFR_NO_CORRECTION;
}

// Main function - Refactored with Guard Clauses
byte correctionAFRClosedLoop(void)
{
  byte AFRValue = AFR_NO_CORRECTION;

  // Guard: O2 sensor disabled or DFCO active
  if (configPage6.egoType == 0) { return AFRValue; }
  if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) != 0) { return AFRValue; }

  // Preserve current correction while waiting for next cycle
  AFRValue = currentStatus.egoCorrection;

  // Guard: Ignition count not yet reached (overflow-safe check)
  if (((uint16_t)(ignitionCount - AFRnextCycle)) >= UINT16_HALF_RANGE) {
    return AFRValue;
  }

  // Update next cycle
  AFRnextCycle = ignitionCount + configPage6.egoCount;

  // Guard: Closed-loop conditions not met
  if (!isClosedLoopActive()) {
    return AFR_NO_CORRECTION;
  }

  // Select and execute algorithm
  if (configPage6.egoAlgorithm == EGO_ALGORITHM_SIMPLE) {
    AFRValue = simpleAFRCorrection(currentStatus.egoCorrection);
  }
  else if (configPage6.egoAlgorithm == EGO_ALGORITHM_PID) {
    AFRValue = pidAFRCorrection();
  }
  else {
    AFRValue = AFR_NO_CORRECTION; // Algorithm disabled
  }

  // Final authority limit enforcement
  const byte maxAuthority = 100 + configPage6.egoLimit;
  const byte minAuthority = 100 - configPage6.egoLimit;
  if (AFRValue < minAuthority) { AFRValue = minAuthority; }
  if (AFRValue > maxAuthority) { AFRValue = maxAuthority; }

  return AFRValue;
}
#endif // REFACTORED - AFR corrections module

#if 0 // REFACTORED - Implementation moved to corrections/ignition_corrections/ignition_corrections.cpp
//******************************** IGNITION ADVANCE CORRECTIONS ********************************
/** Dispatch calculations for all ignition related corrections.
 * @param base_advance - Base ignition advance (deg. ?)
 * @return Advance considering all (~12) individual corrections
 */
int8_t correctionsIgn(int8_t base_advance)
{
  int8_t advance;
  advance = correctionFlexTiming(base_advance);
  advance = correctionWMITiming(advance);
  advance = correctionIATretard(advance);
  advance = correctionCLTadvance(advance);
  advance = correctionIdleAdvance(advance);
  advance = correctionSoftRevLimit(advance);
  advance = correctionNitrous(advance);
  advance = correctionSoftLaunch(advance);
  advance = correctionSoftFlatShift(advance);
  advance = correctionKnockTiming(advance);

  advance = correctionDFCOignition(advance);

  //Fixed timing check must go last
  advance = correctionFixedTiming(advance);
  advance = correctionCrankingFixedTiming(advance); //This overrides the regular fixed timing, must come last

  return advance;
}
/** Correct ignition timing to configured fixed value.
 * Must be called near end to override all other corrections.
 */
int8_t correctionFixedTiming(int8_t advance)
{
  int8_t ignFixValue = advance;
  if (configPage2.fixAngEnable == 1) { ignFixValue = configPage4.FixAng; } //Check whether the user has set a fixed timing angle
  return ignFixValue;
}
/** Correct ignition timing to configured fixed value to use during craning.
 * Must be called near end to override all other corrections.
 */
int8_t correctionCrankingFixedTiming(int8_t advance)
{
  int8_t ignCrankFixValue = advance;
  if ( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  { 
    if ( configPage2.crkngAddCLTAdv == 0 ) { ignCrankFixValue = configPage4.CrankAng; } //Use the fixed cranking ignition angle
    else { ignCrankFixValue = correctionCLTadvance(configPage4.CrankAng); } //Use the CLT compensated cranking ignition angle
  }
  return ignCrankFixValue;
}

int8_t correctionFlexTiming(int8_t advance)
{
  int16_t ignFlexValue = advance;
  if( configPage2.flexEnabled == 1 ) //Check for flex being enabled
  {
    ignFlexValue = (int16_t) table2D_getValue(&flexAdvTable, currentStatus.ethanolPct) - OFFSET_IGNITION; //Negative values are achieved with offset
    currentStatus.flexIgnCorrection = (int8_t) ignFlexValue; //This gets cast to a signed 8 bit value to allows for negative advance (ie retard) values here. 
    ignFlexValue = (int8_t) advance + currentStatus.flexIgnCorrection;
  }
  return (int8_t) ignFlexValue;
}

int8_t correctionWMITiming(int8_t advance)
{
  if( (configPage10.wmiEnabled >= 1) && (configPage10.wmiAdvEnabled == 1) && !BIT_CHECK(currentStatus.status4, BIT_STATUS4_WMI_EMPTY) ) //Check for wmi being enabled
  {
    if( (currentStatus.TPS >= configPage10.wmiTPS) && (currentStatus.RPM >= configPage10.wmiRPM) && (currentStatus.MAP/2 >= configPage10.wmiMAP) && (temperatureAddOffset(currentStatus.IAT) >= configPage10.wmiIAT) )
    {
      return advance + (int8_t)table2D_getValue(&wmiAdvTable, (uint8_t)((uint16_t)currentStatus.MAP/2U)) - OFFSET_IGNITION; //Negative values are achieved with offset
    }
  }
  return advance;
}
/** Ignition correction for inlet air temperature (IAT).
 */
int8_t correctionIATretard(int8_t advance)
{
  int8_t advanceIATadjust = table2D_getValue(&IATRetardTable, (uint8_t)currentStatus.IAT);

  return advance - advanceIATadjust;
}
/** Ignition correction for coolant temperature (CLT).
 */
int8_t correctionCLTadvance(int8_t advance)
{
  int8_t ignCLTValue = advance;
  //Adjust the advance based on CLT.
  int8_t advanceCLTadjust = (int16_t)(table2D_getValue(&CLTAdvanceTable, temperatureAddOffset(currentStatus.coolant))) - 15;
  ignCLTValue = (advance + advanceCLTadjust);
  
  return ignCLTValue;
}
/** Ignition Idle advance correction.
 */
// ============================================================================
// REFACTORED: correctionIdleAdvance() - Guard Clauses Pattern
// Complexity reduced: 10 -> 6
// Lines reduced: 43 -> 38 (12% reduction)
// Nesting reduced: 4 levels -> 2 levels
// ============================================================================

static const int IDLE_RPM_DELTA_MIN = 0;
static const int IDLE_RPM_DELTA_MAX = 100; // ±500 RPM range
static const int IDLE_RPM_DELTA_OFFSET = 50; // Center offset

// Helper: Check if idle conditions are met
static inline bool isIdleConditionsActive(void)
{
  // Guard: RPM above idle threshold
  if (currentStatus.RPM >= (configPage2.idleAdvRPM * 100)) { return false; }

  // Guard: Vehicle moving (VSS check)
  if (configPage2.vssMode != 0) {
    if (currentStatus.vss >= configPage2.idleAdvVss) { return false; }
  }

  // Guard: Throttle not closed (check algorithm TPS vs CTPS)
  if (configPage2.idleAdvAlgorithm == 0) {
    // TPS-based
    if (currentStatus.TPS >= configPage2.idleAdvTPS) { return false; }
  }
  else {
    // CTPS-based
    if (currentStatus.CTPSActive != 1) { return false; }
  }

  return true;
}

// Main function - Refactored with Guard Clauses
int8_t correctionIdleAdvance(int8_t advance)
{
  int8_t ignIdleValue = advance;

  // Update idle advance active flag
  if (!idleAdvActive && BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) {
    // Activate when: No IAC or RPM above threshold
    if ((configPage6.iacAlgorithm == 0) ||
        (currentStatus.RPM > (((uint16_t)currentStatus.CLIdleTarget * 10) - (uint16_t)IGN_IDLE_THRESHOLD))) {
      idleAdvActive = true;
    }
  }
  else if (idleAdvActive && !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) {
    idleAdvActive = false;
  }

  // Guard: Idle advance disabled
  if (configPage2.idleAdvEnabled < 1) { return ignIdleValue; }

  // Guard: Delay not elapsed
  if (runSecsX10 < (configPage2.idleAdvDelay * 5)) { return ignIdleValue; }

  // Guard: Idle advance not active
  if (!idleAdvActive) { return ignIdleValue; }

  // Calculate RPM delta from target
  int idleRPMdelta = (currentStatus.CLIdleTarget - (currentStatus.RPM / 10)) + IDLE_RPM_DELTA_OFFSET;
  idleRPMdelta = constrain(idleRPMdelta, IDLE_RPM_DELTA_MIN, IDLE_RPM_DELTA_MAX);

  // Check if idle conditions are met
  if (!isIdleConditionsActive()) {
    idleAdvTaper = 0;
    return ignIdleValue;
  }

  // Taper delay before applying advance
  if (idleAdvTaper < configPage9.idleAdvStartDelay) {
    if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) {
      idleAdvTaper++;
    }
    return ignIdleValue;
  }

  // Calculate and apply idle advance
  int8_t advanceIdleAdjust = (int8_t)table2D_getValue(&idleAdvanceTable, (uint8_t)idleRPMdelta) - 15;

  if (configPage2.idleAdvEnabled == 1) {
    // Additive mode
    ignIdleValue = advance + advanceIdleAdjust;
  }
  else if (configPage2.idleAdvEnabled == 2) {
    // Override mode
    ignIdleValue = advanceIdleAdjust;
  }

  return ignIdleValue;
}
/** Ignition soft revlimit correction.
 */
int8_t correctionSoftRevLimit(int8_t advance)
{
  byte ignSoftRevValue = advance;
  BIT_CLEAR(currentStatus.status2, BIT_STATUS2_SFTLIM);

  if (configPage6.engineProtectType == PROTECT_CUT_IGN || configPage6.engineProtectType == PROTECT_CUT_BOTH) 
  {
    if (currentStatus.RPMdiv100 >= configPage4.SoftRevLim) //Softcut RPM limit
    {
      BIT_SET(currentStatus.status2, BIT_STATUS2_SFTLIM);
      if( softLimitTime < configPage4.SoftLimMax )
      {
        if (configPage2.SoftLimitMode == SOFT_LIMIT_RELATIVE) { ignSoftRevValue = ignSoftRevValue - configPage4.SoftLimRetard; } //delay timing by configured number of degrees in relative mode
        else if (configPage2.SoftLimitMode == SOFT_LIMIT_FIXED) { ignSoftRevValue = configPage4.SoftLimRetard; } //delay timing to configured number of degrees in fixed mode

        if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { softLimitTime++; }
      }
    }
    else if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { softLimitTime = 0; } //Only reset time at runSecsX10 update rate
  }

  return ignSoftRevValue;
}
/** Ignition Nitrous oxide correction.
 */
int8_t correctionNitrous(int8_t advance)
{
  byte ignNitrous = advance;
  //Check if nitrous is currently active
  if(configPage10.n2o_enable > 0)
  {
    //Check which stage is running (if any)
    if( (currentStatus.nitrous_status == NITROUS_STAGE1) || (currentStatus.nitrous_status == NITROUS_BOTH) )
    {
      ignNitrous -= configPage10.n2o_stage1_retard;
    }
    if( (currentStatus.nitrous_status == NITROUS_STAGE2) || (currentStatus.nitrous_status == NITROUS_BOTH) )
    {
      ignNitrous -= configPage10.n2o_stage2_retard;
    }
  }

  return ignNitrous;
}
/** Ignition soft launch correction.
 */
int8_t correctionSoftLaunch(int8_t advance)
{
  uint8_t ignSoftLaunchValue = advance;
  //SoftCut rev limit for 2-step launch control.
  if(  configPage6.launchEnabled && currentStatus.clutchTrigger && \
      (currentStatus.clutchEngagedRPM < ((unsigned int)(configPage6.flatSArm) * 100)) && \
      (currentStatus.RPM > ((unsigned int)(configPage6.lnchSoftLim) * 100)) && \
      (currentStatus.TPS >= configPage10.lnchCtrlTPS) && \
      ( (configPage2.vssMode == 0) || ((configPage2.vssMode > 0) && (currentStatus.vss <= configPage10.lnchCtrlVss)) ) \
    )
  {
    currentStatus.launchingSoft = true;
    BIT_SET(currentStatus.status2, BIT_STATUS2_SLAUNCH);
    ignSoftLaunchValue = configPage6.lnchRetard;
  }
  else
  {
    currentStatus.launchingSoft = false;
    BIT_CLEAR(currentStatus.status2, BIT_STATUS2_SLAUNCH);
  }

  return ignSoftLaunchValue;
}
/** Ignition correction for soft flat shift.
 */
int8_t correctionSoftFlatShift(int8_t advance)
{
  int8_t ignSoftFlatValue = advance;

  if(configPage6.flatSEnable && currentStatus.clutchTrigger && (currentStatus.clutchEngagedRPM > ((unsigned int)(configPage6.flatSArm) * 100)) && (currentStatus.RPM > (currentStatus.clutchEngagedRPM - (configPage6.flatSSoftWin * 100) ) ) )
  {
    BIT_SET(currentStatus.status5, BIT_STATUS5_FLATSS);
    ignSoftFlatValue = configPage6.flatSRetard;
  }
  else { BIT_CLEAR(currentStatus.status5, BIT_STATUS5_FLATSS); }

  return ignSoftFlatValue;
}


uint8_t _calculateKnockRecovery(uint8_t curKnockRetard)
{
  uint8_t tmpKnockRetard = curKnockRetard;
  //Check whether we are in knock recovery
  if((micros() - knockStartTime) > (configPage10.knock_duration * 100000UL)) //knock_duration is in seconds*10
  {
    //Calculate how many recovery steps have occurred since the 
    uint32_t timeInRecovery = (micros() - knockStartTime) - (configPage10.knock_duration * 100000UL);
    uint8_t recoverySteps = timeInRecovery / (configPage10.knock_recoveryStepTime * 100000UL);
    int8_t recoveryTimingAdj = 0;
    if(recoverySteps > knockLastRecoveryStep) 
    { 
      recoveryTimingAdj = (recoverySteps - knockLastRecoveryStep) * configPage10.knock_recoveryStep;
      knockLastRecoveryStep = recoverySteps;
    }

    if(recoveryTimingAdj < currentStatus.knockRetard)
    {
      //Add the timing back in provided we haven't reached the end of the recovery period
      tmpKnockRetard = currentStatus.knockRetard - recoveryTimingAdj;
    }
    else 
    {
      //Recovery is complete. Knock adjustment is set to 0 and we reset the knock status
      tmpKnockRetard = 0;
      BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
      knockStartTime = 0;
      currentStatus.knockCount = 0;
    }
  }

  return tmpKnockRetard;
}

// ============================================================================
// REFACTORED: correctionKnockTiming() - Strategy Pattern
// Complexity reduced: 12+ -> 6
// Lines reduced: 86 -> 65 (24% reduction)
// Nesting reduced: 4 levels -> 2 levels
// ============================================================================

// Helper: Calculate knock retard amount based on count
static inline byte calculateKnockRetard(void)
{
  return configPage10.knock_firstStep +
         ((currentStatus.knockCount - configPage10.knock_count) * configPage10.knock_stepSize);
}

// Helper: Digital knock detection algorithm
static inline byte knockDetectionDigital(void)
{
  byte tmpKnockRetard = 0;

  // Guard: Knock count below threshold
  if (currentStatus.knockCount < configPage10.knock_count) {
    BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE);
    return 0;
  }

  // Knock active - check for additional events
  if (BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE)) {
    tmpKnockRetard = currentStatus.knockRetard;

    // Check for new knock pulse
    if (BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE)) {
      // Guard: Step time not elapsed
      if ((micros() - knockStartTime) > (configPage10.knock_stepTime * 1000UL)) {
        currentStatus.knockCount++;
        tmpKnockRetard = calculateKnockRetard();
        knockStartTime = micros();
        knockLastRecoveryStep = 0;
      }
    }

    tmpKnockRetard = _calculateKnockRecovery(tmpKnockRetard);
  }
  else {
    // Activate knock retard
    knockStartTime = micros();
    tmpKnockRetard = calculateKnockRetard();
    BIT_SET(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
    knockLastRecoveryStep = 0;
  }

  BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE);
  return tmpKnockRetard;
}

// Helper: Analog knock detection algorithm
static inline byte knockDetectionAnalog(void)
{
  byte tmpKnockRetard = 0;

  // Knock active - check for additional events
  if (BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE)) {
    // Guard: Step time not elapsed
    if ((micros() - knockStartTime) > (configPage10.knock_stepTime * 1000UL)) {
      uint16_t tmpKnockReading = getAnalogKnock();

      if (tmpKnockReading > configPage10.knock_threshold) {
        currentStatus.knockCount++;
        tmpKnockRetard = calculateKnockRetard();
        knockStartTime = micros();
        knockLastRecoveryStep = 0;
      }
    }

    tmpKnockRetard = _calculateKnockRecovery(tmpKnockRetard);
  }
  else {
    // Poll analog sensor at 30Hz
    if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_30HZ)) {
      uint16_t tmpKnockReading = getAnalogKnock();

      if (tmpKnockReading > configPage10.knock_threshold) {
        knockStartTime = micros();
        tmpKnockRetard = configPage10.knock_firstStep;
        BIT_SET(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
        knockLastRecoveryStep = 0;
      }
    }
  }

  return tmpKnockRetard;
}

// Main function - Refactored with Strategy Pattern
int8_t correctionKnockTiming(int8_t advance)
{
  byte tmpKnockRetard = 0;

  // Strategy pattern - select knock algorithm
  if (configPage10.knock_mode == KNOCK_MODE_DIGITAL) {
    tmpKnockRetard = knockDetectionDigital();
  }
  else if (configPage10.knock_mode == KNOCK_MODE_ANALOG) {
    tmpKnockRetard = knockDetectionAnalog();
  }

  // Enforce maximum retard limit
  tmpKnockRetard = min(tmpKnockRetard, configPage10.knock_maxRetard);
  currentStatus.knockRetard = tmpKnockRetard;

  return advance - tmpKnockRetard;
}

/** Ignition DFCO taper correction.
 */
int8_t correctionDFCOignition(int8_t advance)
{
  int8_t dfcoRetard = advance;
  if ( (configPage9.dfcoTaperEnable == 1) && BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) )
  {
    if ( dfcoTaper != 0 )
    {
      dfcoRetard -= map(dfcoTaper, configPage9.dfcoTaperTime, 0, 0, configPage9.dfcoTaperAdvance);
    }
    else { dfcoRetard -= configPage9.dfcoTaperAdvance; } //Taper ended, use full value
  }
  else { dfcoTaper = configPage9.dfcoTaperTime; } //Keep updating the duration until DFCO is active
  return dfcoRetard;
}
#endif // REFACTORED - Ignition corrections module (END OF ALL 14 IGNITION FUNCTIONS)

#if 0 // REFACTORED - Implementation moved to corrections/dwell_corrections/dwell_corrections.cpp
/** Ignition Dwell Correction.
 */
uint16_t correctionsDwell(uint16_t dwell)
{
  uint16_t tempDwell = dwell;
  uint16_t sparkDur_uS = (configPage4.sparkDur * 100); //Spark duration is in mS*10. Multiple it by 100 to get spark duration in uS
  if(currentStatus.actualDwell == 0) { currentStatus.actualDwell = tempDwell; } //Initialise the actualDwell value if this is the first time being called

  //**************************************************************************************************************************
  //Pull battery voltage based dwell correction and apply if needed
  currentStatus.dwellCorrection = table2D_getValue(&dwellVCorrectionTable, currentStatus.battery10);
  if (currentStatus.dwellCorrection != 100) { tempDwell = div100(dwell) * currentStatus.dwellCorrection; }


  //**************************************************************************************************************************
  //Dwell error correction is a basic closed loop to keep the dwell time consistent even when adjusting its end time for the per tooth timing.
  //This is mostly of benefit to low resolution triggers at low rpm (<1500)
  if( (configPage2.perToothIgn  == true) && (configPage4.dwellErrCorrect == 1) )
  {
    int16_t error = tempDwell - currentStatus.actualDwell;
    if(tempDwell > INT16_MAX) { tempDwell = INT16_MAX; } //Prevent overflow when casting to signed int
    if(error > ((int16_t)tempDwell / 2)) { error += error; } //Double correction amount if actual dwell is less than 50% of the requested dwell
    if(error > 0) { tempDwell += error; }
  }

  //**************************************************************************************************************************
  /*
  Dwell limiter - If the total required dwell time per revolution is longer than the maximum time available at the current RPM, reduce dwell. This can occur if there are multiple sparks per revolution
  This only times this can occur are:
  1. Single channel spark mode where there will be nCylinders/2 sparks per revolution
  2. Rotary ignition in wasted spark configuration (FC/FD), results in 2 pulses per rev. RX-8 is fully sequential resulting in 1 pulse, so not required
  */
  uint16_t dwellPerRevolution = tempDwell + sparkDur_uS;
  int8_t pulsesPerRevolution = 1;
  if( ( (configPage4.sparkMode == IGN_MODE_SINGLE) || ((configPage4.sparkMode == IGN_MODE_ROTARY) && (configPage10.rotaryType != ROTARY_IGN_RX8)) ) && (configPage2.nCylinders > 1) ) //No point in running this for 1 cylinder engines
  {
    pulsesPerRevolution = (configPage2.nCylinders >> 1);
    dwellPerRevolution = dwellPerRevolution * pulsesPerRevolution;
  }
  if(dwellPerRevolution > revolutionTime)
  {
    //Possibly need some method of reducing spark duration here as well, but this is a start
    uint16_t adjustedSparkDur = udiv_32_16(sparkDur_uS * revolutionTime, dwellPerRevolution);
    tempDwell = udiv_32_16(revolutionTime, (uint16_t)pulsesPerRevolution) - adjustedSparkDur;
  }

  return tempDwell;
}
#endif // REFACTORED - Dwell corrections module
