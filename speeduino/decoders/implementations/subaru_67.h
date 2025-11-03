#ifndef SUBARU_67_H
#define SUBARU_67_H
#include <stdint.h>
void triggerSetup_Subaru67(void);
void triggerPri_Subaru67(void);
void triggerSec_Subaru67(void);
uint16_t getRPM_Subaru67(void);
int getCrankAngle_Subaru67(void);
void triggerSetEndTeeth_Subaru67(void);
#endif
