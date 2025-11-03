#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_Daihatsu(void) {
    triggerActualTeeth = configPage2.nCylinders + 1;
    triggerToothAngle = 720 / triggerActualTeeth;
    toothCurrentCount = triggerActualTeeth;
    triggerFilterTime = 60000000L / MAX_RPM / configPage2.nCylinders;
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * 2U);
}

void triggerPri_Daihatsu(void) {}
void triggerSec_Daihatsu(void) { triggerHandler_DualWheel(); }
uint16_t getRPM_Daihatsu(void) { return stdGetRPM(CAM_SPEED); }
int getCrankAngle_Daihatsu(void) { return getCrankAngle_DualWheel(); }
void triggerSetEndTeeth_Daihatsu(void) {}
