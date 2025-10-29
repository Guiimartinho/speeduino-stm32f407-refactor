/**
 * @file fuel_calculations.h
 * @brief Fuel calculation functions
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * This module handles all fuel-related calculations:
 * - Pulsewidth calculation (PW)
 * - VE table lookup
 * - Duty cycle limiting
 * - Staging logic for multiple injectors
 *
 * The core PW equation combines:
 * - REQ_FUEL (base fuel requirement)
 * - VE (volumetric efficiency from 3D table)
 * - MAP (manifold pressure, optional multiplier)
 * - AFR (target vs actual, optional closed loop)
 * - Corrections (warmup, accel, etc.)
 * - Injector opening time
 */

#ifndef FUEL_CALCULATIONS_H
#define FUEL_CALCULATIONS_H

#include "globals.h"

/**
 * @brief Calculate injector pulsewidth
 *
 * Core fuel calculation using optimized fixed-point math (100% float-free).
 * Formula: PW = (REQ_FUEL * VE * MAP * AFR * corrections) + injOpen
 *
 * Special modes:
 * - multiplyMAP: Optional MAP correction (100% or baro-referenced)
 * - includeAFR: Closed-loop AFR correction (wideband O2 only)
 * - incorporateAFR: Stoich vs target AFR correction
 * - AE_MODE_ADDER: Acceleration enrichment as percentage of REQ_FUEL
 *
 * @param REQ_FUEL Base fuel requirement in microseconds
 * @param VE Volumetric efficiency percentage (0-255)
 * @param MAP Manifold absolute pressure in kPa
 * @param corrections Combined corrections percentage (0-1000+)
 * @param injOpen Injector opening time in microseconds
 * @return Pulsewidth in microseconds (0-65535)
 */
uint16_t PW(int REQ_FUEL, byte VE, long MAP, uint16_t corrections, int injOpen);

/**
 * @brief Lookup VE value from primary 3D fuel table
 *
 * Performs 3D table interpolation using:
 * - X axis: RPM
 * - Y axis: Load (varies by fuel algorithm - speed density, alpha-n, etc.)
 *
 * Also updates currentStatus.fuelLoad for TunerStudio display.
 *
 * @return VE percentage (0-255)
 */
uint8_t getVE1(void);

/**
 * @brief Calculate maximum allowed pulsewidth based on duty cycle limit
 *
 * Prevents injector saturation by limiting PW to a percentage of revolution time.
 * Default limit is typically 85% to ensure injectors have time to close.
 *
 * Calculation accounts for:
 * - 2-stroke vs 4-stroke (4-stroke has 2x time available)
 * - Number of squirts per revolution
 * - Revolution time (decreases as RPM increases)
 *
 * @return Maximum pulsewidth in microseconds
 */
uint16_t calculatePWLimit(void);

/**
 * @brief Calculate staging pulsewidths for all cylinders
 *
 * Handles two staging modes:
 *
 * TABLE MODE:
 * - Uses 3D staging table to split fuel between primary/secondary injectors
 * - Percentage split based on RPM and load
 * - Example: 70% primary, 30% secondary
 *
 * AUTO MODE:
 * - Primary injectors used up to their duty cycle limit
 * - Excess fuel automatically routed to secondary injectors
 * - Scales based on injector flow rate ratios
 *
 * Distributes PW1 (primary) and PW2 (secondary) to all 8 channels
 * based on cylinder count and injection mode (sequential, semi-seq, paired).
 *
 * Special cases:
 * - Non-sequential requires minimum 4 revolution cuts to prevent half cycles
 * - Sequential with both fuel/ign cut requires ignition delay after fuel returns
 *
 * @param pwLimit Maximum allowed pulsewidth from calculatePWLimit()
 */
void calculateStaging(uint32_t pwLimit);

#endif // FUEL_CALCULATIONS_H
