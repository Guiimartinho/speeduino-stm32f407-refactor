#ifndef RENIX_H
#define RENIX_H
#include <stdint.h>
void triggerSetup_Renix(void);
void triggerPri_Renix(void);
void triggerSec_Renix(void);
uint16_t getRPM_Renix(void);
int getCrankAngle_Renix(void);
void triggerSetEndTeeth_Renix(void);
#endif
