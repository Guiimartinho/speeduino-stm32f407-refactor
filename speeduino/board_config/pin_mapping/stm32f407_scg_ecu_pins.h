/**
 * @file stm32f407_scg_ecu_pins.h
 * @brief STM32F407 pin mapping for SCG-ECU 2.0
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 * Board by: Seaside Customs Garage (dvjcodec)
 * GitHub: https://github.com/dvjcodec/SCG-ECU-2.0-STM32F407-8x8
 *
 * Pin Layout (59 pins mapped):
 * - 6 Analog inputs: PA0-4, PB0 (ADC 0-5V)
 * - 3 Digital inputs: PC13, PE6, PE1 (Crank, Cam, Clutch)
 * - 8 Injector outputs: PE8-15 (Low-side 7A)
 * - 8 Ignition outputs: PD8-13, PB14-15 (High-side 5V/12V)
 * - 9 Auxiliary outputs: PE2-5, PC6-7, PD14-15, PE4 (PWM controls)
 * - 3 Stepper outputs: PA8, PC8-9 (IAC control)
 * - Communication: UART, CAN, USB, I2C, SPI
 *
 * SPECIAL FEATURES:
 * - Integrated wideband controller (SLC-FREE 2.0)
 * - Integrated barometer sensor (I2C)
 * - Software PWM for IGN5/IGN7 (PD8/PD11 have no hardware timer)
 *
 * @date 2025-11-06
 * @author Claude Code - SCG-ECU 2.0 Project
 */

#ifndef STM32F407_SCG_ECU_PINS_H
#define STM32F407_SCG_ECU_PINS_H

#include "../../globals.h"

/**
 * @brief Configure STM32F407 pin assignments for SCG-ECU 2.0
 * @details Sets all pin variables to SCG-ECU 2.0 specific GPIO pins
 * @note 100% based on real hardware schematic and CSV pinout
 */
void stm32f407ScgEcuConfigurePins(void);

#endif // STM32F407_SCG_ECU_PINS_H
