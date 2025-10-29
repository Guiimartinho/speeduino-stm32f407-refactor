/**
 * @file dwell_corrections.h
 * @brief Dwell corrections interface - coil dwell time correction functions
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module handles dwell time corrections:
 * - Battery voltage compensation (primary correction)
 * - Future: Temperature-based dwell adjustments
 *
 * Dwell time is critical for proper ignition coil saturation.
 * Too short = weak spark, too long = coil overheating.
 *
 * @note All functions preserve 100% original logic from corrections.cpp
 */

#ifndef DWELL_CORRECTIONS_H
#define DWELL_CORRECTIONS_H

#include "../../globals.h"

// ============================================================================
// DWELL CORRECTIONS INTERFACE
// ============================================================================

/**
 * @brief Function pointer type for dwell corrections
 * @param dwell Base dwell time in milliseconds (typically 3-5ms)
 * @return Corrected dwell time in milliseconds
 * @note Dwell must be positive and reasonable (<20ms typical max)
 */
typedef uint16_t (*DwellCorrectionFn)(uint16_t dwell);

/**
 * @brief Dwell corrections interface structure
 * @note Currently single correction, structured for future expansion
 */
typedef struct {
    // Main dwell correction pipeline
    DwellCorrectionFn correctionsDwell;

} DwellCorrectionsInterface;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
// These link to the original implementations in corrections.cpp

/**
 * @brief Main dwell corrections
 * @param dwell Base dwell time
 * @return Corrected dwell time (adjusted for battery voltage)
 * @note Lower voltage requires longer dwell for proper coil saturation
 */
uint16_t correctionsDwell(uint16_t dwell);

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Get dwell corrections interface
 * @return Pointer to dwell corrections interface (never NULL)
 * @note Returns const interface with all function pointers set
 */
const DwellCorrectionsInterface* dwellCorrectionsGetInterface(void);

#endif // DWELL_CORRECTIONS_H
