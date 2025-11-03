/**
 * @file gm_7x.h
 * @brief GM 7X decoder interface
 *
 * @author Speeduino Team
 * @date 2025-01-31
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef GM_7X_H
#define GM_7X_H

#include <stdint.h>

void triggerSetup_GM7X(void);
void triggerPri_GM7X(void);
void triggerSec_GM7X(void);
uint16_t getRPM_GM7X(void);
int getCrankAngle_GM7X(void);
void triggerSetEndTeeth_GM7X(void);

#endif // GM_7X_H
