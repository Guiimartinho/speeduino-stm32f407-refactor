/**
 * @file stm32f407_pins.h
 * @brief STM32F407 pin mapping configuration
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 * Board ID: 60 (Black F407VE)
 *
 * Migrated from init.cpp case 60 (lines 2443-2622)
 *
 * Pin Layout:
 * - 8 Injector channels: PD12-15, PE9, PE11, PE13-14
 * - 8 Ignition channels: PD7, PB9, PA8, PD10, PD9, PB7, (2 undefined)
 * - Trigger: PE0 (primary), PE1 (secondary)
 * - Sensors: PC0-5 (IAT, TPS, MAP, CLT, O2, BAT)
 * - Idle: PC7 (idle1), PD3 (idle2)
 * - Boost: PC6
 * - Fan: PE6
 * - Fuel Pump: PE3
 * - Stepper: PE2 (enable), PE5 (step), PE7 (dir)
 * - Tacho: PC13
 * - Flex: PD4
 * - Baro: PB1
 */

#ifndef STM32F407_PINS_H
#define STM32F407_PINS_H

#include "../../globals.h"

/**
 * @brief Configure STM32F407 pin assignments
 * @details Sets all pin variables to STM32F407-specific GPIO pins
 * @note 100% preserves original logic from init.cpp case 60
 */
void stm32f407ConfigurePins(void);

#endif // STM32F407_PINS_H
