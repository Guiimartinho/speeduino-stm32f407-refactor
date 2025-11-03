/**
 * @file honda_j32.h
 * @brief Honda J32 decoder interface
 * @details REFACTORED from decoders.cpp (lines 2407-2593)
 *
 * Pattern: 24 teeth (22 physical) with 2 missing teeth
 * - 3.2 liter 6 cylinder SOHC
 * - 15 degrees nominal spacing
 * - Teeth 14 and 22 have 18 degree spacing
 * - Missing tooth after tooth 14 and 22
 * - 7 teeth, gap, 15 teeth, gap pattern
 * - Non-sequential mode
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef HONDA_J32_H
#define HONDA_J32_H

#include <stdint.h>

// Public interface functions
void triggerSetup_HondaJ32(void);
void triggerPri_HondaJ32(void);
void triggerSec_HondaJ32(void);
uint16_t getRPM_HondaJ32(void);
int getCrankAngle_HondaJ32(void);
void triggerSetEndTeeth_HondaJ32(void);

#endif // HONDA_J32_H
