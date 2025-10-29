/**
 * @file vvt_control.h
 * @brief Variable Valve Timing (VVT) control system interface
 * @details Manages VVT solenoids for cam phasing control
 *
 * Control Modes:
 * - Open Loop: Table-based duty cycle
 * - Closed Loop: PID control to target angle
 * - On/Off: Binary switching
 *
 * Features:
 * - Dual VVT support (VVT1 + VVT2)
 * - Coolant temperature delay
 * - Angle error detection
 * - Hold duty at target
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef VVT_CONTROL_H
#define VVT_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace vvt {

/**
 * @brief VVT control modes
 */
enum class VVTMode : uint8_t {
    OPEN_LOOP = 0U,    ///< Table-based duty cycle
    ONOFF = 1U,        ///< Binary on/off control
    CLOSED_LOOP = 2U   ///< PID control to target angle
};

/**
 * @brief VVT load source
 */
enum class VVTLoadSource : uint8_t {
    MAP = 0U,  ///< Use MAP for table lookup
    TPS = 1U   ///< Use TPS for table lookup
};

/**
 * @brief Initialize VVT control system
 * @details Configures PID, GPIO, timers
 * @pre Pin mapping completed, coolant sensor working
 * @post VVT disabled until warmup complete
 * @return true if initialization successful
 * @safety VVT initially disabled (0% duty)
 */
bool initialise(void);

/**
 * @brief Update VVT control logic
 * @details Handles open/closed loop modes for VVT1 and VVT2
 * @note Call at 30Hz from main loop (every 33ms)
 * @pre VVT must be initialized
 * @post VVT duty and angles updated
 * @safety Includes angle error detection
 */
void update(void);

/**
 * @brief Disable VVT control (failsafe)
 * @details Forces VVT solenoids off
 * @post VVT duty = 0%, solenoids OFF, PID reset
 * @safety Safe to call from any context
 */
void disable(void);

/**
 * @brief Get VVT1 duty cycle
 * @return Duty cycle (0-200, where 200 = 100%)
 */
uint8_t getVVT1Duty(void);

/**
 * @brief Get VVT2 duty cycle
 * @return Duty cycle (0-200, where 200 = 100%)
 */
uint8_t getVVT2Duty(void);

/**
 * @brief Check if VVT is enabled and hot
 * @return true if VVT active
 */
bool isActive(void);

/**
 * @brief Check if VVT1 has angle error
 * @return true if angle out of valid range
 */
bool hasVVT1Error(void);

/**
 * @brief Check if VVT2 has angle error
 * @return true if angle out of valid range
 */
bool hasVVT2Error(void);

} // namespace vvt
} // namespace speeduino

#endif // VVT_CONTROL_H
