/**
 * @file miata_9905.h
 * @brief Miata '99-'05 decoder interface
 * @details REFACTORED from decoders.cpp (lines 2596-2893)
 *
 * Pattern: 4x 70 degree duration teeth at cam speed
 * - Alternating 70° and 110° spacing
 * - Tooth #1 at 355° ATDC
 * - Sequential mode with VVT support
 * - Cam signal provides sync (single/double pulse pattern)
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef MIATA_9905_H
#define MIATA_9905_H

#include <stdint.h>

// Public interface functions
void triggerSetup_Miata9905(void);
void triggerPri_Miata9905(void);
void triggerSec_Miata9905(void);
uint16_t getRPM_Miata9905(void);
int getCrankAngle_Miata9905(void);
int getCamAngle_Miata9905(void);
void triggerSetEndTeeth_Miata9905(void);

#endif // MIATA_9905_H
