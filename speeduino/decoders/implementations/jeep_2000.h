/**
 * @file jeep_2000.h
 * @brief Jeep 2000 decoder interface
 * @details REFACTORED from decoders.cpp (lines 2075-2171)
 *
 * Pattern: 24 crank teeth over 720 degrees in groups of 4
 * - '91 to 2000 6-cylinder Jeep engines
 * - 12 tooth angles defined (only need 360 degrees)
 * - Cam signal provides sync
 * - Non-sequential mode
 *
 * @author Speeduino Team
 * @date 2025-11-03
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#ifndef JEEP_2000_H
#define JEEP_2000_H

#include <stdint.h>

// Public interface functions
void triggerSetup_Jeep2000(void);
void triggerPri_Jeep2000(void);
void triggerSec_Jeep2000(void);
uint16_t getRPM_Jeep2000(void);
int getCrankAngle_Jeep2000(void);
void triggerSetEndTeeth_Jeep2000(void);

#endif // JEEP_2000_H
