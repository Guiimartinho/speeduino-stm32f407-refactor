/**
 * @file boost_control.h
 * @brief Turbocharger boost control system interface
 * @details Manages wastegate solenoid for boost pressure control
 *
 * Control Modes:
 * - Open Loop: Table-based duty cycle
 * - Closed Loop: PID control to target pressure
 *
 * Features:
 * - Gear-based boost compensation
 * - Flex fuel boost adjustment
 * - Overboost protection
 * - Duty cycle limiting
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef BOOST_CONTROL_H
#define BOOST_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace boost {

/**
 * @brief Boost control modes
 */
enum class BoostMode : uint8_t {
    OPEN_LOOP = 0U,    ///< Table-based duty cycle
    CLOSED_LOOP = 1U   ///< PID control to target
};

/**
 * @brief Initialize boost control system
 * @details Configures PID, GPIO, sets initial state
 * @pre Pin mapping completed
 * @post Boost disabled, wastegate open
 * @return true if initialization successful
 * @safety Wastegate initially open (no boost)
 */
bool initialise(void);

/**
 * @brief Update boost control logic
 * @details Handles open/closed loop modes
 * @note Call at 30Hz from main loop (every 33ms)
 * @pre Boost must be initialized
 * @post Boost duty and target updated
 * @safety Includes overboost protection
 */
void update(void);

/**
 * @brief Disable boost control (failsafe)
 * @details Forces wastegate fully open
 * @post Boost duty = 0%, wastegate open, PID reset
 * @safety Safe to call from any context
 */
void disable(void);

/**
 * @brief Get current boost duty cycle
 * @return Duty cycle in 0.01% units (0-10000 = 0.00%-100.00%)
 */
uint16_t getDuty(void);

/**
 * @brief Get current boost target
 * @return Target boost pressure in kPa * 2
 */
uint16_t getTarget(void);

/**
 * @brief Check if boost control is enabled
 * @return true if enabled in config
 */
bool isEnabled(void);

/**
 * @brief Apply gear-based boost compensation
 * @details Modifies duty or target based on current gear
 * @note Called internally by update()
 */
void applyGearCompensation(void);

} // namespace boost
} // namespace speeduino

#endif // BOOST_CONTROL_H
