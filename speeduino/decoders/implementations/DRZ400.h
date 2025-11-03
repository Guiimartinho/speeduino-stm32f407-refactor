#ifndef DRZ400_H
#define DRZ400_H
#include <stdint.h>
void triggerSetup_DRZ400(void);
void triggerPri_DRZ400(void);
void triggerSec_DRZ400(void);
uint16_t getRPM_DRZ400(void);
int getCrankAngle_DRZ400(void);
void triggerSetEndTeeth_DRZ400(void);
#endif
