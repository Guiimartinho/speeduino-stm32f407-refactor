#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

void triggerSetup_non360(void)
{
    triggerToothAngle = (360U * configPage4.TrigAngMul) / configPage4.triggerTeeth;
    toothCurrentCount = UINT8_MAX;
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth));
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle);
}

void triggerPri_non360(void) {}
void triggerSec_non360(void) {}

uint16_t getRPM_non360(void)
{
    uint16_t tempRPM = 0;
    if ((currentStatus.hasSync == true) && (toothCurrentCount != 0)) {
        if (currentStatus.RPM < currentStatus.crankRPM) {
            tempRPM = crankingGetRPM(configPage4.triggerTeeth, CRANK_SPEED);
        } else {
            tempRPM = stdGetRPM(CRANK_SPEED);
        }
    }
    return tempRPM;
}

int getCrankAngle_non360(void)
{
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();
    
    if (tempToothCurrentCount == 0) { tempToothCurrentCount = configPage4.triggerTeeth; }
    
    int crankAngle = (tempToothCurrentCount - 1) * triggerToothAngle;
    crankAngle = (crankAngle / configPage4.TrigAngMul) + configPage4.triggerAngle;
    
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);
    
    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }
    
    return crankAngle;
}

void triggerSetEndTeeth_non360(void) {}
