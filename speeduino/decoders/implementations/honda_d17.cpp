/**
 * @file honda_d17.cpp
 * @brief Honda D17 decoder implementation
 * @details REFACTORED from decoders.cpp (lines 2297-2387)
 *
 * Pattern: 12 + 1 teeth (13th tooth at half spacing)
 * - 1.7 liter 4 cylinder SOHC
 * - 30 degrees per tooth (360/12)
 * - 13th tooth detected by half-gap pattern
 * - Non-sequential mode
 *
 * ORIGINAL: 91 lines
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

/** @brief Number of regular teeth (excluding 13th magic tooth) */
constexpr uint8_t REGULAR_TEETH_COUNT = 12U;

/** @brief Total tooth count including 13th tooth */
constexpr uint8_t TOTAL_TEETH_COUNT = 13U;

/** @brief Divisor for half-gap detection */
constexpr uint8_t HALF_GAP_DIVISOR = 1U;

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing
// ============================================================================

/**
 * @brief Handle tooth 1 (revolution boundary)
 * @details Updates revolution timing
 * @complexity 2
 */
static inline void handleTooth1(void) {
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;

    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

/**
 * @brief Detect 13th tooth by gap pattern
 * @details 13th tooth has half the gap of regular teeth
 * @return true if 13th tooth detected
 * @complexity 2
 */
static inline bool detect13thTooth(void) {
    // Target gap is half of last gap
    unsigned long targetGap = lastGap >> HALF_GAP_DIVISOR;

    // If current gap is less than half of previous gap, we're at 13th tooth
    return (curGap < targetGap);
}

/**
 * @brief Handle regular tooth (not tooth 1 or 13)
 * @details Updates timing for regular teeth
 * @complexity 1
 */
static inline void handleRegularTooth(void) {
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup HondaD17 decoder
 * @details Initializes decoder state
 *
 * PRESERVES: decoders.cpp lines 2297-2304
 *
 * @complexity 2
 */
void triggerSetup_HondaD17(void)
{
    // 360 degrees / 12 teeth = 30 degrees per tooth
    triggerToothAngle = 360 / REGULAR_TEETH_COUNT;

    // Calculate max stall time (minimum 50 RPM)
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle);

    // Set decoder flags (non-sequential, no secondary)
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes 12 regular teeth + 13th tooth with half-gap
 *
 * PRESERVES: decoders.cpp lines 2306-2346
 * ORIGINAL: 41 lines, complexity ~6
 * REFACTORED: 38 lines, complexity 5, uses helper functions
 *
 * @complexity 5
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_HondaD17(void)
{
    // Update gap tracking
    lastGap = curGap;
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Increment tooth counter
    toothCurrentCount++;

    // Flag valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Handle tooth 13 overflow when synced
    if ((toothCurrentCount == TOTAL_TEETH_COUNT) && (currentStatus.hasSync == true)) {
        toothCurrentCount = 0;
        return;
    }

    // Handle tooth 1 when synced
    if ((toothCurrentCount == 1) && (currentStatus.hasSync == true)) {
        handleTooth1();
        return;
    }

    // Detect 13th tooth and achieve sync
    if (detect13thTooth()) {
        toothCurrentCount = 0;
        currentStatus.hasSync = true;
        return;
    }

    // Regular tooth processing
    // Note: 13th tooth timing is not recorded
    handleRegularTooth();
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Not implemented for HondaD17 (4+1 cam signal not supported)
 *
 * PRESERVES: decoders.cpp line 2347
 *
 * @complexity 1
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_HondaD17(void)
{
    // The 4+1 signal on the cam is not yet supported
    // If this ever changes, update BIT_DECODER_HAS_SECONDARY in setup
    return;
}

/**
 * @brief Get current RPM
 * @details Uses standard RPM calculation at crank speed
 *
 * PRESERVES: decoders.cpp lines 2348-2351
 *
 * @return RPM value
 * @complexity 1
 */
uint16_t getRPM_HondaD17(void)
{
    return stdGetRPM(CRANK_SPEED);
}

/**
 * @brief Get current crank angle
 * @details Calculates angle from tooth position + interpolation
 *
 * PRESERVES: decoders.cpp lines 2352-2383
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 4
 */
int getCrankAngle_HondaD17(void)
{
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    int crankAngle;

    // Special case: last tooth seen was the 13th tooth (tooth 0)
    // Use 12th tooth as reference instead
    if (tempToothCurrentCount == 0) {
        crankAngle = (11 * triggerToothAngle) + configPage4.triggerAngle;
    } else {
        // Regular tooth: calculate from tooth position
        crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) + configPage4.triggerAngle;
    }

    // Add interpolation (degrees since last tooth)
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
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
 * @details Not required for HondaD17 (empty implementation)
 *
 * PRESERVES: decoders.cpp lines 2385-2387
 *
 * @complexity 1
 */
void triggerSetEndTeeth_HondaD17(void)
{
    // No end teeth calculation required for HondaD17
}
