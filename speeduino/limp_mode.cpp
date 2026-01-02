/**
 * @file limp_mode.cpp
 * @brief Explicit limp-home mode implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 */

#include "limp_mode.h"
#include "globals.h"
#include "storage_safety.h"

// =============================================================================
// PRIVATE STATE
// =============================================================================

static uint8_t activeTriggers = LIMP_TRIGGER_NONE;  ///< Current active triggers
static uint32_t limpModeStartTime = 0;              ///< Time when limp mode started (seclx10)
static bool limpModeActive = false;                 ///< Master limp mode flag
static uint32_t triggerClearTime = 0;               ///< Time when last trigger was cleared

// Reason strings for diagnostics
static const char* const triggerStrings[] = {
    "None",
    "CLT Sensor",
    "IAT Sensor",
    "MAP Sensor",
    "TPS Sensor",
    "RAM Corrupt",
    "EEPROM Corrupt",
    "Overtemp",
    "Low Oil"
};

// =============================================================================
// SENSOR FAULT DETECTION
// =============================================================================

/**
 * @brief Check if CLT sensor is faulted
 */
static inline bool isCLTFaulted(void)
{
    return BIT_CHECK(currentStatus.status5, BIT_STATUS5_SENSOR_CLT);
}

/**
 * @brief Check if IAT sensor is faulted
 */
static inline bool isIATFaulted(void)
{
    return BIT_CHECK(currentStatus.status5, BIT_STATUS5_SENSOR_IAT);
}

/**
 * @brief Check if MAP sensor is out of valid range
 */
static inline bool isMAPFaulted(void)
{
    // MAP should be 10-400 kPa for realistic operation
    // Below 10 kPa = sensor shorted, above 400 kPa = sensor open or boost runaway
    return (currentStatus.MAP < 10) || (currentStatus.MAP > 400);
}

/**
 * @brief Check if TPS sensor is faulted (stuck at min or max)
 */
static inline bool isTPSFaulted(void)
{
    // TPS ADC stuck at extremes for too long indicates fault
    // This is a simple check - could be enhanced with history tracking
    return (currentStatus.tpsADC < 2) || (currentStatus.tpsADC > 253);
}

/**
 * @brief Check if engine is overtemperature
 */
static inline bool isOvertemp(void)
{
    // Default: 110°C is overtemp (configPage4.coolantProtRPM could be used)
    return (currentStatus.coolant > 110);
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

void limpModeInit(void)
{
    activeTriggers = LIMP_TRIGGER_NONE;
    limpModeStartTime = 0;
    limpModeActive = false;
    triggerClearTime = 0;
}

uint8_t limpModeUpdate(void)
{
    uint8_t newTriggers = LIMP_TRIGGER_NONE;

    // Check sensor faults
    if (isCLTFaulted()) { newTriggers |= LIMP_TRIGGER_CLT_FAULT; }
    if (isIATFaulted()) { newTriggers |= LIMP_TRIGGER_IAT_FAULT; }

    // Only check MAP/TPS when engine is running to avoid false positives at startup
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN))
    {
        if (isMAPFaulted()) { newTriggers |= LIMP_TRIGGER_MAP_FAULT; }
        if (isTPSFaulted()) { newTriggers |= LIMP_TRIGGER_TPS_FAULT; }
        if (isOvertemp()) { newTriggers |= LIMP_TRIGGER_OVERTEMP; }
    }

    // Check storage corruption
    if (isStorageCorrupted()) { newTriggers |= LIMP_TRIGGER_RAM_CORRUPT; }

    // Check if any triggers changed from active to cleared
    uint8_t clearedTriggers = activeTriggers & ~newTriggers;
    if (clearedTriggers != 0)
    {
        triggerClearTime = seclx10;
    }

    // Update active triggers (new triggers or existing that haven't cooled down)
    if (newTriggers != LIMP_TRIGGER_NONE)
    {
        activeTriggers = newTriggers;
        if (!limpModeActive)
        {
            limpModeActive = true;
            limpModeStartTime = seclx10;
        }
    }
    else if (activeTriggers != LIMP_TRIGGER_NONE)
    {
        // Check cooldown period
        if ((seclx10 - triggerClearTime) >= LIMP_MODE_COOLDOWN_SEC10)
        {
            activeTriggers = LIMP_TRIGGER_NONE;
            limpModeActive = false;
            limpModeStartTime = 0;
        }
    }

    return activeTriggers;
}

bool isLimpModeActive(void)
{
    return limpModeActive;
}

uint8_t getLimpModeTriggers(void)
{
    return activeTriggers;
}

uint16_t getLimpModeRPMLimit(void)
{
    if (!limpModeActive) { return 0xFFFF; }
    return LIMP_MODE_MAX_RPM;
}

int8_t getLimpModeTimingCorrection(void)
{
    if (!limpModeActive) { return 0; }
    return -LIMP_MODE_TIMING_RETARD;  // Negative = retard
}

uint8_t getLimpModeFuelCorrection(void)
{
    if (!limpModeActive) { return 100; }
    return LIMP_MODE_FUEL_ENRICH;
}

bool shouldDisableBoostInLimpMode(void)
{
    // Disable boost for any sensor fault that could affect boost safety
    if (!limpModeActive) { return false; }

    // Disable boost if MAP, TPS, IAT, or CLT is faulted
    uint8_t boostDisableTriggers = LIMP_TRIGGER_MAP_FAULT |
                                    LIMP_TRIGGER_TPS_FAULT |
                                    LIMP_TRIGGER_IAT_FAULT |
                                    LIMP_TRIGGER_CLT_FAULT |
                                    LIMP_TRIGGER_OVERTEMP;

    return (activeTriggers & boostDisableTriggers) != 0;
}

void limpModeSetTrigger(uint8_t trigger)
{
    activeTriggers |= trigger;
    if (!limpModeActive)
    {
        limpModeActive = true;
        limpModeStartTime = seclx10;
    }
}

void limpModeClearTrigger(uint8_t trigger)
{
    activeTriggers &= ~trigger;
    triggerClearTime = seclx10;
}

void limpModeForceExit(void)
{
    activeTriggers = LIMP_TRIGGER_NONE;
    limpModeActive = false;
    limpModeStartTime = 0;
}

uint32_t getLimpModeDuration(void)
{
    if (!limpModeActive) { return 0; }
    return (seclx10 - limpModeStartTime) / 10;  // Convert to seconds
}

const char* getLimpModeReasonString(void)
{
    if (activeTriggers == LIMP_TRIGGER_NONE) { return triggerStrings[0]; }

    // Return first active trigger's string
    for (uint8_t i = 0; i < 8; i++)
    {
        if (activeTriggers & (1U << i)) { return triggerStrings[i + 1]; }
    }

    return triggerStrings[0];
}
