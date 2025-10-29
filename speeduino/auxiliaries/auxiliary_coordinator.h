/**
 * @file auxiliary_coordinator.h
 * @brief Main coordinator for all auxiliary systems
 * @details Orchestrates initialization and updates for all auxiliary modules
 *
 * Manages:
 * - Air Conditioning Control
 * - Fan Control
 * - Boost Control
 * - VVT Control
 * - Nitrous Control
 * - WMI Control
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef AUXILIARY_COORDINATOR_H
#define AUXILIARY_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>

namespace speeduino {
namespace auxiliaries {

/**
 * @brief Initialize all auxiliary systems
 * @details Calls init for all enabled auxiliary modules
 * @pre Pin mapping and configuration loaded
 * @post All auxiliaries initialized and ready
 * @return true if all initializations successful
 * @safety All systems start in safe states (OFF/disabled)
 */
bool initialiseAll(void);

/**
 * @brief Update all auxiliary systems
 * @details Calls update for all active auxiliary modules
 * @note Call at 10-30Hz from main loop
 * @pre Auxiliaries must be initialized
 * @post All auxiliary states updated
 */
void updateAll(void);

/**
 * @brief Disable all auxiliary systems (emergency shutdown)
 * @details Forces all auxiliaries OFF immediately
 * @post All auxiliaries disabled
 * @safety Safe to call from any context
 */
void disableAll(void);

/**
 * @brief Initialize auxiliary PWM systems
 * @details Legacy interface for compatibility
 * @deprecated Use initialiseAll() instead
 */
void initialiseAuxPWM(void);

} // namespace auxiliaries
} // namespace speeduino

#endif // AUXILIARY_COORDINATOR_H
