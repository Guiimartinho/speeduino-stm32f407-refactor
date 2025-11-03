/**
 * @file basic_distributor.h
 * @brief Basic Distributor decoder interface
 *
 * @author Speeduino Team
 * @date 2025-01-31
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef BASIC_DISTRIBUTOR_H
#define BASIC_DISTRIBUTOR_H

#include <stdint.h>

void triggerSetup_BasicDistributor(void);
void triggerPri_BasicDistributor(void);
void triggerSec_BasicDistributor(void);
uint16_t getRPM_BasicDistributor(void);
int getCrankAngle_BasicDistributor(void);
void triggerSetEndTeeth_BasicDistributor(void);

#endif // BASIC_DISTRIBUTOR_H
