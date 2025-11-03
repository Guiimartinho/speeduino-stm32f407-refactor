/**
 * @file fuel_corrections.h
 * @brief Fuel corrections interface - all fuel-related correction functions
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module handles all fuel correction calculations:
 * - Warmup Enrichment (WUE)
 * - After Start Enrichment (ASE)
 * - Cranking enrichment
 * - Acceleration enrichment (TPS/MAP based)
 * - Environmental corrections (IAT, baro, fuel temp)
 * - Battery voltage correction
 * - Special corrections (launch, flex fuel)
 * - DFCO (Deceleration Fuel Cut-Off)
 *
 * @note All functions preserve 100% original logic from corrections.cpp
 */

#ifndef FUEL_CORRECTIONS_H
#define FUEL_CORRECTIONS_H

#include "../../globals.h"

// ============================================================================
// GLOBAL STATE (Acceleration enrichment tracking)
// ============================================================================

/// MAP DOT value seen when MAE was activated (defined in fuel_corrections.cpp)
extern byte activateMAPDOT;

/// TPS DOT value seen when TAE was activated (defined in fuel_corrections.cpp)
extern byte activateTPSDOT;

// ============================================================================
// FUEL CORRECTIONS INTERFACE
// ============================================================================

/**
 * @brief Function pointer type for fuel corrections that return byte (percentage)
 * @return Correction percentage (100 = no correction)
 */
typedef byte (*FuelCorrectionByteFn)(void);

/**
 * @brief Function pointer type for fuel corrections that return uint16_t
 * @return Correction value
 */
typedef uint16_t (*FuelCorrectionU16Fn)(void);

/**
 * @brief Function pointer type for boolean fuel corrections (DFCO)
 * @return true/false
 */
typedef bool (*FuelCorrectionBoolFn)(void);

/**
 * @brief Fuel corrections interface structure
 * @note Groups all fuel correction function pointers
 */
typedef struct {
    // Main fuel correction pipeline
    FuelCorrectionU16Fn correctionsFuel;

    // Individual correction components
    FuelCorrectionByteFn correctionWUE;           // Warmup enrichment
    FuelCorrectionU16Fn correctionCranking;       // Cranking enrichment
    FuelCorrectionByteFn correctionASE;           // After start enrichment
    FuelCorrectionU16Fn correctionAccel;          // Acceleration enrichment
    FuelCorrectionByteFn correctionFloodClear;    // Flood clear check
    FuelCorrectionByteFn correctionAFRClosedLoop; // Closed loop AFR
    FuelCorrectionByteFn correctionFlex;          // Flex fuel
    FuelCorrectionByteFn correctionFuelTemp;      // Fuel temp
    FuelCorrectionByteFn correctionBatVoltage;    // Battery voltage
    FuelCorrectionByteFn correctionIATDensity;    // IAT density
    FuelCorrectionByteFn correctionBaro;          // Barometric pressure
    FuelCorrectionByteFn correctionLaunch;        // Launch control
    FuelCorrectionByteFn correctionDFCOfuel;      // DFCO taper
    FuelCorrectionBoolFn correctionDFCO;          // DFCO check

} FuelCorrectionsInterface;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
// These link to the original implementations in corrections.cpp

// Main pipeline
uint16_t correctionsFuel(void);

// Individual corrections
byte correctionWUE(void);
uint16_t correctionCranking(void);
byte correctionASE(void);
uint16_t correctionAccel(void);
byte correctionFloodClear(void);
byte correctionAFRClosedLoop(void);
byte correctionFlex(void);
byte correctionFuelTemp(void);
byte correctionBatVoltage(void);
byte correctionIATDensity(void);
byte correctionBaro(void);
byte correctionLaunch(void);
byte correctionDFCOfuel(void);
bool correctionDFCO(void);

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Get fuel corrections interface
 * @return Pointer to fuel corrections interface (never NULL)
 * @note Returns const interface with all function pointers set
 */
const FuelCorrectionsInterface* fuelCorrectionsGetInterface(void);

#endif // FUEL_CORRECTIONS_H
