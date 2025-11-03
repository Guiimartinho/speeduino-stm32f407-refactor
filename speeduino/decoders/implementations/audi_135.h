/**
 * @file audi_135.h
 * @brief Audi 135 decoder interface
 * @details REFACTORED from decoders.cpp (lines 2179-2291)
 *
 * Pattern: 135 teeth on crank + 1 tooth on cam
 * - Only every 3rd crank tooth used (effective 45 teeth)
 * - 8 degrees per effective tooth
 * - Cam signal provides sequential sync
 * - Sequential mode
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef AUDI_135_H
#define AUDI_135_H

#include <stdint.h>

// Public interface functions
void triggerSetup_Audi135(void);
void triggerPri_Audi135(void);
void triggerSec_Audi135(void);
uint16_t getRPM_Audi135(void);
int getCrankAngle_Audi135(void);
void triggerSetEndTeeth_Audi135(void);

#endif // AUDI_135_H
