/**
 * @file ignition_calculations.h
 * @brief Ignition calculation functions
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * This module handles all ignition-related calculations:
 * - Advance table lookup
 * - Dwell time calculation
 * - Ignition angle calculation for all 8 cylinders
 *
 * Dwell is the charging time for ignition coils (typically 3-5ms).
 * Advance is the spark timing relative to TDC (0-60 degrees BTDC typical).
 * Ignition angles define start (dwell begins) and end (spark fires) positions.
 */

#ifndef IGNITION_CALCULATIONS_H
#define IGNITION_CALCULATIONS_H

#include "globals.h"

/**
 * @brief Lookup ignition advance from 3D ignition table
 *
 * Performs 3D table interpolation using:
 * - X axis: RPM
 * - Y axis: Load (varies by ignition algorithm - speed density, alpha-n, etc.)
 *
 * Result is passed through correctionsIgn() for knock, coolant, and other corrections.
 * Also updates currentStatus.ignLoad for TunerStudio display.
 *
 * @return Ignition advance in degrees BTDC (can be negative for ATDC)
 */
int8_t getAdvance1(void);

/**
 * @brief Calculate dwell time for ignition coils
 *
 * Dwell selection logic:
 * 1. If cranking: Use dwellCrank (typically longer for cold starts)
 * 2. If running and useDwellMap enabled: Lookup from 3D dwell table
 * 3. If running and useDwellMap disabled: Use fixed dwellRun value
 * 4. Apply corrections (battery voltage compensation, etc.)
 *
 * Dwell is stored as milliseconds * 10 in config (eg. 4.3ms = 43).
 * Returned value is in microseconds for accurate timing.
 *
 * @return Dwell time in microseconds (typically 3000-5000us)
 */
uint16_t calculateDwell(void);

/**
 * @brief Calculate ignition start and end angles for all cylinders
 *
 * For each active cylinder (based on nCylinders config):
 * - Calculates ignition end angle (when spark fires)
 * - Calculates ignition start angle (when dwell begins)
 * - Accounts for advance, dwell angle, and cylinder phasing
 *
 * Special handling:
 * - Sequential mode (4, 6, 8 cyl): All channels independent, requires sync
 * - Wasted spark mode: Paired cylinders fire together
 * - Rotary mode (4 cyl only): Trailing spark calculation with split table
 * - Half-sync vs full-sync: Automatic switching based on engine state
 *
 * Cylinder configurations supported:
 * - 1, 2, 3, 4, 5, 6, 8 cylinders
 * - Wasted spark or sequential
 * - Rotary (leading + trailing)
 *
 * @param dwellAngle Dwell time converted to crankshaft degrees
 */
void calculateIgnitionAngles(uint16_t dwellAngle);

#endif // IGNITION_CALCULATIONS_H
