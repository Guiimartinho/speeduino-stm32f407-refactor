/**
 * @file ignition_corrections.cpp
 * @brief Ignition corrections implementation - interface provider
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module provides the ignition corrections interface.
 * All actual implementations remain in corrections.cpp (100% preserved).
 *
 * This file only provides the interface structure with function pointers
 * linking to the original implementations.
 */

#include "ignition_corrections.h"
#include "../../corrections.h"

// ============================================================================
// INTERFACE DEFINITION
// ============================================================================

/**
 * @brief Static const interface structure
 * @note All function pointers link to original implementations in corrections.cpp
 * @note Stored in flash (const), zero RAM overhead
 */
static const IgnitionCorrectionsInterface ignitionCorrectionsInterface = {
    // Main ignition correction pipeline
    .correctionsIgn = &correctionsIgn,

    // Override corrections
    .correctionFixedTiming = &correctionFixedTiming,
    .correctionCrankingFixedTiming = &correctionCrankingFixedTiming,

    // Environmental corrections
    .correctionIATretard = &correctionIATretard,
    .correctionCLTadvance = &correctionCLTadvance,

    // Fuel type corrections
    .correctionFlexTiming = &correctionFlexTiming,
    .correctionWMITiming = &correctionWMITiming,

    // Protection corrections
    .correctionKnockTiming = &correctionKnockTiming,
    .correctionSoftRevLimit = &correctionSoftRevLimit,

    // Special feature corrections
    .correctionIdleAdvance = &correctionIdleAdvance,
    .correctionNitrous = &correctionNitrous,
    .correctionSoftLaunch = &correctionSoftLaunch,
    .correctionSoftFlatShift = &correctionSoftFlatShift,
    .correctionDFCOignition = &correctionDFCOignition,
};

// ============================================================================
// PUBLIC API
// ============================================================================

const IgnitionCorrectionsInterface* ignitionCorrectionsGetInterface(void)
{
  // Return const interface (never NULL)
  return &ignitionCorrectionsInterface;
}
