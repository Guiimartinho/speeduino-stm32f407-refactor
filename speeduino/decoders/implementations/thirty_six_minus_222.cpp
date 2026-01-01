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

/// FASE P: ThirtySixMinus222 missing tooth sync configuration
struct ThirtySixMinus222SyncConfig {
    uint8_t nCylinders;
    uint8_t systemCountState;
    uint16_t targetToothCount;
};

static const ThirtySixMinus222SyncConfig syncConfigs[] PROGMEM = {
    {4, 1, 19}, {6, 1, 12}, {4, 0, 35}, {6, 0, 34}
};

/// @brief Match sync config
static inline bool matchSyncConfig(const ThirtySixMinus222SyncConfig* cfg, uint8_t nCyl, uint8_t state) {
    return (cfg->nCylinders == nCyl) && (cfg->systemCountState == state);
}

static inline uint16_t getSyncTooth(uint8_t nCylinders, uint8_t systemCountState) {
    const uint8_t configCount = sizeof(syncConfigs) / sizeof(ThirtySixMinus222SyncConfig);
    for (uint8_t i = 0; i < configCount; i++) {
        const ThirtySixMinus222SyncConfig* cfg = &syncConfigs[i];
        if (matchSyncConfig(cfg, nCylinders, systemCountState)) { return cfg->targetToothCount; }
    }
    return 0;
}

/// FASE Q: ThirtySixMinus222 RPM excluded teeth configuration
struct ThirtySixMinus222RPMExclusion {
    uint8_t nCylinders;
    uint8_t excludedTooth;
};

static const ThirtySixMinus222RPMExclusion rpmExclusions[] PROGMEM = {
    {4, 19}, {4, 16}, {4, 34}, {6, 9}, {6, 12}, {6, 33}
};

/// @brief Match exclusion
static inline bool matchExclusion(const ThirtySixMinus222RPMExclusion* excl, uint8_t nCyl, uint8_t tooth) {
    return (excl->nCylinders == nCyl) && (excl->excludedTooth == tooth);
}

static inline bool isToothExcludedFromRPM(uint8_t nCyl, uint8_t toothCount) {
    const uint8_t exclusionCount = sizeof(rpmExclusions) / sizeof(ThirtySixMinus222RPMExclusion);
    for (uint8_t i = 0; i < exclusionCount; i++) {
        const ThirtySixMinus222RPMExclusion* excl = &rpmExclusions[i];
        if (matchExclusion(excl, nCyl, toothCount)) { return true; }
    }
    return false;
}

/// FASE N: ThirtySixMinus222 end teeth configuration (data-driven)
struct ThirtySixMinus222EndTeethConfig {
    uint8_t nCylinders;
    uint8_t ignitionChannel;
    uint8_t advanceMax;
    uint16_t endTooth;
};

static const ThirtySixMinus222EndTeethConfig endTeethConfigs[] PROGMEM = {
    {4, 1, 10, 36}, {4, 1, 20, 35}, {4, 1, 30, 34}, {4, 1, 255, 31},
    {4, 2, 30, 16}, {4, 2, 255, 13},
    {6, 1, 10, 36}, {6, 1, 20, 35}, {6, 1, 30, 34}, {6, 1, 40, 33}, {6, 1, 255, 31},
    {6, 2, 20, 9},  {6, 2, 255, 6},
    {6, 3, 10, 23}, {6, 3, 20, 22}, {6, 3, 30, 21}, {6, 3, 40, 20}, {6, 3, 255, 19}
};

/// @brief Match end teeth config
static inline bool matchEndTeethConfig(const ThirtySixMinus222EndTeethConfig* cfg, uint8_t nCyl, uint8_t ch, uint8_t adv) {
    return (cfg->nCylinders == nCyl) && (cfg->ignitionChannel == ch) && (adv < cfg->advanceMax);
}

static inline uint16_t getEndTooth(uint8_t nCylinders, uint8_t channel, uint8_t advance) {
    const uint8_t configCount = sizeof(endTeethConfigs) / sizeof(ThirtySixMinus222EndTeethConfig);
    for (uint8_t i = 0; i < configCount; i++) {
        const ThirtySixMinus222EndTeethConfig* cfg = &endTeethConfigs[i];
        if (matchEndTeethConfig(cfg, nCylinders, channel, advance)) { return cfg->endTooth; }
    }
    return 0;
}

namespace {

/// @brief Process missing tooth detection and sync
/// @param gapMult Gap multiplier detected
static inline void processMissingTooth(uint8_t gapMult) {
    if (toothSystemCount == 1) {
        toothCurrentCount = getSyncTooth(configPage2.nCylinders, 1);
        toothSystemCount = 0;
        currentStatus.hasSync = true;
    } else {
        toothSystemCount = 1;
        toothCurrentCount++;
        toothCurrentCount++;
    }
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    triggerFilterTime = 0;
}

/// @brief Process normal tooth (no gap detected)
static inline void processNormalTooth() {
    if (toothCurrentCount > 36) {
        toothCurrentCount = 1;
        revolutionOne = !revolutionOne;
        toothOneMinusOneTime = toothOneTime;
        toothOneTime = curTime;
        currentStatus.startRevolutions++;
    } else if (toothSystemCount == 1) {
        toothCurrentCount = getSyncTooth(configPage2.nCylinders, 0);
        currentStatus.hasSync = true;
    }

    setFilter(curGap);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    toothSystemCount = 0;
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

void triggerSetup_ThirtySixMinus222(void) {
    triggerToothAngle = 10;
    triggerActualTeeth = 30;
    triggerFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * 36));
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

void triggerPri_ThirtySixMinus222(void) {
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    if (curGap < triggerFilterTime) { return; }

    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    if (toothSystemCount == 0) {
        targetGap = ((toothLastToothTime - toothLastMinusOneToothTime)) * 2;
    }

    if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0)) {
        curGap = 0;
    }

    if (curGap > targetGap) {
        processMissingTooth(2);
    } else {
        processNormalTooth();
    }

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    handlePerToothIgnition();
}

void triggerSec_ThirtySixMinus222(void) {}

uint16_t getRPM_ThirtySixMinus222(void) {
    uint16_t tempRPM = 0;
    if (currentStatus.RPM < currentStatus.crankRPM) {
        bool isExcluded = isToothExcludedFromRPM(configPage2.nCylinders, toothCurrentCount);

        if (!isExcluded && BIT_CHECK(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT)) {
            tempRPM = crankingGetRPM(36, CRANK_SPEED);
        } else {
            tempRPM = currentStatus.RPM;
        }
    } else {
        tempRPM = stdGetRPM(CRANK_SPEED);
    }
    return tempRPM;
}

int getCrankAngle_ThirtySixMinus222(void) {
    return 0;
}

void triggerSetEndTeeth_ThirtySixMinus222(void) {
    uint8_t nCyl = configPage2.nCylinders;
    uint8_t advance = currentStatus.advance;

    ignition1EndTooth = getEndTooth(nCyl, 1, advance);
    ignition2EndTooth = getEndTooth(nCyl, 2, advance);

    if (nCyl == 6) {
        ignition3EndTooth = getEndTooth(nCyl, 3, advance);
    }
}
