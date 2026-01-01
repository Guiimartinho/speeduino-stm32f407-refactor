#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

// ============================================================================
// FILE SCOPE HELPERS (to reduce nesting depth)
// ============================================================================

/// @brief Handle sync loss on bad trigger (file scope)
static inline void handleTriggerSyncLoss(void) {
    if (currentStatus.hasSync) { currentStatus.syncLossCounter++; }
    currentStatus.hasSync = false;
    toothCurrentCount = 0;
}

/// @brief Handle first tooth detection (file scope)
static inline void handleFirstTooth(void) {
    toothCurrentCount = 1;
    triggerToothAngle = 0;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.hasSync = true;
}

/// @brief Handle second tooth detection (file scope)
static inline void handleSecondTooth(void) {
    toothCurrentCount = 2;
    triggerToothAngle = 157;
}

/// @brief Handle valid rising edge trigger (file scope)
static inline void handleValidRisingEdge(void) {
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);
    targetGap = lastGap;
    toothCurrentCount++;
    if (curGap > targetGap) { handleFirstTooth(); }
    else { handleSecondTooth(); }
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
    currentStatus.startRevolutions++;
}

/// @brief Calculate cranking RPM with validation (file scope)
static inline uint16_t calcCrankingRPM(void) {
    bool invalidTiming = (toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0);
    if (invalidTiming) { return 0; }

    int tempToothAngle;
    unsigned long toothTime;
    noInterrupts();
    tempToothAngle = triggerToothAngle;
    SetRevolutionTime(toothOneTime - toothOneMinusOneTime);
    toothTime = (toothLastToothTime - toothLastMinusOneToothTime);
    interrupts();
    toothTime = toothTime * 36;
    return ((unsigned long)tempToothAngle * (MICROS_PER_MIN/10U)) / toothTime;
}

void triggerSetup_Harley(void) {
    triggerToothAngle = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 60U);
    if (currentStatus.initialisationComplete == false) { toothLastToothTime = micros(); }
    triggerFilterTime = 1500;
}

void triggerPri_Harley(void) {
    lastGap = curGap;
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    setFilter(curGap);
    if (curGap <= triggerFilterTime) { return; }

    bool isRisingEdge = (READ_PRI_TRIGGER() == HIGH);
    if (isRisingEdge) { handleValidRisingEdge(); }
    else { handleTriggerSyncLoss(); }
}

void triggerSec_Harley(void) { return; }

uint16_t getRPM_Harley(void) {
    if (!currentStatus.hasSync) { return 0; }

    uint16_t tempRPM;
    bool isCranking = (currentStatus.RPM < (unsigned int)(configPage4.crankRPM * 100));
    if (isCranking) { tempRPM = calcCrankingRPM(); }
    else { tempRPM = stdGetRPM(CRANK_SPEED); }

    return tempRPM;
}

int getCrankAngle_Harley(void) {
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    int crankAngle;
    if ((tempToothCurrentCount == 1) || (tempToothCurrentCount == 3)) {
        crankAngle = 0 + configPage4.triggerAngle;
    } else {
        crankAngle = 157 + configPage4.triggerAngle;
    }

    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_Harley(void) {}
