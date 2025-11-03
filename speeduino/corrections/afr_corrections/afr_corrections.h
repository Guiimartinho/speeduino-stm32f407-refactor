/**
 * @file afr_corrections.h
 * @brief AFR corrections interface - target AFR calculation and closed-loop control
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module handles AFR (Air-Fuel Ratio) related calculations:
 * - Target AFR calculation from tables
 * - Closed-loop AFR feedback (already in fuel_corrections)
 *
 * AFR target is used by closed-loop control to maintain optimal combustion.
 * Different operating conditions (idle, cruise, WOT) require different AFR targets.
 *
 * @note All functions preserve 100% original logic from corrections.cpp
 */

#ifndef AFR_CORRECTIONS_H
#define AFR_CORRECTIONS_H

#include "../../globals.h"
#include "../../src/PID_v1/PID_v1.h"

// ============================================================================
// GLOBAL STATE (shared with corrections.cpp for initialization)
// ============================================================================

/// PID controller global variables (defined in afr_corrections.cpp)
extern long PID_O2, PID_output, PID_AFRTarget;
extern PID egoPID;
extern uint16_t AFRnextCycle;

// ============================================================================
// AFR CORRECTIONS INTERFACE
// ============================================================================

/**
 * @brief Function pointer type for AFR target calculation
 * @param afrLookUpTable AFR table (RPM vs Load)
 * @param current Current engine status
 * @param page2 Configuration page 2
 * @param page6 Configuration page 6
 * @return Target AFR value (typically 10.0-18.0 AFR, stored as uint8_t)
 */
typedef uint8_t (*AfrTargetCalculationFn)(
    table3d16RpmLoad &afrLookUpTable,
    const statuses &current,
    const config2 &page2,
    const config6 &page6
);

/**
 * @brief Function pointer type for AFR closed-loop correction
 * @return Correction percentage (100 = no correction, <100 = lean, >100 = rich)
 */
typedef byte (*AfrClosedLoopCorrectionFn)(void);

/**
 * @brief AFR corrections interface structure
 * @note Groups all AFR-related functions (target calculation and closed-loop control)
 */
typedef struct {
    // Target AFR calculation
    AfrTargetCalculationFn calculateAfrTarget;

    // Closed-loop AFR correction (Simple & PID algorithms)
    AfrClosedLoopCorrectionFn correctionAFRClosedLoop;

} AfrCorrectionsInterface;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

/**
 * @brief Calculate target AFR from tables
 * @param afrLookUpTable AFR table to use
 * @param current Current engine status (RPM, load, etc.)
 * @param page2 Configuration page 2 (AFR settings)
 * @param page6 Configuration page 6 (additional AFR settings)
 * @return Target AFR value
 * @note Used by closed-loop control to determine correction direction
 * @note Considers operating mode (idle, cruise, WOT, power enrichment)
 */
uint8_t calculateAfrTarget(
    table3d16RpmLoad &afrLookUpTable,
    const statuses &current,
    const config2 &page2,
    const config6 &page6
);

/**
 * @brief Closed-loop AFR feedback correction
 * @return Correction percentage (100 = no correction, <100 = lean, >100 = rich)
 * @note Supports both Simple (1% steps) and PID algorithms
 * @note Only active when all closed-loop conditions are met:
 *       - Engine warmed up (coolant > egoTemp)
 *       - RPM > egoRPM
 *       - TPS < egoTPSMax
 *       - O2 sensor in valid range (ego_min < O2 < ego_max)
 *       - After startup delay (runSecs > ego_sdelay)
 *       - MAP within valid range (egoMAPMin < MAP < egoMAPMax)
 *       - DFCO not active
 */
byte correctionAFRClosedLoop(void);

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Get AFR corrections interface
 * @return Pointer to AFR corrections interface (never NULL)
 * @note Returns const interface with all function pointers set
 */
const AfrCorrectionsInterface* afrCorrectionsGetInterface(void);

#endif // AFR_CORRECTIONS_H
