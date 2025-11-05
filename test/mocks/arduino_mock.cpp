/**
 * @file arduino_mock.cpp
 * @brief Arduino API mock implementation for native testing
 *
 * Provides global state storage for mocked Arduino functions.
 * Must be linked with test executables.
 *
 * @date 2025-11-05
 * @version 1.0
 */

#include "Arduino.h"

// ============================================================================
// GLOBAL MOCK STATE DEFINITIONS
// ============================================================================

/**
 * @brief Mock microseconds counter
 *
 * Can be manipulated by tests to simulate time passage.
 * - micros() returns this value
 * - delayMicroseconds() increments this value
 */
uint32_t mock_micros_value = 0;

/**
 * @brief Mock milliseconds counter
 *
 * Can be manipulated by tests to simulate time passage.
 * - millis() returns this value
 * - delay() increments this value
 */
uint32_t mock_millis_value = 0;

/**
 * @brief Mock digital pin states (256 pins max)
 *
 * Stores digital pin values set by digitalWrite().
 * Read by digitalRead().
 * Index = pin number, Value = HIGH or LOW
 */
uint8_t mock_digital_pins[256] = {0};

/**
 * @brief Mock analog pin values (64 pins max)
 *
 * Stores analog pin values:
 * - Set by analogWrite() for PWM output
 * - Set by test code, read by analogRead() for ADC input
 * Index = pin number, Value = 0-1023 (ADC) or 0-255 (PWM)
 */
uint16_t mock_analog_pins[64] = {0};

// ============================================================================
// SERIAL MOCK INSTANCES
// ============================================================================

/**
 * @brief Global Serial object (USB/UART0)
 */
SerialMock Serial;

/**
 * @brief Global Serial1 object (UART1)
 */
SerialMock Serial1;

/**
 * @brief Global Serial2 object (UART2)
 */
SerialMock Serial2;

/**
 * @brief Global Serial3 object (UART3)
 */
SerialMock Serial3;

// ============================================================================
// MOCK RESET FUNCTION (for tests)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset all mock state to default values
 *
 * Call this at the beginning of each test to ensure clean state.
 * Resets:
 * - Time counters to 0
 * - All pin states to LOW/0
 */
void mock_reset_arduino_state(void) {
    mock_micros_value = 0;
    mock_millis_value = 0;

    for (int i = 0; i < 256; i++) {
        mock_digital_pins[i] = 0;
    }

    for (int i = 0; i < 64; i++) {
        mock_analog_pins[i] = 0;
    }
}

/**
 * @brief Advance mock time by specified microseconds
 *
 * Useful for simulating time passage in tests.
 *
 * @param us Microseconds to advance
 */
void mock_advance_time_us(uint32_t us) {
    mock_micros_value += us;
    mock_millis_value += (us / 1000);
}

/**
 * @brief Advance mock time by specified milliseconds
 *
 * Useful for simulating time passage in tests.
 *
 * @param ms Milliseconds to advance
 */
void mock_advance_time_ms(uint32_t ms) {
    mock_millis_value += ms;
    mock_micros_value += (ms * 1000UL);
}

/**
 * @brief Set mock time to specific value
 *
 * @param us Microseconds value
 */
void mock_set_time_us(uint32_t us) {
    mock_micros_value = us;
    mock_millis_value = us / 1000;
}

/**
 * @brief Set analog pin value (for simulating ADC reads)
 *
 * @param pin Pin number (0-63)
 * @param value ADC value (0-1023)
 */
void mock_set_analog_pin(uint8_t pin, uint16_t value) {
    if (pin < 64) {
        mock_analog_pins[pin] = value;
    }
}

/**
 * @brief Set digital pin value (for simulating digital reads)
 *
 * @param pin Pin number (0-255)
 * @param value Pin state (HIGH or LOW)
 */
void mock_set_digital_pin(uint8_t pin, uint8_t value) {
    if (pin < 256) {
        mock_digital_pins[pin] = value;
    }
}

/**
 * @brief Get digital pin value (for verifying digitalWrite calls)
 *
 * @param pin Pin number (0-255)
 * @return Pin state (HIGH or LOW)
 */
uint8_t mock_get_digital_pin(uint8_t pin) {
    if (pin < 256) {
        return mock_digital_pins[pin];
    }
    return 0;
}

/**
 * @brief Get analog pin value (for verifying analogWrite calls)
 *
 * @param pin Pin number (0-63)
 * @return Pin value (0-255 for PWM, 0-1023 for ADC)
 */
uint16_t mock_get_analog_pin(uint8_t pin) {
    if (pin < 64) {
        return mock_analog_pins[pin];
    }
    return 0;
}

/**
 * @brief Generate random number in range (mocked)
 * @param howbig Maximum value (exclusive)
 * @return Random value from 0 to howbig-1
 */
long random(long howbig) {
    if (howbig == 0) {
        return 0;
    }
    return rand() % howbig;
}

/**
 * @brief Generate random number in range (mocked)
 * @param howsmall Minimum value (inclusive)
 * @param howbig Maximum value (exclusive)
 * @return Random value from howsmall to howbig-1
 */
long random(long howsmall, long howbig) {
    if (howsmall >= howbig) {
        return howsmall;
    }
    long diff = howbig - howsmall;
    return random(diff) + howsmall;
}

#ifdef __cplusplus
}
#endif
