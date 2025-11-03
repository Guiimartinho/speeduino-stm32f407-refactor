#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

namespace {

/// @brief Detect gap type and set tooth position
/// @param curGap Current gap duration
/// @param targetGap Target gap for 1.5x detection
/// @param targetGap2 Target gap for 3x detection
/// @return true if gap detected
static inline bool detectGapAndSetTooth(unsigned long curGap, unsigned long targetGap, unsigned long targetGap2) {
    if (curGap <= targetGap) { return false; }

    if (curGap < targetGap2) {
        toothCurrentCount = 20;
        currentStatus.hasSync = true;
    } else {
        toothCurrentCount = 1;
        currentStatus.hasSync = true;
    }

    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    triggerFilterTime = 0;
    return true;
}

/// @brief Process normal tooth (no gap)
static inline void processNormalTooth() {
    if ((toothCurrentCount > 36) || (toothCurrentCount == 1)) {
        toothCurrentCount = 1;
        revolutionOne = !revolutionOne;
        toothOneMinusOneTime = toothOneTime;
        toothOneTime = curTime;
        currentStatus.startRevolutions++;
    }

    setFilter(curGap);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

/// @brief Handle per-tooth ignition timing
static inline void handlePerToothIgnition() {
    if (configPage2.perToothIgn == true) {
        int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
        crankAngle = ignitionLimits(crankAngle);
        checkPerToothTiming(crankAngle, toothCurrentCount);
    }
}

} // anonymous namespace

void triggerSetup_ThirtySixMinus21(void) {
    triggerToothAngle = 10;
    triggerActualTeeth = 33;
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 36));
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    checkSyncToothCount = (configPage4.triggerTeeth) >> 1;
    toothLastMinusOneToothTime = 0;
    toothCurrentCount = 0;
    toothOneTime = 0;
    toothOneMinusOneTime = 0;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * triggerToothAngle * 2U);
}

void triggerPri_ThirtySixMinus21(void) {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap < triggerFilterTime) { return; }

    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    targetGap2 = (3 * (toothLastToothTime - toothLastMinusOneToothTime));
    targetGap = targetGap2 >> 1;

    if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0)) {
        curGap = 0;
    }

    if (!detectGapAndSetTooth(curGap, targetGap, targetGap2)) {
        processNormalTooth();
    }

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    handlePerToothIgnition();
}

void triggerSec_ThirtySixMinus21(void) {}

uint16_t getRPM_ThirtySixMinus21(void) {
    uint16_t tempRPM = 0;
    if (currentStatus.RPM < currentStatus.crankRPM) {
        if ((toothCurrentCount != 20) && (BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT))) {
            tempRPM = crankingGetRPM(36, CRANK_SPEED);
        } else {
            tempRPM = currentStatus.RPM;
        }
    } else {
        tempRPM = stdGetRPM(CRANK_SPEED);
    }
    return tempRPM;
}

int getCrankAngle_ThirtySixMinus21(void) {
    return 0;
}

void triggerSetEndTeeth_ThirtySixMinus21(void) {
    ignition1EndTooth = 10;
    ignition2EndTooth = 28;
}
