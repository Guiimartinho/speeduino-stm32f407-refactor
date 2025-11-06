/**
 * @file led_mocks.cpp
 * @brief Mock implementations for LED Button System functions
 *
 * These mocks allow testing LED system logic without hardware dependencies
 */

#include "test_mocks.h"

// ============================================================================
// LED BUTTON SYSTEM TYPE DEFINITIONS
// ============================================================================

typedef enum {
    LED_MODE_NORMAL = 0,
    LED_MODE_SHIFT_LIGHT = 1,
    LED_MODE_ERROR_CODES = 2,
    LED_MODE_TUNING = 3,
    LED_MODE_DIAGNOSTIC = 4,
    LED_MODE_COUNT = 5
} LedMode_t;

// ============================================================================
// MOCK STATE VARIABLES
// ============================================================================

static LedMode_t currentMode = LED_MODE_NORMAL;
static byte errorCode = 0;
static bool configSaved = false;

// ============================================================================
// MOCK FUNCTION IMPLEMENTATIONS
// ============================================================================

void ledButtonInit(void) {
    // Mock initialization
    currentMode = LED_MODE_NORMAL;
    errorCode = 0;
}

void ledButtonUpdate(void) {
    // Mock update - does nothing in tests
}

void ledSetMode(LedMode_t mode) {
    if (mode < LED_MODE_COUNT) {
        currentMode = mode;
    }
}

LedMode_t ledGetMode(void) {
    return currentMode;
}

void ledSaveConfig(void) {
    configSaved = true;
}

void ledLoadConfig(void) {
    // Mock load - does nothing in tests
}

void ledFactoryReset(void) {
    currentMode = LED_MODE_NORMAL;
    errorCode = 0;
    configSaved = false;
}

void ledAddError(byte error) {
    errorCode = error;
}

void ledClearAllErrors(void) {
    errorCode = 0;
}

byte ledGetErrorCode(void) {
    return errorCode;
}

bool ledIsConfigSaved(void) {
    return configSaved;
}
