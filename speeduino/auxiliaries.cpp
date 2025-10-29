/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory

This file contains only global variables for auxiliary systems.
All function implementations have been moved to modular files under auxiliaries/ folder.
*/
#include "globals.h"
#include "auxiliaries.h"
#include "maths.h"
#include "src/PID_v1/PID_v1.h"
#include "decoders.h"
#include "timers.h"
#include "utilities.h"
#include "units.h"

// VVT PWM state variables (non-static for modular access)
long vvt1_pwm_value;
long vvt2_pwm_value;
volatile unsigned int vvt1_pwm_cur_value;
volatile unsigned int vvt2_pwm_cur_value;
long vvt_pid_target_angle;
long vvt2_pid_target_angle;
long vvt_pid_current_angle;
long vvt2_pid_current_angle;
volatile bool vvt1_pwm_state;
volatile bool vvt2_pwm_state;
volatile bool vvt1_max_pwm;
volatile bool vvt2_max_pwm;
volatile char nextVVT;
byte boostCounter;
byte vvtCounter;

// GPIO port/mask variables for all auxiliary pins
PORT_TYPE boost_pin_port;
PINMASK_TYPE boost_pin_mask;
PORT_TYPE n2o_stage1_pin_port;
PINMASK_TYPE n2o_stage1_pin_mask;
PORT_TYPE n2o_stage2_pin_port;
PINMASK_TYPE n2o_stage2_pin_mask;
PORT_TYPE n2o_arming_pin_port;
PINMASK_TYPE n2o_arming_pin_mask;
PORT_TYPE aircon_comp_pin_port;
PINMASK_TYPE aircon_comp_pin_mask;
PORT_TYPE aircon_fan_pin_port;
PINMASK_TYPE aircon_fan_pin_mask;
PORT_TYPE aircon_req_pin_port;
PINMASK_TYPE aircon_req_pin_mask;
PORT_TYPE vvt1_pin_port;
PINMASK_TYPE vvt1_pin_mask;
PORT_TYPE vvt2_pin_port;
PINMASK_TYPE vvt2_pin_mask;
PORT_TYPE fan_pin_port;
PINMASK_TYPE fan_pin_mask;

// Fan PWM variables (STM32 only)
#if defined(PWM_FAN_AVAILABLE)
volatile bool fan_pwm_state;
uint16_t fan_pwm_max_count; //Used for variable PWM frequency
volatile unsigned int fan_pwm_cur_value;
long fan_pwm_value;
#endif
table2D_u8_u8_4 fanPWMTable(&configPage6.fanPWMBins, &configPage9.PWMFanDuty);

// Air conditioning state variables
bool acIsEnabled;
bool acStandAloneFanIsEnabled;
uint8_t acStartDelay;
uint8_t acTPSLockoutDelay;
uint8_t acRPMLockoutDelay;
uint8_t acAfterEngineStartDelay;
bool waitedAfterCranking;

// Boost control state variables
long boost_pwm_target_value;
volatile bool boost_pwm_state;
volatile unsigned int boost_pwm_cur_value = 0;

// VVT timing variables
uint32_t vvtWarmTime;
bool vvtIsHot;
bool vvtTimeHold;
uint16_t vvt_pwm_max_count; //Used for variable PWM frequency
uint16_t boost_pwm_max_count; //Used for variable PWM frequency
table2D_u8_s16_6 flexBoostTable(&configPage10.flexBoostBins, &configPage10.flexBoostAdj);

// PID controllers
integerPID_ideal boostPID(&currentStatus.MAP, &currentStatus.boostDuty , &currentStatus.boostTarget, &configPage10.boostSens, &configPage10.boostIntv, configPage6.boostKP, configPage6.boostKI, configPage6.boostKD, DIRECT);
integerPID vvtPID(&vvt_pid_current_angle, &currentStatus.vvt1Duty, &vvt_pid_target_angle, configPage10.vvtCLKP, configPage10.vvtCLKI, configPage10.vvtCLKD, configPage6.vvtPWMdir);
integerPID vvt2PID(&vvt2_pid_current_angle, &currentStatus.vvt2Duty, &vvt2_pid_target_angle, configPage10.vvtCLKP, configPage10.vvtCLKI, configPage10.vvtCLKD, configPage4.vvt2PWMdir);

// All function implementations have been moved to:
// - auxiliaries/auxiliary_coordinator.cpp  (legacy interface wrappers)
// - auxiliaries/air_conditioning/aircon_control.cpp
// - auxiliaries/fan_control/fan_control.cpp
// - auxiliaries/boost_control/boost_control.cpp
// - auxiliaries/vvt_control/vvt_control.cpp
// - auxiliaries/nitrous_control/nitrous_control.cpp
// - auxiliaries/wmi_control/wmi_control.cpp
