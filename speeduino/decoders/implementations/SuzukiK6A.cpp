#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_SuzukiK6A(void) {
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL));
    triggerSecFilterTime = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 1;
    toothSystemCount = 1;
    triggerToothAngle = 3;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle);
}

void triggerPri_SuzukiK6A(void) {
    curTime = micros();
    curGap = curTime - toothSystemLastToothTime;
    if (curGap < triggerFilterTime && currentStatus.startRevolutions != 0) { return; }

    toothSystemCount++;
    if (currentStatus.hasSync == false) { toothLastToothTime = curTime; return; }

    if (toothSystemCount >= 3) {
        BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);
        toothSystemLastToothTime = curTime;
        toothSystemCount = 0;
        toothCurrentCount++;

        if (toothCurrentCount == 1 || toothCurrentCount > 120) {
            toothCurrentCount = 1;
            toothOneMinusOneTime = toothOneTime;
            toothOneTime = curTime;
            currentStatus.hasSync = true;
            currentStatus.startRevolutions++;
        }

        setFilter(curGap);
        toothLastMinusOneToothTime = toothLastToothTime;
        toothLastToothTime = curTime;
    }
}

void triggerSec_SuzukiK6A(void) {}
uint16_t getRPM_SuzukiK6A(void) { return stdGetRPM(CRANK_SPEED); }

int getCrankAngle_SuzukiK6A(void) {
    int crankAngle = 0;
    if (currentStatus.hasSync) {
        unsigned long tempToothLastToothTime;
        int tempToothCurrentCount;
        noInterrupts();
        tempToothCurrentCount = toothCurrentCount;
        tempToothLastToothTime = toothLastToothTime;
        lastCrankAngleCalc = micros();
        interrupts();

        if (tempToothCurrentCount == 0) { tempToothCurrentCount = 120; }
        crankAngle = ((tempToothCurrentCount - 1) * 3) + configPage4.triggerAngle;
        elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
        crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

        if (crankAngle >= 720) { crankAngle -= 720; }
        if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }
    }
    return crankAngle;
}

void triggerSetEndTeeth_SuzukiK6A(void) {}
