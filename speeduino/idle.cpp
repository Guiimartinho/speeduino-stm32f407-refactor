/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
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
uint16_t idle_pwm_max_count; //Used for variable PWM frequency
volatile unsigned int idle_pwm_cur_value;
long idle_pid_target_value;
long FeedForwardTerm;
unsigned long idle_pwm_target_value;
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

void initialiseIdle(bool forcehoming)
{
  //By default, turn off the PWM interrupt (It gets turned on below if needed)
  IDLE_TIMER_DISABLE();

  //Pin masks must always be initialised, regardless of whether PWM idle is used. This is required for STM32 to prevent issues if the IRQ function fires on restart/overflow
  idle_pin_port = portOutputRegister(digitalPinToPort(pinIdle1));
  idle_pin_mask = digitalPinToBitMask(pinIdle1);
  idle2_pin_port = portOutputRegister(digitalPinToPort(pinIdle2));
  idle2_pin_mask = digitalPinToBitMask(pinIdle2);

  //Initialising comprises of setting the 2D tables with the relevant values from the config pages
  switch(configPage6.iacAlgorithm)
  {
    case IAC_ALGORITHM_NONE:       
      //Case 0 is no idle control ('None')
      break;

    case IAC_ALGORITHM_ONOFF:
      //Case 1 is on/off idle control
      if ((temperatureAddOffset(currentStatus.coolant)) < configPage6.iacFastTemp)
      {
        IDLE_PIN_HIGH();
        idleOn = true;
      }
      break;

    case IAC_ALGORITHM_PWM_OL:
      //Case 2 is PWM open loop
      enableIdle();
      break;

    case IAC_ALGORITHM_PWM_OLCL:
      //Case 6 is PWM closed loop with open loop table used as feed forward
      idlePID.SetOutputLimits(percentage(configPage2.iacCLminValue, idle_pwm_max_count<<2), percentage(configPage2.iacCLmaxValue, idle_pwm_max_count<<2));
      idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
      idlePID.SetMode(AUTOMATIC); //Turn PID on
      idle_pid_target_value = 0;
      idlePID.Initialize();
      idleCounter = 0;

      break;

    case IAC_ALGORITHM_PWM_CL:
      //Case 3 is PWM closed loop
      idlePID.SetOutputLimits(percentage(configPage2.iacCLminValue, idle_pwm_max_count<<2), percentage(configPage2.iacCLmaxValue, idle_pwm_max_count<<2));
      idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
      idlePID.SetMode(AUTOMATIC); //Turn PID on
      idle_pid_target_value = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
      idlePID.Initialize();
      idleCounter = 0;

      break;

    case IAC_ALGORITHM_STEP_OL:
      //Case 2 is Stepper open loop
      iacStepTime_uS = configPage6.iacStepTime * 1000;
      iacCoolTime_uS = configPage9.iacCoolTime * 1000;

      if (forcehoming)
      {
        //Change between modes running make engine stall
        completedHomeSteps = 0;
        idleStepper.curIdleStep = 0;
        idleStepper.stepperStatus = SOFF;
      }

      configPage6.iacPWMrun = false; // just in case. This needs to be false with stepper idle
      break;

    case IAC_ALGORITHM_STEP_CL:
      //Case 5 is Stepper closed loop
      iacStepTime_uS = configPage6.iacStepTime * 1000;
      iacCoolTime_uS = configPage9.iacCoolTime * 1000;

      if (forcehoming)
      {
        //Change between modes running make engine stall
        completedHomeSteps = 0;
        idleStepper.curIdleStep = 0;
        idleStepper.stepperStatus = SOFF;
      }

      idlePID.SetSampleTime(250); //4Hz means 250ms
      idlePID.SetOutputLimits((configPage2.iacCLminValue * 3)<<2, (configPage2.iacCLmaxValue * 3)<<2); //Maximum number of steps; always less than home steps count.
      idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
      idlePID.SetMode(AUTOMATIC); //Turn PID on
      configPage6.iacPWMrun = false; // just in case. This needs to be false with stepper idle
      idle_pid_target_value = currentStatus.CLIdleTarget * 3;
      idlePID.Initialize();
      break;

    case IAC_ALGORITHM_STEP_OLCL:
      //Case 7 is Stepper closed loop with open loop table used as feed forward
      iacStepTime_uS = configPage6.iacStepTime * 1000;
      iacCoolTime_uS = configPage9.iacCoolTime * 1000;

      if (forcehoming)
      {
        //Change between modes running make engine stall
        completedHomeSteps = 0;
        idleStepper.curIdleStep = 0;
        idleStepper.stepperStatus = SOFF;
      }

      idlePID.SetSampleTime(250); //4Hz means 250ms
      idlePID.SetOutputLimits((configPage2.iacCLminValue * 3)<<2, (configPage2.iacCLmaxValue * 3)<<2); //Maximum number of steps; always less than home steps count.
      idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
      idlePID.SetMode(AUTOMATIC); //Turn PID on
      configPage6.iacPWMrun = false; // just in case. This needs to be false with stepper idle
      idle_pid_target_value = 0;
      idlePID.Initialize();
      break;

    default:
      //Well this just shouldn't happen
      break;
  }

  initialiseIdleUpOutput();

  idleInitComplete = configPage6.iacAlgorithm; //Sets which idle method was initialised
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

/*
Checks whether a step is currently underway or whether the motor is in 'cooling' state (ie whether it's ready to begin another step or not)
Returns:
True: If a step is underway or motor is 'cooling'
False: If the motor is ready for another step
*/
static inline byte checkForStepping(void)
{
  bool isStepping = false;
  unsigned int timeCheck;
  
  if( (idleStepper.stepperStatus == STEPPING) || (idleStepper.stepperStatus == COOLING) )
  {
    if (idleStepper.stepperStatus == STEPPING)
    {
      timeCheck = iacStepTime_uS;
    }
    else 
    {
      timeCheck = iacCoolTime_uS;
    }

    if(micros() > (idleStepper.stepStartTime + timeCheck) )
    {         
      if(idleStepper.stepperStatus == STEPPING)
      {
        //Means we're currently in a step, but it needs to be turned off
        digitalWrite(pinStepperStep, LOW); //Turn off the step
        idleStepper.stepStartTime = micros();

	//Set status to COOLING. In next cycle, status will be set to SOFF and set stepper power OFF based on given settings
        idleStepper.stepperStatus = COOLING; //'Cooling' is the time the stepper needs to sit in LOW state before the next step can be made
                  
        isStepping = true;
      }
      else
      {
        //Means we're in COOLING status but have been in this state long enough. Go into off state
        idleStepper.stepperStatus = SOFF;
        if(configPage9.iacStepperPower == STEPPER_POWER_WHEN_ACTIVE) 
        { 
          //Disable the DRV8825, but only if we're at the final step in this cycle or within the hysteresis range. 
          if ( (idleStepper.curIdleStep >= (idleStepper.targetIdleStep - configPage6.iacStepHyster)) && (idleStepper.curIdleStep <= (idleStepper.targetIdleStep + configPage6.iacStepHyster))) //Hysteresis check
          { 
            digitalWrite(pinStepperEnable, HIGH); 
          } 
        }
      }
    }
    else
    {
      //Means we're in a step, but it doesn't need to turn off yet. No further action at this time
      isStepping = true;
    }
  }
  return isStepping;
}

/*
Performs a step
*/
static inline void doStep(void)
{
  int16_t error = idleStepper.targetIdleStep - idleStepper.curIdleStep;
  if ( (error < -((int8_t)configPage6.iacStepHyster)) || (error > configPage6.iacStepHyster) ) //Hysteresis check
  {
    // the home position for a stepper is pintle fully seated, i.e. no airflow.
    if (error < 0)
    {
      // we are moving toward the home position (reducing air)
      digitalWrite(pinStepperDir, STEPPER_LESS_AIR_DIRECTION() );
      idleStepper.curIdleStep--;
    }
    else
    {
      // we are moving away from the home position (adding air).
      digitalWrite(pinStepperDir, STEPPER_MORE_AIR_DIRECTION() );
      idleStepper.curIdleStep++;
    }

    digitalWrite(pinStepperEnable, LOW); //Enable the DRV8825
    digitalWrite(pinStepperStep, HIGH);
    idleStepper.stepStartTime = micros();
    idleStepper.stepperStatus = STEPPING;
    idleOn = true;

    BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
  }
  else
    BIT_CLEAR(currentStatus.status2, BIT_STATUS2_IDLE);
}

/*
Checks whether the stepper has been homed yet. If it hasn't, will handle the next step
Returns:
True: If the system has been homed. No other action is taken
False: If the motor has not yet been homed. Will also perform another homing step.
*/
static inline byte isStepperHomed(void)
{
  bool isHomed = true; //As it's the most common scenario, default value is true
  if( completedHomeSteps < (configPage6.iacStepHome * 3) ) //Home steps are divided by 3 from TS
  {
    digitalWrite(pinStepperDir, STEPPER_LESS_AIR_DIRECTION() ); //homing the stepper closes off the air bleed
    digitalWrite(pinStepperEnable, LOW); //Enable the DRV8825
    digitalWrite(pinStepperStep, HIGH);
    idleStepper.stepStartTime = micros();
    idleStepper.stepperStatus = STEPPING;
    completedHomeSteps++;
    idleOn = true;
    isHomed = false;
  }
  return isHomed;
}

/**
 * Handle IdleUp output pin control.
 *
 * Checks IdleUp input pin status (with polarity support) and controls
 * the corresponding output pin if enabled. Updates currentStatus flags.
 *
 * @note IdleUp is typically used for A/C compressor load compensation
 * @note Supports both normal (ground-switched) and inverted (5V) polarity
 */
static inline void handleIdleUpOutput(void)
{
  if (configPage2.idleUpEnabled == true)
  {
    if (configPage2.idleUpPolarity == 0) { currentStatus.idleUpActive = !digitalRead(pinIdleUp); }
    else { currentStatus.idleUpActive = digitalRead(pinIdleUp); }

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
  else { currentStatus.idleUpActive = false; }
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
 */
static void handleIdle_PWM_OL(void)
{
  if( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  {
    currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
    idleTaper = 0;
  }
  else if ( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN))
  {
    if( configPage6.iacPWMrun == true)
    {
      currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
      idleTaper = 0;
    }
  }
  else
  {
    if ( idleTaper < configPage2.idleTaperTime )
    {
      currentStatus.idleLoad = map(idleTaper, 0, configPage2.idleTaperTime,\
      table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant)),\
      table2D_getValue(&iacPWMTable, temperatureAddOffset(currentStatus.coolant)));
      if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { idleTaper++; }
    }
    else
    {
      currentStatus.idleLoad = table2D_getValue(&iacPWMTable, temperatureAddOffset(currentStatus.coolant));
    }
    if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true) { currentStatus.idleLoad += configPage15.airConIdleSteps; }
  }

  if(currentStatus.idleUpActive == true) { currentStatus.idleLoad += configPage2.idleUpAdder; }

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
 */
static inline bool handleCrankingIdlePWM(void)
{
  if( BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) )
  {
    currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
    idle_pwm_target_value = percentage(currentStatus.idleLoad, idle_pwm_max_count);
    idle_pid_target_value = idle_pwm_target_value << 2;
    idlePID.Initialize();
    return true;
  }
  else if ( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN))
  {
    if( configPage6.iacPWMrun == true)
    {
      currentStatus.idleLoad = table2D_getValue(&iacCrankDutyTable, temperatureAddOffset(currentStatus.coolant));
      idle_pwm_target_value = percentage(currentStatus.idleLoad, idle_pwm_max_count);
    }
    return true;
  }
  return false;
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
 */
static void handleIdle_PWM_CL(bool &PID_computed)
{
  if( handleCrankingIdlePWM() ) { return; }

  idle_cl_target_rpm = (uint16_t)currentStatus.CLIdleTarget * 10;
  if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_1HZ) ) { idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD); }

  PID_computed = idlePID.Compute(true);
  long TEMP_idle_pwm_target_value;
  if(PID_computed == true)
  {
    TEMP_idle_pwm_target_value = idle_pid_target_value;

    if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
    {
      TEMP_idle_pwm_target_value += percentage(configPage15.airConIdleSteps, idle_pwm_max_count<<2);
      if(TEMP_idle_pwm_target_value > (idle_pwm_max_count<<2)) { TEMP_idle_pwm_target_value = (idle_pwm_max_count<<2); }
    }

    if(currentStatus.idleUpActive == true)
    {
      TEMP_idle_pwm_target_value += percentage(configPage2.idleUpAdder, idle_pwm_max_count<<2);
      if(TEMP_idle_pwm_target_value > (idle_pwm_max_count<<2)) { TEMP_idle_pwm_target_value = (idle_pwm_max_count<<2); }
    }

    idle_pwm_target_value = TEMP_idle_pwm_target_value>>2;
    currentStatus.idleLoad = udiv_32_16(idle_pwm_target_value * 100UL, idle_pwm_max_count);
  }
  idleCounter++;
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
 */
static void handleIdle_PWM_OLCL(bool &PID_computed)
{
  if( handleCrankingIdlePWM() ) { return; }

  FeedForwardTerm = percentage(table2D_getValue(&iacPWMTable, temperatureAddOffset(currentStatus.coolant)), idle_pwm_max_count<<2);

  if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true)
  {
    FeedForwardTerm += percentage(configPage15.airConIdleSteps, (idle_pwm_max_count<<2));
    if(FeedForwardTerm > (idle_pwm_max_count<<2)) { FeedForwardTerm = (idle_pwm_max_count<<2); }
  }

  if(currentStatus.idleUpActive == true)
  {
    FeedForwardTerm += percentage(configPage2.idleUpAdder, (idle_pwm_max_count<<2));
    if(FeedForwardTerm > (idle_pwm_max_count<<2)) { FeedForwardTerm = (idle_pwm_max_count<<2); }
  }

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
 * Supports:
 * - Cranking steps (iacCrankStepsTable)
 * - Running steps (iacStepTable)
 * - Taper transition between cranking and running
 * - IdleUp adder
 * - Air conditioning idle-up
 *
 * @note Only executes when stepper is homed and not currently stepping
 */
static void handleIdle_STEP_OL(void)
{
  if( (checkForStepping() == false) && (isStepperHomed() == true) )
  {
    if( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) )
    {
      idleStepper.targetIdleStep = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3;
      if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }
      idleTaper = 0;
    }
    else
    {
      if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) && (currentStatus.RPM > 0))
      {
        if ( idleTaper < configPage2.idleTaperTime )
        {
          idleStepper.targetIdleStep = map(idleTaper, 0, configPage2.idleTaperTime,\
          table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3,\
          table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3);
          if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) ) { idleTaper++; }
        }
        else
        {
          idleStepper.targetIdleStep = table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3;
        }
        if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }

        if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true) { idleStepper.targetIdleStep += configPage15.airConIdleSteps; }

        iacStepTime_uS = configPage6.iacStepTime * 1000;
        iacCoolTime_uS = configPage9.iacCoolTime * 1000;
      }
    }
    limitStepperMaxSteps();
    doStep();
  }
}

/**
 * Handle IAC_ALGORITHM_STEP_CL / STEP_OLCL - Stepper closed-loop control.
 *
 * Implements PID-based stepper control with optional open-loop feedforward.
 * Both STEP_CL (pure closed-loop) and STEP_OLCL (CL+OL) use this function
 * with conditional logic based on algorithm selection.
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
 */
static void handleIdle_STEP_CL_OLCL(bool &PID_computed)
{
  if( (checkForStepping() == false) && (isStepperHomed() == true) )
  {
    if( !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN) )
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
    else
    {
      if( BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ) )
      {
        idle_cl_target_rpm = (uint16_t)currentStatus.CLIdleTarget * 10;
        if( idleTaper < configPage2.idleTaperTime )
        {
          uint16_t minValue = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3;
          if( idle_pid_target_value < minValue<<2 ) { idle_pid_target_value = minValue<<2; }
          uint16_t maxValue = idle_pid_target_value>>2;
          if( configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL ) { maxValue = table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3; }

          FeedForwardTerm = map(idleTaper, 0, configPage2.idleTaperTime, minValue, maxValue)<<2;
          idleTaper++;
          idle_pid_target_value = FeedForwardTerm;
        }
        else if (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL)
        {
          FeedForwardTerm = (table2D_getValue(&iacStepTable, temperatureAddOffset(currentStatus.coolant)) * 3)<<2;
          if (((currentStatus.RPM - idle_cl_target_rpm) > configPage2.iacRPMlimitHysteresis*10) || (currentStatus.TPS > configPage2.iacTPSlimit) || lastDFCOValue )
          {
            idlePID.ResetIntegeral();
          }
        }
        else { FeedForwardTerm = idle_pid_target_value; }
      }

      PID_computed = idlePID.Compute(true, FeedForwardTerm);

      if( (currentStatus.TPS > configPage2.iacTPSlimit) || lastDFCOValue
      || ((configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL) && (idleTaper < configPage2.idleTaperTime)) )
      {
        idle_pid_target_value = FeedForwardTerm;
      }
      idleStepper.targetIdleStep = idle_pid_target_value>>2;

      if(configPage15.airConIdleSteps>0 && BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON) == true) { idleStepper.targetIdleStep += configPage15.airConIdleSteps; }
    }

    if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; }

    limitStepperMaxSteps();
    doStep();
  }
  if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_1HZ))
  {
    idlePID.SetTunings(configPage6.idleKP, configPage6.idleKI, configPage6.idleKD);
    iacStepTime_uS = configPage6.iacStepTime * 1000;
    iacCoolTime_uS = configPage9.iacCoolTime * 1000;
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
 */
static inline void handlePWMEdgeCases(void)
{
  if( (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_OL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_CL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_PWM_OLCL) )
  {
    if(currentStatus.idleLoad >= 100)
    {
      BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
      IDLE_TIMER_DISABLE();
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
    else if (currentStatus.idleLoad == 0)
    {
      disableIdle();
    }
    else
    {
      BIT_SET(currentStatus.status2, BIT_STATUS2_IDLE);
      IDLE_TIMER_ENABLE();
    }
  }
}

void idleControl(void)
{
  if( idleInitComplete != configPage6.iacAlgorithm) { initialiseIdle(false); }
  if( (currentStatus.RPM > 0) || (configPage6.iacPWMrun == true) ) { enableIdle(); }

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
    IDLE_TIMER_DISABLE();
    if (configPage6.iacPWMdir == 0)
    {
      //Normal direction
      IDLE_PIN_LOW();  // Switch pin to low
      if(configPage6.iacChannels == 1) { IDLE2_PIN_HIGH(); } //If 2 idle channels are in use, flip idle2 to be the opposite of idle1
    }
    else
    {
      //Reversed direction
      IDLE_PIN_HIGH();  // Switch pin high
      if(configPage6.iacChannels == 1) { IDLE2_PIN_LOW(); } //If 2 idle channels are in use, flip idle2 to be the opposite of idle1
    }
  }
  else if( (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_CL) || (configPage6.iacAlgorithm == IAC_ALGORITHM_STEP_OLCL) )
  {
    //Only disable the stepper motor if homing is completed
    if( (checkForStepping() == false) && (isStepperHomed() == true) )
    {
        /* for open loop stepper we should just move to the cranking position when
           disabling idle, since the only time this function is called in this scenario
           is if the engine stops.
        */
        idleStepper.targetIdleStep = table2D_getValue(&iacCrankStepsTable, temperatureAddOffset(currentStatus.coolant)) * 3; //All temps are offset by 40 degrees. Step counts are divided by 3 in TS. Multiply back out here
        if(currentStatus.idleUpActive == true) { idleStepper.targetIdleStep += configPage2.idleUpAdder; } //Add Idle Up amount if active?

        //limit to the configured max steps. This must include any idle up adder, to prevent over-opening.
        if (idleStepper.targetIdleStep > (configPage9.iacMaxSteps * 3) )
        {
          idleStepper.targetIdleStep = configPage9.iacMaxSteps * 3;
        }
        idle_pid_target_value = idleStepper.targetIdleStep<<2;
    }
  }
  BIT_CLEAR(currentStatus.status2, BIT_STATUS2_IDLE); //Turn the idle control flag off
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
    if (configPage6.iacPWMdir == 0)
    {
      //Normal direction
      #if defined (CORE_TEENSY41) //PIT TIMERS count down and have opposite effect on PWM
      IDLE_PIN_HIGH();
      if(configPage6.iacChannels == 1) { IDLE2_PIN_LOW(); }
      #else
      IDLE_PIN_LOW();  // Switch pin to low (1 pin mode)
      if(configPage6.iacChannels == 1) { IDLE2_PIN_HIGH(); } //If 2 idle channels are in use, flip idle2 to be the opposite of idle1
      #endif
    }
    else
    {
      //Reversed direction
      #if defined (CORE_TEENSY41) //PIT TIMERS count down and have opposite effect on PWM
      IDLE_PIN_LOW();
      if(configPage6.iacChannels == 1) { IDLE2_PIN_HIGH(); }
      #else
      IDLE_PIN_HIGH();  // Switch pin high
      if(configPage6.iacChannels == 1) { IDLE2_PIN_LOW(); } //If 2 idle channels are in use, flip idle2 to be the opposite of idle1
      #endif
    }
    SET_COMPARE(IDLE_COMPARE, IDLE_COUNTER + (idle_pwm_max_count - idle_pwm_cur_value) );
    idle_pwm_state = false;
  }
  else
  {
    if (configPage6.iacPWMdir == 0)
    {
      //Normal direction
      #if defined (CORE_TEENSY41) //PIT TIMERS count down and have opposite effect on PWM
      IDLE_PIN_LOW();
      if(configPage6.iacChannels == 1) { IDLE2_PIN_HIGH(); }
      #else
      IDLE_PIN_HIGH();  // Switch pin high
      if(configPage6.iacChannels == 1) { IDLE2_PIN_LOW(); } //If 2 idle channels are in use, flip idle2 to be the opposite of idle1
      #endif
    }
    else
    {
      //Reversed direction
      #if defined (CORE_TEENSY41) //PIT TIMERS count down and have opposite effect on PWM
      IDLE_PIN_HIGH();
      if(configPage6.iacChannels == 1) { IDLE2_PIN_LOW(); }
      #else
      IDLE_PIN_LOW();  // Switch pin to low (1 pin mode)
      if(configPage6.iacChannels == 1) { IDLE2_PIN_HIGH(); } //If 2 idle channels are in use, flip idle2 to be the opposite of idle1
      #endif
    }
    SET_COMPARE(IDLE_COMPARE, IDLE_COUNTER + idle_pwm_target_value);
    idle_pwm_cur_value = idle_pwm_target_value;
    idle_pwm_state = true;
  }
}
