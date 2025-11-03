#ifndef HARLEY_H
#define HARLEY_H
#include <stdint.h>
void triggerSetup_Harley(void);
void triggerPri_Harley(void);
void triggerSec_Harley(void);
uint16_t getRPM_Harley(void);
int getCrankAngle_Harley(void);
void triggerSetEndTeeth_Harley(void);
#endif
