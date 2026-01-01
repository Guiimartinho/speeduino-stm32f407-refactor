#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

// ============================================================================
// FILE SCOPE HELPERS (moved from namespace to reduce nesting depth)
// ============================================================================

/// @brief Calculate gap with micros rollover handling
static inline unsigned long calculateGapWithRollover(unsigned long current, unsigned long last) {
    if (current >= last) { return current - last; }
    return (4294967296UL - last + current);
}

/// @brief Handle per-tooth ignition timing (flattened)
static inline void handlePerToothIgnition(void) {
    if (!configPage2.perToothIgn) { return; }
    int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
    crankAngle = ignitionLimits(crankAngle);
    uint16_t currentTooth = toothCurrentCount;
    bool inSecondHalf = (toothCurrentCount > (triggerActualTeeth / 2));
    if (inSecondHalf) { currentTooth = (toothCurrentCount - (triggerActualTeeth / 2)); }
    checkPerToothTiming(crankAngle, currentTooth);
}

/// @brief Handle initial sync acquisition
static inline void handleSyncAcquisition(void) {
    toothCurrentCount = 2;
    triggerFilterTime = 0;
    currentStatus.hasSync = true;
}

/// @brief Handle resync verification
static inline void handleResyncVerify(void) {
    bool toothMismatch = (toothCurrentCount != 2) && (currentStatus.startRevolutions > 2);
    if (toothMismatch) { currentStatus.syncLossCounter++; }
    if (configPage4.useResync == 1) {
        toothCurrentCount = 2;
        currentStatus.hasSync = true;
    }
}

/// @brief Detect signature tooth and update sync
static inline bool detectSignatureTooth(void) {
    unsigned long targetGap2 = 0;
    unsigned long targetGap3 = 0;
    bool validGap = (curGap > 0) && (curGap < 20000000);
    if (validGap) {
        targetGap2 = curGap * 110UL / 100UL;
        targetGap3 = curGap * 90UL / 100UL;
    }

    bool signatureFound = (curGap2 > targetGap2) && (curGap3 < targetGap3) &&
                          (lastGap < targetGap2) && (lastGap > targetGap3);
    if (!signatureFound) { return false; }

    bool needsInitialSync = !currentStatus.hasSync || (currentStatus.startRevolutions <= configPage4.StgCycles);
    if (needsInitialSync) { handleSyncAcquisition(); }
    else { handleResyncVerify(); }

    lastSyncRevolution = currentStatus.startRevolutions;
    return true;
}

/// @brief Handle rotation completion and sync validation
static inline bool handleRotationCompletion(void) {
    if (toothCurrentCount <= triggerActualTeeth) { return false; }
    bool syncLost = currentStatus.hasSync && ((lastSyncRevolution + 3) < currentStatus.startRevolutions);
    if (syncLost) { currentStatus.hasSync = false; currentStatus.syncLossCounter++; }
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
    return true;
}

// (All helpers moved to file scope above to reduce nesting depth)

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
