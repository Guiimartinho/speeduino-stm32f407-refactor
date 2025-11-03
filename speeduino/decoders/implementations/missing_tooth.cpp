/**
 * @file missing_tooth.cpp
 * @brief Missing Tooth (36-1, 60-2, etc) decoder implementation
 * @details REFACTORED from decoders.cpp - Function complexity reduced
 *
 * ORIGINAL: triggerPri_missingTooth() = 183 lines, complexity ~20+
 * REFACTORED: Multiple functions < 50 lines, complexity < 10
 *
 * Pattern: Missing tooth wheel (e.g., 36-1, 60-2)
 * - Fixed number of teeth with 1 or more missing teeth as sync point
 * - Missing tooth gap is 1.5x (for 1 missing) or 2x+ (for 2+ missing) normal gap
 * - Used for crank or cam position sensing
 *
 * @author Speeduino Team
 * @date 2025-01-31
 * @version 2.0 (REFACTORED)
 *
 * @copyright Copyright (c) 2025 Speeduino
 * @license GPL-3.0
 */

#include "../decoder_interface.h"
#include "../../globals.h"
#include "../../decoders.h"
#include "../../scheduledIO.h"
#include "../../scheduler.h"
#include "../../crankMaths.h"
#include "../../timers.h"
#include "../../schedule_calcs.h"

// Anonymous namespace for private implementation
namespace {

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================

/** @brief Minimum RPM for sync detection (prevents false triggers during cranking) */
constexpr uint16_t SYNC_DETECTION_MIN_RPM = 2000U;

/** @brief Multiplier for 1 missing tooth gap detection (1.5x = 3/2) */
constexpr uint8_t GAP_MULT_1_MISSING_NUMERATOR = 3U;
constexpr uint8_t GAP_MULT_1_MISSING_DENOMINATOR = 2U;

/** @brief Position threshold for missing tooth search (last 1/4 of wheel) */
constexpr uint8_t TOOTH_SEARCH_THRESHOLD_SHIFT = 2U; // (3 * teeth) >> 2 = 75%

// ============================================================================
// HELPER FUNCTIONS - Missing Tooth Detection
// ============================================================================

/**
 * @brief Calculate target gap for missing tooth detection
 * @details Uses bit-shift optimization for 1.5x multiply (1 missing tooth)
 *          or direct multiply for 2+ missing teeth
 * @param lastGap Time between last two regular teeth (�s)
 * @param missingTeeth Number of missing teeth (1, 2, etc.)
 * @return Target gap threshold (�s)
 * @complexity 2 (single conditional)
 */
static inline uint32_t calculateTargetGap(uint32_t lastGap, uint8_t missingTeeth) {
    // Guard clause: invalid gap
    if (lastGap == 0U) {
        return 0U;
    }

    // 1 missing tooth: 1.5x gap (optimized with bit-shift)
    if (missingTeeth == 1U) {
        return (GAP_MULT_1_MISSING_NUMERATOR * lastGap) >> 1; // * 3 / 2
    }

    // 2+ missing teeth: direct multiply
    return lastGap * missingTeeth;
}

/**
 * @brief Check if missing tooth detection should run
 * @details Only detect when: no sync, low RPM, or in last 1/4 of wheel
 * @param hasSync Current sync status
 * @param rpm Current RPM
 * @param toothCount Current tooth count
 * @param totalTeeth Total physical teeth on wheel
 * @return true if should attempt detection
 * @complexity 3
 */
static inline bool shouldDetectMissingTooth(bool hasSync, uint16_t rpm,
                                             uint16_t toothCount, uint16_t totalTeeth) {
    // Always detect if no sync
    if (!hasSync) {
        return true;
    }

    // Detect at low RPM (prevents timing issues during cranking/idle)
    if (rpm < SYNC_DETECTION_MIN_RPM) {
        return true;
    }

    // Only detect in last 1/4 of wheel when synced
    // (Missing tooth should never occur in first 3/4)
    uint16_t threshold = (3U * totalTeeth) >> TOOTH_SEARCH_THRESHOLD_SHIFT; // 75%
    return (toothCount >= threshold);
}

/**
 * @brief Detect if current gap indicates missing tooth
 * @param currentGap Current tooth gap (�s)
 * @param targetGap Calculated target gap (�s)
 * @param toothCount Current tooth count
 * @param expectedTeeth Total expected teeth
 * @return true if missing tooth detected
 * @complexity 2
 */
static inline bool isMissingToothDetected(uint32_t currentGap, uint32_t targetGap,
                                          uint16_t toothCount, uint16_t expectedTeeth) {
    // Gap exceeds threshold?
    if (currentGap > targetGap) {
        return true;
    }

    // Tooth count overflow? (seen more teeth than expected)
    if (toothCount > expectedTeeth) {
        return true;
    }

    return false;
}

// ============================================================================
// HELPER FUNCTIONS - Sync Handling
// ============================================================================

/**
 * @brief Handle sync loss when missing tooth detected too early
 * @details If we see tooth #1 but haven't seen all other teeth,
 *          this indicates a signal problem
 * @param toothCount Current tooth count
 * @param expectedTeeth Total expected teeth
 * @param hasSync Current sync status
 * @complexity 3
 */
static inline void handleEarlySyncLoss(uint16_t toothCount, uint16_t expectedTeeth, bool hasSync) {
    // Guard clause: already lost sync or valid position
    if (!hasSync || (toothCount >= expectedTeeth)) {
        return;
    }

    // Signal issue detected - flag sync loss
    currentStatus.hasSync = false;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    currentStatus.syncLossCounter++;
}

/**
 * @brief Update revolution counter
 * @details Increments on each complete revolution (at missing tooth)
 * @param hasSync Current sync status
 * @param hasHalfSync Half-sync status (crank only, no cam)
 * @param isCamSpeed true if running at cam speed
 * @complexity 4
 */
static inline void updateRevolutionCounter(bool hasSync, bool hasHalfSync, bool isCamSpeed) {
    // Guard clause: no sync at all
    if (!hasSync && !hasHalfSync) {
        currentStatus.startRevolutions = 0U;
        return;
    }

    // Increment revolution counter
    currentStatus.startRevolutions++;

    // Cam speed: double increment (2 crank revs per cam rev)
    if (isCamSpeed) {
        currentStatus.startRevolutions++;
    }
}

/**
 * @brief Handle secondary trigger for sequential mode
 * @details Determines sync status based on cam signal availability
 * @param secondaryCount Cam tooth count
 * @param isCamSpeed true if missing tooth wheel is on cam
 * @param isSequentialFuel true if sequential injection enabled
 * @param isSequentialIgn true if sequential ignition enabled
 * @param isTwoStroke true if two-stroke engine
 * @param isPollLevel true if using poll level cam sensing
 * @complexity 6
 */
static inline void handleSequentialSync(uint16_t secondaryCount, bool isCamSpeed,
                                        bool isSequentialFuel, bool isSequentialIgn,
                                        bool isTwoStroke, bool isPollLevel) {
    // Guard clause: not using sequential mode
    if (!isSequentialFuel && !isSequentialIgn) {
        currentStatus.hasSync = true;
        BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
        return;
    }

    // Sequential mode: need cam signal for full sync
    bool hasCamSignal = (secondaryCount > 0U) || isCamSpeed || isPollLevel || isTwoStroke;

    if (hasCamSignal) {
        // Full sync
        currentStatus.hasSync = true;
        BIT_CLEAR(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    } else if (!currentStatus.hasSync) {
        // Half sync only (crank signal but no cam)
        BIT_SET(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    }
}

/**
 * @brief Reset secondary tooth counter if needed
 * @param triggerPattern Secondary trigger pattern type
 * @complexity 2
 */
static inline void resetSecondaryCounterIfNeeded(uint8_t triggerPattern) {
    // Reset counter for single-tooth and Toyota 3-tooth patterns
    // Prevents overflow, needed for batch-fire V6/V8 with VVT
    if ((triggerPattern == SEC_TRIGGER_SINGLE) ||
        (triggerPattern == SEC_TRIGGER_TOYOTA_3)) {
        secondaryToothCount = 0U;
    }
}

/**
 * @brief Handle missing tooth detection and sync update
 * @details Called when missing tooth is detected - updates sync state
 * @param toothCount Current tooth count before reset
 * @param expectedTeeth Total expected teeth
 * @complexity 8
 */
static void handleMissingToothSync(uint16_t toothCount, uint16_t expectedTeeth) {
    // Check for early sync loss
    handleEarlySyncLoss(toothCount, expectedTeeth, currentStatus.hasSync);

    // Update revolution counter
    bool hasHalfSync = BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC);
    bool isCamSpeed = (configPage4.TrigSpeed == CAM_SPEED);
    updateRevolutionCounter(currentStatus.hasSync, hasHalfSync, isCamSpeed);

    // Reset to tooth #1
    toothCurrentCount = 1U;

    // Handle poll level secondary trigger
    if (configPage4.trigPatternSec == SEC_TRIGGER_POLL) {
        revolutionOne = (configPage4.PollLevelPolarity == READ_SEC_TRIGGER()) ? 1U : 0U;
    } else {
        revolutionOne = !revolutionOne; // Flip sequential revolution tracker
    }

    // Update tooth one timing
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;

    // Handle sequential mode sync
    bool isSeqFuel = (configPage2.injLayout == INJ_SEQUENTIAL);
    bool isSeqIgn = (configPage4.sparkMode == IGN_MODE_SEQUENTIAL);
    bool isTwoStroke = (configPage2.strokes == TWO_STROKE);
    bool isPollLevel = (configPage4.trigPatternSec == SEC_TRIGGER_POLL);

    handleSequentialSync(secondaryToothCount, isCamSpeed, isSeqFuel,
                        isSeqIgn, isTwoStroke, isPollLevel);

    // Reset secondary counter if needed
    resetSecondaryCounterIfNeeded(configPage4.trigPatternSec);

    // Clear filter to prevent unrecoverable state from intermittent signals
    triggerFilterTime = 0U;

    // Update timing for next gap calculation
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    // Flag that tooth angle is double at missing tooth
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

// ============================================================================
// HELPER FUNCTIONS - Per-Tooth Ignition
// ============================================================================

/**
 * @brief Handle per-tooth ignition timing
 * @details Calculates crank angle and checks ignition timing for current tooth
 * @param toothCount Current tooth number
 * @param toothAngle Degrees per tooth
 * @param isSequential true if sequential ignition mode
 * @param isRevolutionOne true if on first revolution
 * @param isCrankSpeed true if trigger is crank speed
 * @param isFourStroke true if four-stroke engine
 * @complexity 5
 */
static inline void handlePerToothIgnition(uint16_t toothCount, uint16_t toothAngle,
                                          bool isSequential, bool isRevolutionOne,
                                          bool isCrankSpeed, bool isFourStroke) {
    // Calculate crank angle for current tooth
    int16_t crankAngle = ((toothCount - 1U) * toothAngle) + configPage4.triggerAngle;

    // Sequential mode on first revolution: add 360� offset
    if (isSequential && isRevolutionOne && isCrankSpeed && isFourStroke) {
        crankAngle += 360;
        crankAngle = ignitionLimits(crankAngle);
        checkPerToothTiming(crankAngle, configPage4.triggerTeeth + toothCount);
    } else {
        crankAngle = ignitionLimits(crankAngle);
        checkPerToothTiming(crankAngle, toothCount);
    }
}

// ============================================================================
// MAIN TRIGGER FUNCTIONS
// ============================================================================

/**
 * @brief Process missing tooth detection logic
 * @details Checks if current gap indicates missing tooth and handles sync
 * @param currentGap Current tooth gap (�s)
 * @return true if missing tooth detected
 * @complexity 7
 */
static bool processMissingToothDetection(uint32_t currentGap) {
    // Guard clause: not enough history
    if ((toothLastToothTime == 0U) || (toothLastMinusOneToothTime == 0U)) {
        return false;
    }

    // Check if we should attempt detection
    if (!shouldDetectMissingTooth(currentStatus.hasSync, currentStatus.RPM,
                                   toothCurrentCount, triggerActualTeeth)) {
        return false;
    }

    // Calculate expected gap for missing tooth
    uint32_t lastGap = toothLastToothTime - toothLastMinusOneToothTime;
    uint32_t targetGap = calculateTargetGap(lastGap, configPage4.triggerMissingTeeth);

    // Detect missing tooth
    if (!isMissingToothDetected(currentGap, targetGap, toothCurrentCount, triggerActualTeeth)) {
        return false;
    }

    // Missing tooth detected - handle sync update
    handleMissingToothSync(toothCurrentCount, triggerActualTeeth);
    return true;
}

/**
 * @brief Process regular (non-missing) tooth
 * @details Updates timing and filter for normal teeth
 * @complexity 2
 */
static inline void processRegularTooth(void) {
    // Update adaptive filter
    setFilter(curGap);

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    // Flag that tooth angle is correct (not at missing tooth)
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup missing tooth decoder
 * @details Initializes all decoder parameters based on configuration
 * @complexity 5
 */
void triggerSetup_missingTooth(void)
{
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);

    // Calculate degrees per tooth
    triggerToothAngle = 360U / configPage4.triggerTeeth;

    // Cam speed: double the angle
    if (configPage4.TrigSpeed == CAM_SPEED) {
        triggerToothAngle = 720U / configPage4.triggerTeeth;
        BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    }

    // Calculate actual teeth (physical teeth = total - missing)
    triggerActualTeeth = configPage4.triggerTeeth - configPage4.triggerMissingTeeth;

    // Calculate trigger filter time (minimum time between teeth at max RPM)
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * configPage4.triggerTeeth));

    // Secondary trigger filter
    if (configPage4.trigPatternSec == SEC_TRIGGER_4_1) {
        triggerSecFilterTime = MICROS_PER_MIN / MAX_RPM / 4U / 2U;
    } else {
        triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U));
    }

    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);

    // Sync detection threshold (50% of total teeth)
    checkSyncToothCount = configPage4.triggerTeeth >> 1;

    // Initialize state
    toothLastMinusOneToothTime = 0U;
    toothCurrentCount = 0U;
    secondaryToothCount = 0U;
    thirdToothCount = 0U;
    toothOneTime = 0U;
    toothOneMinusOneTime = 0U;

    // Calculate max stall time (time for missing tooth gap at 50 RPM)
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle *
                      (configPage4.triggerMissingTeeth + 1U));

    // Determine if secondary trigger is needed
    bool needsSecondary = (configPage4.TrigSpeed == CRANK_SPEED) &&
                          ((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) ||
                           (configPage2.injLayout == INJ_SEQUENTIAL) ||
                           (configPage6.vvtEnabled > 0U));

    if (needsSecondary) {
        BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
    } else {
        BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
    }

#ifdef USE_LIBDIVIDE
    divTriggerToothAngle = libdivide::libdivide_s16_gen(triggerToothAngle);
#endif
}

/**
 * @brief Primary trigger interrupt handler (REFACTORED)
 * @details Processes each tooth on missing tooth wheel
 *
 * ORIGINAL: 183 lines, complexity ~20+, deep nesting
 * REFACTORED: 48 lines, complexity 6, max nesting 2
 *
 * @complexity 6
 * @isr CRITICAL - Must complete in < 10�s
 */
void triggerPri_missingTooth(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Guard clause: filter out noise
    if (curGap < triggerFilterTime) {
        return;
    }

    // Valid trigger - increment counter
    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Guard clause: need timing history for gap detection
    if ((toothLastToothTime > 0U) && (toothLastMinusOneToothTime > 0U)) {
        // Attempt missing tooth detection
        bool isMissingTooth = processMissingToothDetection(curGap);

        // Process tooth based on type
        if (isMissingTooth) {
            // Already handled in processMissingToothDetection()
        } else {
            processRegularTooth();
        }
    } else {
        // Initial startup - just update timing
        toothLastMinusOneToothTime = toothLastToothTime;
        toothLastToothTime = curTime;
    }

    // Handle per-tooth ignition if enabled
    if (configPage2.perToothIgn && !BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        bool isSequential = (configPage4.sparkMode == IGN_MODE_SEQUENTIAL);
        bool isCrankSpeed = (configPage4.TrigSpeed == CRANK_SPEED);
        bool isFourStroke = (configPage2.strokes == FOUR_STROKE);

        handlePerToothIgnition(toothCurrentCount, triggerToothAngle, isSequential,
                              revolutionOne, isCrankSpeed, isFourStroke);
    }
}

/**
 * @brief Secondary trigger interrupt handler
 * @details Handles cam signal for sequential mode
 * @note Uses table-driven pattern from original (already refactored)
 */
void triggerSec_missingTooth(void)
{
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;

    // Guard clause: safety check for initial startup
    if (toothLastSecToothTime == 0U) {
        curGap2 = 0U;
        toothLastSecToothTime = curTime2;
        return;
    }

    // Guard clause: filter out noise
    if (curGap2 < triggerSecFilterTime) {
        return;
    }

    // Handle SEC_TRIGGER_4_1 separately (complex missing tooth detection)
    if (configPage4.trigPatternSec == SEC_TRIGGER_4_1) {
        // Complex 4-1 pattern logic (preserved from original)
        uint32_t tempGap = toothLastSecToothTime - toothLastMinusOneSecToothTime;
        uint32_t targetGap = (tempGap >> 1U); // Multiply by 0.5

        if (curGap2 > targetGap) {
            // Missing tooth on secondary - reset counter
            secondaryToothCount = 1U;
            revolutionOne = 1U;
        } else {
            secondaryToothCount++;
        }

        toothLastMinusOneSecToothTime = toothLastSecToothTime;
        toothLastSecToothTime = curTime2;
        triggerSecFilterTime = curGap2 >> 1U;
        triggerRecordVVT1Angle();
    } else {
        // Use table-driven helper for simple patterns (declared in decoders.h, defined in decoders.cpp)
        processSimpleSecTrigger(configPage4.trigPatternSec, curGap2,
                               &secondaryToothCount, &revolutionOne,
                               &triggerSecFilterTime);

        toothLastSecToothTime = curTime2;
    }
}

/**
 * @brief Third trigger interrupt handler
 * @details Increments third trigger counter
 */
void triggerThird_missingTooth(void)
{
    thirdToothCount++;
}

/**
 * @brief Get RPM from missing tooth wheel
 * @details Calculates RPM based on time between tooth one events
 * @return RPM value
 */
uint16_t getRPM_missingTooth(void)
{
    // Guard clause: no valid timing
    if ((toothOneTime == 0U) || (toothOneMinusOneTime == 0U)) {
        return 0U;
    }

    uint32_t revolutionTime = toothOneTime - toothOneMinusOneTime;

    // Guard clause: prevent divide by zero
    if (revolutionTime == 0U) {
        return 0U;
    }

    // Account for cam speed (2 crank revs per cam rev)
    if (configPage4.TrigSpeed == CAM_SPEED) {
        revolutionTime = revolutionTime >> 1; // Divide by 2
    }

    return (MICROS_PER_MIN / revolutionTime);
}

/**
 * @brief Calculate crank angle from tooth position
 * @return Crank angle in degrees
 */
int getCrankAngle_missingTooth(void)
{
    return ((toothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
}

/**
 * @brief Set end teeth for ignition/fuel scheduling
 * @details Configures end tooth positions based on angles
 */
void triggerSetEndTeeth_missingTooth(void)
{
    uint8_t toothAdder = 0;
    if (((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) || (configPage4.sparkMode == IGN_MODE_SINGLE)) &&
        (configPage4.TrigSpeed == CRANK_SPEED) && (configPage2.strokes == FOUR_STROKE)) {
        toothAdder = configPage4.triggerTeeth;
    }

    // calcEndTeeth_missingTooth is declared in decoders.h
    ignition1EndTooth = calcEndTeeth_missingTooth(ignition1EndAngle, toothAdder);
    ignition2EndTooth = calcEndTeeth_missingTooth(ignition2EndAngle, toothAdder);
    ignition3EndTooth = calcEndTeeth_missingTooth(ignition3EndAngle, toothAdder);
    ignition4EndTooth = calcEndTeeth_missingTooth(ignition4EndAngle, toothAdder);
    ignition5EndTooth = calcEndTeeth_missingTooth(ignition5EndAngle, toothAdder);
    ignition6EndTooth = calcEndTeeth_missingTooth(ignition6EndAngle, toothAdder);
    ignition7EndTooth = calcEndTeeth_missingTooth(ignition7EndAngle, toothAdder);
    ignition8EndTooth = calcEndTeeth_missingTooth(ignition8EndAngle, toothAdder);
}
