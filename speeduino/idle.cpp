/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/

/**
 * @file idle.cpp
 * @brief Idle Air Control (IAC) valve/stepper motor control algorithms
 *
 * @details Implements multiple idle control strategies for engine idle speed regulation:
 *
 * **IAC Algorithms Supported:**
 * - **NONE:** No idle control (manual throttle only)
 * - **ON/OFF:** Simple binary valve control (temperature-based)
 * - **PWM_OL:** PWM open-loop (temperature table lookup)
 * - **PWM_CL:** PWM closed-loop (PID targeting RPM)
 * - **PWM_OLCL:** PWM open+closed loop (table feedforward + PID)
 * - **STEP_OL:** Stepper open-loop (temperature table lookup)
 * - **STEP_CL:** Stepper closed-loop (PID targeting RPM)
 * - **STEP_OLCL:** Stepper open+closed loop (table feedforward + PID)
 *
 * **Key Features:**
 * - PID control with configurable gains (Kp, Ki, Kd)
 * - Cranking vs running mode transitions with taper
 * - Idle-up support for A/C compressor load compensation
 * - Air conditioning load compensation
 * - Stepper motor homing sequence
 * - Dual-channel PWM support (normal/reversed)
 * - Feedforward + feedback control (OLCL modes)
 *
 * **Hardware Support:**
 * - PWM-driven IAC valves (GM, Ford, etc.)
 * - Stepper motors (4-wire bipolar, DRV8825 driver)
 * - Dual-channel PWM (e.g., VW N75 valve)
 *
 * @note All functions follow MISRA-C:2012 guidelines
 * @note PWM algorithms use hardware timer interrupt (idleInterrupt)
 * @note Stepper algorithms use timing-based state machine
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (MISRA-C compliant with Doxygen documentation)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#include "idle.h"
#include "maths.h"
#include "timers.h"
#include "utilities.h"
#include "src/PID_v1/PID_v1.h"
#include "units.h"

#define STEPPER_LESS_AIR_DIRECTION() ((configPage9.iacStepperInv == 0) ? STEPPER_BACKWARD : STEPPER_FORWARD)
#define STEPPER_MORE_AIR_DIRECTION() ((configPage9.iacStepperInv == 0) ? STEPPER_FORWARD : STEPPER_BACKWARD)

byte idleUpOutputHIGH = HIGH; // Used to invert the idle Up Output
byte idleUpOutputLOW = LOW;   // Used to invert the idle Up Output
byte idleCounter; //Used for tracking the number of calls to the idle control function
uint8_t idleTaper;

struct StepperIdle idleStepper;
bool idleOn; //Simply tracks whether idle was on last time around
byte idleInitComplete = 99; //Tracks which idle method was initialised. 99 is a method that will never exist
unsigned int iacStepTime_uS;
unsigned int iacCoolTime_uS;
unsigned int completedHomeSteps;

volatile bool idle_pwm_state;
bool lastDFCOValue;
volatile uint16_t idle_pwm_max_count; //Used for variable PWM frequency, volatile for ISR access
volatile unsigned int idle_pwm_cur_value;
long idle_pid_target_value;
long FeedForwardTerm;
volatile unsigned long idle_pwm_target_value; //volatile for ISR access
long idle_cl_target_rpm;

PORT_TYPE idle_pin_port;
PINMASK_TYPE idle_pin_mask;
PORT_TYPE idle2_pin_port;
PINMASK_TYPE idle2_pin_mask;
PORT_TYPE idleUpOutput_pin_port;
PINMASK_TYPE idleUpOutput_pin_mask;

static table2D_u8_u8_10 iacPWMTable(&configPage6.iacBins, &configPage6.iacOLPWMVal);
static table2D_u8_u8_10 iacStepTable(&configPage6.iacBins, &configPage6.iacOLStepVal);
//Open loop tables specifically for cranking
static table2D_u8_u8_4 iacCrankStepsTable(&configPage6.iacCrankBins, &configPage6.iacCrankSteps);
static table2D_u8_u8_4 iacCrankDutyTable(&configPage6.iacCrankBins, &configPage6.iacCrankDuty);

/*
These functions cover the PWM and stepper idle control
*/

/*
Idle Control
Currently limited to on/off control and open loop PWM and stepper drive
*/
integerPID idlePID(&currentStatus.longRPM, &idle_pid_target_value, &idle_cl_target_rpm, configPage6.idleKP, configPage6.idleKI, configPage6.idleKD, DIRECT); //This is the PID object if that algorithm is used. Needs to be global as it maintains state outside of each function call

//Any common functions associated with starting the Idle
//Typically this is enabling the PWM interrupt
static inline void enableIdle(void)
{
  if( (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_CL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_OL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_OLCL) )
  {
    IDLE_TIMER_ENABLE();
  }
  else if ( (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_CL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL) )
  {

  }
}

// ============================================================================
// Idle ISR Pin Control Helpers (file scope - not inside namespace)
// ============================================================================

// Teensy41 has inverted logic for idle pins
#if defined (CORE_TEENSY41)
  #define IDLE_DIR0_ACTIVE_LOW_PIN1   IDLE_PIN_HIGH
  #define IDLE_DIR0_ACTIVE_LOW_PIN2   IDLE2_PIN_LOW
  #define IDLE_DIR1_ACTIVE_LOW_PIN1   IDLE_PIN_LOW
  #define IDLE_DIR1_ACTIVE_LOW_PIN2   IDLE2_PIN_HIGH
  #define IDLE_DIR0_ACTIVE_HIGH_PIN1  IDLE_PIN_LOW
  #define IDLE_DIR0_ACTIVE_HIGH_PIN2  IDLE2_PIN_HIGH
  #define IDLE_DIR1_ACTIVE_HIGH_PIN1  IDLE_PIN_HIGH
  #define IDLE_DIR1_ACTIVE_HIGH_PIN2  IDLE2_PIN_LOW
#else
  #define IDLE_DIR0_ACTIVE_LOW_PIN1   IDLE_PIN_LOW
  #define IDLE_DIR0_ACTIVE_LOW_PIN2   IDLE2_PIN_HIGH
  #define IDLE_DIR1_ACTIVE_LOW_PIN1   IDLE_PIN_HIGH
  #define IDLE_DIR1_ACTIVE_LOW_PIN2   IDLE2_PIN_LOW
  #define IDLE_DIR0_ACTIVE_HIGH_PIN1  IDLE_PIN_HIGH
  #define IDLE_DIR0_ACTIVE_HIGH_PIN2  IDLE2_PIN_LOW
  #define IDLE_DIR1_ACTIVE_HIGH_PIN1  IDLE_PIN_LOW
  #define IDLE_DIR1_ACTIVE_HIGH_PIN2  IDLE2_PIN_HIGH
#endif

static inline void idleISR_setPins_ActiveLow(void)
{
  bool dualChannel = (configPage6.iacChannels == 1);
  if (configPage6.iacPWMdir == 0) { IDLE_DIR0_ACTIVE_LOW_PIN1(); if (dualChannel) { IDLE_DIR0_ACTIVE_LOW_PIN2(); } }
  else { IDLE_DIR1_ACTIVE_LOW_PIN1(); if (dualChannel) { IDLE_DIR1_ACTIVE_LOW_PIN2(); } }
}

static inline void idleISR_setPins_ActiveHigh(void)
{
  bool dualChannel = (configPage6.iacChannels == 1);
  if (configPage6.iacPWMdir == 0) { IDLE_DIR0_ACTIVE_HIGH_PIN1(); if (dualChannel) { IDLE_DIR0_ACTIVE_HIGH_PIN2(); } }
  else { IDLE_DIR1_ACTIVE_HIGH_PIN1(); if (dualChannel) { IDLE_DIR1_ACTIVE_HIGH_PIN2(); } }
}

static inline void disableIdle_PWM(void)
{
  IDLE_TIMER_DISABLE();
  bool dualChannel = (configPage6.iacChannels == 1);
  if (configPage6.iacPWMdir == 0) { IDLE_PIN_LOW(); if (dualChannel) { IDLE2_PIN_HIGH(); } }
  else { IDLE_PIN_HIGH(); if (dualChannel) { IDLE2_PIN_LOW(); } }
}

namespace {

// ============================================================================
// Idle Initialization Helpers (per-algorithm)
// ============================================================================

static inline void initialiseIdle_None(void)
{
  // No idle control - no initialization needed
}

static inline void initialiseIdle_OnOff(void)
{
  if ((temperatureAddOffset(currentStatus.coolant)) < configPage6.iacFastTemp)
  {
    IDLE_PIN_HIGH();
    idleOn = true;
  }
}

static inline void initialiseIdle_PWM_OL(void)
{
  enableIdle();
}

static inline void initialiseIdle_PWM_CL(void)
{
  idlePID.SetOutputLimits(percentage(configPage2.iacCLminValue, idle_pwm_max_count<<2),
                          percentage(configPage2.iacCLmaxValue, idle_pwm_max_count<<2));
  idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
  idlePID.SetMode(AUTOMATIC);
  idle_pid_target_value = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
  idlePID.Initialize();
  idleCounter = 0;
}

static inline void initialiseIdle_PWM_OLCL(void)
{
  idlePID.SetOutputLimits(percentage(configPage2.iacCLminValue, idle_pwm_max_count<<2),
                          percentage(configPage2.iacCLmaxValue, idle_pwm_max_count<<2));
  idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
  idlePID.SetMode(AUTOMATIC);
  idle_pid_target_value = 0;
  idlePID.Initialize();
  idleCounter = 0;
}

static inline void initialiseIdle_STEP_OL(bool forcehoming)
{
  iacStepTime_uS = configPage6.iacStepTime * 1000;
  iacCoolTime_uS = configPage9.iacCoolTime * 1000;

  if (forcehoming)
  {
    completedHomeSteps = 0;
    idleStepper.curIdleStep = 0;
    idleStepper.stepperStatus = SOFF;
  }

  configPage6.iacPWMrun = false;
}

static inline void initialiseIdle_STEP_CL(bool forcehoming)
{
  iacStepTime_uS = configPage6.iacStepTime * 1000;
  iacCoolTime_uS = configPage9.iacCoolTime * 1000;

  if (forcehoming)
  {
    completedHomeSteps = 0;
    idleStepper.curIdleStep = 0;
    idleStepper.stepperStatus = SOFF;
  }

  idlePID.SetSampleTime(250);
  idlePID.SetOutputLimits((configPage2.iacCLminValue * 3)<<2, (configPage2.iacCLmaxValue * 3)<<2);
  idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
  idlePID.SetMode(AUTOMATIC);
  configPage6.iacPWMrun = false;
  idle_pid_target_value = currentStatus.CLIdleTarget * 3;
  idlePID.Initialize();
}

static inline void initialiseIdle_STEP_OLCL(bool forcehoming)
{
  iacStepTime_uS = configPage6.iacStepTime * 1000;
  iacCoolTime_uS = configPage9.iacCoolTime * 1000;

  if (forcehoming)
  {
    completedHomeSteps = 0;
    idleStepper.curIdleStep = 0;
    idleStepper.stepperStatus = SOFF;
  }

  idlePID.SetSampleTime(250);
  idlePID.SetOutputLimits((configPage2.iacCLminValue * 3)<<2, (configPage2.iacCLmaxValue * 3)<<2);
  idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
  idlePID.SetMode(AUTOMATIC);
  configPage6.iacPWMrun = false;
  idle_pid_target_value = 0;
  idlePID.Initialize();
}

// ============================================================================
// Stepper State Machine Helpers (must be before disableIdle_Stepper)
// ============================================================================

static inline bool handleStepperState_STEPPING(unsigned int iacStepTime_uS)
{
  if(micros() > (idleStepper.stepStartTime + iacStepTime_uS) )
  {
    digitalWrite(pinStepperStep, LOW);
    idleStepper.stepStartTime = micros();
    idleStepper.stepperStatus = COOLING;
    return true;
  }
  return true;
}

/** @brief Helper to disable stepper when within hysteresis band
 *  @complexity Low (C=2, N=2)
 */
static inline void disableStepperIfOnTarget(void)
{
  if ( (idleStepper.curIdleStep >= (idleStepper.targetIdleStep - configPage6.iacStepHyster)) &&
       (idleStepper.curIdleStep <= (idleStepper.targetIdleStep + configPage6.iacStepHyster)) )
  {
    digitalWrite(pinStepperEnable, HIGH);
  }
}

/** @brief Handle stepper COOLING state
 *  @complexity Low (C=3, N:2) - reduced from N:3
 */
static inline bool handleStepperState_COOLING(unsigned int iacCoolTime_uS)
{
  // Guard: still cooling
  if(micros() <= (idleStepper.stepStartTime + iacCoolTime_uS) ) { return true; }

  // Cooling complete
  idleStepper.stepperStatus = SOFF;

  if(configPage9.iacStepperPower == STEPPER_POWER_WHEN_ACTIVE)
  {
    disableStepperIfOnTarget();
  }

  return false;
}

static inline byte checkForStepping(void)
{
  if (idleStepper.stepperStatus == STEPPING)
  {
    return handleStepperState_STEPPING(iacStepTime_uS);
  }
  else if (idleStepper.stepperStatus == COOLING)
  {
    return handleStepperState_COOLING(iacCoolTime_uS);
  }
  return false;
}

/** @brief Helper to execute a single stepper step
 *  @complexity Low (C=1, N=1)
 */
static inline void executeStepperStep(int16_t error)
{
  if (error < 0)
  {
    digitalWrite(pinStepperDir, STEPPER_LESS_AIR_DIRECTION() );
    idleStepper.curIdleStep--;
  }
  else
  {
    digitalWrite(pinStepperDir, STEPPER_MORE_AIR_DIRECTION() );
    idleStepper.curIdleStep++;
  }

  digitalWrite(pinStepperEnable, LOW);
  digitalWrite(pinStepperStep, HIGH);
  idleStepper.stepStartTime = micros();
  idleStepper.stepperStatus = STEPPING;
  idleOn = true;
  BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
}

/** @brief Perform one stepper step if target differs from current position
 *  @complexity Low (C=3, N:2) - reduced from N:3
 */
static inline void doStep(void)
{
  int16_t error = idleStepper.targetIdleStep - idleStepper.curIdleStep;

  // Check if step is needed (outside hysteresis band)
  if ( (error < -((int8_t)configPage6.iacStepHyster)) || (error > configPage6.iacStepHyster) )
  {
    executeStepperStep(error);
  }
  else
  {
    BIT_CLEAR(currentStatus.status2, BIT_STATUS2_IDLE);
  }
}

/**
 * @brief Check if stepper homing is complete, with timeout protection
 * @return true if homed or timed out, false if still homing
 *
 * @details Stepper homing drives the valve to the closed position by stepping
 *          backwards until mechanical stop. A timeout prevents infinite homing
 *          if the stepper gets stuck (e.g., mechanical jam, driver failure).
 *
 * @note Timeout is 30 seconds - typical homing takes 5-10 seconds
 */
static inline byte isStepperHomed(void)
{
  static unsigned long homingStartTime = 0UL;
  static bool homingTimedOut = false;
  static constexpr unsigned long HOMING_TIMEOUT_MS = 30000UL;  // 30 second timeout

  bool isHomed = true;

  // Check if we need to start homing
  if( completedHomeSteps < (configPage6.iacStepHome * 3) )
  {
    // Record homing start time on first step
    if (completedHomeSteps == 0)
    {
      homingStartTime = millis();
      homingTimedOut = false;
    }

    // Check for timeout
    if ((millis() - homingStartTime) > HOMING_TIMEOUT_MS)
    {
      // Timeout occurred - abort homing and mark as "complete" to prevent infinite loop
      homingTimedOut = true;
      completedHomeSteps = configPage6.iacStepHome * 3;  // Force complete
      idleStepper.curIdleStep = 0;  // Assume at home position
      digitalWrite(pinStepperEnable, HIGH);  // Disable stepper to save power
      // Could set an error flag here if status4 has a spare bit
      return true;  // Report as homed (even though it timed out)
    }

    // Normal homing step
    digitalWrite(pinStepperDir, STEPPER_LESS_AIR_DIRECTION() );
    digitalWrite(pinStepperEnable, LOW);
    digitalWrite(pinStepperStep, HIGH);
    idleStepper.stepStartTime = micros();
    idleStepper.stepperStatus = STEPPING;
    completedHomeSteps++;
    idleOn = true;
    isHomed = false;
  }
  return isHomed;
}

/** @brief Disable idle for stepper - set to crank position
 *  @complexity Low (C=3, N:2) - reduced from N:3
 */
static inline void disableIdle_Stepper(void)
{
  // Guard: only execute when stepper is ready
  if( (checkForStepping() != false) || (isStepperHomed() != true) ) { return; }

  idleStepper.targetIdleStep = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3;
  if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }

  if (idleStepper.targetIdleStep > (configPage9.iacMaxSteps * 3) )
  {
    idleStepper.targetIdleStep = configPage9.iacMaxSteps * 3;
  }

  idle_pid_target_value = idleStepper.targetIdleStep<<2;
}

// ============================================================================
// STEP_CL/OLCL Helpers
// ============================================================================

static inline void handleStepperCranking(void)
{
  idleStepper.targetIdleStep = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3;
  if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }

  if (idleStepper.targetIdleStep > (configPage9.iacMaxSteps * 3) )
  {
    idleStepper.targetIdleStep = configPage9.iacMaxSteps * 3;
  }

  idleTaper = 0;
  idle_pid_target_value = idleStepper.targetIdleStep << 2;
  idlePID.ResetIntegeral();
  FeedForwardTerm = idle_pid_target_value;
}

static inline void handleStepperTaper(void)
{
  uint16_t minValue = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3;
  if( idle_pid_target_value < minValue<<2 ) { idle_pid_target_value = minValue<<2; }

  uint16_t maxValue = idle_pid_target_value>>2;
  if( configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL )
  {
    maxValue = table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3;
  }

  FeedForwardTerm = map(idleTaper, 0, configPage2.idleTaperTime, minValue, maxValue)<<2;
  idleTaper++;
  idle_pid_target_value = FeedForwardTerm;
}

static inline void handleStepperRunning_OLCL(void)
{
  FeedForwardTerm = (table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3)<<2;

  if (((currentStatus.RPM - idle_cl_target_rpm) > configPage2.iacRPMlimitHysteresis*10) ||
      (currentStatus.TPS > configPage2.iacTPSlimit) || lastDFCOValue )
  {
    idlePID.ResetIntegeral();
  }
}

// ============================================================================
// STEP_CL/OLCL Running Mode Helpers
// ============================================================================

/** @brief Handle stepper running mode logic (10Hz update)
 *  @complexity Low (C=4, N=3)
 */
static inline void handleStepperRunning_10Hz(void)
{
  idle_cl_target_rpm = (uint16_t)currentStatus.CLIdleTarget * 10;

  if( idleTaper < configPage2.idleTaperTime )
  {
    handleStepperTaper();
  }
  else if (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL)
  {
    handleStepperRunning_OLCL();
  }
  else
  {
    FeedForwardTerm = idle_pid_target_value;
  }
}

/** @brief Apply idle adders (idleUp + airCon) to target step
 *  @complexity Low (C=3, N:2)
 */
static inline void applyIdleAdders(void)
{
  if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
  {
    idleStepper.targetIdleStep += configPage15.airConIdleSteps;
  }

  if(currentStatus.idleUpActive == true)
  {
    idleStepper.targetIdleStep += configPage2.idleUpAdder;
  }
}

/** @brief Update idle PID tunings and timings at 1Hz
 *  @complexity Low (C=1, N=1)
 */
static inline void updateIdleTunings1Hz(void)
{
  idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
  iacStepTime_uS = configPage6.iacStepTime * 1000;
  iacCoolTime_uS = configPage9.iacCoolTime * 1000;
}

// ============================================================================
// STEP_OL Helpers
// ============================================================================

/** @brief Handle stepper open-loop running mode with taper
 *  @complexity Medium (C=4, N:2)
 */
static inline void handleStepperOL_Running_10Hz(void)
{
  if ( idleTaper < configPage2.idleTaperTime )
  {
    idleStepper.targetIdleStep = map(idleTaper, 0, configPage2.idleTaperTime,\
    table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3,\
    table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3);
    idleTaper++;
  }
  else
  {
    idleStepper.targetIdleStep = table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3;
  }

  if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }
  if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
  {
    idleStepper.targetIdleStep += configPage15.airConIdleSteps;
  }

  iacStepTime_uS = configPage6.iacStepTime * 1000;
  iacCoolTime_uS = configPage9.iacCoolTime * 1000;
}

} // anonymous namespace

void initialiseIdle(bool forcehoming)
{
  IDLE_TIMER_DISABLE();

  idle_pin_port = portOutputRegister(digitalPinToPort(pinIdle1));
  idle_pin_mask = digitalPinToBitMask(pinIdle1);
  idle2_pin_port = portOutputRegister(digitalPinToPort(pinIdle2));
  idle2_pin_mask = digitalPinToBitMask(pinIdle2);

  switch(configPage6.iacAlgorithm)
  {
    case IAC_ALGORITHM_NONE:       initialiseIdle_None();             break;
    case IAC_ALGORITHM_ONOFF:      initialiseIdle_OnOff();            break;
    case IAC_ALGORITHM_PWM_OL:     initialiseIdle_PWM_OL();           break;
    case IAC_ALGORITHM_PWM_CL:     initialiseIdle_PWM_CL();           break;
    case IAC_ALGORITHM_PWM_OLCL:   initialiseIdle_PWM_OLCL();         break;
    case IAC_ALGORITHM_STEP_OL:    initialiseIdle_STEP_OL(forcehoming); break;
    case IAC_ALGORITHM_STEP_CL:    initialiseIdle_STEP_CL(forcehoming); break;
    case IAC_ALGORITHM_STEP_OLCL:  initialiseIdle_STEP_OLCL(forcehoming); break;
    default: break;
  }

  initialiseIdleUpOutput();
  idleInitComplete = configPage6.iacAlgorithm;
  currentStatus.idleLoad = 0;
}

void initialiseIdleUpOutput(void)
{
  if (configPage2.idleUpOutputInv == 1) { idleUpOutputHIGH = LOW; idleUpOutputLOW = HIGH; }
  else { idleUpOutputHIGH = HIGH; idleUpOutputLOW = LOW; }

  if(configPage2.idleUpEnabled > 0) { digitalWrite(pinIdleUpOutput, idleUpOutputLOW); } //Initialise program with the idle up output in the off state if it is enabled.
  currentStatus.idleUpOutputActive = false;

  idleUpOutput_pin_port = portOutputRegister(digitalPinToPort(pinIdleUpOutput));
  idleUpOutput_pin_mask = digitalPinToBitMask(pinIdleUpOutput);
}

/**
 * Handle IdleUp output pin control.
 *
 * Checks IdleUp input pin status (with polarity support) and controls
 * the corresponding output pin if enabled. Updates currentStatus flags.
 *
 * @note IdleUp is typically used for A/C compressor load compensation
 * @note Supports both normal (ground-switched) and inverted (5V) polarity
 * @complexity Low (C=5, N:2) - reduced from N:3
 */
static inline void handleIdleUpOutput(void)
{
  // Guard: idleUp disabled
  if (configPage2.idleUpEnabled != true)
  {
    currentStatus.idleUpActive = false;
    return;
  }

  // Read input pin with polarity support
  if (configPage2.idleUpPolarity == 0) { currentStatus.idleUpActive = !digitalRead(pinIdleUp); }
  else { currentStatus.idleUpActive = digitalRead(pinIdleUp); }

  // Control output pin if enabled
  if (configPage2.idleUpOutputEnabled == true)
  {
    if (currentStatus.idleUpActive == true)
    {
      digitalWrite(pinIdleUpOutput, idleUpOutputHIGH);
      currentStatus.idleUpOutputActive = true;
    }
    else
    {
      digitalWrite(pinIdleUpOutput, idleUpOutputLOW);
      currentStatus.idleUpOutputActive = false;
    }
  }
}

/**
 * Handle IAC_ALGORITHM_NONE - No idle control.
 *
 * This is a no-op function that exists for documentation purposes
 * and maintains consistent switch/case structure.
 */
static inline void handleIdle_None(void)
{
  // No idle control
}

/**
 * Handle IAC_ALGORITHM_ONOFF - Simple on/off idle control.
 *
 * Implements basic binary idle control based on coolant temperature.
 * If temperature is below threshold, turn valve fully ON.
 * Otherwise, turn valve OFF.
 *
 * @note This is the simplest IAC algorithm, suitable for simple solenoid valves
 */
static void handleIdle_OnOff(void)
{
  if ( (temperatureAddOffset(currentStatus.coolant)) < configPage6.iacFastTemp)
  {
    IDLE_PIN_HIGH();
    idleOn = true;
    BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
    currentStatus.idleLoad = 100;
  }
  else if (idleOn)
  {
    IDLE_PIN_LOW();
    idleOn = false;
    BIT_CLEAR(currentStatus.status2, BIT_STATUS2_IDLE);
    currentStatus.idleLoad = 0;
  }
}

/**
 * Handle IAC_ALGORITHM_PWM_OL - PWM open-loop idle control.
 *
 * Implements temperature-based PWM idle control using lookup tables.
 * Uses cranking table during cranking/pre-run, then transitions to
 * running table with optional taper period.
 *
 * Supports:
 * - Cranking duty cycle (iacCrankDutyTable)
 * - Running duty cycle (iacPWMTable)
 * - Taper transition between cranking and running
 * - IdleUp adder for A/C load compensation
 * - Air conditioning idle-up
 *
 * @note This is open-loop (no RPM feedback), suitable for simple systems
 * @complexity Medium (C=7, N:2) - reduced from N:3
 */
static void handleIdle_PWM_OL(void)
{
  // Cranking mode
  if( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  {
    currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
    idleTaper = 0;
  }
  // Pre-run mode (with iacPWMrun enabled)
  else if ( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) && configPage6.iacPWMrun == true)
  {
    currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
    idleTaper = 0;
  }
  // Running mode - taper transition
  else if ( idleTaper < configPage2.idleTaperTime )
  {
    currentStatus.idleLoad = map(idleTaper, 0, configPage2.idleTaperTime,\
    table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant)),\
    table2D_getValue(&iacPWMTable, temperatureAddOffset(currentStatus.coolant)));
    if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { idleTaper++; }
    if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
    {
      currentStatus.idleLoad += configPage15.airConIdleSteps;
    }
  }
  // Running mode - steady state
  else
  {
    currentStatus.idleLoad = table2D_getValue(&iacPWMTable, temperatureAddOffset(currentStatus.coolant));
    if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
    {
      currentStatus.idleLoad += configPage15.airConIdleSteps;
    }
  }

  // Apply idle up adder
  if(currentStatus.idleUpActive == true) { currentStatus.idleLoad += configPage2.idleUpAdder; }

  // Clamp and convert to PWM value
  if( currentStatus.idleLoad > 100 ) { currentStatus.idleLoad = 100; }
  idle_pwm_target_value = percentage(currentStatus.idleLoad, idle_pwm_max_count);
}

/**
 * Handle cranking idle for PWM closed-loop algorithms.
 *
 * Common cranking logic shared by PWM_CL and PWM_OLCL algorithms.
 * Uses cranking duty table and initializes PID for smooth transition.
 *
 * @return true if cranking or pre-run (caller should skip running logic)
 * @return false if engine is running (caller should proceed with running logic)
 * @complexity Low (C=4, N:2) - reduced from N:3
 */
static inline bool handleCrankingIdlePWM(void)
{
  // Cranking mode
  if( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  {
    currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
    idle_pwm_target_value = percentage(currentStatus.idleLoad, idle_pwm_max_count);
    idle_pid_target_value = idle_pwm_target_value << 2;
    idlePID.Initialize();
    return true;
  }

  // Pre-run mode
  if ( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) )
  {
    if( configPage6.iacPWMrun == true)
    {
      currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
      idle_pwm_target_value = percentage(currentStatus.idleLoad, idle_pwm_max_count);
    }
    return true;
  }

  return false; // Running mode
}

/** @brief Apply air con and idle up adders to PID output (PWM_CL)
 *  @complexity Low (C=4, N:2)
 */
static inline long applyPWM_CL_Adders(long target_value)
{
  // Air conditioning adder
  if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
  {
    target_value += percentage(configPage15.airConIdleSteps, idle_pwm_max_count<<2);
    if(target_value > (idle_pwm_max_count<<2)) { target_value = (idle_pwm_max_count<<2); }
  }

  // Idle up adder
  if(currentStatus.idleUpActive == true)
  {
    target_value += percentage(configPage2.idleUpAdder, idle_pwm_max_count<<2);
    if(target_value > (idle_pwm_max_count<<2)) { target_value = (idle_pwm_max_count<<2); }
  }

  return target_value;
}

/**
 * Handle IAC_ALGORITHM_PWM_CL - PWM closed-loop idle control.
 *
 * Implements PID-based idle control targeting CLIdleTarget RPM.
 * Uses PID output directly without feedforward term.
 *
 * Supports:
 * - Cranking duty cycle (iacCrankDutyTable)
 * - PID control for running (no feedforward)
 * - Air conditioning idle-up (added to PID output)
 * - IdleUp adder (added to PID output)
 *
 * @param[in,out] PID_computed Set to true if PID was computed this cycle
 *
 * @note This is closed-loop (RPM feedback via PID)
 * @complexity Medium (C=5, N:2) - reduced from N:3
 */
static void handleIdle_PWM_CL(bool &PID_computed)
{
  if( handleCrankingIdlePWM() ) { return; }

  idle_cl_target_rpm = (uint16_t)currentStatus.CLIdleTarget * 10;
  if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_1HZ) ) { idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD); }

  PID_computed = idlePID.Compute(true);

  if(PID_computed == true)
  {
    long TEMP_idle_pwm_target_value = applyPWM_CL_Adders(idle_pid_target_value);

    idle_pwm_target_value = TEMP_idle_pwm_target_value>>2;
    currentStatus.idleLoad = udiv_32_16(idle_pwm_target_value * 100UL, idle_pwm_max_count);
  }
  idleCounter++;
}

/** @brief Calculate feedforward term with air con and idle up adders (PWM_OLCL)
 *  @complexity Low (C=4, N:2)
 */
static inline long calculatePWM_OLCL_Feedforward(void)
{
  long feedforward = percentage(table2D_getValue(&iacPWMTable, temperatureAddOffset(currentStatus.coolant)), idle_pwm_max_count<<2);

  // Air conditioning adder
  if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
  {
    feedforward += percentage(configPage15.airConIdleSteps, (idle_pwm_max_count<<2));
    if(feedforward > (idle_pwm_max_count<<2)) { feedforward = (idle_pwm_max_count<<2); }
  }

  // Idle up adder
  if(currentStatus.idleUpActive == true)
  {
    feedforward += percentage(configPage2.idleUpAdder, (idle_pwm_max_count<<2));
    if(feedforward > (idle_pwm_max_count<<2)) { feedforward = (idle_pwm_max_count<<2); }
  }

  return feedforward;
}

/**
 * Handle IAC_ALGORITHM_PWM_OLCL - PWM open+closed loop idle control.
 *
 * Implements PID-based idle control with open-loop table as feedforward term.
 * Combines OL table lookup with CL PID correction for optimal response.
 *
 * Supports:
 * - Cranking duty cycle (iacCrankDutyTable)
 * - Running duty cycle table as feedforward (iacPWMTable)
 * - PID control with feedforward
 * - Air conditioning idle-up (added to feedforward)
 * - IdleUp adder (added to feedforward)
 * - Integral reset on TPS/RPM limits
 *
 * @param[in,out] PID_computed Set to true if PID was computed this cycle
 *
 * @note This combines OL and CL for better transient response
 * @complexity Medium (C=6, N:2) - reduced from N:3
 */
static void handleIdle_PWM_OLCL(bool &PID_computed)
{
  if( handleCrankingIdlePWM() ) { return; }

  FeedForwardTerm = calculatePWM_OLCL_Feedforward();

  idle_cl_target_rpm = (uint16_t)currentStatus.CLIdleTarget * 10;
  if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_1HZ) ) { idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD); }
  if((currentStatus.RPM - idle_cl_target_rpm > configPage2.iacRPMlimitHysteresis*10) || (currentStatus.TPS > configPage2.iacTPSlimit))
  {
    idlePID.ResetIntegeral();
  }

  PID_computed = idlePID.Compute(true, FeedForwardTerm);

  if(PID_computed == true)
  {
    idle_pwm_target_value = idle_pid_target_value>>2;
    currentStatus.idleLoad = ((unsigned long)(idle_pwm_target_value * 100UL) / idle_pwm_max_count);
  }
  idleCounter++;
}

/**
 * Limit stepper target to configured max steps and update currentStatus.idleLoad.
 *
 * Common helper for all stepper algorithms to enforce max step limit
 * and calculate idleLoad percentage for display/logging.
 *
 * @note idleLoad is divided by 2 if max steps exceeds UINT8_MAX
 */
static inline void limitStepperMaxSteps(void)
{
  if (idleStepper.targetIdleStep > (configPage9.iacMaxSteps * 3) )
  {
    idleStepper.targetIdleStep = configPage9.iacMaxSteps * 3;
  }
  if( ((uint16_t)configPage9.iacMaxSteps * 3) > UINT8_MAX ) { currentStatus.idleLoad = idleStepper.curIdleStep / 2; }
  else { currentStatus.idleLoad = idleStepper.curIdleStep; }
}

/**
 * Handle IAC_ALGORITHM_STEP_OL - Stepper open-loop idle control.
 *
 * Implements temperature-based stepper control using lookup tables.
 * Uses cranking table during cranking, then transitions to running table
 * with optional taper period.
 *
 * Refactored from 37 lines, Nesting: N:4 → N:2
 * Complexity: C:8 → C:5
 *
 * Supports:
 * - Cranking steps (iacCrankStepsTable)
 * - Running steps (iacStepTable)
 * - Taper transition between cranking and running
 * - IdleUp adder
 * - Air conditioning idle-up
 *
 * @note Only executes when stepper is homed and not currently stepping
 * @complexity Medium (C=5, N=2)
 */
static void handleIdle_STEP_OL(void)
{
  // Guard: only execute when stepper is ready
  if( (checkForStepping() != false) || (isStepperHomed() != true) ) { return; }

  // Cranking mode
  if( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) )
  {
    idleStepper.targetIdleStep = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3;
    if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }
    idleTaper = 0;
    limitStepperMaxSteps();
    doStep();
    return;
  }

  // Running mode (10Hz update)
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) && (currentStatus.RPM > 0))
  {
    handleStepperOL_Running_10Hz();
  }

  limitStepperMaxSteps();
  doStep();
}

/**
 * Handle IAC_ALGORITHM_STEP_CL / STEP_OLCL - Stepper closed-loop control.
 *
 * Implements PID-based stepper control with optional open-loop feedforward.
 * Both STEP_CL (pure closed-loop) and STEP_OLCL (CL+OL) use this function
 * with conditional logic based on algorithm selection.
 *
 * Refactored from 57 to 42 lines (26% reduction)
 * Complexity: C:8 → C:6, Nesting: N:4 → N:2
 *
 * Supports:
 * - Cranking steps (iacCrankStepsTable)
 * - PID control for running
 * - Optional feedforward from table (STEP_OLCL only)
 * - Taper transition
 * - IdleUp adder
 * - Air conditioning idle-up
 * - Integral reset on TPS/RPM limits or DFCO
 *
 * @param[in,out] PID_computed Set to true if PID was computed this cycle
 *
 * @note Only executes when stepper is homed and not currently stepping
 * @note STEP_CL uses PID output only, STEP_OLCL adds table feedforward
 * @complexity Medium (C=6, N=2)
 */
static void handleIdle_STEP_CL_OLCL(bool &PID_computed)
{
  // Guard: only execute when stepper is ready
  if( (checkForStepping() != false) || (isStepperHomed() != true) ) { return; }

  // Cranking mode
  if( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) )
  {
    handleStepperCranking();
    limitStepperMaxSteps();
    doStep();
    return;
  }

  // Running mode
  if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { handleStepperRunning_10Hz(); }

  PID_computed = idlePID.Compute(true, FeedForwardTerm);

  // Reset PID to feedforward if conditions met
  if( (currentStatus.TPS > configPage2.iacTPSlimit) || lastDFCOValue
   || ((configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL) && (idleTaper < configPage2.idleTaperTime)) )
  {
    idle_pid_target_value = FeedForwardTerm;
  }

  idleStepper.targetIdleStep = idle_pid_target_value>>2;
  applyIdleAdders();

  limitStepperMaxSteps();
  doStep();

  // Update tunings/timings at 1Hz
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_1HZ)) { updateIdleTunings1Hz(); }
}

/** @brief Helper to set pins for 100% PWM duty
 *  @complexity Low (C=3, N:2)
 */
static inline void setPWM100PercentPins(void)
{
  if (configPage6.iacPWMdir == 0)
  {
    IDLE_PIN_HIGH();
    if(configPage6.iacChannels == 1) { IDLE2_PIN_LOW(); }
  }
  else
  {
    IDLE_PIN_LOW();
    if(configPage6.iacChannels == 1) { IDLE2_PIN_HIGH(); }
  }
}

/**
 * Handle PWM idle edge cases (100% and 0% duty cycle).
 *
 * For PWM idle algorithms, handles special cases where duty cycle
 * reaches 100% (fully open) or 0% (fully closed). In these cases,
 * PWM timer is disabled and pins are set to static states.
 *
 * @note Only applies to PWM algorithms (OL, CL, OLCL)
 * @note Respects iacPWMdir for normal/reversed operation
 * @note Handles dual channel configuration (iacChannels == 1)
 * @complexity Medium (C=5, N:2) - reduced from N:3
 */
static inline void handlePWMEdgeCases(void)
{
  // Guard: only for PWM algorithms
  if( (configPage6.iacAlgorithm != IAC_ALGORITHM_PWM_OL) &&
      (configPage6.iacAlgorithm != IAC_ALGORITHM_PWM_CL) &&
      (configPage6.iacAlgorithm != IAC_ALGORITHM_PWM_OLCL) )
  {
    return;
  }

  // 100% duty - disable PWM, set pins high
  if(currentStatus.idleLoad >= 100)
  {
    BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
    IDLE_TIMER_DISABLE();
    setPWM100PercentPins();
  }
  // 0% duty - disable idle completely
  else if (currentStatus.idleLoad == 0)
  {
    disableIdle();
  }
  // Normal PWM operation
  else
  {
    BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
    IDLE_TIMER_ENABLE();
  }
}

//=============================================================================
// ANTI-STALL PROTECTION
//=============================================================================

/**
 * @brief Check for anti-stall condition and force idle valve open
 * @return true if anti-stall is active (overrides normal idle control)
 *
 * @details Detects when engine RPM drops dangerously low while running
 *          (below ANTI_STALL_RPM threshold). When triggered:
 *          - Forces idle valve to maximum opening
 *          - Maintains for ANTI_STALL_HOLD_TIME after RPM recovers
 *
 * @note Only active after engine has been running (>500ms after crank)
 * @note Requires configPage6.antiStallEnabled (bit 7 of iacPWMrun)
 */
static bool checkAntiStall(void)
{
  static bool antiStallActive = false;
  static unsigned long antiStallStartTime = 0;

  // Anti-stall thresholds
  static constexpr uint16_t ANTI_STALL_RPM = 300;       // RPM threshold
  static constexpr uint16_t ANTI_STALL_RECOVER_RPM = 500; // RPM to deactivate
  static constexpr unsigned long ANTI_STALL_HOLD_MS = 2000UL; // Hold time after recovery

  // Guard: not applicable during crank
  if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return false; }

  // Guard: engine not running yet
  if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) { return false; }

  // Guard: haven't been running long enough (wait 500ms after start)
  if (currentStatus.runSecs < 1) { return false; }

  // Check for stall condition
  if (currentStatus.RPM < ANTI_STALL_RPM && currentStatus.RPM > 0)
  {
    if (!antiStallActive)
    {
      antiStallActive = true;
      antiStallStartTime = millis();
    }
    // Force idle to maximum
    idle_pwm_target_value = idle_pwm_max_count;
    return true;
  }

  // Check for recovery
  if (antiStallActive)
  {
    if (currentStatus.RPM >= ANTI_STALL_RECOVER_RPM)
    {
      // Hold for a short time after recovery
      if ((millis() - antiStallStartTime) > ANTI_STALL_HOLD_MS)
      {
        antiStallActive = false;
      }
    }
    // Still active - keep valve open
    idle_pwm_target_value = idle_pwm_max_count;
    return true;
  }

  return false;
}

void idleControl(void)
{
  if( idleInitComplete != configPage6.iacAlgorithm) { initialiseIdle(false); }
  if( (currentStatus.RPM > 0) || (configPage6.iacPWMrun == true) ) { enableIdle(); }

  // SAFETY: Check anti-stall first - overrides normal control
  if (checkAntiStall()) { return; }

  handleIdleUpOutput();

  bool PID_computed = false;
  switch(configPage6.iacAlgorithm)
  {
    case IAC_ALGORITHM_NONE:
      handleIdle_None();
      break;

    case IAC_ALGORITHM_ONOFF:
      handleIdle_OnOff();
      break;

    case IAC_ALGORITHM_PWM_OL:
      handleIdle_PWM_OL();
      break;

    case IAC_ALGORITHM_PWM_CL:
      handleIdle_PWM_CL(PID_computed);
      break;

    case IAC_ALGORITHM_PWM_OLCL:
      handleIdle_PWM_OLCL(PID_computed);
      break;


    case IAC_ALGORITHM_STEP_OL:
      handleIdle_STEP_OL();
      break;

    case IAC_ALGORITHM_STEP_OLCL:
    case IAC_ALGORITHM_STEP_CL:
      handleIdle_STEP_CL_OLCL(PID_computed);
      break;

    default:
      //There really should be a valid idle type
      break;
  }
  lastDFCOValue = BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO);

  handlePWMEdgeCases();
}


//This function simply turns off the idle PWM and sets the pin low
void disableIdle(void)
{
  if( (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_CL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_OL) )
  {
    disableIdle_PWM();
  }
  else if( (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OL) ||
           (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_CL) ||
           (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL) )
  {
    disableIdle_Stepper();
  }

  BIT_CLEAR(currentStatus.status2, BIT_STATUS2_IDLE);
  currentStatus.idleLoad = 0;
}

#if defined(CORE_AVR) //AVR chips use the ISR for this
ISR(TIMER1_COMPC_vect) //cppcheck-suppress misra-c2012-8.2
#else
void idleInterrupt(void) //Most ARM chips can simply call a function
#endif
{
  if (idle_pwm_state)
  {
    idleISR_setPins_ActiveLow();
    SET_COMPARE(IDLE_COMPARE, IDLE_COUNTER + (idle_pwm_max_count - idle_pwm_cur_value) );
    idle_pwm_state = false;
  }
  else
  {
    idleISR_setPins_ActiveHigh();
    SET_COMPARE(IDLE_COMPARE, IDLE_COUNTER + idle_pwm_target_value);
    idle_pwm_cur_value = idle_pwm_target_value;
    idle_pwm_state = true;
  }
}
