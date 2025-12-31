#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_RoverMEMS() {
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL));
    triggerSecFilterTime = 0;
    secondaryToothCount = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 1;
    triggerToothAngle = 2;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle);
}

void triggerPri_RoverMEMS() {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap < triggerFilterTime) { return; }

    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    if (toothCurrentCount >= 361) {
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

void triggerSec_RoverMEMS() {}
uint16_t getRPM_RoverMEMS() { return stdGetRPM(CRANK_SPEED); }

int getCrankAngle_RoverMEMS() {
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

void triggerSetEndTeeth_RoverMEMS() {}
