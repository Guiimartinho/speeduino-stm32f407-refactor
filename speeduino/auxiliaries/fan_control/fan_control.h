/**
 * @file fan_control.h
 * @brief Cooling fan control system interface
 * @details Supports digital on/off and PWM-based fan control
 *
 * Control Modes:
 * - Mode 1: Digital on/off (hysteresis-based)
 * - Mode 2: PWM duty cycle (table-based)
 *
 * Features:
 * - Temperature-based activation
 * - A/C integration (fan boost)
 * - Cranking disable option
 * - Engine off operation option
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace fan {

/**
 * @brief Fan control modes
 */
enum class FanMode : uint8_t {
    DISABLED = 0U,  ///< Fan control disabled
    DIGITAL = 1U,   ///< On/off control with hysteresis
    PWM = 2U        ///< PWM duty cycle control
};

/**
 * @brief Initialize fan control system
 * @details Configures GPIO, sets initial state, disables timer
 * @pre Pin mapping completed
 * @post Fan OFF, timer disabled
 * @return true if initialization successful
 * @safety Fan initially OFF
 */
bool initialise(void);

/**
 * @brief Update fan control logic
 * @details Handles digital and PWM modes
 * @note Call at 10Hz from main loop (every 100ms)
 * @pre Fan must be initialized
 * @post Fan state and duty updated
 */
void update(void);

/**
 * @brief Force fan off (emergency shutdown)
 * @details Disables fan immediately regardless of temperature
 * @post Fan OFF, timer disabled
 * @safety Safe to call from any context
 */
void forceOff(void);

/**
 * @brief Get current fan duty cycle
 * @return Duty cycle (0-200, where 200 = 100%)
 */
uint8_t getDuty(void);

/**
 * @brief Check if fan is currently active
 * @return true if fan ON, false if OFF
 */
bool isActive(void);

/**
 * @brief Get current fan mode
 * @return Current operating mode
 */
FanMode getMode(void);

} // namespace fan
} // namespace speeduino

#endif // FAN_CONTROL_H
