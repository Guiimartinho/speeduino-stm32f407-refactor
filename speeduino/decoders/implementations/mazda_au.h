/**
 * @file mazda_au.h
 * @brief Mazda AU decoder interface
 * @details REFACTORED from decoders.cpp (lines 2902-3043)
 *
 * Pattern: 4 crank teeth with uneven spacing + cam tooth
 * - Alternating 72° and 108° tooth spacing
 * - Tooth #2 after single cam tooth
 * - Tooth #1 at 348° ATDC
 * - Sequential mode
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef MAZDA_AU_H
#define MAZDA_AU_H

#include <stdint.h>

// Public interface functions
void triggerSetup_MazdaAU(void);
void triggerPri_MazdaAU(void);
void triggerSec_MazdaAU(void);
uint16_t getRPM_MazdaAU(void);
int getCrankAngle_MazdaAU(void);
void triggerSetEndTeeth_MazdaAU(void);

#endif // MAZDA_AU_H
