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

/// @brief Handle sync verification for second CAM restart
static inline void verifySyncOnSecondCam(void) {
    bool toothMismatch = (toothCurrentCount != (configPage4.triggerTeeth - 1U)) && (currentStatus.startRevolutions > 2U);
    if (toothMismatch) { currentStatus.syncLossCounter++; }
    if (configPage4.useResync == 1) { toothCurrentCount = configPage4.triggerTeeth - 1; }
}

/// @brief Handle second CAM restart case (file scope)
static inline void handleSecondCamRestart(void) {
    if (!currentStatus.hasSync) {
        toothLastToothTime = micros();
        toothLastMinusOneToothTime = micros() - 1500000;
        toothCurrentCount = configPage4.triggerTeeth - 1;
        currentStatus.hasSync = true;
    } else {
        verifySyncOnSecondCam();
    }
    revolutionOne = 1;
    triggerSecFilterTime = curGap << 2;
    secondaryToothCount = 1;
}

/// @brief Handle first start case (file scope)
static inline void handleFirstStart(void) {
    toothLastToothTime = micros();
    toothLastMinusOneToothTime = micros() - 1500000;
    toothCurrentCount = 1;
    revolutionOne = 1;
    currentStatus.hasSync = true;
}

/// @brief Handle other sync cases (file scope)
static inline void handleOtherSync(void) {
    triggerSecFilterTime = curGap + (curGap >> 1);
    secondaryToothCount++;
    checkSyncToothCount = 1;
}

/// @brief Handle per-tooth ignition timing (file scope)
static inline void handlePerToothIgnition(void) {
    if (!configPage2.perToothIgn) { return; }
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) { return; }

    int16_t crankAngle = ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
    bool isSequentialSecondRev = (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) &&
                                  (revolutionOne == true) && (configPage4.TrigSpeed == CRANK_SPEED);
    if (isSequentialSecondRev) {
        crankAngle += 360;
        checkPerToothTiming(crankAngle, (configPage4.triggerTeeth + toothCurrentCount));
    } else {
        checkPerToothTiming(crankAngle, toothCurrentCount);
    }
}

namespace {

/// FASE S: Weber sync state machine (explicit enum-based)
enum WeberSyncState {
    WEBER_SYNC_SECOND_CAM_RESTART,  ///< secondaryToothCount==2 && checkSyncToothCount==3
    WEBER_SYNC_FIRST_START,         ///< !hasSync && toothCurrentCount>=3 && secondaryToothCount==0
    WEBER_SYNC_OTHER                ///< All other cases (noise/normal)
};

/// @brief Determine Weber sync state
static inline WeberSyncState getWeberSyncState(uint8_t secCount, uint8_t checkCount, bool hasSync, uint8_t toothCount) {
    if ((secCount == 2) && (checkCount == 3)) { return WEBER_SYNC_SECOND_CAM_RESTART; }
    if (!hasSync && (toothCount >= 3) && (secCount == 0)) { return WEBER_SYNC_FIRST_START; }
    return WEBER_SYNC_OTHER;
}

} // anonymous namespace

void triggerSetup_Weber(void) {
    triggerSetup_DualWheel();
}

void triggerPri_Weber(void) {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap < triggerFilterTime) { return; }

    toothCurrentCount++;
    if (checkSyncToothCount > 0) { checkSyncToothCount++; }
    if (triggerSecFilterTime <= curGap) { triggerSecFilterTime = curGap + (curGap >> 1); }
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    if (currentStatus.hasSync == true) {
        if ((toothCurrentCount == 1) || (toothCurrentCount > configPage4.triggerTeeth)) {
            toothCurrentCount = 1;
            revolutionOne = !revolutionOne;
            toothOneMinusOneTime = toothOneTime;
            toothOneTime = curTime;
            currentStatus.startRevolutions++;
        }

        setFilter(curGap);
    } else {
        if ((secondaryToothCount == 1) && (checkSyncToothCount == 4)) {
            toothCurrentCount = 2;
            currentStatus.hasSync = true;
            revolutionOne = 0;
        }
    }

    handlePerToothIgnition();
}

void triggerSec_Weber(void) {
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;

    if (curGap2 < triggerSecFilterTime) {
        triggerSecFilterTime = curGap + (curGap >> 1);
        checkSyncToothCount = 1;
        return;
    }

    toothLastSecToothTime = curTime2;

    WeberSyncState state = getWeberSyncState(secondaryToothCount, checkSyncToothCount, currentStatus.hasSync, toothCurrentCount);

    switch (state) {
        case WEBER_SYNC_SECOND_CAM_RESTART:
            handleSecondCamRestart();
            break;

        case WEBER_SYNC_FIRST_START:
            handleFirstStart();
            break;

        case WEBER_SYNC_OTHER:
            handleOtherSync();
            break;
    }
}

uint16_t getRPM_Weber(void) {
    return getRPM_DualWheel();
}

int getCrankAngle_Weber(void) {
    return getCrankAngle_DualWheel();
}

void triggerSetEndTeeth_Weber(void) {
    triggerSetEndTeeth_DualWheel();
}

// Aliases for legacy init.cpp compatibility (Webber with two b's)
void triggerPri_Webber(void) { triggerPri_Weber(); }
void triggerSec_Webber(void) { triggerSec_Weber(); }
