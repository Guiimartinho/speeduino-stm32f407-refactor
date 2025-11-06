/**
 * @file led_button_mocks.cpp
 * @brief All mocks needed for LED Button System tests (combined for PlatformIO)
 */

#include <stdint.h>
#include <cstdlib>

// Arduino byte type
typedef uint8_t byte;

// ============================================================================
// ARDUINO MOCK STATE
// ============================================================================

uint32_t mock_micros_value = 0;
uint32_t mock_millis_value = 0;
uint8_t mock_digital_pins[256] = {0};

// ============================================================================
// LED MODE DEFINITIONS (from test file)
// ============================================================================

// Note: LedMode_t is also defined in test file - must match exactly
enum LedMode_t {
    LED_MODE_NORMAL = 0,
    LED_MODE_SHIFT_LIGHT = 1,
    LED_MODE_ERROR_CODES = 2,
    LED_MODE_TUNING = 3,
    LED_MODE_DIAGNOSTIC = 4,
    LED_MODE_COUNT = 5
};

// ============================================================================
// LED MOCK STATE
// ============================================================================

static LedMode_t currentMode = LED_MODE_NORMAL;
static byte errorCode = 0;

// ============================================================================
// LED MOCK FUNCTIONS
// ============================================================================

void ledButtonInit(void) {
    currentMode = LED_MODE_NORMAL;
    errorCode = 0;
}

void ledButtonUpdate(void) {
    // Mock - does nothing
}

void ledSetMode(LedMode_t mode) {
    if (mode < LED_MODE_COUNT) {
        currentMode = mode;
    }
}

void ledSaveConfig(void) {
    // Mock - does nothing
}

void ledLoadConfig(void) {
    // Mock - does nothing
}

void ledFactoryReset(void) {
    currentMode = LED_MODE_NORMAL;
    errorCode = 0;
}

void ledAddError(byte error) {
    errorCode = error;
}

void ledClearAllErrors(void) {
    errorCode = 0;
}

// ============================================================================
// ARDUINO MOCK FUNCTIONS
// ============================================================================

void pinMode(uint8_t pin, uint8_t mode) {
    // Mock - does nothing
}

void digitalWrite(uint8_t pin, uint8_t value) {
    if (pin < 256) {
        mock_digital_pins[pin] = value;
    }
}

uint8_t digitalRead(uint8_t pin) {
    if (pin < 256) {
        return mock_digital_pins[pin];
    }
    return 0;
}

void delay(uint32_t ms) {
    mock_millis_value += ms;
    mock_micros_value += (ms * 1000UL);
}

void delayMicroseconds(uint32_t us) {
    mock_micros_value += us;
}

uint32_t millis(void) {
    return mock_millis_value;
}

uint32_t micros(void) {
    return mock_micros_value;
}

// Random functions (C++ linkage for overloading)
long random(long howbig) {
    if (howbig == 0) {
        return 0;
    }
    return rand() % howbig;
}

long random(long howsmall, long howbig) {
    if (howsmall >= howbig) {
        return howsmall;
    }
    long diff = howbig - howsmall;
    return random(diff) + howsmall;
}
