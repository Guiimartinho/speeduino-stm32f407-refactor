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

/// FASE R: DSM 420a sync configuration
struct DSM420aSyncConfig {
    bool priTriggerState;       ///< Expected primary trigger state (HIGH or LOW)
    uint8_t expectedToothCount; ///< Tooth count for this sync point
};

/// @brief Sync configuration table
static const DSM420aSyncConfig syncConfigs[2] PROGMEM = {
    {true,  13},  ///< Primary HIGH when secondary falls -> tooth 13
    {false,  5}   ///< Primary LOW when secondary falls -> tooth 5
};

/// @brief Get expected tooth count from primary state
static inline uint8_t getExpectedTooth(bool priState) {
    for (uint8_t i = 0; i < 2; i++) {
        if (syncConfigs[i].priTriggerState == priState) { return syncConfigs[i].expectedToothCount; }
    }
    return 0;
}

/// @brief Validate and set DSM 420a sync based on primary state (file scope)
static inline void processDSM420aSync(bool priState, bool hasSync, uint8_t currentToothCount) {
    uint8_t expectedTooth = getExpectedTooth(priState);

    if (!hasSync) {
        toothCurrentCount = expectedTooth;
        currentStatus.hasSync = true;
        return;
    }
    if (currentToothCount != expectedTooth) {
        currentStatus.syncLossCounter++;
        toothCurrentCount = expectedTooth;
    }
}

namespace {

// processDSM420aSync and syncConfigs moved to file scope

/// @brief Initialize tooth angle lookup table
static inline void initToothAngles() {
    toothAngles[0] = 711;
    toothAngles[1] = 111;
    toothAngles[2] = 131;
    toothAngles[3] = 151;
    toothAngles[4] = 171;
    toothAngles[5] = toothAngles[1] + 180;
    toothAngles[6] = toothAngles[2] + 180;
    toothAngles[7] = toothAngles[3] + 180;
    toothAngles[8] = toothAngles[4] + 180;
    toothAngles[9]  = toothAngles[1] + 360;
    toothAngles[10] = toothAngles[2] + 360;
    toothAngles[11] = toothAngles[3] + 360;
    toothAngles[12] = toothAngles[4] + 360;
    toothAngles[13] = toothAngles[1] + 540;
    toothAngles[14] = toothAngles[2] + 540;
    toothAngles[15] = toothAngles[3] + 540;
}

/// @brief Handle rotation completion
/// @return true if rotation completed
static inline bool handleRotationCompletion() {
    if ((toothCurrentCount > 16) && (currentStatus.hasSync == true)) {
        toothCurrentCount = 1;
        toothOneMinusOneTime = toothOneTime;
        toothOneTime = curTime;
        currentStatus.startRevolutions++;
        return true;
    }
    return false;
}

/// @brief Handle per-tooth ignition timing
static inline void handlePerToothIgnition() {
    if (configPage2.perToothIgn == true) {
        int16_t crankAngle = toothAngles[(toothCurrentCount - 1)] + configPage4.triggerAngle;
        crankAngle = ignitionLimits(crankAngle);
        checkPerToothTiming(crankAngle, toothCurrentCount);
    }
}

} // anonymous namespace

void triggerSetup_420a(void) {
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 360UL));
    triggerSecFilterTime = 0;
    secondaryToothCount = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    toothCurrentCount = 1;
    triggerToothAngle = 20;
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    toothSystemCount = 0;
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM/50U) * 93U);

    initToothAngles();
}

void triggerPri_420a(void) {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap < triggerFilterTime) { return; }

    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0)) {
        curGap = 0;
    }

    handleRotationCompletion();

    triggerFilterTime = 0;
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    handlePerToothIgnition();
}

void triggerSec_420a(void) {
    bool priState = READ_PRI_TRIGGER();
    processDSM420aSync(priState, currentStatus.hasSync, toothCurrentCount);
}

uint16_t getRPM_420a(void) {
    uint16_t tempRPM = 0;
    if (currentStatus.RPM < currentStatus.crankRPM) {
        tempRPM = stdGetRPM(CAM_SPEED);
    } else {
        tempRPM = stdGetRPM(CAM_SPEED);
    }
    return tempRPM;
}

int getCrankAngle_420a(void) {
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    int crankAngle;
    crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;

    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    if (crankAngle >= 720) { crankAngle -= 720; }
    if (crankAngle < 0) { crankAngle += 360; }

    return crankAngle;
}

void triggerSetEndTeeth_420a(void) {
    if (currentStatus.advance < 9) {
        ignition1EndTooth = 1;
        ignition2EndTooth = 5;
        ignition3EndTooth = 9;
        ignition4EndTooth = 13;
    } else {
        ignition1EndTooth = 16;
        ignition2EndTooth = 4;
        ignition3EndTooth = 8;
        ignition4EndTooth = 12;
    }
}
