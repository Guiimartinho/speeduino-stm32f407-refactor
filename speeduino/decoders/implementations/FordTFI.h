#ifndef FORD_TFI_H
#define FORD_TFI_H
#include <stdint.h>
void triggerSetup_FordTFI(void);
void triggerPri_FordTFI(void);
void triggerSec_FordTFI(void);
uint16_t getRPM_FordTFI(void);
int getCrankAngle_FordTFI(void);
void triggerSetEndTeeth_FordTFI(void);
#endif
