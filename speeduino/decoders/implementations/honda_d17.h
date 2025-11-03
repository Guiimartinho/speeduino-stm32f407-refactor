/**
 * @file honda_d17.h
 * @brief Honda D17 decoder interface
 * @details REFACTORED from decoders.cpp (lines 2297-2387)
 *
 * Pattern: 12 + 1 teeth (13th tooth at half spacing)
 * - 1.7 liter 4 cylinder SOHC
 * - 30 degrees per tooth (12 teeth)
 * - 13th tooth detected by gap (half spacing)
 * - Non-sequential mode
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef HONDA_D17_H
#define HONDA_D17_H

#include <stdint.h>

// Public interface functions
void triggerSetup_HondaD17(void);
void triggerPri_HondaD17(void);
void triggerSec_HondaD17(void);
uint16_t getRPM_HondaD17(void);
int getCrankAngle_HondaD17(void);
void triggerSetEndTeeth_HondaD17(void);

#endif // HONDA_D17_H
