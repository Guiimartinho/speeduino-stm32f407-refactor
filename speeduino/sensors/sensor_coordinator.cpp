/**
 * @file sensor_coordinator.cpp
 * @brief Sensor coordinator implementation - unified API for all sensors
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module coordinates all sensor subsystems.
 * All actual implementations remain in sensors.cpp (100% preserved).
 *
 * ARCHITECTURE:
 * - Direct wrapper to original functions (no function pointer overhead)
 * - Guard clauses for safety
 * - 100% preservation of original logic
 *
 * PRESERVES 100% LOGIC from original sensors.cpp
 */

#include "sensor_coordinator.h"
#include "../sensors.h"
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
// Public API Implementation
// ============================================================================

void sensorCoordinatorInitialize(void)
{
  // Guard clause: Prevent re-initialization
  if (isInitialized) {
    return;
  }

  // Call original initialization (from sensors.cpp)
  initialiseADC();

  // Mark as initialized
  isInitialized = true;
}

// ============================================================================
// ANALOG SENSORS API
// ============================================================================

void sensorCoordinatorReadTPS(bool useFilter)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readTPS(useFilter);
}

void sensorCoordinatorReadMAP(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readMAP();
}

int16_t sensorCoordinatorGetMAPDelta(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getMAPDelta();
}

uint32_t sensorCoordinatorGetMAPDeltaTime(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getMAPDeltaTime();
}

void sensorCoordinatorReadCLT(bool useFilter)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readCLT(useFilter);
}

void sensorCoordinatorReadIAT(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readIAT();
}

void sensorCoordinatorReadO2(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readO2();
}

void sensorCoordinatorReadO2_2(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readO2_2();
}

void sensorCoordinatorReadBat(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readBat();
}

void sensorCoordinatorReadBaro(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  readBaro();
}

void sensorCoordinatorInitialiseMAPBaro(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  initialiseMAPBaro();
}

void sensorCoordinatorResetMAPcycleAndEvent(void)
{
  // Guard clause
  if (!isInitialized) {
    return;
  }

  resetMAPcycleAndEvent();
}

// ============================================================================
// DIGITAL SENSORS API
// ============================================================================

uint16_t sensorCoordinatorGetSpeed(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getSpeed();
}

byte sensorCoordinatorGetGear(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getGear();
}

uint32_t sensorCoordinatorVssGetPulseGap(byte historyIndex)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return vssGetPulseGap(historyIndex);
}

void sensorCoordinatorFlexPulse(void)
{
  // No guard clause in ISR (must be fast)
  flexPulse();
}

void sensorCoordinatorKnockPulse(void)
{
  // No guard clause in ISR (must be fast)
  knockPulse();
}

void sensorCoordinatorVssPulse(void)
{
  // No guard clause in ISR (must be fast)
  vssPulse();
}

// ============================================================================
// DERIVED/AUXILIARY SENSORS API
// ============================================================================

byte sensorCoordinatorGetFuelPressure(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getFuelPressure();
}

byte sensorCoordinatorGetOilPressure(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getOilPressure();
}

uint8_t sensorCoordinatorGetAnalogKnock(void)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return getAnalogKnock();
}

uint16_t sensorCoordinatorReadAuxanalog(uint8_t analogPin)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return readAuxanalog(analogPin);
}

uint16_t sensorCoordinatorReadAuxdigital(uint8_t digitalPin)
{
  // Guard clause
  if (!isInitialized) {
    return 0;
  }

  return readAuxdigital(digitalPin);
}

// ============================================================================
// UTILITY API
// ============================================================================

bool sensorCoordinatorIsInitialized(void)
{
  return isInitialized;
}

const char* sensorCoordinatorGetName(void)
{
  return "Sensor Coordinator";
}
