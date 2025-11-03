/**
 * @file dual_wheel.h
 * @brief Dual Wheel decoder interface
 *
 * @author Speeduino Team
 * @date 2025-01-31
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef DUAL_WHEEL_H
#define DUAL_WHEEL_H

#include <stdint.h>

void triggerSetup_DualWheel(void);
void triggerPri_DualWheel(void);
void triggerSec_DualWheel(void);
uint16_t getRPM_DualWheel(void);
int getCrankAngle_DualWheel(void);
void triggerSetEndTeeth_DualWheel(void);

#endif // DUAL_WHEEL_H
