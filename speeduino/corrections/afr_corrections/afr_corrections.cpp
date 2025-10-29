/**
 * @file afr_corrections.cpp
 * @brief AFR corrections implementation - interface provider
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module provides the AFR corrections interface.
 * All actual implementations remain in corrections.cpp (100% preserved).
 *
 * This file only provides the interface structure with function pointers
 * linking to the original implementations.
 */

#include "afr_corrections.h"
#include "../../corrections.h"

// ============================================================================
// INTERFACE DEFINITION
// ============================================================================

/**
 * @brief Static const interface structure
 * @note All function pointers link to original implementations in corrections.cpp
 * @note Stored in flash (const), zero RAM overhead
 */
static const AfrCorrectionsInterface afrCorrectionsInterface = {
    // Target AFR calculation
    .calculateAfrTarget = &calculateAfrTarget,
};

// ============================================================================
// PUBLIC API
// ============================================================================

const AfrCorrectionsInterface* afrCorrectionsGetInterface(void)
{
  // Return const interface (never NULL)
  return &afrCorrectionsInterface;
}
