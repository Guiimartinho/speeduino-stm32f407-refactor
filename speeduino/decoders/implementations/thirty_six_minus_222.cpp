#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

namespace {

/// FASE P: ThirtySixMinus222 missing tooth sync configuration
/// @brief Subaru 36-2-2-2 pattern uses two double-gaps for sync
/// toothSystemCount tracks state: 0=normal, 1=saw first double-gap
struct ThirtySixMinus222SyncConfig {
    uint8_t nCylinders;        ///< Engine cylinder count (4 or 6)
    uint8_t systemCountState;  ///< toothSystemCount value (0 or 1)
    uint16_t targetToothCount; ///< Tooth count to set for this sync point
};

/// @brief Static configuration table for 36-2-2-2 sync points
/// After second double-gap (systemCount=1, double-gap detected): first tooth after 2x2 missing
/// After single tooth following first gap (systemCount=1, no double-gap): single missing tooth
static const ThirtySixMinus222SyncConfig syncConfigs[] PROGMEM = {
    {4, 1, 19},  ///< H4: tooth 19 is first after 2x2 missing
    {6, 1, 12},  ///< H6: tooth 12 is first after 2x2 missing
    {4, 0, 35},  ///< H4: tooth 35 is after single missing
    {6, 0, 34}   ///< H6: tooth 34 is after single missing
};

/// @brief Get sync tooth count for ThirtySixMinus222 pattern
/// @param nCylinders Engine cylinder count (4 or 6)
/// @param systemCountState toothSystemCount state (0 or 1)
/// @return Tooth count to set
static inline uint16_t getSyncTooth(uint8_t nCylinders, uint8_t systemCountState) {
    const uint8_t configCount = sizeof(syncConfigs) / sizeof(ThirtySixMinus222SyncConfig);

    for (uint8_t i = 0; i < configCount; i++) {
        const ThirtySixMinus222SyncConfig* cfg = &syncConfigs[i];

        if (cfg->nCylinders == nCylinders && cfg->systemCountState == systemCountState) {
            return cfg->targetToothCount;
        }
    }

    return 0; ///< Should never reach here
}

/// FASE Q: ThirtySixMinus222 RPM excluded teeth configuration
/// @brief Certain teeth near missing tooth gaps cannot be used for RPM calculation
struct ThirtySixMinus222RPMExclusion {
    uint8_t nCylinders;    ///< Engine cylinder count
    uint8_t excludedTooth; ///< Tooth number to exclude
};

/// @brief Table of teeth excluded from RPM calculation
static const ThirtySixMinus222RPMExclusion rpmExclusions[] PROGMEM = {
    {4, 19}, {4, 16}, {4, 34},  ///< H4 excluded teeth
    {6,  9}, {6, 12}, {6, 33}   ///< H6 excluded teeth
};

/// @brief Check if current tooth is excluded from RPM calculation
/// @param nCyl Engine cylinder count
/// @param toothCount Current tooth number
/// @return true if tooth should be excluded from RPM calculation
static inline bool isToothExcludedFromRPM(uint8_t nCyl, uint8_t toothCount) {
    const uint8_t exclusionCount = sizeof(rpmExclusions) / sizeof(ThirtySixMinus222RPMExclusion);

    for (uint8_t i = 0; i < exclusionCount; i++) {
        const ThirtySixMinus222RPMExclusion* excl = &rpmExclusions[i];
        if (excl->nCylinders == nCyl && excl->excludedTooth == toothCount) {
            return true;
        }
    }
    return false;
}

/// FASE N: ThirtySixMinus222 end teeth configuration (data-driven)
/// @brief Subaru H4 and H6 engines use 36-2-2-2 crank pattern
/// End tooth varies by advance timing to prevent coil overlap
struct ThirtySixMinus222EndTeethConfig {
    uint8_t nCylinders;      ///< Engine cylinder count (4 or 6)
    uint8_t ignitionChannel; ///< Ignition channel number (1, 2, or 3)
    uint8_t advanceMax;      ///< Maximum advance for this tooth (degrees)
    uint16_t endTooth;       ///< End tooth for this advance range
};

/// @brief Static configuration table for 36-2-2-2 end teeth
/// H4 (4-cylinder): 2 ignition channels with advance-based tooth selection
/// H6 (6-cylinder): 3 ignition channels with advance-based tooth selection
static const ThirtySixMinus222EndTeethConfig endTeethConfigs[] PROGMEM = {
    {4, 1, 10, 36},  {4, 1, 20, 35},  {4, 1, 30, 34},  {4, 1, 255, 31},
    {4, 2, 30, 16},  {4, 2, 255, 13},
    {6, 1, 10, 36},  {6, 1, 20, 35},  {6, 1, 30, 34},  {6, 1, 40, 33},  {6, 1, 255, 31},
    {6, 2, 20, 9},   {6, 2, 255, 6},
    {6, 3, 10, 23},  {6, 3, 20, 22},  {6, 3, 30, 21},  {6, 3, 40, 20},  {6, 3, 255, 19}
};

/// @brief Set end tooth using data-driven lookup
/// @param nCylinders Engine cylinder count
/// @param channel Ignition channel number
/// @param advance Current advance (degrees)
/// @return End tooth number
static inline uint16_t getEndTooth(uint8_t nCylinders, uint8_t channel, uint8_t advance) {
    const uint8_t configCount = sizeof(endTeethConfigs) / sizeof(ThirtySixMinus222EndTeethConfig);

    for (uint8_t i = 0; i < configCount; i++) {
        const ThirtySixMinus222EndTeethConfig* cfg = &endTeethConfigs[i];

        if (cfg->nCylinders == nCylinders &&
            cfg->ignitionChannel == channel &&
            advance < cfg->advanceMax) {
            return cfg->endTooth;
        }
    }

    return 0; ///< Should never reach here
}

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
