/**
 * @file board_config.cpp
 * @brief Main board configuration implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * PRESERVES 100% LOGIC from init.cpp setPinMapping() (lines 1216-3062)
 * Original: 1853 lines in single function
 * Modular: Function-based architecture with table-driven board selection
 *
 * This module is the main entry point for all board configuration.
 * It orchestrates pin mapping, pin mode setup, and port pointer configuration.
 */

#include "board_config.h"
#include "board_registry.h"
#include "pin_mapping/pin_setup.h"
#include "../globals.h"

void boardConfigSetPinMapping(byte boardID)
{
  // Force set defaults. Will be overwritten by board-specific config
  // PRESERVES init.cpp lines 1219-1220
  injectorOutputControl = OUTPUT_CONTROL_DIRECT;
  ignitionOutputControl = OUTPUT_CONTROL_DIRECT;

  // Avoid potential divide by 0 when starting decoders
  // PRESERVES init.cpp line 1222
  if (configPage4.triggerTeeth == 0U) {
    configPage4.triggerTeeth = 4U;
  }

  // Get board-specific configuration function from registry
  // This replaces the giant switch-case (init.cpp lines 1224-2767)
  BoardConfigFunc configFunc = boardRegistryGetConfig(boardID);

  // If board not found, use default
  if (configFunc == NULL) {
    configFunc = boardRegistryGetDefault();
  }

  // Execute board-specific pin configuration
  if (configFunc != NULL) {
    configFunc();
  }

  // Configure user-programmable pins
  // PRESERVES init.cpp lines 2769-2806
  boardConfigSetupProgrammablePins();
}

void boardConfigSetupProgrammablePins(void)
{
  // PRESERVES init.cpp lines 2771-2806
  pinSetupConfigureProgrammablePins();

  // PRESERVES init.cpp lines 2808-2819
  pinSetupConfigureResetControl();
}

void boardConfigSetupPinModes(void)
{
  // PRESERVES init.cpp lines 2822-3049
  pinSetupConfigurePinModes();

  // PRESERVES init.cpp lines 2837-2838
  pinSetupApplyLegacyMAP();
}

void boardConfigSetupPortPointers(void)
{
  // PRESERVES init.cpp lines 2840-2875 (ignition)
  pinSetupConfigureIgnitionPorts();

  // PRESERVES init.cpp lines 2877-2925 (injector)
  pinSetupConfigureInjectorPorts();

  // Additional port pointers (tacho, pump, triggers, flex)
  // PRESERVES init.cpp lines 2926-3061

  // Tacho and fuel pump output ports
  tach_pin_port = portOutputRegister(digitalPinToPort(pinTachOut));
  tach_pin_mask = digitalPinToBitMask(pinTachOut);
  pump_pin_port = portOutputRegister(digitalPinToPort(pinFuelPump));
  pump_pin_mask = digitalPinToBitMask(pinFuelPump);

  // Trigger input ports (must come after pinMode statements)
  triggerPri_pin_port = portInputRegister(digitalPinToPort(pinTrigger));
  triggerPri_pin_mask = digitalPinToBitMask(pinTrigger);
  triggerSec_pin_port = portInputRegister(digitalPinToPort(pinTrigger2));
  triggerSec_pin_mask = digitalPinToBitMask(pinTrigger2);
  triggerThird_pin_port = portInputRegister(digitalPinToPort(pinTrigger3));
  triggerThird_pin_mask = digitalPinToBitMask(pinTrigger3);

  // Flex sensor input port
  flex_pin_port = portInputRegister(digitalPinToPort(pinFlex));
  flex_pin_mask = digitalPinToBitMask(pinFlex);
}
