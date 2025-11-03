#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

namespace {

struct Nissan360WindowConfig {
    uint8_t nCylinders;
    uint8_t durationMin;
    uint8_t durationMax;
    uint16_t targetToothCount;
};

static const Nissan360WindowConfig nissan360WindowConfigs[] PROGMEM = {
    {4, 15, 17, 16},
    {4, 11, 13, 102},
    {4,  7,  9, 188},
    {4,  3,  5, 274},
    {6,  3,  5, 124},
    {8,  6,  8, 56}
};

static inline bool processNissan360Window(uint8_t secondaryDuration, uint8_t nCylinders, uint16_t* outToothCount)
{
    const uint8_t configCount = sizeof(nissan360WindowConfigs) / sizeof(Nissan360WindowConfig);
    for (uint8_t i = 0; i < configCount; i++) {
        const Nissan360WindowConfig* cfg = &nissan360WindowConfigs[i];
        if (cfg->nCylinders == nCylinders && secondaryDuration >= cfg->durationMin && secondaryDuration <= cfg->durationMax) {
            *outToothCount = cfg->targetToothCount;
            return true;
        }
    }
    return false;
}

} // namespace

void triggerSetup_Nissan360(void)
{
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL));
    triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U;
    secondaryToothCount = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 1;
    triggerToothAngle = 2;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle);
}

void triggerPri_Nissan360(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap < triggerFilterTime) { return; }
    
    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);
    
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
    
    if (currentStatus.hasSync == true) {
        if (toothCurrentCount == 361) {
            toothCurrentCount = 1;
            toothOneMinusOneTime = toothOneTime;
            toothOneTime = curTime;
            currentStatus.startRevolutions++;
        }
        setFilter(curGap);
        
        if (configPage2.perToothIgn == true) {
            int16_t crankAngle = ((toothCurrentCount-1) * 2) + configPage4.triggerAngle;
            if (crankAngle > CRANK_ANGLE_MAX_IGN) {
                crankAngle -= CRANK_ANGLE_MAX_IGN;
                checkPerToothTiming(crankAngle, (toothCurrentCount/2));
            } else {
                checkPerToothTiming(crankAngle, toothCurrentCount);
            }
        }
    }
}

void triggerSec_Nissan360(void)
{
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;
    toothLastSecToothTime = curTime2;
    
    byte trigEdge = (configPage4.TrigEdgeSec == 0) ? LOW : HIGH;
    
    if ((secondaryToothCount == 0) || (READ_SEC_TRIGGER() == trigEdge)) {
        secondaryToothCount = toothCurrentCount;
        return;
    }
    
    byte secondaryDuration = toothCurrentCount - secondaryToothCount;
    uint16_t matchedToothCount = 0;
    
    if (currentStatus.hasSync == false) {
        if (processNissan360Window(secondaryDuration, configPage2.nCylinders, &matchedToothCount)) {
            toothCurrentCount = matchedToothCount;
            currentStatus.hasSync = true;
        } else {
            currentStatus.hasSync = false;
            currentStatus.syncLossCounter++;
        }
    } else if (configPage4.useResync == true) {
        if (processNissan360Window(secondaryDuration, configPage2.nCylinders, &matchedToothCount)) {
            toothCurrentCount = matchedToothCount;
        }
    }
}

uint16_t getRPM_Nissan360(void)
{
    uint16_t tempRPM;
    if ((currentStatus.hasSync == true) && (toothLastToothTime != 0) && (toothLastMinusOneToothTime != 0)) {
        if (currentStatus.startRevolutions < 2) {
            noInterrupts();
            SetRevolutionTime((toothLastToothTime - toothLastMinusOneToothTime) * 180);
            interrupts();
        } else {
            noInterrupts();
            SetRevolutionTime((toothOneTime - toothOneMinusOneTime) >> 1);
            interrupts();
        }
        tempRPM = RpmFromRevolutionTimeUs(revolutionTime);
        MAX_STALL_TIME = revolutionTime << 1;
    } else {
        tempRPM = 0;
    }
    return tempRPM;
}

int getCrankAngle_Nissan360(void)
{
    int crankAngle = 0;
    int tempToothLastToothTime;
    int tempToothLastMinusOneToothTime;
    int tempToothCurrentCount;
    
    noInterrupts();
    tempToothLastToothTime = toothLastToothTime;
    tempToothLastMinusOneToothTime = toothLastMinusOneToothTime;
    tempToothCurrentCount = toothCurrentCount;
    lastCrankAngleCalc = micros();
    interrupts();
    
    crankAngle = ((tempToothCurrentCount - 1) * 2) + configPage4.triggerAngle;
    unsigned long halfTooth = (tempToothLastToothTime - tempToothLastMinusOneToothTime) / 2;
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    if (elapsedTime > halfTooth) {
        crankAngle += 1;
    }
    
    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }
    
    return crankAngle;
}

void triggerSetEndTeeth_Nissan360(void)
{
    byte offset_teeth = 4;
    if ((ignition1EndAngle - offset_teeth) > configPage4.triggerAngle) {
        ignition1EndTooth = ((ignition1EndAngle - configPage4.triggerAngle) / 2) - offset_teeth;
    } else {
        ignition1EndTooth = ((ignition1EndAngle + 720 - configPage4.triggerAngle) / 2) - offset_teeth;
    }
    if ((ignition2EndAngle - offset_teeth) > configPage4.triggerAngle) {
        ignition2EndTooth = ((ignition2EndAngle - configPage4.triggerAngle) / 2) - offset_teeth;
    } else {
        ignition2EndTooth = ((ignition2EndAngle + 720 - configPage4.triggerAngle) / 2) - offset_teeth;
    }
    if ((ignition3EndAngle - offset_teeth) > configPage4.triggerAngle) {
        ignition3EndTooth = ((ignition3EndAngle - configPage4.triggerAngle) / 2) - offset_teeth;
    } else {
        ignition3EndTooth = ((ignition3EndAngle + 720 - configPage4.triggerAngle) / 2) - offset_teeth;
    }
    if ((ignition4EndAngle - offset_teeth) > configPage4.triggerAngle) {
        ignition4EndTooth = ((ignition4EndAngle - configPage4.triggerAngle) / 2) - offset_teeth;
    } else {
        ignition4EndTooth = ((ignition4EndAngle + 720 - configPage4.triggerAngle) / 2) - offset_teeth;
    }
}
