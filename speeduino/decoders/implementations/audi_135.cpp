/**
 * @file audi_135.cpp
 * @brief Audi 135 decoder implementation
 * @details REFACTORED from decoders.cpp (lines 2179-2291)
 *
 * Pattern: 135 teeth on crank + 1 tooth on cam
 * - Only every 3rd crank tooth used (effective 45 teeth)
 * - 8 degrees per effective tooth
 * - Cam signal provides sequential sync
 * - Sequential mode
 *
 * ORIGINAL: 113 lines
 * REFACTORED: Modular functions with guard clauses
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

/** @brief Physical teeth on crank wheel */
constexpr uint8_t PHYSICAL_TEETH_COUNT = 135U;

/** @brief Effective teeth (every 3rd physical tooth) */
constexpr uint8_t EFFECTIVE_TEETH_COUNT = 45U;

/** @brief Tooth angle in degrees (360 / 45 = 8) */
constexpr uint8_t TOOTH_ANGLE_DEG = 8U;

/** @brief Count every 3rd physical tooth */
constexpr uint8_t TOOTH_SYSTEM_DIVISOR = 3U;

/** @brief Cam teeth for secondary filter */
constexpr uint8_t CAM_TEETH_COUNT = 2U;

/** @brief Revolution startup threshold */
constexpr uint8_t REVOLUTION_STARTUP_THRESHOLD = 100U;

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing
// ============================================================================

/**
 * @brief Handle revolution boundary (tooth 1 or overflow)
 * @details Resets counters and updates timing
 * @complexity 3
 */
static inline void handleRevolutionBoundary(void) {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    revolutionOne = !revolutionOne;
    currentStatus.startRevolutions++;
}

/**
 * @brief Process every 3rd physical tooth
 * @details Main tooth processing logic after filter and sync
 * @complexity 4
 */
static inline void processEvery3rdTooth(void) {
    // Flag valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Update system timing
    toothSystemLastToothTime = curTime;
    toothSystemCount = 0;

    // Increment effective tooth counter
    toothCurrentCount++;

    // Check for revolution boundary
    if ((toothCurrentCount == 1) || (toothCurrentCount > EFFECTIVE_TEETH_COUNT)) {
        handleRevolutionBoundary();
    }

    // Update adaptive filter
    setFilter(curGap);

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

/**
 * @brief Handle resync conditions
 * @details Processes different resync scenarios based on configuration
 * @complexity 3
 */
static inline void handleResyncConditions(void) {
    // Forced resync if enabled
    if (configPage4.useResync == 1) {
        toothCurrentCount = 0;
        toothSystemCount = TOOTH_SYSTEM_DIVISOR;
        return;
    }

    // Resync during startup if tooth count incorrect
    if ((currentStatus.startRevolutions < REVOLUTION_STARTUP_THRESHOLD) &&
        (toothCurrentCount != EFFECTIVE_TEETH_COUNT)) {
        toothCurrentCount = 0;
    }
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup Audi135 decoder
 * @details Initializes decoder state and filter times
 *
 * PRESERVES: decoders.cpp lines 2179-2191
 *
 * @complexity 3
 */
void triggerSetup_Audi135(void)
{
    // 135/3 = 45 effective teeth, 360/45 = 8 degrees per effective tooth
    triggerToothAngle = TOOTH_ANGLE_DEG;

    // Initialize counters
    toothCurrentCount = UINT8_MAX; // Default value
    toothSystemCount = 0;

    // Calculate filter time based on physical teeth (shortest possible time at max RPM)
    triggerFilterTime = (unsigned long)(MICROS_PER_SEC / (MAX_RPM / 60U * PHYSICAL_TEETH_COUNT));

    // Calculate secondary filter time for cam signal (2 teeth at cam speed)
    triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * CAM_TEETH_COUNT)) / 2U;

    // Calculate max stall time (minimum 50 RPM)
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle);

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes every 3rd physical tooth (45 effective teeth)
 *
 * PRESERVES: decoders.cpp lines 2193-2229
 * ORIGINAL: 37 lines, complexity ~7
 * REFACTORED: 30 lines, complexity 5, uses helper functions
 *
 * @complexity 5
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_Audi135(void)
{
    curTime = micros();
    curGap = curTime - toothSystemLastToothTime;

    // Guard clause: filter out noise (except during startup)
    if ((curGap <= triggerFilterTime) && (currentStatus.startRevolutions != 0)) {
        return;
    }

    // Increment physical tooth counter
    toothSystemCount++;

    // Guard clause: no sync yet
    if (currentStatus.hasSync == false) {
        toothLastToothTime = curTime;
        return;
    }

    // Only process every 3rd physical tooth
    if (toothSystemCount >= TOOTH_SYSTEM_DIVISOR) {
        processEvery3rdTooth();
    }
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Cam signal provides sync and revolution reset
 *
 * PRESERVES: decoders.cpp lines 2231-2249
 * ORIGINAL: 19 lines (with commented filter code)
 * REFACTORED: 16 lines, complexity 3, uses helper function
 *
 * @complexity 3
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_Audi135(void)
{
    // Handle initial sync
    if (currentStatus.hasSync == false) {
        toothCurrentCount = 0;
        currentStatus.hasSync = true;
        toothSystemCount = TOOTH_SYSTEM_DIVISOR; // Next primary tooth will be counted
        revolutionOne = 1; // Sequential revolution reset
        return;
    }

    // Handle resync conditions
    handleResyncConditions();

    // Sequential revolution reset
    revolutionOne = 1;
}

/**
 * @brief Get current RPM
 * @details Uses standard RPM calculation at crank speed
 *
 * PRESERVES: decoders.cpp lines 2251-2254
 *
 * @return RPM value
 * @complexity 1
 */
uint16_t getRPM_Audi135(void)
{
    return stdGetRPM(CRANK_SPEED);
}

/**
 * @brief Get current crank angle
 * @details Calculates angle from tooth position + interpolation
 *
 * PRESERVES: decoders.cpp lines 2256-2286
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 4
 */
int getCrankAngle_Audi135(void)
{
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;
    bool tempRevolutionOne;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    tempRevolutionOne = revolutionOne;
    lastCrankAngleCalc = micros();
    interrupts();

    // Handle case where secondary tooth was last seen
    if (tempToothCurrentCount == 0) {
        tempToothCurrentCount = EFFECTIVE_TEETH_COUNT;
    }

    // Calculate base angle from tooth position
    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;

    // Add interpolation (degrees since last tooth)
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    // Sequential check (add 360 if on second revolution)
    if (tempRevolutionOne) {
        crankAngle += 360;
    }

    // Wrap to valid range
    if (crankAngle >= 720) {
        crankAngle -= 720;
    }
    if (crankAngle < 0) {
        crankAngle += CRANK_ANGLE_MAX;
    }

    return crankAngle;
}

/**
 * @brief Set end teeth for ignition scheduling
 * @details Not required for Audi135 (empty implementation)
 *
 * PRESERVES: decoders.cpp lines 2288-2290
 *
 * @complexity 1
 */
void triggerSetEndTeeth_Audi135(void)
{
    // No end teeth calculation required for Audi135
}
