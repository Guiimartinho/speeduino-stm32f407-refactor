/**
 * @file ignition_corrections.h
 * @brief Ignition corrections interface - all ignition timing correction functions
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module handles all ignition timing correction calculations:
 * - Environmental corrections (IAT retard, CLT advance)
 * - Timing corrections (flex fuel, WMI)
 * - Protection corrections (knock, rev limit)
 * - Special corrections (launch, flat shift, nitrous)
 * - DFCO ignition timing
 *
 * @note All functions preserve 100% original logic from corrections.cpp
 * @note All correction functions take and return int8_t (signed degrees)
 */

#ifndef IGNITION_CORRECTIONS_H
#define IGNITION_CORRECTIONS_H

#include "../../globals.h"

// ============================================================================
// IGNITION CORRECTIONS INTERFACE
// ============================================================================

/**
 * @brief Function pointer type for ignition corrections
 * @param advance Current ignition advance in degrees
 * @return Corrected ignition advance in degrees
 * @note All ignition corrections follow this signature
 */
typedef int8_t (*IgnitionCorrectionFn)(int8_t advance);

/**
 * @brief Ignition corrections interface structure
 * @note Groups all ignition correction function pointers
 * @note Corrections applied in pipeline order for safety
 */
typedef struct {
    // Main ignition correction pipeline
    IgnitionCorrectionFn correctionsIgn;

    // Override corrections (highest priority)
    IgnitionCorrectionFn correctionFixedTiming;         // Fixed timing override
    IgnitionCorrectionFn correctionCrankingFixedTiming; // Cranking timing

    // Environmental corrections
    IgnitionCorrectionFn correctionIATretard;  // IAT-based retard
    IgnitionCorrectionFn correctionCLTadvance; // CLT-based advance

    // Fuel type corrections
    IgnitionCorrectionFn correctionFlexTiming; // Flex fuel timing
    IgnitionCorrectionFn correctionWMITiming;  // Water/methanol injection

    // Protection corrections (safety-critical)
    IgnitionCorrectionFn correctionKnockTiming;    // Knock protection
    IgnitionCorrectionFn correctionSoftRevLimit;   // Rev limiter

    // Special feature corrections
    IgnitionCorrectionFn correctionIdleAdvance;    // Idle control
    IgnitionCorrectionFn correctionNitrous;        // Nitrous timing
    IgnitionCorrectionFn correctionSoftLaunch;     // Launch control
    IgnitionCorrectionFn correctionSoftFlatShift;  // Flat shift
    IgnitionCorrectionFn correctionDFCOignition;   // DFCO timing

} IgnitionCorrectionsInterface;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
// These link to the original implementations in corrections.cpp

// Main pipeline
int8_t correctionsIgn(int8_t advance);

// Override corrections
int8_t correctionFixedTiming(int8_t advance);
int8_t correctionCrankingFixedTiming(int8_t advance);

// Environmental corrections
int8_t correctionIATretard(int8_t advance);
int8_t correctionCLTadvance(int8_t advance);

// Fuel type corrections
int8_t correctionFlexTiming(int8_t advance);
int8_t correctionWMITiming(int8_t advance);

// Protection corrections
int8_t correctionKnockTiming(int8_t advance);
int8_t correctionSoftRevLimit(int8_t advance);

// Special feature corrections
int8_t correctionIdleAdvance(int8_t advance);
int8_t correctionNitrous(int8_t advance);
int8_t correctionSoftLaunch(int8_t advance);
int8_t correctionSoftFlatShift(int8_t advance);
int8_t correctionDFCOignition(int8_t advance);

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Get ignition corrections interface
 * @return Pointer to ignition corrections interface (never NULL)
 * @note Returns const interface with all function pointers set
 */
const IgnitionCorrectionsInterface* ignitionCorrectionsGetInterface(void);

#endif // IGNITION_CORRECTIONS_H
