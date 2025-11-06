/**
 * @file arduino_mock.cpp
 * @brief Implementation of Arduino mock variables and functions for native tests
 *
 * Provides the actual storage for mock state variables declared in Arduino.h
 * @date 2025-11-06
 * @version 1.0
 */

#include "Arduino.h"
#include <stdlib.h>

// ============================================================================
// MOCK STATE VARIABLES - IMPLEMENTATION
// ============================================================================

/**
 * @brief Mock microseconds counter
 * Tests can directly modify this value to simulate time passage
 */
uint32_t mock_micros_value = 0;

/**
 * @brief Mock milliseconds counter
 * Tests can directly modify this value to simulate time passage
 */
uint32_t mock_millis_value = 0;

/**
 * @brief Mock digital pin states (256 pins)
 * Index by pin number, value is HIGH or LOW
 */
uint8_t mock_digital_pins[256] = {0};

/**
 * @brief Mock analog pin values (64 pins)
 * Index by pin number, value is 0-1023 (10-bit ADC)
 */
uint16_t mock_analog_pins[64] = {0};

// ============================================================================
// SERIAL MOCK OBJECTS
// ============================================================================

SerialMock Serial;
SerialMock Serial1;
SerialMock Serial2;
SerialMock Serial3;

// ============================================================================
// RANDOM FUNCTIONS (C++ overloads)
// ============================================================================

#ifdef __cplusplus

/**
 * @brief Generate random number in range [0, howbig)
 * @param howbig Upper bound (exclusive)
 * @return Random number [0, howbig-1]
 */
long random(long howbig) {
    if (howbig == 0) return 0;
    return rand() % howbig;
}

/**
 * @brief Generate random number in range [howsmall, howbig)
 * @param howsmall Lower bound (inclusive)
 * @param howbig Upper bound (exclusive)
 * @return Random number [howsmall, howbig-1]
 */
long random(long howsmall, long howbig) {
    if (howsmall >= howbig) return howsmall;
    long diff = howbig - howsmall;
    return random(diff) + howsmall;
}

#endif // __cplusplus
