#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_DRZ400(void) {
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 720UL));
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U;
    secondaryToothCount = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 0;
    triggerToothAngle = 1;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle);
}

void triggerPri_DRZ400(void) {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap >= triggerFilterTime) {
        toothCurrentCount++;
        BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);
        if (toothCurrentCount >= 721) {
            toothCurrentCount = 1;
            toothOneMinusOneTime = toothOneTime;
            toothOneTime = curTime;
            currentStatus.startRevolutions++;
        }
        toothLastMinusOneToothTime = toothLastToothTime;
        toothLastToothTime = curTime;
    }
}

void triggerSec_DRZ400(void) {
    curTime2 = micros();
    toothLastSecToothTime = curTime2;
    toothCurrentCount = 0;
    currentStatus.hasSync = true;
}

uint16_t getRPM_DRZ400(void) { return stdGetRPM(CRANK_SPEED); }

int getCrankAngle_DRZ400(void) {
    int crankAngle = 0;
    if (currentStatus.hasSync) {
        unsigned long tempToothLastToothTime;
        int tempToothCurrentCount;
        noInterrupts();
        tempToothCurrentCount = toothCurrentCount;
        tempToothLastToothTime = toothLastToothTime;
        lastCrankAngleCalc = micros();
        interrupts();
        
        crankAngle = (tempToothCurrentCount - 1) + configPage4.triggerAngle;
        elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
        crankAngle += timeToAngleDegPerMicroSec(elapsedTime);
        
        if (crankAngle >= 720) { crankAngle -= 720; }
        if (crankAngle < 0) { crankAngle += 360; }
    }
    return crankAngle;
}

void triggerSetEndTeeth_DRZ400(void) {}
