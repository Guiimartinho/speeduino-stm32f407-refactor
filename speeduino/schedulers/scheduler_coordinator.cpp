/**
 * @file scheduler_coordinator.cpp
 * @brief Scheduler coordinator implementation - unified API for scheduling
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module coordinates all scheduling subsystems.
 * All actual implementations remain in scheduler.cpp (100% preserved).
 *
 * ARCHITECTURE:
 * - Direct wrapper to original functions (no function pointer overhead)
 * - Guard clauses for safety (except ISRs)
 * - 100% preservation of original logic
 *
 * PRESERVES 100% LOGIC from original scheduler.cpp
 */

#include "scheduler_coordinator.h"
#include "../scheduler.h"
#include "../globals.h"

// Anonymous namespace for private state
namespace {

/**
 * @brief Initialization complete flag
 * @note Prevents API calls before initialization
 */
static volatile bool isInitialized = false;

} // anonymous namespace

// ============================================================================
// INITIALIZATION API
// ============================================================================

void schedulerCoordinatorInitialize(void)
{
  // Guard clause: Prevent re-initialization
  if (isInitialized) {
    return;
  }

  // Call original initialization (from scheduler.cpp)
  initialiseSchedulers();

  // Mark as initialized
  isInitialized = true;
}

void schedulerCoordinatorBeginPriming(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  // Call original priming (from scheduler.cpp)
  beginInjectorPriming();
}

// ============================================================================
// FUEL SCHEDULING API
// ============================================================================

void schedulerCoordinatorSetFuel(uint8_t scheduleNum,
                                  uint32_t timeout,
                                  uint32_t duration)
{
  // Guard clause: Check initialization
  if (!isInitialized) {
    return;
  }

  // Guard clause: Validate channel (0-7 for 8 channels)
  if (scheduleNum >= INJ_CHANNELS) {
    return;
  }

  // Direct call to original inline function based on channel
  // NOTE: setFuelSchedule() is inline function in scheduler.h
  switch (scheduleNum) {
    case 0:
      setFuelSchedule(fuelSchedule1, timeout, duration);
      break;
    case 1:
      setFuelSchedule(fuelSchedule2, timeout, duration);
      break;
    case 2:
      setFuelSchedule(fuelSchedule3, timeout, duration);
      break;
    case 3:
      setFuelSchedule(fuelSchedule4, timeout, duration);
      break;
#if (INJ_CHANNELS >= 5)
    case 4:
      setFuelSchedule(fuelSchedule5, timeout, duration);
      break;
#endif
#if (INJ_CHANNELS >= 6)
    case 5:
      setFuelSchedule(fuelSchedule6, timeout, duration);
      break;
#endif
#if (INJ_CHANNELS >= 7)
    case 6:
      setFuelSchedule(fuelSchedule7, timeout, duration);
      break;
#endif
#if (INJ_CHANNELS >= 8)
    case 7:
      setFuelSchedule(fuelSchedule8, timeout, duration);
      break;
#endif
    default:
      // Should never reach here due to guard clause above
      break;
  }
}

void schedulerCoordinatorDisableFuel(uint8_t channel)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  // Guard clause: Validate channel
  if (channel >= INJ_CHANNELS) {
    return;
  }

  // Direct call to original function (from scheduler.cpp)
  disableFuelSchedule(channel);
}

void schedulerCoordinatorDisableAllFuel(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  // Direct call to original function (from scheduler.cpp)
  disableAllFuelSchedules();
}

// ============================================================================
// IGNITION SCHEDULING API
// ============================================================================

void schedulerCoordinatorSetIgnition(uint8_t scheduleNum,
                                      uint32_t timeout,
                                      uint32_t duration)
{
  // Guard clause: Check initialization
  if (!isInitialized) {
    return;
  }

  // Guard clause: Validate channel (0-7 for 8 channels)
  if (scheduleNum >= IGN_CHANNELS) {
    return;
  }

  // Direct call to original inline function based on channel
  // NOTE: setIgnitionSchedule() is inline function in scheduler.h
  switch (scheduleNum) {
    case 0:
      setIgnitionSchedule(ignitionSchedule1, timeout, duration);
      break;
#if (IGN_CHANNELS >= 2)
    case 1:
      setIgnitionSchedule(ignitionSchedule2, timeout, duration);
      break;
#endif
#if (IGN_CHANNELS >= 3)
    case 2:
      setIgnitionSchedule(ignitionSchedule3, timeout, duration);
      break;
#endif
#if (IGN_CHANNELS >= 4)
    case 3:
      setIgnitionSchedule(ignitionSchedule4, timeout, duration);
      break;
#endif
#if (IGN_CHANNELS >= 5)
    case 4:
      setIgnitionSchedule(ignitionSchedule5, timeout, duration);
      break;
#endif
#if (IGN_CHANNELS >= 6)
    case 5:
      setIgnitionSchedule(ignitionSchedule6, timeout, duration);
      break;
#endif
#if (IGN_CHANNELS >= 7)
    case 6:
      setIgnitionSchedule(ignitionSchedule7, timeout, duration);
      break;
#endif
#if (IGN_CHANNELS >= 8)
    case 7:
      setIgnitionSchedule(ignitionSchedule8, timeout, duration);
      break;
#endif
    default:
      // Should never reach here due to guard clause above
      break;
  }
}

void schedulerCoordinatorRefreshIgnition1(uint32_t timeToEnd)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  // Direct call to original function (from scheduler.cpp)
  // NOTE: This function uses noInterrupts() internally
  refreshIgnitionSchedule1(timeToEnd);
}

void schedulerCoordinatorDisableIgnition(uint8_t channel)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  // Guard clause: Validate channel
  if (channel >= IGN_CHANNELS) {
    return;
  }

  // Direct call to original function (from scheduler.cpp)
  disableIgnSchedule(channel);
}

void schedulerCoordinatorDisableAllIgnition(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  // Direct call to original function (from scheduler.cpp)
  disableAllIgnSchedules();
}

// ============================================================================
// UTILITY API
// ============================================================================

bool schedulerCoordinatorIsInitialized(void)
{
  return isInitialized;
}

const char* schedulerCoordinatorGetName(void)
{
  return "Scheduler Coordinator";
}
