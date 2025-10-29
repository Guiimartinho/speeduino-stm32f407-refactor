/**
 * @file dwell_corrections.cpp
 * @brief Dwell corrections implementation - interface provider
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module provides the dwell corrections interface.
 * All actual implementations remain in corrections.cpp (100% preserved).
 *
 * This file only provides the interface structure with function pointers
 * linking to the original implementations.
 */

#include "dwell_corrections.h"
#include "../../corrections.h"

// ============================================================================
// INTERFACE DEFINITION
// ============================================================================

/**
 * @brief Static const interface structure
 * @note All function pointers link to original implementations in corrections.cpp
 * @note Stored in flash (const), zero RAM overhead
 */
static const DwellCorrectionsInterface dwellCorrectionsInterface = {
    // Main dwell correction pipeline
    .correctionsDwell = &correctionsDwell,
};

// ============================================================================
// PUBLIC API
// ============================================================================

const DwellCorrectionsInterface* dwellCorrectionsGetInterface(void)
{
  // Return const interface (never NULL)
  return &dwellCorrectionsInterface;
}
