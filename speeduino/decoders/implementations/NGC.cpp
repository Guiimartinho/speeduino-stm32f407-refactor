#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_NGC(void) {
    triggerToothAngle = 2;
    toothCurrentCount = 0;
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 180UL));
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 60U);
}

void triggerPri_NGC(void) {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap >= triggerFilterTime) {
        toothCurrentCount++;
        BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);
        
        if (toothCurrentCount >= 181) {
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

void triggerSec_NGC(void) {
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;
    if (curGap2 >= triggerSecFilterTime) {
        toothLastSecToothTime = curTime2;
        toothCurrentCount = 1;
        currentStatus.hasSync = true;
    }
}

uint16_t getRPM_NGC(void) {
    if (currentStatus.hasSync && toothCurrentCount != 0) {
        if (currentStatus.RPM < currentStatus.crankRPM) {
            return crankingGetRPM(180, CRANK_SPEED);
        }
        return stdGetRPM(CRANK_SPEED);
    }
    return 0;
}

int getCrankAngle_NGC(void) {
    int crankAngle = 0;
    if (currentStatus.hasSync) {
        unsigned long tempToothLastToothTime;
        int tempToothCurrentCount;
        noInterrupts();
        tempToothCurrentCount = toothCurrentCount;
        tempToothLastToothTime = toothLastToothTime;
        lastCrankAngleCalc = micros();
        interrupts();
        
        crankAngle = ((tempToothCurrentCount - 1) * 2) + configPage4.triggerAngle;
        elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
        crankAngle += timeToAngleDegPerMicroSec(elapsedTime);
        
        if (crankAngle >= 720) { crankAngle -= 720; }
        if (crankAngle < 0) { crankAngle += 360; }
    }
    return crankAngle;
}

void triggerSetEndTeeth_NGC(void) {}
