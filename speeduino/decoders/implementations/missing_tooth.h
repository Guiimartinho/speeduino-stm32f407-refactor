/**
 * @file missing_tooth.h
 * @brief Missing Tooth decoder interface
 * @details Public API for missing tooth wheel decoders (36-1, 60-2, etc.)
 *
 * @author Speeduino Team
 * @date 2025-01-31
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef MISSING_TOOTH_H
#define MISSING_TOOTH_H

#include <stdint.h>

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Setup missing tooth decoder
 * @details Initializes decoder parameters, filters, and state
 * @pre Configuration loaded (configPage4.triggerTeeth, etc.)
 * @post Decoder ready for trigger interrupts
 */
void triggerSetup_missingTooth(void);

/**
 * @brief Primary trigger ISR (crank/cam teeth)
 * @details Processes each physical tooth on the wheel
 * @warning CRITICAL ISR - Must complete in < 10µs
 * @isr Called from hardware interrupt
 */
void triggerPri_missingTooth(void);

/**
 * @brief Secondary trigger ISR (cam signal for sequential)
 * @details Processes cam teeth for revolution detection
 * @isr Called from hardware interrupt
 */
void triggerSec_missingTooth(void);

/**
 * @brief Third trigger ISR (flex fuel sensor, etc.)
 * @details Increments third trigger counter
 * @isr Called from hardware interrupt
 */
void triggerThird_missingTooth(void);

/**
 * @brief Get current RPM
 * @details Calculates RPM from tooth one timing
 * @return RPM value (0 if no valid data)
 */
uint16_t getRPM_missingTooth(void);

/**
 * @brief Get crank angle for specific tooth
 * @param toothCounter Tooth number (1-based)
 * @return Crank angle in degrees
 */
int getCrankAngle_missingTooth(int toothCounter);

/**
 * @brief Configure end teeth for scheduling
 * @details Sets ignition/fuel end tooth positions
 */
void triggerSetEndTeeth_missingTooth(void);

#endif // MISSING_TOOTH_H
