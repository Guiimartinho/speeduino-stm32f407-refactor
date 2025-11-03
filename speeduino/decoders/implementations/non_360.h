#ifndef NON_360_H
#define NON_360_H
#include <stdint.h>
void triggerSetup_non360(void);
void triggerPri_non360(void);
void triggerSec_non360(void);
uint16_t getRPM_non360(void);
int getCrankAngle_non360(void);
void triggerSetEndTeeth_non360(void);
#endif
