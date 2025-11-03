#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

namespace {

/// @brief Calculate gap with micros rollover handling
/// @param current Current time
/// @param last Last time
/// @return Gap duration
static inline unsigned long calculateGapWithRollover(unsigned long current, unsigned long last) {
    if (current >= last) {
        return current - last;
    } else {
        return (4294967296UL - last + current);
    }
}

/// @brief Handle rotation completion and sync validation
/// @return true if rotation completed
static inline bool handleRotationCompletion() {
    if (toothCurrentCount <= triggerActualTeeth) { return false; }

    if ((currentStatus.hasSync == true) && ((lastSyncRevolution) + 3 < currentStatus.startRevolutions)) {
        currentStatus.hasSync = false;
        currentStatus.syncLossCounter++;
    }

    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
    return true;
}

/// @brief Handle per-tooth ignition timing
static inline void handlePerToothIgnition() {
    if (configPage2.perToothIgn == true) {
        int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
        crankAngle = ignitionLimits((crankAngle));
        uint16_t currentTooth = toothCurrentCount;
        if (toothCurrentCount > (triggerActualTeeth / 2)) {
            currentTooth = (toothCurrentCount - (triggerActualTeeth / 2));
        }
        checkPerToothTiming(crankAngle, currentTooth);
    }
}

/// @brief Detect signature tooth and update sync
/// @return true if signature detected
static inline bool detectSignatureTooth() {
    unsigned long targetGap2 = 0;
    unsigned long targetGap3 = 0;

    if ((curGap > 0) && (curGap < 20000000)) {
        targetGap2 = curGap * 110UL / 100UL;
        targetGap3 = curGap * 90UL / 100UL;
    }

    if ((curGap2 > targetGap2) && (curGap3 < targetGap3) && (lastGap < targetGap2) && (lastGap > targetGap3)) {
        if ((currentStatus.hasSync == false) || (currentStatus.startRevolutions <= configPage4.StgCycles)) {
            toothCurrentCount = 2;
            triggerFilterTime = 0;
            currentStatus.hasSync = true;
        } else {
            if ((toothCurrentCount != 2) && (currentStatus.startRevolutions > 2)) {
                currentStatus.syncLossCounter++;
            }
            if (configPage4.useResync == 1) {
                toothCurrentCount = 2;
                currentStatus.hasSync = true;
            }
        }
        lastSyncRevolution = currentStatus.startRevolutions;
        return true;
    }
    return false;
}

} // anonymous namespace

void triggerSetup_FordTFI(void) {
    triggerActualTeeth = configPage2.nCylinders;
    if (triggerActualTeeth == 0) { triggerActualTeeth = 1; }

    triggerToothAngle = 720U / triggerActualTeeth;
    toothCurrentCount = 0;
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 30U * configPage2.nCylinders));
    triggerSecFilterTime = triggerFilterTime * 4U / 5U;
    lastSyncRevolution = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);

    if (configPage2.nCylinders <= 4U) {
        MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/90U) * triggerToothAngle);
    } else {
        MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle);
    }

#ifdef USE_LIBDIVIDE
    divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

void triggerPri_FordTFI(void) {
    curTime = micros();
    curGap = calculateGapWithRollover(curTime, toothLastToothTime);

    if (curGap < triggerFilterTime) { return; }

    if (currentStatus.hasSync == true) {
        setFilter(curGap);
    } else {
        triggerFilterTime = 0;
    }

    toothCurrentCount++;

    handleRotationCompletion();

    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    handlePerToothIgnition();

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

void triggerSec_FordTFI(void) {
    curTime2 = micros();
    curGap2 = calculateGapWithRollover(curTime2, toothLastSecToothTime);

    if ((toothLastSecToothTime == 0)) {
        curGap2 = 0;
        toothLastSecToothTime = curTime2;
    }

    if (curGap2 < triggerSecFilterTime) { return; }

    detectSignatureTooth();

    toothLastSecToothTime = curTime2;
    lastGap = curGap3;
    curGap3 = curGap2;

    triggerSecFilterTime = curGap >> 1;
}

uint16_t getRPM_FordTFI(void) {
    uint16_t tempRPM;
    uint8_t distributorSpeed = CAM_SPEED;

    if (currentStatus.RPM < currentStatus.crankRPM || currentStatus.RPM < 1500) {
        tempRPM = crankingGetRPM(triggerActualTeeth, distributorSpeed);
    } else {
        tempRPM = stdGetRPM(distributorSpeed);
    }

    MAX_STALL_TIME = revolutionTime << 1;
    if (MAX_STALL_TIME < 366667UL) { MAX_STALL_TIME = 366667UL; }

    return tempRPM;
}

int getCrankAngle_FordTFI(void) {
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    if (tempToothCurrentCount == 0) { tempToothCurrentCount = 2; }

    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;

    elapsedTime = calculateGapWithRollover(lastCrankAngleCalc, tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += CRANK_ANGLE_MAX; }

    return crankAngle;
}

void triggerSetEndTeeth_FordTFI(void) {
    int tempEndAngle = (ignition1EndAngle - configPage4.triggerAngle);
    tempEndAngle = ignitionLimits((tempEndAngle));

    setEndTeethFromDistributorConfig(tempEndAngle, configPage2.nCylinders);
}
