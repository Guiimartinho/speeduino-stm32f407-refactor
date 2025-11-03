#ifndef DAIHATSU_H
#define DAIHATSU_H
#include <stdint.h>
void triggerSetup_Daihatsu(void);
void triggerPri_Daihatsu(void);
void triggerSec_Daihatsu(void);
uint16_t getRPM_Daihatsu(void);
int getCrankAngle_Daihatsu(void);
void triggerSetEndTeeth_Daihatsu(void);
#endif
