/**
 * @file honda_j32.cpp
 * @brief Honda J32 decoder implementation
 * @details REFACTORED from decoders.cpp (lines 2407-2593)
 *
 * Pattern: 24 teeth (22 physical) with 2 missing teeth
 * - 3.2 liter 6 cylinder SOHC (J32A4)
 * - Missing tooth after teeth 14 and 22
 * - 7 teeth, gap, 15 teeth, gap pattern
 * - Teeth 14 and 22 have 18 degree spacing (vs 15 deg nominal)
 * - Non-sequential mode
 *
 * ORIGINAL: 187 lines (includes FASE U and FASE V extractions)
 * REFACTORED: Preserves helper functions and lookup table
 *
 * @author Speeduino Team
 * @date 2025-11-03
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
// CONSTANTS
// ============================================================================

/** @brief Nominal tooth count (24 teeth, but only 22 physical) */
constexpr uint8_t NOMINAL_TEETH_COUNT = 24U;

/** @brief Total teeth including rollover */
constexpr uint8_t TEETH_WITH_ROLLOVER = 25U;

/** @brief Tooth 14 - first unusual spacing tooth */
constexpr uint8_t UNUSUAL_TOOTH_1 = 14U;

/** @brief Tooth 22 - second unusual spacing tooth */
constexpr uint8_t UNUSUAL_TOOTH_2 = 22U;

/** @brief Tooth 15 - first tooth after first missing tooth */
constexpr uint8_t AFTER_MISSING_1 = 15U;

/** @brief Tooth 23 - first tooth after second missing tooth */
constexpr uint8_t AFTER_MISSING_2 = 23U;

/** @brief Tooth 16 - sync position (first in string of 7) */
constexpr uint8_t SYNC_TOOTH = 16U;

/** @brief Number of teeth between gaps during sync */
constexpr uint8_t TEETH_BETWEEN_GAPS = 15U;

/** @brief Gap validation multiplier (1.5x = 3/2) */
constexpr uint8_t GAP_MULTIPLIER_NUM = 3U;
constexpr uint8_t GAP_MULTIPLIER_DEN = 2U;

// ============================================================================
// TOOTH ANGLE LOOKUP TABLE (FASE V)
// ============================================================================

/**
 * @brief Base angle for each tooth position (1-24)
 * @details Teeth 14 and 22 have 18deg spacing instead of normal 15deg
 * Index 0 unused, teeth 1-24 have pre-calculated angles
 */
static const uint16_t hondaJ32ToothAngles[25] PROGMEM = {
    0,    // tooth 0 (unused)
    15,   // tooth 1: 1 * 15 = 15
    30,   // tooth 2: 2 * 15 = 30
    45,   // tooth 3: 3 * 15 = 45
    60,   // tooth 4: 4 * 15 = 60
    75,   // tooth 5: 5 * 15 = 75
    90,   // tooth 6: 6 * 15 = 90
    105,  // tooth 7: 7 * 15 = 105
    120,  // tooth 8: 8 * 15 = 120
    135,  // tooth 9: 9 * 15 = 135
    150,  // tooth 10: 10 * 15 = 150
    165,  // tooth 11: 11 * 15 = 165
    180,  // tooth 12: 12 * 15 = 180
    195,  // tooth 13: 13 * 15 = 195
    213,  // tooth 14: 13*15 + 18 = 213 (UNUSUAL 18deg spacing)
    228,  // tooth 15: 213 + 15 = 228
    243,  // tooth 16: 228 + 15 = 243
    258,  // tooth 17: 243 + 15 = 258
    273,  // tooth 18: 258 + 15 = 273
    288,  // tooth 19: 273 + 15 = 288
    303,  // tooth 20: 288 + 15 = 303
    318,  // tooth 21: 303 + 15 = 318
    333,  // tooth 22: 21*15 + 18 = 333 (UNUSUAL 18deg spacing)
    348,  // tooth 23: 333 + 15 = 348
    363   // tooth 24: 348 + 15 = 363
};

// ============================================================================
// HELPER FUNCTIONS - Tooth Classification (FASE U)
// ============================================================================

/**
 * @brief Check if tooth has unusual spacing (18deg vs 15deg)
 * @details Teeth 14 and 22 are wider and should not update lastGap
 * @param tooth Tooth number to check
 * @return true if tooth 14 or 22
 * @complexity 1
 */
static inline bool isUnusualSpacingTooth_HondaJ32(uint8_t tooth) {
    return (tooth == UNUSUAL_TOOTH_1 || tooth == UNUSUAL_TOOTH_2);
}

/**
 * @brief Check if tooth is first after missing tooth
 * @details Teeth 15 and 23 follow the two missing teeth in the pattern
 * @param tooth Tooth number to check
 * @return true if tooth 15 or 23
 * @complexity 1
 */
static inline bool isAfterMissingTooth_HondaJ32(uint8_t tooth) {
    return (tooth == AFTER_MISSING_2 || tooth == AFTER_MISSING_1);
}

/**
 * @brief Validate that gap is actually big (at least 1.5x last gap)
 * @details Used to verify we're at a missing tooth, not a noise spike
 * @param curGap Current gap time
 * @param lastGap Previous gap time
 * @return true if gap is 1.5x or larger than last gap
 * @complexity 1
 */
static inline bool isBigGapValid_HondaJ32(uint16_t curGap, uint16_t lastGap) {
    return (curGap >= ((lastGap >> 1) * GAP_MULTIPLIER_NUM));
}

/**
 * @brief Achieve sync when we detect we're at tooth 16
 * @details Initializes tooth1 times based on known position and gap width
 * @param curTime Current time in microseconds
 * @param lastGap Last gap time
 * @complexity 2
 */
static inline void achieveSync_HondaJ32(uint16_t curTime, uint16_t lastGap) {
    currentStatus.hasSync = true;
    toothCurrentCount = SYNC_TOOTH;  // First tooth in string of 7 after second gap
    toothOneTime = curTime - (TEETH_BETWEEN_GAPS * lastGap);  // Calculate tooth 1 time backwards
    toothOneMinusOneTime = toothOneTime - (NOMINAL_TEETH_COUNT * lastGap);  // Full revolution back
}

/**
 * @brief Get base angle for tooth count using lookup table
 * @details Handles unusual spacing of teeth 14 and 22
 * @param toothCount Current tooth count (1-24)
 * @return Base angle in degrees
 * @complexity 1
 */
static inline int getBaseAngle_HondaJ32(uint16_t toothCount) {
    // Safety: invalid tooth count
    if (toothCount >= TEETH_WITH_ROLLOVER) {
        return 0;
    }
    return pgm_read_word(&hondaJ32ToothAngles[toothCount]);
}

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing
// ============================================================================

/**
 * @brief Handle revolution boundary (tooth 25 rollover)
 * @details Updates timing and revolution counter
 * @complexity 2
 */
static inline void handleRevolutionBoundary(void) {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
    SetRevolutionTime(toothOneTime - toothOneMinusOneTime);
}

/**
 * @brief Process tooth after missing tooth when synced
 * @details Validates gap and handles sync loss
 * @return true if sync maintained
 * @complexity 2
 */
static inline bool processAfterMissingToothSynced(void) {
    // Account for missing tooth
    toothCurrentCount++;

    // Gap should be big, if not we lost sync
    if (!isBigGapValid_HondaJ32(curGap, lastGap)) {
        currentStatus.hasSync = false;
        toothCurrentCount = 1;
        return false;
    }

    return true;
}

/**
 * @brief Process tooth during sync acquisition
 * @details Counts teeth between gaps to identify position
 * @complexity 3
 */
static inline void processSyncAcquisition(void) {
    // Regular tooth or startup
    if (!isBigGapValid_HondaJ32(curGap, lastGap) || lastGap == 0) {
        toothCurrentCount++;  // Increment teeth between gaps
        lastGap = curGap;
        return;
    }

    // First tooth after missing tooth
    if (toothCurrentCount == TEETH_BETWEEN_GAPS) {
        // 15 teeth since gap before this, so we just passed second gap
        achieveSync_HondaJ32(curTime, lastGap);
    } else {
        // Unclear which gap we just passed, reset counter
        toothCurrentCount = 1;
    }
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup HondaJ32 decoder
 * @details Initializes decoder state and filter times
 *
 * PRESERVES: decoders.cpp lines 2407-2423
 *
 * @complexity 2
 */
void triggerSetup_HondaJ32(void)
{
    // 360 degrees / 24 teeth = 15 degrees per tooth
    triggerToothAngle = 360 / NOMINAL_TEETH_COUNT;

    // Calculate max stall time (minimum 50 RPM)
    // Note: Using /10U instead of /50U in original
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 10U) * triggerToothAngle);

    // Calculate trigger filter time (24 teeth at max RPM)
    triggerFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60 * NOMINAL_TEETH_COUNT));

    // Initialize all timing variables to zero
    toothLastToothTime = 0;
    toothCurrentCount = 0;
    toothOneTime = 0;
    toothOneMinusOneTime = 0;
    lastGap = 0;
    revolutionOne = 0;

    // Set decoder flags (non-sequential, no secondary)
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes 24 nominal teeth with complex pattern
 *
 * PRESERVES: decoders.cpp lines 2459-2512
 * ORIGINAL: 54 lines, complexity ~8
 * REFACTORED: 45 lines, complexity 6, uses helper functions
 *
 * @complexity 6
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_HondaJ32(void)
{
    // Called only on rising edges (as we lose sight of tooth)
    curTime = micros();
    curGap = curTime - toothLastToothTime;
    toothLastToothTime = curTime;

    // Flag valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Process based on sync status
    if (currentStatus.hasSync == true) {
        // We have sync
        toothCurrentCount++;

        // Handle rollover (normal sized tooth here)
        if (toothCurrentCount == TEETH_WITH_ROLLOVER) {
            handleRevolutionBoundary();
            return;
        }

        // First tooth after a missing tooth
        if (isAfterMissingTooth_HondaJ32(toothCurrentCount)) {
            processAfterMissingToothSynced();
            return;
        }

        // Update lastGap for normal teeth (not teeth 14 or 22)
        if (!isUnusualSpacingTooth_HondaJ32(toothCurrentCount)) {
            lastGap = curGap;
        }
        // else: teeth 14 or 22, take no further action
    } else {
        // We do not have sync yet
        // While syncing, treat teeth 14 and 22 as normal teeth
        processSyncAcquisition();
    }
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Not implemented for HondaJ32
 *
 * PRESERVES: decoders.cpp lines 2515-2518
 * Note: No compelling reason to implement cam timing on J32
 * (Semi-sequential injection, wasted spark, no VTC, just VTEC)
 *
 * @complexity 1
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_HondaJ32(void)
{
    // No cam timing implementation needed for J32
    return;
}

/**
 * @brief Get current RPM
 * @details Uses revolution time for accurate RPM
 *
 * PRESERVES: decoders.cpp lines 2520-2523
 *
 * @return RPM value
 * @complexity 1
 */
uint16_t getRPM_HondaJ32(void)
{
    // revolutionTime set by SetRevolutionTime() in triggerPri
    return RpmFromRevolutionTimeUs(revolutionTime);
}

/**
 * @brief Get current crank angle
 * @details Uses lookup table for unusual tooth spacing
 *
 * PRESERVES: decoders.cpp lines 2566-2588
 * Uses FASE V tooth angle lookup table
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 3
 */
int getCrankAngle_HondaJ32(void)
{
    int crankAngle;
    uint16_t tempToothCurrentCount;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    lastCrankAngleCalc = micros();
    elapsedTime = lastCrankAngleCalc - toothLastToothTime;
    interrupts();

    // Use lookup table for base angle (handles unusual teeth 14 and 22)
    crankAngle = getBaseAngle_HondaJ32(tempToothCurrentCount);

    // Add interpolation and trigger offset
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime) + configPage4.triggerAngle;

    // Wrap to valid range
    if (crankAngle >= 720) {
        crankAngle -= 720;
    }
    if (crankAngle > CRANK_ANGLE_MAX) {
        crankAngle -= CRANK_ANGLE_MAX;
    }
    if (crankAngle < 0) {
        crankAngle += 360;
    }

    return crankAngle;
}

/**
 * @brief Set end teeth for ignition scheduling
 * @details Not required for HondaJ32 (empty implementation)
 *
 * PRESERVES: decoders.cpp lines 2590-2593
 *
 * @complexity 1
 */
void triggerSetEndTeeth_HondaJ32(void)
{
    // No end teeth calculation required for HondaJ32
    return;
}
