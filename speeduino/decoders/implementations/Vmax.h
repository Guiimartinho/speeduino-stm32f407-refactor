#ifndef VMAX_H
#define VMAX_H
#include <stdint.h>
void triggerSetup_Vmax(void);
void triggerPri_Vmax(void);
void triggerSec_Vmax(void);
uint16_t getRPM_Vmax(void);
int getCrankAngle_Vmax(void);
void triggerSetEndTeeth_Vmax(void);
#endif
