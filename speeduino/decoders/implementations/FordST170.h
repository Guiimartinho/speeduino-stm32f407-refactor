#ifndef FORD_ST170_H
#define FORD_ST170_H
#include <stdint.h>
void triggerSetup_FordST170(void);
void triggerPri_FordST170(void);
void triggerSec_FordST170(void);
uint16_t getRPM_FordST170(void);
int getCrankAngle_FordST170(void);
void triggerSetEndTeeth_FordST170(void);
#endif
