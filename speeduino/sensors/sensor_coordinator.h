/**
 * @file sensor_coordinator.h
 * @brief Sensor coordinator - main API for sensor management
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module orchestrates all sensor readings and processing.
 * Replaces direct function calls with modular architecture.
 *
 * Original API preserved:
 * - Maintains compatibility with existing code
 * - Same function names and signatures
 * - 100% behavioral equivalence
 *
 * @note This is the ONLY sensor module that should be included by speeduino.ino
 */

#ifndef SENSOR_COORDINATOR_H
#define SENSOR_COORDINATOR_H

#include "../globals.h"

/**
 * @brief Initialize sensor system
 * @details Sets up all sensor subsystems (analog, digital, filtering)
 * @post Sensors ready for operation
 * @note Called once during ECU initialization
 * @note Thread-safe: Called only from init sequence
 */
void sensorCoordinatorInitialize(void);

// ============================================================================
// ANALOG SENSORS API
// ============================================================================

/**
 * @brief Read Throttle Position Sensor
 * @param useFilter Enable/disable filtering (default: true)
 * @note Called frequently (30Hz typical)
 * @note Uses EMA filtering with configurable alpha
 */
void sensorCoordinatorReadTPS(bool useFilter = true);

/**
 * @brief Read Manifold Absolute Pressure
 * @note Called frequently (main loop, ~100Hz)
 * @note Critical for load calculation
 */
void sensorCoordinatorReadMAP(void);

/**
 * @brief Get MAP change between last 2 readings
 * @return Delta MAP in kPa (signed)
 * @note Used for acceleration enrichment
 */
int16_t sensorCoordinatorGetMAPDelta(void);

/**
 * @brief Get time between last 2 MAP readings
 * @return Time in microseconds
 * @note Used for MAP rate of change calculation
 */
uint32_t sensorCoordinatorGetMAPDeltaTime(void);

/**
 * @brief Read Coolant Temperature
 * @param useFilter Enable/disable filtering (default: true)
 * @note Uses thermistor calibration table
 * @note Heavily filtered (slow-changing sensor)
 */
void sensorCoordinatorReadCLT(bool useFilter = true);

/**
 * @brief Read Intake Air Temperature
 * @note Uses thermistor calibration table
 * @note Medium filtering (moderate-changing sensor)
 */
void sensorCoordinatorReadIAT(void);

/**
 * @brief Read Primary O2 Sensor
 * @note Narrowband: 0-1V (stoich = 0.45V)
 * @note Wideband: 0-5V (linear AFR)
 * @note Medium filtering
 */
void sensorCoordinatorReadO2(void);

/**
 * @brief Read Secondary O2 Sensor
 * @note Used for dual O2 setups
 */
void sensorCoordinatorReadO2_2(void);

/**
 * @brief Read Battery Voltage
 * @note Used for injector dead-time correction
 * @note Used for dwell correction
 * @note Heavy filtering (slow-changing)
 */
void sensorCoordinatorReadBat(void);

/**
 * @brief Read Barometric Pressure
 * @note Typically read once at startup or periodically
 * @note Used for altitude compensation
 */
void sensorCoordinatorReadBaro(void);

/**
 * @brief Initialize MAP and Baro calibration
 * @note Called at startup to establish baseline
 */
void sensorCoordinatorInitialiseMAPBaro(void);

/**
 * @brief Reset MAP cycle tracking
 * @note Called on sync loss or engine stop
 */
void sensorCoordinatorResetMAPcycleAndEvent(void);

// ============================================================================
// DIGITAL SENSORS API
// ============================================================================

/**
 * @brief Get current vehicle speed
 * @return Speed in configured units (km/h or mph)
 * @note Calculated from VSS pulses
 */
uint16_t sensorCoordinatorGetSpeed(void);

/**
 * @brief Get current gear
 * @return Gear number (0 = neutral, 1-6 = gears)
 * @note Calculated from speed and RPM ratio
 */
byte sensorCoordinatorGetGear(void);

/**
 * @brief Get VSS pulse gap for history
 * @param historyIndex Index in history (0 = most recent)
 * @return Time between pulses in microseconds
 */
uint32_t sensorCoordinatorVssGetPulseGap(byte historyIndex);

/**
 * @brief Flex fuel pulse ISR handler
 * @note Called from interrupt
 * @note Measures flex fuel sensor pulse width
 */
void sensorCoordinatorFlexPulse(void);

/**
 * @brief Knock sensor pulse ISR handler
 * @note Called from interrupt
 * @note Measures knock sensor pulse
 */
void sensorCoordinatorKnockPulse(void);

/**
 * @brief VSS pulse ISR handler
 * @note Called from interrupt
 * @note Records vehicle speed pulses
 */
void sensorCoordinatorVssPulse(void);

// ============================================================================
// DERIVED/AUXILIARY SENSORS API
// ============================================================================

/**
 * @brief Get fuel pressure
 * @return Fuel pressure in configured units
 * @note Analog sensor, typically 0-5V = 0-100 PSI
 */
byte sensorCoordinatorGetFuelPressure(void);

/**
 * @brief Get oil pressure
 * @return Oil pressure in configured units
 * @note Analog sensor, typically 0-5V = 0-100 PSI
 */
byte sensorCoordinatorGetOilPressure(void);

/**
 * @brief Get analog knock sensor reading
 * @return Knock intensity (0-255)
 * @note Filtered analog value, not pulse counter
 */
uint8_t sensorCoordinatorGetAnalogKnock(void);

/**
 * @brief Read auxiliary analog input
 * @param analogPin Pin number to read
 * @return Raw ADC value (0-4095)
 */
uint16_t sensorCoordinatorReadAuxanalog(uint8_t analogPin);

/**
 * @brief Read auxiliary digital input
 * @param digitalPin Pin number to read
 * @return Digital value (0 or 1)
 */
uint16_t sensorCoordinatorReadAuxdigital(uint8_t digitalPin);

// ============================================================================
// UTILITY API
// ============================================================================

/**
 * @brief Check if sensor coordinator is initialized
 * @return true if initialized, false otherwise
 */
bool sensorCoordinatorIsInitialized(void);

/**
 * @brief Get sensor coordinator name (for debugging)
 * @return "Sensor Coordinator"
 */
const char* sensorCoordinatorGetName(void);

#endif // SENSOR_COORDINATOR_H
