/**
 * @file weber.h
 * @brief Weber-Marelli decoder interface
 *
 * Weber-Marelli trigger with 2 wheels:
 * - 4 teeth 90deg apart on crank
 * - 2 teeth 90deg apart on cam
 *
 * Uses DualWheel decoder base with custom secondary trigger logic
 * for state machine-based sync validation.
 */

#ifndef WEBER_H
#define WEBER_H

#include <stdint.h>

void triggerSetup_Weber(void);
void triggerPri_Weber(void);
void triggerSec_Weber(void);
uint16_t getRPM_Weber(void);
int getCrankAngle_Weber(void);
void triggerSetEndTeeth_Weber(void);

static inline void triggerThird_Weber(void) {}

#endif // WEBER_H
