/**
 * @file sensor_interface.h
 * @brief Sensor interface - common structure for all sensors
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This defines the common interface that all sensors must implement.
 * Enables O(1) sensor selection via function pointers.
 *
 * Sensor Types:
 * - Analog: TPS, MAP, CLT, IAT, O2, Battery, Baro
 * - Digital: VSS, Flex Fuel, Knock
 * - Derived: Speed, Gear, Fuel/Oil Pressure
 *
 * @note All sensor implementations remain in sensors.cpp (100% preserved)
 */

#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include "../globals.h"

// ============================================================================
// SENSOR INTERFACE
// ============================================================================

/**
 * @brief Common interface for all sensors
 * @note Function pointers allow O(1) dispatch
 * @note All functions are optional (can be NULL if not applicable)
 */
typedef struct {
    /**
     * @brief Initialize sensor hardware (ADC, pins, calibration)
     * @note Called once during ECU initialization
     * @note NOT timing-critical
     */
    void (*init)(void);

    /**
     * @brief Read raw sensor value from hardware
     * @return Raw ADC value (0-4095 for 12-bit ADC)
     * @note May be called frequently (100Hz+)
     * @note Should be fast (<100µs)
     */
    uint16_t (*readRaw)(void);

    /**
     * @brief Get calibrated sensor value
     * @return Calibrated value in appropriate units
     *         - Temperature: °C or °F
     *         - Pressure: kPa
     *         - Voltage: mV
     *         - Percentage: 0-100
     * @note Uses calibration table lookup
     * @note Returns cached value if sensor read recently
     */
    int16_t (*getValue)(void);

    /**
     * @brief Check if sensor is valid (not failed)
     * @return true if sensor reading is valid
     * @return false if sensor failed (open/short circuit)
     * @note Detects:
     *       - Open circuit (reading too high)
     *       - Short circuit (reading too low)
     *       - Out of range values
     */
    bool (*isValid)(void);

    /**
     * @brief Apply filtering to sensor reading
     * @param rawValue Raw ADC reading
     * @return Filtered value
     * @note Typically uses EMA (Exponential Moving Average)
     * @note Filter strength configurable per sensor
     */
    uint16_t (*applyFilter)(uint16_t rawValue);

    /**
     * @brief Sensor name for debugging/logging
     * @note Must be const string literal
     */
    const char* name;

    /**
     * @brief Unique sensor ID
     * @note Used for lookup in sensor registry
     */
    uint8_t sensorID;

} SensorInterface;

// ============================================================================
// SENSOR IDS
// ============================================================================
// Used for O(1) lookup in sensor registry

#define SENSOR_TPS        0   // Throttle Position Sensor
#define SENSOR_MAP        1   // Manifold Absolute Pressure
#define SENSOR_CLT        2   // Coolant Temperature
#define SENSOR_IAT        3   // Intake Air Temperature
#define SENSOR_O2         4   // Oxygen/Lambda Sensor (primary)
#define SENSOR_O2_2       5   // Oxygen/Lambda Sensor (secondary)
#define SENSOR_BATTERY    6   // Battery Voltage
#define SENSOR_BARO       7   // Barometric Pressure
#define SENSOR_VSS        8   // Vehicle Speed Sensor
#define SENSOR_FLEX       9   // Flex Fuel Sensor
#define SENSOR_KNOCK      10  // Knock Sensor
#define SENSOR_FUEL_PRESSURE  11  // Fuel Pressure
#define SENSOR_OIL_PRESSURE   12  // Oil Pressure

#define SENSOR_COUNT      13  // Total number of sensors

// ============================================================================
// SENSOR STATUS FLAGS
// ============================================================================

/**
 * @brief Sensor status bits
 * @note Used in statusSensors global variable
 */
#define BIT_SENSOR_VALID      0  // Sensor reading is valid
#define BIT_SENSOR_FAILED     1  // Sensor has failed
#define BIT_SENSOR_FILTERED   2  // Filtering is enabled
#define BIT_SENSOR_CALIBRATED 3  // Calibration table loaded
#define BIT_SENSOR_UNUSED4    4
#define BIT_SENSOR_UNUSED5    5
#define BIT_SENSOR_UNUSED6    6
#define BIT_SENSOR_UNUSED7    7

#endif // SENSOR_INTERFACE_H
