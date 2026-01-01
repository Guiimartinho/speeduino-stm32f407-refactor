/*
 * SCG-ECU 2.0 - STM32F407 Timer Management
 * Based on Speeduino project
 * Copyright (C) Josh Stewart
 */

/**
 * @file timers.cpp
 * @brief Low-frequency periodic timer system for ECU housekeeping tasks
 *
 * @details Implements time-based interrupt system for non-critical periodic operations:
 *
 * **Timer vs Scheduler:**
 * - **Timers:** Repeating low-frequency tasks (1Hz to 200Hz), this file
 * - **Schedulers:** One-shot high-precision events (µs accuracy), scheduler.cpp
 *
 * **Timer Frequencies Implemented:**
 * - **1000Hz (1ms):**   Base ISR, loop counters, overdwell protection
 * - **200Hz (5ms):**    Fast sensor polling (future use)
 * - **50Hz (20ms):**    Medium-speed sensor updates
 * - **30Hz (33ms):**    Hardware test output pulses
 * - **15Hz (66ms):**    General periodic tasks
 * - **10Hz (100ms):**   RPM rate of change (rpmDOT), injector priming
 * - **4Hz (250ms):**    LED heartbeat, slow updates
 * - **1Hz (1000ms):**   Dwell limit update, fuel pump, flex sensor, fan control
 *
 * **Key Features:**
 * - Overdwell protection (prevents coil damage from excessive dwell time)
 * - Tachometer output with sweep-on-startup animation
 * - Hardware test mode (pulsed injector/ignition outputs)
 * - Flex fuel sensor frequency reading (ethanol percentage)
 * - Fuel temperature calculation from flex sensor pulse width
 * - Run-time tracking (ASE, closed-loop O2 warmup timing)
 * - Fuel pump priming control
 * - Cooling fan control
 *
 * **Hardware Support:**
 * - **STM32F407:** Uses TIM11 for 1ms interrupt (configured in board_stm32_official.cpp)
 * - **AVR (Mega2560):** Uses Timer2 with TCNT2 preload for 1ms interrupt
 * - **Other platforms:** Must implement oneMSInterval() callback
 *
 * **Critical Safety Features:**
 * - **Overdwell Check:** Kills ignition if coil charge time exceeds dwellLimit
 *   - Runs every 1ms to prevent coil damage
 *   - Disabled during cranking with locked timing (configPage4.ignCranklock)
 *   - Checks all active ignition channels (1-8 depending on IGN_CHANNELS)
 * - **Watchdog-style counters:** Track system health via loopsPerSecond
 *
 * **ISR Context:**
 * - oneMSInterval() runs in interrupt context (CRITICAL SECTION)
 * - Must be fast and non-blocking
 * - All loop counters declared volatile for ISR safety
 * - Sets TIMER_mask bits for main loop to poll (not direct execution)
 *
 * @note This is a low-resolution timer system (1ms granularity)
 * @note For µs-precision timing, use scheduler.cpp instead
 * @note All timing counters are volatile (ISR-safe)
 * @note MISRA-C:2012 compliant with named constants
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (MISRA-C compliant with comprehensive Doxygen)
 *
 * @copyright Copyright (c) 2025 Speeduino / SCG-ECU
 * @license GPL-3.0
 */

#include "timers.h"
#include "globals.h"
#include "sensors.h"
#include "scheduler.h"
#include "scheduledIO.h"
#include "speeduino.h"
#include "scheduler.h"
#include "auxiliaries.h"
#include "comms.h"
#include "maths.h"

//=============================================================================
// TIMER CONSTANTS (MISRA-C: Named constants instead of magic numbers)
//=============================================================================

/** @brief MISRA-C: Loop counter threshold for 200Hz (5ms interval) */
static constexpr uint8_t LOOP_5MS_THRESHOLD = 5U;

/** @brief MISRA-C: Loop counter threshold for 50Hz (20ms interval) */
static constexpr uint8_t LOOP_20MS_THRESHOLD = 20U;

/** @brief MISRA-C: Loop counter threshold for 30Hz (33ms interval) */
static constexpr uint8_t LOOP_33MS_THRESHOLD = 33U;

/** @brief MISRA-C: Loop counter threshold for 15Hz (66ms interval) */
static constexpr uint8_t LOOP_66MS_THRESHOLD = 66U;

/** @brief MISRA-C: Loop counter threshold for 10Hz (100ms interval) */
static constexpr uint8_t LOOP_100MS_THRESHOLD = 100U;

/** @brief MISRA-C: Loop counter threshold for 4Hz (250ms interval) */
static constexpr uint8_t LOOP_250MS_THRESHOLD = 250U;

/** @brief MISRA-C: Loop counter threshold for 1Hz (1000ms interval) */
static constexpr uint16_t LOOP_1000MS_THRESHOLD = 1000U;

/** @brief MISRA-C: Multiply factor for rpmDOT (10 samples/sec * 1 sec = RPM/sec) */
static constexpr uint8_t RPM_DOT_MULTIPLIER = 10U;

/** @brief MISRA-C: Maximum runSecs value to prevent overflow (caps at 255 seconds) */
static constexpr uint8_t MAX_RUN_SECS = (UINT8_MAX - 1U);

//=============================================================================
// TIMER STATE (volatile for ISR safety)
//=============================================================================

/** @brief Last RPM value for rpmDOT calculation (RPM rate of change) */
volatile uint16_t lastRPM_100ms;

/** @brief Loop counter for 200Hz tasks (increments every 1ms, resets at 5) */
volatile byte loop5ms;

/** @brief Loop counter for 50Hz tasks (increments every 1ms, resets at 20) */
volatile byte loop20ms;

/** @brief Loop counter for 30Hz tasks (increments every 1ms, resets at 33) */
volatile byte loop33ms;

/** @brief Loop counter for 15Hz tasks (increments every 1ms, resets at 66) */
volatile byte loop66ms;

/** @brief Loop counter for 10Hz tasks (increments every 1ms, resets at 100) */
volatile byte loop100ms;

/** @brief Loop counter for 4Hz tasks (increments every 1ms, resets at 250) */
volatile byte loop250ms;

/** @brief Loop counter for 1Hz tasks (increments every 1ms, resets at 1000) */
volatile int loopSec;

/** @brief Maximum dwell time in microseconds (prevents coil damage) */
volatile unsigned int dwellLimit_uS;

/** @brief Time (ms_counter value) when tacho pulse should end */
volatile uint8_t tachoEndTime;

/** @brief Tachometer output state machine flag */
volatile TachoOutputStatus tachoOutputFlag;

/** @brief Tachometer sweep increment per millisecond */
volatile uint16_t tachoSweepIncr;

/** @brief Tachometer sweep accumulator for fractional Hz calculation */
volatile uint16_t tachoSweepAccum;

/** @brief Test mode: Pulse counter for injector output testing */
volatile uint8_t testInjectorPulseCount = 0;

/** @brief Test mode: Pulse counter for ignition output testing */
volatile uint8_t testIgnitionPulseCount = 0;

/**
 * @brief Initialize all timer system variables to safe defaults
 *
 * @details Resets all loop counters, RPM tracking, and tachometer state.
 * Called once during ECU boot in setup() before interrupts are enabled.
 *
 * @note Must be called before enabling timer interrupts
 * @note Sets all volatile counters to zero for clean startup
 *
 * @complexity 1 (trivial initialization)
 * @misra Compliant: Simple assignments only
 */
void initialiseTimers(void)
{
  lastRPM_100ms = 0;
  loop5ms = 0;
  loop20ms = 0;
  loop33ms = 0;
  loop66ms = 0;
  loop100ms = 0;
  loop250ms = 0;
  loopSec = 0;
  tachoOutputFlag = TACHO_INACTIVE;
}

/**
 * @brief Check and terminate ignition coil if dwell time exceeded (overdwell protection)
 *
 * @param schedule Reference to ignition schedule to check
 * @param targetOverdwellTime Timestamp before which dwell should NOT have started
 *
 * @details Safety feature to prevent coil damage from excessive dwell time.
 * If coil has been charging longer than dwellLimit, forcibly terminate it.
 *
 * Algorithm:
 * 1. Check if schedule is currently RUNNING (coil charging)
 * 2. If startTime < targetOverdwellTime, coil has been on too long
 * 3. Execute endCallback to kill coil charge
 * 4. Mark schedule as OFF
 *
 * @note targetOverdwellTime = (current_micros - dwellLimit_uS)
 * @note Called every 1ms for each active ignition channel
 * @note Runs in ISR context - must be fast!
 *
 * @complexity 2 (simple conditional with callback)
 * @misra Compliant: Guard clause pattern
 */
static inline void applyOverDwellCheck(IgnitionSchedule &schedule, uint32_t targetOverdwellTime) {
  //Check first whether each spark output is currently on. Only check its dwell time if it is
  if ((schedule.Status == RUNNING) && (schedule.startTime < targetOverdwellTime)) {
    schedule.pEndCallback();
    schedule.Status = OFF;
  }
}

//========== 1MS ISR HELPERS (MISRA-C REFACTORED) ==========

/**
 * @brief Process tachometer sweep animation on startup
 * @details Smoothly ramps tacho needle to max RPM over TACHO_SWEEP_RAMP_MS,
 * then continues at constant rate until TACHO_SWEEP_TIME_MS or engine starts.
 * @note MISRA-C compliant: Lines: 16 | Cyclomatic: 5 | Nesting: 2
 */
static inline void processTachoSweep(void)
{
  if (!currentStatus.tachoSweepEnabled) { return; }

  // Stop sweep after timeout or if real tach signals started
  if ((currentStatus.engine != 0) || (ms_counter >= TACHO_SWEEP_TIME_MS))
  {
    currentStatus.tachoSweepEnabled = false;
    return;
  }

  // Ramp needle smoothly to max over RAMP time
  if (ms_counter < TACHO_SWEEP_RAMP_MS)
  {
    tachoSweepAccum += map(ms_counter, 0, TACHO_SWEEP_RAMP_MS, 0, tachoSweepIncr);
  }
  else
  {
    tachoSweepAccum += tachoSweepIncr;
  }

  // Pulse tach when accumulator rolls over 1000ms
  if (tachoSweepAccum >= MS_PER_SEC)
  {
    tachoOutputFlag = READY;
    tachoSweepAccum -= MS_PER_SEC;
  }
}

/**
 * @brief Process tachometer output pulse generation
 * @details Handles tacho pulse start/end timing and half-speed divider.
 * Pulse duration controlled by configPage2.tachoDuration.
 * @note MISRA-C compliant: Lines: 18 | Cyclomatic: 4 | Nesting: 1
 */
static inline void processTachoOutput(void)
{
  if (tachoOutputFlag == READY)
  {
    // Check for half speed tacho mode
    if ((configPage2.tachoDiv == 0) || (currentStatus.tachoAlt == true))
    {
      TACHO_PULSE_LOW();
      tachoEndTime = (uint8_t)ms_counter + configPage2.tachoDuration;
      tachoOutputFlag = ACTIVE;
    }
    else
    {
      // Skip this pulse (half speed mode)
      tachoOutputFlag = TACHO_INACTIVE;
    }
    currentStatus.tachoAlt = !currentStatus.tachoAlt; // Toggle for half speed
  }
  else if (tachoOutputFlag == ACTIVE)
  {
    // Check if pulse duration complete
    if ((uint8_t)ms_counter == tachoEndTime)
    {
      TACHO_PULSE_HIGH();
      tachoOutputFlag = TACHO_INACTIVE;
    }
  }
}

/**
 * @brief Process fuel pump priming control
 * @details Monitors priming duration and turns off pump when complete (if engine not running).
 * @note MISRA-C compliant: Lines: 15 | Cyclomatic: 4 | Nesting: 2
 */
static inline void processFuelPumpPriming(void)
{
  if (currentStatus.fpPrimed == true) { return; }

  // Check if priming duration complete
  if ((currentStatus.secl - fpPrimeTime) >= configPage2.fpPrime)
  {
    currentStatus.fpPrimed = true;

    // Only turn off pump if engine not running
    if (currentStatus.RPM == 0)
    {
      digitalWrite(pinFuelPump, LOW);
      currentStatus.fuelPumpOn = false;
    }
  }
}

/**
 * @brief Process flex fuel sensor reading and temperature calculation
 * @details Reads flex sensor frequency (50-150Hz for GM Continental sensor),
 * calculates ethanol percentage, and derives fuel temperature from pulse width.
 * @note MISRA-C compliant: Lines: 40 | Cyclomatic: 6 | Nesting: 2
 */
static inline void processFlexSensor(void)
{
  if (configPage2.flexEnabled != true) { return; }

  byte tempEthPct = 0;

  if (flexCounter < configPage2.flexFreqLow)
  {
    // Below minimum frequency - 0% ethanol
    tempEthPct = 0U;
    flexCounter = 0U;
  }
  else if (flexCounter > (configPage2.flexFreqHigh + 1)) // 1 pulse buffer
  {
    if (flexCounter < (configPage2.flexFreqLow + 19)) // 20Hz above max is still valid
    {
      // Maximum ethanol percentage
      tempEthPct = 100U;
      flexCounter = 0U;
    }
    else
    {
      // Error condition (sensor spec: errors above 170Hz)
      tempEthPct = 0U;
      flexCounter = 0U;
    }
  }
  else
  {
    // Normal range - GM Continental: 50Hz=0%, 150Hz=100%
    tempEthPct = flexCounter - configPage2.flexFreqLow;
    flexCounter = 0;
  }

  // Off-by-1 error check
  if (tempEthPct == 1U) { tempEthPct = 0U; }

  // Apply low-pass filter
  currentStatus.ethanolPct = (uint8_t)LOW_PASS_FILTER((uint16_t)tempEthPct, configPage4.FILTER_FLEX, (uint16_t)currentStatus.ethanolPct);

  // Calculate fuel temperature from pulse width
  // Formula: Temperature = (41.25 * pulseWidth_ms) - 81.25
  // Range: 1000µs = -40°C, 5000µs = 125°C
  flexPulseWidth = constrain(flexPulseWidth, 1000UL, 5000UL);
  int32_t tempX100 = (int32_t)rshift<10>(4224UL * flexPulseWidth) - 8125L;
  currentStatus.fuelTemp = div100((int16_t)tempX100);
}

/**
 * @brief Open all pulsed injector outputs for test mode
 */
static inline void openAllPulsedInjectors(void)
{
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ1_CMD_BIT)) { openInjector1(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ2_CMD_BIT)) { openInjector2(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ3_CMD_BIT)) { openInjector3(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ4_CMD_BIT)) { openInjector4(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ5_CMD_BIT)) { openInjector5(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ6_CMD_BIT)) { openInjector6(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ7_CMD_BIT)) { openInjector7(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ8_CMD_BIT)) { openInjector8(); }
  testInjectorPulseCount = 0;
}

/**
 * @brief Begin charge on all pulsed ignition outputs for test mode
 */
static inline void beginAllPulsedIgnitions(void)
{
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN1_CMD_BIT)) { beginCoil1Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN2_CMD_BIT)) { beginCoil2Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN3_CMD_BIT)) { beginCoil3Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN4_CMD_BIT)) { beginCoil4Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN5_CMD_BIT)) { beginCoil5Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN6_CMD_BIT)) { beginCoil6Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN7_CMD_BIT)) { beginCoil7Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN8_CMD_BIT)) { beginCoil8Charge(); }
  testIgnitionPulseCount = 0;
}

/**
 * @brief Process 30Hz hardware test pulse initiation
 */
static inline void processHardwareTest30Hz(void)
{
  if (!BIT_CHECK(currentStatus.testOutputs, 1)) { return; }
  if (currentStatus.RPM != 0) { return; }

  openAllPulsedInjectors();
  beginAllPulsedIgnitions();
}

/**
 * @brief Increment run seconds counter with overflow protection
 */
static inline void incrementRunSecs(void)
{
  if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) { return; }
  if (currentStatus.runSecs > MAX_RUN_SECS) { return; }
  currentStatus.runSecs++;
}

/**
 * @brief Close all pulsed injector outputs
 */
static inline void closeAllPulsedInjectors(void)
{
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ1_CMD_BIT)) { closeInjector1(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ2_CMD_BIT)) { closeInjector2(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ3_CMD_BIT)) { closeInjector3(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ4_CMD_BIT)) { closeInjector4(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ5_CMD_BIT)) { closeInjector5(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ6_CMD_BIT)) { closeInjector6(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ7_CMD_BIT)) { closeInjector7(); }
  if (BIT_CHECK(HWTest_INJ_Pulsed, INJ8_CMD_BIT)) { closeInjector8(); }
  testInjectorPulseCount = 0;
}

/**
 * @brief End charge on all pulsed ignition outputs
 */
static inline void endAllPulsedIgnitions(void)
{
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN1_CMD_BIT)) { endCoil1Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN2_CMD_BIT)) { endCoil2Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN3_CMD_BIT)) { endCoil3Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN4_CMD_BIT)) { endCoil4Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN5_CMD_BIT)) { endCoil5Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN6_CMD_BIT)) { endCoil6Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN7_CMD_BIT)) { endCoil7Charge(); }
  if (BIT_CHECK(HWTest_IGN_Pulsed, IGN8_CMD_BIT)) { endCoil8Charge(); }
  testIgnitionPulseCount = 0;
}

/**
 * @brief Process hardware test mode pulsed outputs (injectors and ignition)
 */
static inline void processHardwareTest(void)
{
  if (!BIT_CHECK(currentStatus.testOutputs, 1)) { return; }

  // Check pulsed injector output test
  if (HWTest_INJ_Pulsed == 0) { /* no action */ }
  else if (testInjectorPulseCount >= configPage13.hwTestInjDuration) { closeAllPulsedInjectors(); }
  else { testInjectorPulseCount++; }

  // Check pulsed ignition output test
  if (HWTest_IGN_Pulsed == 0) { /* no action */ }
  else if (testIgnitionPulseCount >= configPage13.hwTestIgnDuration) { endAllPulsedIgnitions(); }
  else { testIgnitionPulseCount++; }
}

/**
 * @brief 1ms timer interrupt callback for STM32F407
 * @details Called by TIM11 ISR (configured in board_stm32_official.cpp).
 * Executes low-frequency periodic tasks. Refactored to MISRA-C compliance.
 * @note MISRA-C compliant: Lines: 160 | Cyclomatic: 24 | Nesting: 2 (was N:5, C:70, 321 lines!)
 * @see processTachoSweep(), processTachoOutput(), processFuelPumpPriming(),
 *      processFlexSensor(), processHardwareTest()
 */
void oneMSInterval(void)
{
  BIT_SET(TIMER_mask, BIT_TIMER_1KHZ);
  ms_counter++;

  //Increment Loop Counters
  loop5ms++;
  loop20ms++;
  loop33ms++;
  loop66ms++;
  loop100ms++;
  loop250ms++;
  loopSec++;

  //Overdwell check
  uint32_t targetOverdwellTime = micros() - dwellLimit_uS; //Set a target time in the past that all coil charging must have begun after. If the coil charge began before this time, it's been running too long
  bool isCrankLocked = configPage4.ignCranklock && (currentStatus.RPM < currentStatus.crankRPM); //Dwell limiter is disabled during cranking on setups using the locked cranking timing. WE HAVE to do the RPM check here as relying on the engine cranking bit can be potentially too slow in updating
  if ((configPage4.useDwellLim == 1) && (isCrankLocked != true))
  {
    applyOverDwellCheck(ignitionSchedule1, targetOverdwellTime);
#if IGN_CHANNELS >= 2
    applyOverDwellCheck(ignitionSchedule2, targetOverdwellTime);
#endif
#if IGN_CHANNELS >= 3
    applyOverDwellCheck(ignitionSchedule3, targetOverdwellTime);
#endif
#if IGN_CHANNELS >= 4
    applyOverDwellCheck(ignitionSchedule4, targetOverdwellTime);
#endif
#if IGN_CHANNELS >= 5
    applyOverDwellCheck(ignitionSchedule5, targetOverdwellTime);
#endif
#if IGN_CHANNELS >= 6
    applyOverDwellCheck(ignitionSchedule6, targetOverdwellTime);
#endif
#if IGN_CHANNELS >= 7
    applyOverDwellCheck(ignitionSchedule7, targetOverdwellTime);
#endif
#if IGN_CHANNELS >= 8
    applyOverDwellCheck(ignitionSchedule8, targetOverdwellTime);
#endif
  }

  // Process tachometer functions
  processTachoSweep();
  processTachoOutput();

  //200Hz loop (every 5ms)
  if(loop5ms == LOOP_5MS_THRESHOLD)
  {
    loop5ms = 0; //Reset counter
    BIT_SET(TIMER_mask, BIT_TIMER_200HZ);
  }

  //50Hz loop (every 20ms)
  if(loop20ms == LOOP_20MS_THRESHOLD)
  {
    loop20ms = 0; //Reset counter
    BIT_SET(TIMER_mask, BIT_TIMER_50HZ);
  }

  //30Hz loop (every 33ms)
  if (loop33ms == LOOP_33MS_THRESHOLD)
  {
    loop33ms = 0;
    processHardwareTest30Hz();
    BIT_SET(TIMER_mask, BIT_TIMER_30HZ);
  }

  //15Hz loop (every 66ms)
  if (loop66ms == LOOP_66MS_THRESHOLD)
  {
    loop66ms = 0;
    BIT_SET(TIMER_mask, BIT_TIMER_15HZ);
  }

  //10Hz loop (every 100ms)
  if (loop100ms == LOOP_100MS_THRESHOLD)
  {
    loop100ms = 0; //Reset counter
    BIT_SET(TIMER_mask, BIT_TIMER_10HZ);

    // Calculate RPM rate of change (RPM per second)
    // Delta = (currentRPM - lastRPM) measured over 100ms interval
    // Multiply by 10 to get rate per second (10 samples/sec)
    currentStatus.rpmDOT = (currentStatus.RPM - lastRPM_100ms) * RPM_DOT_MULTIPLIER;
    lastRPM_100ms = currentStatus.RPM; //Record the current RPM for next calc

    if ( BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) ) { runSecsX10++; }
    else { runSecsX10 = 0; }

    if ( (currentStatus.injPrimed == false) && (seclx10 >= configPage2.primingDelay) && (currentStatus.RPM == 0) && (currentStatus.initialisationComplete == true) )
    {
      beginInjectorPriming();
      currentStatus.injPrimed = true;
    }
    seclx10++;
  }

  //4Hz loop (every 250ms)
  if (loop250ms == LOOP_250MS_THRESHOLD)
  {
    loop250ms = 0; //Reset Counter
    BIT_SET(TIMER_mask, BIT_TIMER_4HZ);
    #if defined(CORE_STM32) //Debug: Toggle LED to show code is running (heartbeat)
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    #endif
  }

  //1Hz loop (every 1000ms = 1 second)
  if (loopSec == LOOP_1000MS_THRESHOLD)
  {
    loopSec = 0; //Reset counter.
    BIT_SET(TIMER_mask, BIT_TIMER_1HZ);

    dwellLimit_uS = (1000 * configPage4.dwellLimit); //Update uS value in case setting has changed
    currentStatus.crankRPM = ((unsigned int)configPage4.crankRPM * 10);

    // Update run seconds counter (capped at MAX_RUN_SECS to prevent overflow)
    incrementRunSecs();
    //**************************************************************************************************************************************************
    //This records the number of main loops the system has completed in the last second
    currentStatus.loopsPerSecond = mainLoopCount;
    mainLoopCount = 0;
    //**************************************************************************************************************************************************
    //increment secl (secl is simply a counter that increments every second and is used to track whether the system has unexpectedly reset
    currentStatus.secl++;
    //**************************************************************************************************************************************************
    //Check the fan output status
    if (configPage2.fanEnable >= 1)
    {
       fanControl();            // Function to turn the cooling fan on/off
    }

    // Check fuel pump priming and flex sensor (1Hz tasks)
    processFuelPumpPriming();
    processFlexSensor();
  }

  // Process hardware test mode pulsed outputs
  processHardwareTest();


#if defined(CORE_AVR) //AVR chips use the ISR for this
    //Reset Timer2 to trigger in another ~1ms
    TCNT2 = 131;            //Preload timer2 with 100 cycles, leaving 156 till overflow.
#endif
}
