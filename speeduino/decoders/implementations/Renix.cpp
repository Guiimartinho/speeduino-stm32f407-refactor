#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_Renix(void) {
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 12UL));
    triggerSecFilterTime = 0;
    secondaryToothCount = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 1;
    toothSystemCount = 1;
    triggerToothAngle = 60;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 93U);
}

void triggerPri_Renix(void) {
    curTime = micros();
    curGap = curTime - toothSystemLastToothTime;
    if (curGap >= triggerFilterTime && currentStatus.startRevolutions != 0) { return; }
    
    toothSystemCount++;
    if (currentStatus.hasSync == false) { toothLastToothTime = curTime; return; }
    
    if (toothSystemCount >= 3) {
        BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);
        toothSystemLastToothTime = curTime;
        toothSystemCount = 0;
        toothCurrentCount++;
        
        if (toothCurrentCount == 1 || toothCurrentCount > 12) {
            toothCurrentCount = 1;
            toothOneMinusOneTime = toothOneTime;
            toothOneTime = curTime;
            currentStatus.startRevolutions++;
        }
        
        setFilter(curGap);
        toothLastMinusOneToothTime = toothLastToothTime;
        toothLastToothTime = curTime;
    }
}

void triggerSec_Renix(void) {
    if (currentStatus.hasSync == false) {
        toothCurrentCount = 0;
        currentStatus.hasSync = true;
        toothSystemCount = 3;
    }
}

uint16_t getRPM_Renix(void) { return stdGetRPM(CRANK_SPEED); }

int getCrankAngle_Renix(void) {
    int crankAngle = 0;
    if (currentStatus.hasSync) {
        unsigned long tempToothLastToothTime;
        int tempToothCurrentCount;
        noInterrupts();
        tempToothCurrentCount = toothCurrentCount;
        tempToothLastToothTime = toothLastToothTime;
        lastCrankAngleCalc = micros();
        interrupts();
        
        if (tempToothCurrentCount == 0) { tempToothCurrentCount = 12; }
        crankAngle = ((tempToothCurrentCount - 1) * 60) + configPage4.triggerAngle;
        elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
        crankAngle += timeToAngleDegPerMicroSec(elapsedTime);
        
        if (crankAngle >= 720) { crankAngle -= 720; }
        if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }
    }
    return crankAngle;
}

void triggerSetEndTeeth_Renix(void) {}
