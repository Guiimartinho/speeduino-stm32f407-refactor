/**
 * @file limp_mode.h
 * @brief Explicit limp-home mode for degraded engine operation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 *
 * Limp mode provides safe, degraded operation when sensor failures occur.
 * Instead of simply using default values (which can be dangerous), limp mode:
 *
 * 1. **Reduces RPM limit** to prevent engine damage
 * 2. **Retards ignition** for detonation safety margin
 * 3. **Enriches fuel** to prevent lean conditions
 * 4. **Disables boost control** to reduce stress
 * 5. **Sets explicit status flag** for driver notification
 *
 * **Triggers:**
 * - CLT sensor fault (BIT_STATUS5_SENSOR_CLT)
 * - IAT sensor fault (BIT_STATUS5_SENSOR_IAT)
 * - MAP sensor fault (value outside valid range)
 * - TPS sensor fault (value outside valid range)
 * - Critical RAM corruption detected
 *
 * **Recovery:**
 * - Sensor faults: Automatic when sensor returns to valid range
 * - Other faults: Require power cycle or reset
 *
 * @note Limp mode is more conservative than OEM defaults
 * @note All limits are configurable via TunerStudio (future)
 *
 * @author SCG-ECU Team
 * @date 2025-01-02
 * @version 1.0
 */

#ifndef LIMP_MODE_H
#define LIMP_MODE_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// LIMP MODE CONFIGURATION (Compile-time defaults)
// =============================================================================

/**
 * @brief Maximum RPM in limp mode (absolute limit)
 * @note Set conservatively to prevent high-RPM damage with bad sensors
 */
#define LIMP_MODE_MAX_RPM           4000U

/**
 * @brief Ignition timing retard in limp mode (degrees)
 * @note Negative = retard, reduces power but increases safety margin
 */
#define LIMP_MODE_TIMING_RETARD     10

/**
 * @brief Fuel enrichment in limp mode (percent)
 * @note 110 = 10% rich, prevents lean conditions with sensor issues
 */
#define LIMP_MODE_FUEL_ENRICH       115U

/**
 * @brief Cooldown period after limp trigger clears (seconds * 10)
 * @note Prevents oscillating in/out of limp mode
 */
#define LIMP_MODE_COOLDOWN_SEC10    100U  // 10 seconds

// =============================================================================
// LIMP MODE TRIGGER FLAGS
// =============================================================================

/**
 * @brief Limp mode trigger reasons (bitmask)
 */
#define LIMP_TRIGGER_NONE           0x00U
#define LIMP_TRIGGER_CLT_FAULT      0x01U  ///< CLT sensor failure
#define LIMP_TRIGGER_IAT_FAULT      0x02U  ///< IAT sensor failure
#define LIMP_TRIGGER_MAP_FAULT      0x04U  ///< MAP sensor out of range
#define LIMP_TRIGGER_TPS_FAULT      0x08U  ///< TPS sensor failure
#define LIMP_TRIGGER_RAM_CORRUPT    0x10U  ///< RAM corruption detected
#define LIMP_TRIGGER_EEPROM_CORRUPT 0x20U  ///< EEPROM corruption detected
#define LIMP_TRIGGER_OVERTEMP       0x40U  ///< Engine overtemperature
#define LIMP_TRIGGER_LOW_OIL        0x80U  ///< Low oil pressure (if sensor present)

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize limp mode subsystem
 *
 * Called once at startup. Resets all limp mode state.
 */
void limpModeInit(void);

/**
 * @brief Update limp mode status based on current sensor readings
 *
 * Should be called periodically (e.g., every main loop iteration).
 * Checks all sensor status flags and activates/deactivates limp mode.
 *
 * @return Current limp mode trigger flags (0 = not in limp mode)
 */
uint8_t limpModeUpdate(void);

/**
 * @brief Check if limp mode is currently active
 *
 * @return true if any limp mode trigger is active
 */
bool isLimpModeActive(void);

/**
 * @brief Get current limp mode trigger flags
 *
 * @return Bitmask of active triggers (LIMP_TRIGGER_*)
 */
uint8_t getLimpModeTriggers(void);

/**
 * @brief Get limp mode RPM limit
 *
 * @return Maximum RPM when in limp mode, or 0xFFFF if not in limp mode
 */
uint16_t getLimpModeRPMLimit(void);

/**
 * @brief Get limp mode timing correction
 *
 * @return Degrees of timing retard to apply (negative value)
 */
int8_t getLimpModeTimingCorrection(void);

/**
 * @brief Get limp mode fuel correction
 *
 * @return Fuel multiplier percentage (100 = no change, 115 = 15% rich)
 */
uint8_t getLimpModeFuelCorrection(void);

/**
 * @brief Check if boost should be disabled in limp mode
 *
 * @return true if boost should be disabled
 */
bool shouldDisableBoostInLimpMode(void);

/**
 * @brief Manually trigger limp mode (for testing or external triggers)
 *
 * @param trigger Trigger flag to set (LIMP_TRIGGER_*)
 */
void limpModeSetTrigger(uint8_t trigger);

/**
 * @brief Clear a specific limp mode trigger
 *
 * @param trigger Trigger flag to clear (LIMP_TRIGGER_*)
 */
void limpModeClearTrigger(uint8_t trigger);

/**
 * @brief Force exit limp mode (emergency override)
 *
 * @note Use with caution - may result in engine damage if sensors are truly failed
 */
void limpModeForceExit(void);

/**
 * @brief Get time in limp mode (seconds)
 *
 * @return Seconds since limp mode was activated, 0 if not active
 */
uint32_t getLimpModeDuration(void);

/**
 * @brief Get primary limp mode reason as string
 *
 * @return Human-readable string for primary trigger
 */
const char* getLimpModeReasonString(void);

#endif // LIMP_MODE_H
