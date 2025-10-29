/**
 * @file aircon_control.h
 * @brief Air conditioning control system interface
 * @details Manages A/C compressor and fan with multiple safety lockouts
 *
 * Features:
 * - Coolant temperature lockout
 * - High TPS lockout (WOT cut)
 * - RPM range lockout
 * - Post-cranking delay
 * - Compressor on delay
 * - Stand-alone fan control
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 *
 * @see auxiliaries.h for legacy interface
 */

#ifndef AIRCON_CONTROL_H
#define AIRCON_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

// Bit positions in currentStatus.airConStatus
#define BIT_AIRCON_REQUEST     0U  ///< A/C button pressed
#define BIT_AIRCON_COMPRESSOR  1U  ///< Compressor active
#define BIT_AIRCON_RPM_LOCKOUT 2U  ///< RPM out of range
#define BIT_AIRCON_TPS_LOCKOUT 3U  ///< TPS above threshold
#define BIT_AIRCON_TURNING_ON  4U  ///< In turn-on delay
#define BIT_AIRCON_CLT_LOCKOUT 5U  ///< Coolant too hot
#define BIT_AIRCON_FAN         6U  ///< Standalone fan active

namespace speeduino {
namespace aircon {

/**
 * @brief Initialize air conditioning control system
 * @details Sets up GPIO pins, clears status bits, configures delays
 * @pre Pin mapping must be completed
 * @post Air conditioning system ready for operation
 * @return true if A/C enabled in config, false otherwise
 * @safety Initializes to safe state (compressor OFF)
 */
bool initialise(void);

/**
 * @brief Update air conditioning control logic
 * @details Checks lockouts and controls compressor/fan
 * @note Call at 10Hz from main loop (every 100ms)
 * @pre A/C must be initialized
 * @post currentStatus.airConStatus updated
 * @safety Multiple lockout checks prevent unsafe operation
 */
void update(void);

/**
 * @brief Check if A/C request button is pressed
 * @details Reads digital input pin accounting for polarity
 * @return true if A/C requested by driver, false otherwise
 * @note Updates BIT_AIRCON_REQUEST in currentStatus.airConStatus
 */
bool isRequested(void);

/**
 * @brief Force A/C off immediately (emergency shutdown)
 * @details Disables compressor and fan, sets safe state
 * @post Compressor and fan OFF, status bits cleared
 * @safety Safe to call from any context
 */
void forceOff(void);

/**
 * @brief Check if A/C system is enabled in configuration
 * @return true if A/C enabled, false otherwise
 */
bool isEnabled(void);

/**
 * @brief Check if compressor is currently active
 * @return true if compressor ON, false otherwise
 */
bool isCompressorOn(void);

/**
 * @brief Check if any lockout condition is active
 * @return true if locked out, false if OK to run
 */
bool isLockedOut(void);

} // namespace aircon
} // namespace speeduino

#endif // AIRCON_CONTROL_H
