/**
 * @file sensor_polling.cpp
 * @brief Sensor polling functions implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * All sensor polling organized by frequency using LOOP_TIMER mechanism.
 * Each function clears its timer bit and performs frequency-specific reads.
 */

#include "sensor_polling.h"
#include "sensors.h"
#include "auxiliaries.h"
#include "idle.h"
#include "auxiliaries.h"
#include "engineProtection.h"
#include "storage.h"
#include "comms.h"
#include "comms_legacy.h"
#include "corrections.h"
#include "comms_CAN.h"
#include "comms_secondary.h"
#include "SD_logger.h"
#include "decoders.h"
#include "scheduledIO.h"
#include "modularization_globals.h"
#include "engine_protection.h"
#include "utilities.h"
#include "units.h"
#include "init.h"

// External references to table objects
extern table2D_u8_u8_10 idleTargetTable;

//=============================================================================
// 1 kHz polling (every ~1ms)
//=============================================================================

void pollSensors1KHz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_1KHZ);
  readMAP();
}

//=============================================================================
// 200 Hz polling (every 5ms)
//=============================================================================

void pollSensors200Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_200HZ);

  #if defined(ANALOG_ISR)
    // ADC in free running mode does 1 complete conversion of all 16 channels
    // and then the interrupt is disabled. Every 200Hz we re-enable the interrupt
    // to get another conversion cycle
    BIT_SET(ADCSRA, ADIE); // Enable ADC interrupt
  #endif
}

//=============================================================================
// 50 Hz polling (every 20ms)
//=============================================================================

void pollSensors50Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_50HZ);

  #if defined(NATIVE_CAN_AVAILABLE)
    sendCANBroadcast(50);
  #endif
}

//=============================================================================
// 30 Hz polling (every 33ms)
//=============================================================================

void pollSensors30Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_30HZ);

  // Control loops - Most boost tends to run at about 30Hz
  boostControl();

  // VVT may eventually need to be synced with the cam readings (ie run once per cam rev)
  // but for now run at 30Hz
  vvtControl();

  // Water methanol injection
  wmiControl();

  // TPS read if configured for 30Hz
  #if TPS_READ_FREQUENCY == 30
    readTPS();
  #endif

  // O2 sensor reads (if not using CAN wideband)
  if(configPage2.canWBO == 0)
  {
    readO2();
    readO2_2();
  }

  #if defined(NATIVE_CAN_AVAILABLE)
    sendCANBroadcast(30);
  #endif

  #ifdef SD_LOGGING
    if(configPage13.onboard_log_file_rate == LOGGER_RATE_30HZ) {
      writeSDLogEntry();
    }
  #endif

  // Check for any outstanding EEPROM writes
  // Only write if serial is inactive and enough time has passed
  if((isEepromWritePending() == true)
      && (serialStatusFlag == SERIAL_INACTIVE)
      && (micros() > deferEEPROMWritesUntil))
  {
    writeAllConfig();
  }
}

//=============================================================================
// 15 Hz polling (every 66ms)
//=============================================================================

void pollSensors15Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_15HZ);

  // TPS read if configured for 15Hz
  // Any faster can upset the TPSdot sampling time
  #if TPS_READ_FREQUENCY == 15
    readTPS();
  #endif

  // Check for launch control and flat shift being active
  checkLaunchAndFlatShift();

  #if defined(NATIVE_CAN_AVAILABLE)
    sendCANBroadcast(15);
  #endif

  // Check whether the tooth log buffer is ready
  if(toothHistoryIndex > TOOTH_LOG_SIZE) {
    BIT_SET(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  }
}

//=============================================================================
// 10 Hz polling (every 100ms)
//=============================================================================

void pollSensors10Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_10HZ);

  // Programmable IO checks
  checkProgrammableIO();

  // Idle control - needs to run at 10Hz to align with the idle taper resolution of 0.1s
  idleControl();

  // Air conditioning control
  airConControl();

  // Vehicle speed and gear calculation
  currentStatus.vss = getSpeed();
  currentStatus.gear = getGear();

  #if defined(NATIVE_CAN_AVAILABLE)
    sendCANBroadcast(10);
  #endif

  #ifdef SD_LOGGING
    if(configPage13.onboard_log_file_rate == LOGGER_RATE_10HZ) {
      writeSDLogEntry();
    }
  #endif
}

//=============================================================================
// 4 Hz polling (every 250ms)
//=============================================================================

void pollSensors4Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_4HZ);

  // Slow sensor reads - IAT and CLT can be done less frequently (4 times per second)
  readCLT();
  readIAT();
  readBat();

  // Nitrous control
  nitrousControl();

  // Lookup the current target idle RPM
  // This is aligned with coolant and so needs to be calculated at the same rate CLT is read
  if((configPage2.idleAdvEnabled >= 1) || (configPage6.iacAlgorithm != IAC_ALGORITHM_NONE))
  {
    // All temps are offset by 40 degrees
    currentStatus.CLIdleTarget = (byte)table2D_getValue(&idleTargetTable,
                                  temperatureAddOffset(currentStatus.coolant));

    // Adds Idle Up RPM amount if A/C is turning on
    if(BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON)) {
      currentStatus.CLIdleTarget += configPage15.airConIdleUpRPMAdder;
    }
  }

  #ifdef SD_LOGGING
    if(configPage13.onboard_log_file_rate == LOGGER_RATE_4HZ) {
      writeSDLogEntry();
    }
  #endif

  // Fuel and oil pressure
  currentStatus.fuelPressure = getFuelPressure();
  currentStatus.oilPressure = getOilPressure();

  // Check auxiliary CAN inputs if enabled
  if(BIT_CHECK(statusSensors, BIT_SENSORS_AUX_ENBL))
  {
    readAuxiliaryInputs();
  }
}

//=============================================================================
// 1 Hz polling (every second)
//=============================================================================

void pollSensors1Hz(void)
{
  BIT_CLEAR(TIMER_mask, BIT_TIMER_1HZ);

  // System temperature
  currentStatus.systemTemp = getSystemTemp();

  // Infrequent baro readings are not an issue
  readBaro();

  // WMI indicator handling
  if((configPage10.wmiEnabled > 0) && (configPage10.wmiIndicatorEnabled > 0))
  {
    handleWMIIndicator();
  }

  #ifdef SD_LOGGING
    if(configPage13.onboard_log_file_rate == LOGGER_RATE_1HZ) {
      writeSDLogEntry();
    }

    // SD log sync can take up to 8ms on slow SD cards. To prevent potential issues
    // we only perform this if the RPM is under a safe speed so that there will always
    // be sufficient time for a main loop to run.
    // A sync will be forced if it hasn't taken place within a max period
    if((currentStatus.RPM < SD_SYNC_RPM_THRESHOLD)
        || (msSinceLastSDSync > SD_SYNC_MAX_TIME_PERIOD))
    {
      if(syncSDLog()) {
        msSinceLastSDSync = 0; // Run SD sync and reset
      }
    }
  #endif
}

//=============================================================================
// Auxiliary functions
//=============================================================================

/**
 * @brief Read all 16 auxiliary CAN input channels
 *
 * This is a complex function that handles reading auxiliary inputs from various sources:
 * - External CAN via secondary serial
 * - Internal CAN bus
 * - Local analog pins
 * - Local digital pins
 */
void readAuxiliaryInputs(void)
{
  // Check through the Aux input channels if enabled for CAN or local use
  for(byte AuxinChan = 0; AuxinChan < 16; AuxinChan++)
  {
    currentStatus.current_caninchannel = AuxinChan;

    // External CAN via secondary serial (condition already ensures enable_secondarySerial == 1)
    if(((configPage9.caninput_sel[currentStatus.current_caninchannel] & 12) == 4)
        && (((configPage9.enable_secondarySerial == 1) && ((configPage9.enable_intcan == 0) && (configPage9.intcan_available == 1)))
            || ((configPage9.enable_secondarySerial == 1) && ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 1))
                && ((configPage9.caninput_sel[currentStatus.current_caninchannel] & 64) == 0))
            || ((configPage9.enable_secondarySerial == 1) && ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 0)))))
    {
      // Current input channel is enabled as external & secondary serial enabled (Megas only)
      sendCancommand(2, 0, currentStatus.current_caninchannel, 0,
                    ((configPage9.caninput_source_can_address[currentStatus.current_caninchannel] & 2047) + 0x100));
    }
    // Internal CAN bus (condition already ensures enable_intcan == 1)
    else if(((configPage9.caninput_sel[currentStatus.current_caninchannel] & 12) == 4)
            && (((configPage9.enable_secondarySerial == 1) && ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 1))
                 && ((configPage9.caninput_sel[currentStatus.current_caninchannel] & 64) == 64))
                || ((configPage9.enable_secondarySerial == 0) && ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 1))
                    && ((configPage9.caninput_sel[currentStatus.current_caninchannel] & 128) == 128))))
    {
      #if defined(CORE_STM32)
        // Internal CAN is enabled (verified by outer condition)
        sendCancommand(3, configPage9.speeduino_tsCanId, currentStatus.current_caninchannel, 0,
                      ((configPage9.caninput_source_can_address[currentStatus.current_caninchannel] & 2047) + 0x100));
      #endif
    }
    // Analog local pin
    else if((((configPage9.enable_secondarySerial == 1) || ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 1)))
             && (configPage9.caninput_sel[currentStatus.current_caninchannel] & 12) == 8)
            || (((configPage9.enable_secondarySerial == 0) && ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 0)))
                && (configPage9.caninput_sel[currentStatus.current_caninchannel] & 3) == 2)
            || (((configPage9.enable_secondarySerial == 0) && (configPage9.enable_intcan == 0))
                && ((configPage9.caninput_sel[currentStatus.current_caninchannel] & 3) == 2)))
    {
      // Read analog channel specified
      currentStatus.canin[currentStatus.current_caninchannel] =
          readAuxanalog(pinTranslateAnalog(configPage9.Auxinpina[currentStatus.current_caninchannel] & 63));
    }
    // Digital local pin
    else if((((configPage9.enable_secondarySerial == 1) || ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 1)))
             && (configPage9.caninput_sel[currentStatus.current_caninchannel] & 12) == 12)
            || (((configPage9.enable_secondarySerial == 0) && ((configPage9.enable_intcan == 1) && (configPage9.intcan_available == 0)))
                && (configPage9.caninput_sel[currentStatus.current_caninchannel] & 3) == 3)
            || (((configPage9.enable_secondarySerial == 0) && (configPage9.enable_intcan == 0))
                && ((configPage9.caninput_sel[currentStatus.current_caninchannel] & 3) == 3)))
    {
      // Read digital channel specified
      currentStatus.canin[currentStatus.current_caninchannel] =
          readAuxdigital((configPage9.Auxinpinb[currentStatus.current_caninchannel] & 63) + 1);
    }
  } // For loop going through each channel
}

/**
 * @brief Handle WMI (Water Methanol Injection) empty indicator flashing
 *
 * If water tank is empty, flash the indicator with 1 second interval.
 * Otherwise, set indicator according to configured polarity.
 */
void handleWMIIndicator(void)
{
  // Water tank empty - flash with 1sec interval
  if(BIT_CHECK(currentStatus.status4, BIT_STATUS4_WMI_EMPTY) > 0)
  {
    digitalWrite(pinWMIIndicator, !digitalRead(pinWMIIndicator));
  }
  else
  {
    digitalWrite(pinWMIIndicator, configPage10.wmiIndicatorPolarity ? HIGH : LOW);
  }
}

/**
 * @brief Handle all engine stop procedures
 *
 * Called when time between teeth is too great (engine has stopped).
 * Resets all status flags and turns off outputs.
 */
void handleEngineStop(void)
{
  // Reset all RPM and status values
  currentStatus.RPM = 0;
  currentStatus.RPMdiv100 = 0;
  currentStatus.PW1 = 0;
  currentStatus.VE = 0;
  currentStatus.VE2 = 0;

  // Reset decoder and sync
  resetDecoder();
  currentStatus.hasSync = false;
  BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);

  // Reset counters
  currentStatus.runSecs = 0; // Reset the counter for number of seconds running
  currentStatus.startRevolutions = 0;
  resetMAPcycleAndEvent();
  currentStatus.rpmDOT = 0;
  AFRnextCycle = 0;
  ignitionCount = 0;

  // Turn off all channels
  ignitionChannelsOn = 0;
  fuelChannelsOn = 0;

  // Turn off fuel pump (but only if priming is complete)
  if(currentStatus.fpPrimed == true) {
    FUEL_PUMP_OFF();
    currentStatus.fuelPumpOn = false;
  }

  // Turn off idle PWM if configured
  if(configPage6.iacPWMrun == false) {
    disableIdle();
  }

  // Clear engine status bits
  BIT_CLEAR(currentStatus.engine, BIT_ENGINE_CRANK); // Clear cranking bit (can otherwise get stuck 'on' even with 0 rpm)
  BIT_CLEAR(currentStatus.engine, BIT_ENGINE_WARMUP); // Same as above except for WUE
  BIT_CLEAR(currentStatus.engine, BIT_ENGINE_RUN); // Same as above except for RUNNING status
  BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ASE); // Same as above except for ASE status
  BIT_CLEAR(currentStatus.engine, BIT_ENGINE_ACC); // Same as above but the accel enrich (If using MAP accel enrich a stall will cause this to trigger)
  BIT_CLEAR(currentStatus.engine, BIT_ENGINE_DCC); // Same as above but the decel enleanment

  // Safety check: If for some reason the interrupts have got screwed up (leading to 0rpm), this resets them.
  // This should only be run if the high speed logger is off because it will change the trigger interrupts
  // back to defaults rather than the logger versions
  if((currentStatus.toothLogEnabled == false) && (currentStatus.compositeTriggerUsed == 0)) {
    initialiseTriggers();
  }

  // Turn off VVT outputs
  VVT1_PIN_LOW();
  VVT2_PIN_LOW();
  DISABLE_VVT_TIMER();

  // Disable boost control
  boostDisable();

  // Reset ignition bypass ready for next crank attempt
  if(configPage4.ignBypassEnabled > 0) {
    digitalWrite(pinIgnBypass, LOW);
  }
}
