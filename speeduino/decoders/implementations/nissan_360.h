#ifndef NISSAN_360_H
#define NISSAN_360_H
#include <stdint.h>
void triggerSetup_Nissan360(void);
void triggerPri_Nissan360(void);
void triggerSec_Nissan360(void);
uint16_t getRPM_Nissan360(void);
int getCrankAngle_Nissan360(void);
void triggerSetEndTeeth_Nissan360(void);
#endif
