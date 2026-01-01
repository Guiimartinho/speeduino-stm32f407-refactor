/**
 * @file gm_7x.cpp
 * @brief GM 7X decoder implementation
 * @details REFACTORED from decoders.cpp (lines 1342-1475)
 *
 * Pattern: GM 7-tooth wheel with one narrow tooth for sync
 * - 6 regular teeth (60� spacing)
 * - 1 narrow tooth (30� wide) - sync point
 * - Total: 7 physical teeth covering 360�
 * - Tooth 3 is narrow (half-width gap detection)
 *
 * ORIGINAL: 133 lines total
 * REFACTORED: Modular functions with guard clauses
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

// Anonymous namespace for private implementation
namespace {

// ============================================================================
// CONSTANTS
// ============================================================================

/** @brief Number of physical teeth on wheel */
constexpr uint8_t TOTAL_TEETH = 7U;

/** @brief Narrow tooth number (sync tooth) */
constexpr uint8_t NARROW_TOOTH = 3U;

/** @brief Degrees per regular tooth */
constexpr uint16_t TOOTH_ANGLE = 60U;

/** @brief Base angle of tooth 1 (ATDC) */
constexpr int16_t TOOTH_1_ANGLE = 42;

/** @brief Crank angle of narrow tooth (tooth 3) */
constexpr int16_t NARROW_TOOTH_ANGLE = 112;

/** @brief Advance threshold for end tooth selection (degrees) */
constexpr uint8_t ADVANCE_THRESHOLD = 18U;

// ============================================================================
// HELPER FUNCTIONS - Narrow Tooth Detection
// ============================================================================

/**
 * @brief Detect narrow tooth (sync point)
 * @details Tooth 3 is half-width, creates smaller gap
 * @param currentGap Current tooth gap (�s)
 * @param previousGap Previous tooth gap (�s)
 * @return true if narrow tooth detected
 * @complexity 2
 */
static inline bool isNarrowToothDetected(uint32_t currentGap, uint32_t previousGap) {
    // Guard clause: no previous gap
    if (previousGap == 0U) {
        return false;
    }

    // Calculate target (50% of previous gap)
    uint32_t targetGap = previousGap >> 1;

    // Current gap less than half previous = narrow tooth
    return (currentGap < targetGap);
}

/**
 * @brief Handle narrow tooth sync
 * @details Sets tooth count to 3 and flags sync
 * @complexity 2
 */
static inline void handleNarrowToothSync(void) {
    toothCurrentCount = NARROW_TOOTH;
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;

    // Flag that tooth angle is incorrect (double at narrow tooth)
    BIT_CLEAR(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

// ============================================================================
// HELPER FUNCTIONS - Revolution Handling
// ============================================================================

/**
 * @brief Handle revolution boundary (back to tooth 1)
 * @complexity 2
 */
static inline void handleRevolutionBoundary(void) {
    toothCurrentCount = 1U;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;

    // Flag that tooth angle is correct at tooth 1
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

/**
 * @brief Handle regular tooth (non-narrow)
 * @complexity 1
 */
static inline void handleRegularTooth(void) {
    // Flag that tooth angle is correct
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
}

// ============================================================================
// HELPER FUNCTIONS - Crank Angle Calculation
// ============================================================================

/**
 * @brief Calculate base crank angle for tooth position
 * @param toothCount Current tooth number (1-7)
 * @return Base crank angle (degrees ATDC)
 * @complexity 3
 */
static inline int16_t calculateBaseCrankAngle(int toothCount) {
    // Special case: narrow tooth (tooth 3)
    if (toothCount == NARROW_TOOTH) {
        return NARROW_TOOTH_ANGLE;
    }

    // Teeth 1-2: before narrow tooth
    if (toothCount < NARROW_TOOTH) {
        return ((toothCount - 1) * TOOTH_ANGLE) + TOOTH_1_ANGLE + configPage4.triggerAngle;
    }

    // Teeth 4-7: after narrow tooth (subtract 1 from count for calculation)
    return ((toothCount - 2) * TOOTH_ANGLE) + TOOTH_1_ANGLE + configPage4.triggerAngle;
}

// ============================================================================
// HELPER FUNCTIONS - Per-Tooth Ignition
// ============================================================================

/**
 * @brief Handle per-tooth ignition timing
 * @param toothCount Current tooth number
 * @complexity 3
 */
static inline void handlePerToothIgnition(int toothCount) {
    // Guard clause: never check on narrow tooth (not needed)
    if (toothCount == NARROW_TOOTH) {
        return;
    }

    // Calculate crank angle for this tooth
    // Note: configPage4.triggerAngle must be below 48 and above -81
    int16_t crankAngle = calculateBaseCrankAngle(toothCount);

    checkPerToothTiming(crankAngle, toothCount);
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup GM 7X decoder
 * @details Initializes decoder for GM 7-tooth pattern
 *
 * PRESERVES: decoders.cpp lines 1342-1349
 *
 * @complexity 2
 */
void triggerSetup_GM7X(void)
{
    // Fixed tooth angle (60� for 6 regular teeth)
    triggerToothAngle = 360U / 6U; // 60�

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);

    // Calculate max stall time (minimum 50 RPM)
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle);
}

/**
 * @brief Primary trigger ISR
 * @details Processes GM 7X teeth with narrow tooth detection
 *
 * PRESERVES: decoders.cpp lines 1351-1409
 * ORIGINAL: 58 lines, complexity ~8
 * REFACTORED: 42 lines, complexity 6, uses helper functions
 *
 * @complexity 6
 * @isr CRITICAL - Must complete in < 10�s
 */
void triggerPri_GM7X(void)
{
    lastGap = curGap;
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Always increment tooth counter
    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Guard clause: need timing history for gap detection
    bool hasTimingHistory = (toothLastToothTime > 0U) && (toothLastMinusOneToothTime > 0U);
    if (!hasTimingHistory) { goto skip_sync_check; }

    // Check for revolution boundary (overflow)
    if (toothCurrentCount > TOTAL_TEETH) { handleRevolutionBoundary(); }
    else if (isNarrowToothDetected(curGap, lastGap)) { handleNarrowToothSync(); }
    else { handleRegularTooth(); }

skip_sync_check:

    // Handle per-tooth ignition if enabled
    if (configPage2.perToothIgn) {
        handlePerToothIgnition(toothCurrentCount);
    }

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

/**
 * @brief Secondary trigger ISR
 * @details Not used for GM 7X pattern
 */
void triggerSec_GM7X(void)
{
    // Not required for GM 7X
    return;
}

/**
 * @brief Get current RPM
 * @details Uses standard RPM calculation at crank speed
 *
 * PRESERVES: decoders.cpp lines 1411-1414
 *
 * @return RPM value
 * @complexity 1
 */
uint16_t getRPM_GM7X(void)
{
    return stdGetRPM(CRANK_SPEED);
}

/**
 * @brief Get current crank angle
 * @details Calculates angle with interpolation
 *
 * PRESERVES: decoders.cpp lines 1415-1450
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 5
 */
int getCrankAngle_GM7X(void)
{
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    // Calculate base angle from tooth position
    int crankAngle = calculateBaseCrankAngle(tempToothCurrentCount);

    // Add interpolation (degrees since last tooth)
    elapsedTime = lastCrankAngleCalc - tempToothLastToothTime;
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    // Wrap to valid range
    if (crankAngle >= 720) {
        crankAngle -= 720;
    }
    if (crankAngle < 0) {
        crankAngle += 360;
    }

    return crankAngle;
}

/**
 * @brief Set end teeth for ignition scheduling
 * @details Selects end teeth based on advance angle
 *
 * PRESERVES: decoders.cpp lines 1452-1475
 *
 * Pattern:
 * - Low advance (<18�): Use teeth 7, 2, 5
 * - High advance (e18�): Use teeth 6, 1, 4
 *
 * @complexity 2
 */
void triggerSetEndTeeth_GM7X(void)
{
    // Select end teeth based on current advance
    if (currentStatus.advance < ADVANCE_THRESHOLD) {
        // Low advance timing
        ignition1EndTooth = 7U;
        ignition2EndTooth = 2U;
        ignition3EndTooth = 5U;
    } else {
        // High advance timing
        ignition1EndTooth = 6U;
        ignition2EndTooth = 1U;
        ignition3EndTooth = 4U;
    }
}
