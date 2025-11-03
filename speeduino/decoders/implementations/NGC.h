#ifndef NGC_H
#define NGC_H
#include <stdint.h>
void triggerSetup_NGC(void);
void triggerPri_NGC(void);
void triggerSec_NGC(void);
uint16_t getRPM_NGC(void);
int getCrankAngle_NGC(void);
void triggerSetEndTeeth_NGC(void);
#endif
