/**
 * @file four_g63.h
 * @brief 4G63 / 6G72 CAS decoder interface
 *
 * @author Speeduino Team
 * @date 2025-01-31
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef FOUR_G63_H
#define FOUR_G63_H

#include <stdint.h>

void triggerSetup_4G63(void);
void triggerPri_4G63(void);
void triggerSec_4G63(void);
uint16_t getRPM_4G63(void);
int getCrankAngle_4G63(void);
void triggerSetEndTeeth_4G63(void);

#endif // FOUR_G63_H
