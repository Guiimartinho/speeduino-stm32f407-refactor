/**
 * @file gm_24x.h
 * @brief GM 24X decoder interface
 * @details REFACTORED from decoders.cpp (lines 1954-2065)
 *
 * Pattern: 24 teeth on crank wheel with cam sync
 * - Variable tooth spacing (12-15 degrees)
 * - Cam signal provides sync
 * - Sequential operation
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef GM_24X_H
#define GM_24X_H

#include <stdint.h>

// Public interface functions
void triggerSetup_24X(void);
void triggerPri_24X(void);
void triggerSec_24X(void);
uint16_t getRPM_24X(void);
int getCrankAngle_24X(void);
void triggerSetEndTeeth_24X(void);

#endif // GM_24X_H
