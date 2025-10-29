/**
 * @file corrections_coordinator.cpp
 * @brief Corrections coordinator implementation - unified API for all corrections
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module coordinates all correction subsystems:
 * - Fuel corrections (WUE, ASE, cranking, accel, environmental, etc.)
 * - Ignition corrections (environmental, timing, protection, special)
 * - Dwell corrections (battery voltage compensation)
 * - AFR corrections (target calculation, closed loop)
 *
 * ARCHITECTURE:
 * - Interface-based modular design
 * - Function pointer dispatch for flexibility
 * - 100% preservation of original logic
 * - RTOS-ready (thread-safe after initialization)
 *
 * PRESERVES 100% LOGIC from original corrections.cpp
 */

#include "corrections_coordinator.h"
#include "fuel_corrections/fuel_corrections.h"
#include "ignition_corrections/ignition_corrections.h"
#include "dwell_corrections/dwell_corrections.h"
#include "afr_corrections/afr_corrections.h"
#include "../corrections.h"
#include "../globals.h"

// Anonymous namespace for private state
namespace {

/**
 * @brief Cached interface pointers for fast access
 * @note Set once during initialization, then read-only
 */
static const FuelCorrectionsInterface* fuelInterface = NULL;
static const IgnitionCorrectionsInterface* ignitionInterface = NULL;
static const DwellCorrectionsInterface* dwellInterface = NULL;
static const AfrCorrectionsInterface* afrInterface = NULL;

/**
 * @brief Initialization complete flag
 * @note Prevents API calls before initialization
 */
static volatile bool isInitialized = false;

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

void correctionsCoordinatorInitialize(void)
{
  // Guard clause: Prevent re-initialization
  if (isInitialized) {
    return;
  }

  // Get all subsystem interfaces
  fuelInterface = fuelCorrectionsGetInterface();
  ignitionInterface = ignitionCorrectionsGetInterface();
  dwellInterface = dwellCorrectionsGetInterface();
  afrInterface = afrCorrectionsGetInterface();

  // Call original initialization (from corrections.cpp)
  initialiseCorrections();

  // Mark as initialized
  isInitialized = true;
}

// ============================================================================
// FUEL CORRECTIONS API
// ============================================================================

uint16_t correctionsCoordinatorFuel(void)
{
  // Guard clause
  if (!isInitialized || fuelInterface == NULL || fuelInterface->correctionsFuel == NULL) {
    return 100U; // No correction (100%)
  }

  return fuelInterface->correctionsFuel();
}

uint16_t correctionsCoordinatorCranking(void)
{
  // Guard clause
  if (!isInitialized || fuelInterface == NULL || fuelInterface->correctionCranking == NULL) {
    return 100U;
  }

  return fuelInterface->correctionCranking();
}

uint16_t correctionsCoordinatorAccel(void)
{
  // Guard clause
  if (!isInitialized || fuelInterface == NULL || fuelInterface->correctionAccel == NULL) {
    return 100U;
  }

  return fuelInterface->correctionAccel();
}

bool correctionsCoordinatorDFCO(void)
{
  // Guard clause
  if (!isInitialized || fuelInterface == NULL || fuelInterface->correctionDFCO == NULL) {
    return false;
  }

  return fuelInterface->correctionDFCO();
}

// ============================================================================
// IGNITION CORRECTIONS API
// ============================================================================

int8_t correctionsCoordinatorIgn(int8_t base_advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionsIgn == NULL) {
    return base_advance; // No correction
  }

  return ignitionInterface->correctionsIgn(base_advance);
}

int8_t correctionsCoordinatorFixedTiming(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionFixedTiming == NULL) {
    return advance;
  }

  return ignitionInterface->correctionFixedTiming(advance);
}

int8_t correctionsCoordinatorCrankingFixedTiming(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionCrankingFixedTiming == NULL) {
    return advance;
  }

  return ignitionInterface->correctionCrankingFixedTiming(advance);
}

int8_t correctionsCoordinatorFlexTiming(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionFlexTiming == NULL) {
    return advance;
  }

  return ignitionInterface->correctionFlexTiming(advance);
}

int8_t correctionsCoordinatorWMITiming(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionWMITiming == NULL) {
    return advance;
  }

  return ignitionInterface->correctionWMITiming(advance);
}

int8_t correctionsCoordinatorIATretard(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionIATretard == NULL) {
    return advance;
  }

  return ignitionInterface->correctionIATretard(advance);
}

int8_t correctionsCoordinatorCLTadvance(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionCLTadvance == NULL) {
    return advance;
  }

  return ignitionInterface->correctionCLTadvance(advance);
}

int8_t correctionsCoordinatorIdleAdvance(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionIdleAdvance == NULL) {
    return advance;
  }

  return ignitionInterface->correctionIdleAdvance(advance);
}

int8_t correctionsCoordinatorSoftRevLimit(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionSoftRevLimit == NULL) {
    return advance;
  }

  return ignitionInterface->correctionSoftRevLimit(advance);
}

int8_t correctionsCoordinatorNitrous(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionNitrous == NULL) {
    return advance;
  }

  return ignitionInterface->correctionNitrous(advance);
}

int8_t correctionsCoordinatorSoftLaunch(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionSoftLaunch == NULL) {
    return advance;
  }

  return ignitionInterface->correctionSoftLaunch(advance);
}

int8_t correctionsCoordinatorSoftFlatShift(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionSoftFlatShift == NULL) {
    return advance;
  }

  return ignitionInterface->correctionSoftFlatShift(advance);
}

int8_t correctionsCoordinatorKnockTiming(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionKnockTiming == NULL) {
    return advance;
  }

  return ignitionInterface->correctionKnockTiming(advance);
}

int8_t correctionsCoordinatorDFCOignition(int8_t advance)
{
  // Guard clause
  if (!isInitialized || ignitionInterface == NULL || ignitionInterface->correctionDFCOignition == NULL) {
    return advance;
  }

  return ignitionInterface->correctionDFCOignition(advance);
}

// ============================================================================
// DWELL CORRECTIONS API
// ============================================================================

uint16_t correctionsCoordinatorDwell(uint16_t dwell)
{
  // Guard clause
  if (!isInitialized || dwellInterface == NULL || dwellInterface->correctionsDwell == NULL) {
    return dwell; // No correction
  }

  return dwellInterface->correctionsDwell(dwell);
}

// ============================================================================
// AFR CORRECTIONS API
// ============================================================================

uint8_t correctionsCoordinatorCalculateAfrTarget(
    table3d16RpmLoad &afrLookUpTable,
    const statuses &current,
    const config2 &page2,
    const config6 &page6
)
{
  // Guard clause
  if (!isInitialized || afrInterface == NULL || afrInterface->calculateAfrTarget == NULL) {
    return 147U; // Stoichiometric AFR for gasoline (14.7:1 * 10)
  }

  return afrInterface->calculateAfrTarget(afrLookUpTable, current, page2, page6);
}

// ============================================================================
// UTILITY API
// ============================================================================

bool correctionsCoordinatorIsInitialized(void)
{
  return isInitialized;
}

const char* correctionsCoordinatorGetName(void)
{
  return "Corrections Coordinator";
}
