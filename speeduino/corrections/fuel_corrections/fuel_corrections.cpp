/**
 * @file fuel_corrections.cpp
 * @brief Fuel corrections implementation - interface provider
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module provides the fuel corrections interface.
 * All actual implementations remain in corrections.cpp (100% preserved).
 *
 * This file only provides the interface structure with function pointers
 * linking to the original implementations.
 */

#include "fuel_corrections.h"
#include "../../corrections.h"

// ============================================================================
// INTERFACE DEFINITION
// ============================================================================

/**
 * @brief Static const interface structure
 * @note All function pointers link to original implementations in corrections.cpp
 * @note Stored in flash (const), zero RAM overhead
 */
static const FuelCorrectionsInterface fuelCorrectionsInterface = {
    // Main fuel correction pipeline
    .correctionsFuel = &correctionsFuel,

    // Individual correction components
    .correctionWUE = &correctionWUE,
    .correctionCranking = &correctionCranking,
    .correctionASE = &correctionASE,
    .correctionAccel = &correctionAccel,
    .correctionFloodClear = &correctionFloodClear,
    .correctionAFRClosedLoop = &correctionAFRClosedLoop,
    .correctionFlex = &correctionFlex,
    .correctionFuelTemp = &correctionFuelTemp,
    .correctionBatVoltage = &correctionBatVoltage,
    .correctionIATDensity = &correctionIATDensity,
    .correctionBaro = &correctionBaro,
    .correctionLaunch = &correctionLaunch,
    .correctionDFCOfuel = &correctionDFCOfuel,
    .correctionDFCO = &correctionDFCO,
};

// ============================================================================
// PUBLIC API
// ============================================================================

const FuelCorrectionsInterface* fuelCorrectionsGetInterface(void)
{
  // Return const interface (never NULL)
  return &fuelCorrectionsInterface;
}
