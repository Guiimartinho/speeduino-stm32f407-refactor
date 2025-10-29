/**
 * @file engine_protection.h
 * @brief Engine protection and rev limiting functions
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * This module handles all engine protection mechanisms:
 * - Rev limiting (hard and rolling cut)
 * - Launch control (2-step)
 * - Flat shift control
 * - Engine protection based on oil/coolant conditions
 *
 * Hard Cut: Completely disables fuel/ignition channels when limit reached
 * Rolling Cut: Progressively cuts cylinders based on RPM delta above limit
 */

#ifndef ENGINE_PROTECTION_H
#define ENGINE_PROTECTION_H

#include "globals.h"

/**
 * @brief Calculate maximum allowed RPM based on all active limiters
 *
 * Checks all possible RPM limits and returns the most restrictive:
 * - Fixed hard limit (checkRevLimit)
 * - Engine protection limit (oil/coolant based)
 * - Launch control limit (2-step)
 * - Flat shift limit (clutch based)
 *
 * @return Maximum allowed RPM in actual RPM (not divided by 100)
 */
uint16_t calculateMaxAllowedRPM(void);

/**
 * @brief Apply engine protection based on current RPM vs limit
 *
 * Main protection logic that:
 * 1. Sets/clears hard limit status bit
 * 2. Calls appropriate cut type (hard or rolling)
 * 3. Re-enables channels when RPM drops below limit
 *
 * @param maxAllowedRPM Maximum RPM allowed by all limiters
 */
void applyEngineProtection(uint16_t maxAllowedRPM);

/**
 * @brief Apply full hard cut (all channels off instantly)
 *
 * Disables fuel/ignition channels completely based on protect type:
 * - PROTECT_CUT_OFF: All channels on (protection disabled)
 * - PROTECT_CUT_IGN: Ignition only
 * - PROTECT_CUT_FUEL: Fuel only
 * - PROTECT_CUT_BOTH: Both fuel and ignition
 *
 * @param maxAllowedRPM Maximum allowed RPM (unused but kept for symmetry)
 */
void applyHardCut(uint16_t maxAllowedRPM);

/**
 * @brief Apply rolling cut (progressive cylinder cutting)
 *
 * Cuts cylinders progressively based on RPM delta above limit.
 * Uses rollingCutTable to determine cut percentage.
 *
 * Special handling for 4-stroke sequential to prevent half cycles.
 * Implements minimum revolution counts between cuts.
 *
 * @param maxAllowedRPM Maximum allowed RPM
 */
void applyRollingCut(uint16_t maxAllowedRPM);

/**
 * @brief Check and handle launch control and flat shift activation
 *
 * Launch Control (2-step):
 * - Active when clutch pressed and RPM < flatSArm threshold
 * - Requires TPS above threshold and VSS below limit (if enabled)
 * - Sets launchingHard flag when RPM exceeds launch limit
 *
 * Flat Shift:
 * - Active when clutch pressed and RPM >= flatSArm threshold
 * - RPM limit is the RPM when clutch was engaged
 * - Sets flatShiftingHard flag when over limit
 *
 * Both modes work with hard cut or rolling cut types.
 */
void checkLaunchAndFlatShift(void);

#endif // ENGINE_PROTECTION_H
