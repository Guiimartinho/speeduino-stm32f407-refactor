/**
 * @file pin_setup.cpp
 * @brief Pin mode and port pointer configuration implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * PRESERVES 100% LOGIC from init.cpp setPinMapping() lines 2769-3062
 *
 * All functions preserve exact behavior of original code
 */

#include "pin_setup.h"
#include "../../globals.h"
#include "../../utilities.h"
#include "../../acc_mc33810.h"

void pinSetupConfigureProgrammablePins(void)
{
  // Setup any devices that are using selectable pins
  // PRESERVES init.cpp lines 2771-2806

  if ((configPage6.launchPin != 0) && (configPage6.launchPin < BOARD_MAX_IO_PINS)) {
    pinLaunch = pinTranslate(configPage6.launchPin);
  }

  if ((configPage4.ignBypassPin != 0) && (configPage4.ignBypassPin < BOARD_MAX_IO_PINS)) {
    pinIgnBypass = pinTranslate(configPage4.ignBypassPin);
  }

  if ((configPage2.tachoPin != 0) && (configPage2.tachoPin < BOARD_MAX_IO_PINS)) {
    pinTachOut = pinTranslate(configPage2.tachoPin);
  }

  if ((configPage4.fuelPumpPin != 0) && (configPage4.fuelPumpPin < BOARD_MAX_IO_PINS)) {
    pinFuelPump = pinTranslate(configPage4.fuelPumpPin);
  }

  if ((configPage6.fanPin != 0) && (configPage6.fanPin < BOARD_MAX_IO_PINS)) {
    pinFan = pinTranslate(configPage6.fanPin);
  }

  if ((configPage6.boostPin != 0) && (configPage6.boostPin < BOARD_MAX_IO_PINS)) {
    pinBoost = pinTranslate(configPage6.boostPin);
  }

  if ((configPage6.vvt1Pin != 0) && (configPage6.vvt1Pin < BOARD_MAX_IO_PINS)) {
    pinVVT_1 = pinTranslate(configPage6.vvt1Pin);
  }

  if ((configPage6.useExtBaro != 0) && (configPage6.baroPin < BOARD_MAX_IO_PINS)) {
    pinBaro = pinTranslateAnalog(configPage6.baroPin);
  }

  if ((configPage6.useEMAP != 0) && (configPage10.EMAPPin < BOARD_MAX_IO_PINS)) {
    pinEMAP = pinTranslateAnalog(configPage10.EMAPPin);
  }

  if ((configPage10.fuel2InputPin != 0) && (configPage10.fuel2InputPin < BOARD_MAX_IO_PINS)) {
    pinFuel2Input = pinTranslate(configPage10.fuel2InputPin);
  }

  if ((configPage10.spark2InputPin != 0) && (configPage10.spark2InputPin < BOARD_MAX_IO_PINS)) {
    pinSpark2Input = pinTranslate(configPage10.spark2InputPin);
  }

  if ((configPage2.vssPin != 0) && (configPage2.vssPin < BOARD_MAX_IO_PINS)) {
    pinVSS = pinTranslate(configPage2.vssPin);
  }

  if ((configPage10.fuelPressureEnable) && (configPage10.fuelPressurePin < BOARD_MAX_IO_PINS)) {
    pinFuelPressure = pinTranslateAnalog(configPage10.fuelPressurePin);
  }

  if ((configPage10.oilPressureEnable) && (configPage10.oilPressurePin < BOARD_MAX_IO_PINS)) {
    pinOilPressure = pinTranslateAnalog(configPage10.oilPressurePin);
  }

  if ((configPage10.wmiEmptyPin != 0) && (configPage10.wmiEmptyPin < BOARD_MAX_IO_PINS)) {
    pinWMIEmpty = pinTranslate(configPage10.wmiEmptyPin);
  }

  if ((configPage10.wmiIndicatorPin != 0) && (configPage10.wmiIndicatorPin < BOARD_MAX_IO_PINS)) {
    pinWMIIndicator = pinTranslate(configPage10.wmiIndicatorPin);
  }

  if ((configPage10.wmiEnabledPin != 0) && (configPage10.wmiEnabledPin < BOARD_MAX_IO_PINS)) {
    pinWMIEnabled = pinTranslate(configPage10.wmiEnabledPin);
  }

  if ((configPage10.vvt2Pin != 0) && (configPage10.vvt2Pin < BOARD_MAX_IO_PINS)) {
    pinVVT_2 = pinTranslate(configPage10.vvt2Pin);
  }

  if ((configPage13.onboard_log_trigger_Epin != 0) && (configPage13.onboard_log_trigger_Epin != 0) &&
      (configPage13.onboard_log_tr5_Epin_pin < BOARD_MAX_IO_PINS)) {
    pinSDEnable = pinTranslate(configPage13.onboard_log_tr5_Epin_pin);
  }

  // Currently there's no default pin for Idle Up
  pinIdleUp = pinTranslate(configPage2.idleUpPin);

  // Currently there's no default pin for Idle Up Output
  pinIdleUpOutput = pinTranslate(configPage2.idleUpOutputPin);

  // Currently there's no default pin for closed throttle position sensor
  pinCTPS = pinTranslate(configPage2.CTPSPin);

  // Air conditioning control initialisation
  if ((configPage15.airConCompPin != 0) && (configPage15.airConCompPin < BOARD_MAX_IO_PINS)) {
    pinAirConComp = pinTranslate(configPage15.airConCompPin);
  }

  if ((configPage15.airConFanPin != 0) && (configPage15.airConFanPin < BOARD_MAX_IO_PINS)) {
    pinAirConFan = pinTranslate(configPage15.airConFanPin);
  }

  if ((configPage15.airConReqPin != 0) && (configPage15.airConReqPin < BOARD_MAX_IO_PINS)) {
    pinAirConRequest = pinTranslate(configPage15.airConReqPin);
  }
}

void pinSetupConfigureResetControl(void)
{
  // Reset control is a special case. If reset control is enabled, it needs its initial state set BEFORE its pinMode.
  // If that doesn't happen and reset control is in "Serial Command" mode, the Arduino will end up in a reset loop
  // because the control pin will go low as soon as the pinMode is set to OUTPUT.
  // PRESERVES init.cpp lines 2808-2819

  if ((configPage4.resetControlConfig != 0) && (configPage4.resetControlPin < BOARD_MAX_IO_PINS)) {
    if (configPage4.resetControlPin != 0U) {
      pinResetControl = pinTranslate(configPage4.resetControlPin);
    }
    resetControl = configPage4.resetControlConfig;
    setResetControlPinState();
    pinMode(pinResetControl, OUTPUT);
  }
}

void pinSetupConfigurePinModes(void)
{
  // Finally, set the relevant pin modes for outputs
  // PRESERVES init.cpp lines 2822-3049

  pinMode(pinTachOut, OUTPUT);
  pinMode(pinIdle1, OUTPUT);
  pinMode(pinIdle2, OUTPUT);
  pinMode(pinIdleUpOutput, OUTPUT);
  pinMode(pinFuelPump, OUTPUT);
  pinMode(pinFan, OUTPUT);
  pinMode(pinStepperDir, OUTPUT);
  pinMode(pinStepperStep, OUTPUT);
  pinMode(pinStepperEnable, OUTPUT);
  pinMode(pinBoost, OUTPUT);
  pinMode(pinVVT_1, OUTPUT);
  pinMode(pinVVT_2, OUTPUT);

  if (configPage4.ignBypassEnabled > 0) {
    pinMode(pinIgnBypass, OUTPUT);
  }

  // Configure analog input pins
  #if defined(CORE_STM32)
    #ifdef INPUT_ANALOG
      pinMode(pinMAP, INPUT_ANALOG);
      pinMode(pinO2, INPUT_ANALOG);
      pinMode(pinO2_2, INPUT_ANALOG);
      pinMode(pinTPS, INPUT_ANALOG);
      pinMode(pinIAT, INPUT_ANALOG);
      pinMode(pinCLT, INPUT_ANALOG);
      pinMode(pinBat, INPUT_ANALOG);
      pinMode(pinBaro, INPUT_ANALOG);
    #else
      pinMode(pinMAP, INPUT);
      pinMode(pinO2, INPUT);
      pinMode(pinO2_2, INPUT);
      pinMode(pinTPS, INPUT);
      pinMode(pinIAT, INPUT);
      pinMode(pinCLT, INPUT);
      pinMode(pinBat, INPUT);
      pinMode(pinBaro, INPUT);
    #endif
  #elif defined(CORE_TEENSY41)
    // Teensy 4.1 has a weak pull down resistor that needs to be disabled for all analog pins
    pinMode(pinMAP, INPUT_DISABLE);
    pinMode(pinO2, INPUT_DISABLE);
    pinMode(pinO2_2, INPUT_DISABLE);
    pinMode(pinTPS, INPUT_DISABLE);
    pinMode(pinIAT, INPUT_DISABLE);
    pinMode(pinCLT, INPUT_DISABLE);
    pinMode(pinBat, INPUT_DISABLE);
    pinMode(pinBaro, INPUT_DISABLE);
  #endif

  // Each of the below are only set when their relevant function is enabled
  // This can help prevent pin conflicts that users aren't aware of with unused functions

  if ((configPage2.flexEnabled > 0) && (!pinIsOutput(pinFlex))) {
    pinMode(pinFlex, INPUT); // Standard GM / Continental flex sensor requires pullup, but this should be onboard
  }

  if ((configPage2.vssMode > 1) && (!pinIsOutput(pinVSS))) { // Pin mode 1 for VSS is CAN
    pinMode(pinVSS, INPUT);
  }

  if ((configPage6.launchEnabled > 0) && (!pinIsOutput(pinLaunch))) {
    if (configPage6.lnchPullRes == true) {
      pinMode(pinLaunch, INPUT_PULLUP);
    } else {
      pinMode(pinLaunch, INPUT); // If Launch Pull Resistor is not set make input float
    }
  }

  if ((configPage2.idleUpEnabled > 0) && (!pinIsOutput(pinIdleUp))) {
    if (configPage2.idleUpPolarity == 0) {
      pinMode(pinIdleUp, INPUT_PULLUP); // Normal setting
    } else {
      pinMode(pinIdleUp, INPUT); // Inverted setting
    }
  }

  if ((configPage2.CTPSEnabled > 0) && (!pinIsOutput(pinCTPS))) {
    if (configPage2.CTPSPolarity == 0) {
      pinMode(pinCTPS, INPUT_PULLUP); // Normal setting
    } else {
      pinMode(pinCTPS, INPUT); // Inverted setting
    }
  }

  if ((configPage10.fuel2Mode == FUEL2_MODE_INPUT_SWITCH) && (!pinIsOutput(pinFuel2Input))) {
    if (configPage10.fuel2InputPullup == true) {
      pinMode(pinFuel2Input, INPUT_PULLUP); // With pullup
    } else {
      pinMode(pinFuel2Input, INPUT); // Normal input
    }
  }

  if ((configPage10.spark2Mode == SPARK2_MODE_INPUT_SWITCH) && (!pinIsOutput(pinSpark2Input))) {
    if (configPage10.spark2InputPullup == true) {
      pinMode(pinSpark2Input, INPUT_PULLUP); // With pullup
    } else {
      pinMode(pinSpark2Input, INPUT); // Normal input
    }
  }

  if ((configPage10.fuelPressureEnable > 0) && (!pinIsOutput(pinFuelPressure))) {
    pinMode(pinFuelPressure, INPUT);
  }

  if ((configPage10.oilPressureEnable > 0) && (!pinIsOutput(pinOilPressure))) {
    pinMode(pinOilPressure, INPUT);
  }

  if ((configPage13.onboard_log_trigger_Epin > 0) && (!pinIsOutput(pinSDEnable))) {
    pinMode(pinSDEnable, INPUT);
  }

  if (configPage10.wmiEnabled == 0) { goto skip_wmi_setup; }
  pinMode(pinWMIEnabled, OUTPUT);

  if (configPage10.wmiIndicatorEnabled > 0) {
    pinMode(pinWMIIndicator, OUTPUT);
    if (configPage10.wmiIndicatorPolarity > 0) { digitalWrite(pinWMIIndicator, HIGH); }
  }

  {
    bool emptyPinEnabled = (configPage10.wmiEmptyEnabled > 0) && (!pinIsOutput(pinWMIEmpty));
    if (emptyPinEnabled && configPage10.wmiEmptyPolarity == 0) { pinMode(pinWMIEmpty, INPUT_PULLUP); }
    else if (emptyPinEnabled) { pinMode(pinWMIEmpty, INPUT); }
  }

skip_wmi_setup:

  if ((pinAirConComp > 0) && ((configPage15.airConEnable) == 1)) {
    pinMode(pinAirConComp, OUTPUT);
  }

  if ((pinAirConRequest > 0) && ((configPage15.airConEnable) == 1) && (!pinIsOutput(pinAirConRequest))) {
    if ((configPage15.airConReqPol) == 1) {
      // Inverted: +5V is ON, Use external pull-down resistor for OFF
      pinMode(pinAirConRequest, INPUT);
    } else {
      // Normal: Pin pulled to Ground is ON. Floating (internally pulled up to +5V) is OFF.
      pinMode(pinAirConRequest, INPUT_PULLUP);
    }
  }

  if ((pinAirConFan > 0) && ((configPage15.airConEnable) == 1) && ((configPage15.airConFanEnabled) == 1)) {
    pinMode(pinAirConFan, OUTPUT);
  }
}

void pinSetupApplyLegacyMAP(void)
{
  // This is a legacy mode option to revert the MAP reading behaviour to match what was in place prior to the 201905 firmware
  // PRESERVES init.cpp lines 2837-2838
  if (configPage2.legacyMAP > 0) {
    digitalWrite(pinMAP, HIGH);
  }
}

void pinSetupConfigureIgnitionPorts(void)
{
  // Setup port/mask pointers for direct GPIO access - Ignition outputs
  // PRESERVES init.cpp lines 2840-2875

  if (ignitionOutputControl == OUTPUT_CONTROL_DIRECT) {
    pinMode(pinCoil1, OUTPUT);
    pinMode(pinCoil2, OUTPUT);
    pinMode(pinCoil3, OUTPUT);
    pinMode(pinCoil4, OUTPUT);

    #if (IGN_CHANNELS >= 5)
      pinMode(pinCoil5, OUTPUT);
    #endif
    #if (IGN_CHANNELS >= 6)
      pinMode(pinCoil6, OUTPUT);
    #endif
    #if (IGN_CHANNELS >= 7)
      pinMode(pinCoil7, OUTPUT);
    #endif
    #if (IGN_CHANNELS >= 8)
      pinMode(pinCoil8, OUTPUT);
    #endif

    ign1_pin_port = portOutputRegister(digitalPinToPort(pinCoil1));
    ign1_pin_mask = digitalPinToBitMask(pinCoil1);
    ign2_pin_port = portOutputRegister(digitalPinToPort(pinCoil2));
    ign2_pin_mask = digitalPinToBitMask(pinCoil2);
    ign3_pin_port = portOutputRegister(digitalPinToPort(pinCoil3));
    ign3_pin_mask = digitalPinToBitMask(pinCoil3);
    ign4_pin_port = portOutputRegister(digitalPinToPort(pinCoil4));
    ign4_pin_mask = digitalPinToBitMask(pinCoil4);
    ign5_pin_port = portOutputRegister(digitalPinToPort(pinCoil5));
    ign5_pin_mask = digitalPinToBitMask(pinCoil5);
    ign6_pin_port = portOutputRegister(digitalPinToPort(pinCoil6));
    ign6_pin_mask = digitalPinToBitMask(pinCoil6);
    ign7_pin_port = portOutputRegister(digitalPinToPort(pinCoil7));
    ign7_pin_mask = digitalPinToBitMask(pinCoil7);
    ign8_pin_port = portOutputRegister(digitalPinToPort(pinCoil8));
    ign8_pin_mask = digitalPinToBitMask(pinCoil8);
  }
}

void pinSetupConfigureInjectorPorts(void)
{
  // Setup port/mask pointers for direct GPIO access - Injector outputs
  // PRESERVES init.cpp lines 2877-2925

  if (injectorOutputControl == OUTPUT_CONTROL_DIRECT) {
    pinMode(pinInjector1, OUTPUT);
    pinMode(pinInjector2, OUTPUT);
    pinMode(pinInjector3, OUTPUT);
    pinMode(pinInjector4, OUTPUT);

    #if (INJ_CHANNELS >= 5)
      pinMode(pinInjector5, OUTPUT);
    #endif
    #if (INJ_CHANNELS >= 6)
      pinMode(pinInjector6, OUTPUT);
    #endif
    #if (INJ_CHANNELS >= 7)
      pinMode(pinInjector7, OUTPUT);
    #endif
    #if (INJ_CHANNELS >= 8)
      pinMode(pinInjector8, OUTPUT);
    #endif

    inj1_pin_port = portOutputRegister(digitalPinToPort(pinInjector1));
    inj1_pin_mask = digitalPinToBitMask(pinInjector1);
    inj2_pin_port = portOutputRegister(digitalPinToPort(pinInjector2));
    inj2_pin_mask = digitalPinToBitMask(pinInjector2);
    inj3_pin_port = portOutputRegister(digitalPinToPort(pinInjector3));
    inj3_pin_mask = digitalPinToBitMask(pinInjector3);
    inj4_pin_port = portOutputRegister(digitalPinToPort(pinInjector4));
    inj4_pin_mask = digitalPinToBitMask(pinInjector4);
    inj5_pin_port = portOutputRegister(digitalPinToPort(pinInjector5));
    inj5_pin_mask = digitalPinToBitMask(pinInjector5);
    inj6_pin_port = portOutputRegister(digitalPinToPort(pinInjector6));
    inj6_pin_mask = digitalPinToBitMask(pinInjector6);
    inj7_pin_port = portOutputRegister(digitalPinToPort(pinInjector7));
    inj7_pin_mask = digitalPinToBitMask(pinInjector7);
    inj8_pin_port = portOutputRegister(digitalPinToPort(pinInjector8));
    inj8_pin_mask = digitalPinToBitMask(pinInjector8);
  }

  // MC33810 initialization if needed
  if ((ignitionOutputControl == OUTPUT_CONTROL_MC33810) || (injectorOutputControl == OUTPUT_CONTROL_MC33810)) {
    initMC33810();
    // This is required on as the LED pin can otherwise be reset to an input
    if ((LED_BUILTIN != SCK) && (LED_BUILTIN != MOSI) && (LED_BUILTIN != MISO)) {
      pinMode(LED_BUILTIN, OUTPUT);
    }
  }
}
