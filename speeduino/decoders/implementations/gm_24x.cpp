/**
 * @file gm_24x.cpp
 * @brief GM 24X decoder implementation
 * @details REFACTORED from decoders.cpp (lines 1954-2065)
 *
 * Pattern: 24 teeth with variable spacing + cam sync
 * - Tooth angles pre-calculated in array
 * - 15 degrees average spacing
 * - Cam signal resets tooth count
 * - Sequential mode supported
 *
 * ORIGINAL: 111 lines
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
// HELPER FUNCTIONS - Tooth Angle Management
// ============================================================================

/**
 * @brief Initialize tooth angles array for 24X pattern
 * @details Sets fixed angles for each of the 24 teeth
 * @complexity 2
 */
static inline void initializeToothAngles(void) {
    toothAngles[0] = 12;
    toothAngles[1] = 18;
    toothAngles[2] = 33;
    toothAngles[3] = 48;
    toothAngles[4] = 63;
    toothAngles[5] = 78;
    toothAngles[6] = 102;
    toothAngles[7] = 108;
    toothAngles[8] = 123;
    toothAngles[9] = 138;
    toothAngles[10] = 162;
    toothAngles[11] = 177;
    toothAngles[12] = 183;
    toothAngles[13] = 198;
    toothAngles[14] = 222;
    toothAngles[15] = 237;
    toothAngles[16] = 252;
    toothAngles[17] = 258;
    toothAngles[18] = 282;
    toothAngles[19] = 288;
    toothAngles[20] = 312;
    toothAngles[21] = 327;
    toothAngles[22] = 342;
    toothAngles[23] = 357;
}

/**
 * @brief Handle tooth 1 (revolution boundary)
 * @details Resets counters and updates timing
 * @complexity 3
 */
static inline void handleTooth1(void) {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    revolutionOne = !revolutionOne;
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;
    triggerToothAngle = 15; // Always 15 degrees for tooth #1
}

/**
 * @brief Handle regular tooth (not tooth 1)
 * @details Increments counter and calculates angle
 * @complexity 2
 */
static inline void handleRegularTooth(void) {
    toothCurrentCount++;
    // Calculate angle from difference between current and previous tooth
    triggerToothAngle = toothAngles[(toothCurrentCount - 1)] - toothAngles[(toothCurrentCount - 2)];
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup 24X decoder
 * @details Initializes tooth angles and decoder state
 *
 * PRESERVES: decoders.cpp lines 1954-1988
 *
 * @complexity 3
 */
void triggerSetup_24X(void)
{
    triggerToothAngle = 15;

    // Initialize all 24 tooth angles
    initializeToothAngles();

    // Calculate max stall time (minimum 50 RPM)
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle);

    // Set startup value (only if not initialized to avoid fuel pump staying on)
    if (currentStatus.initialisationComplete == false) {
        toothCurrentCount = 25;
        toothLastToothTime = micros();
    }

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes each of the 24 crank teeth
 *
 * PRESERVES: decoders.cpp lines 1990-2020
 * ORIGINAL: 30 lines, complexity ~5
 * REFACTORED: 25 lines, complexity 4, uses helper functions
 *
 * @complexity 4
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_24X(void)
{
    // Guard clause: waiting for sync (initial state)
    if (toothCurrentCount == 25) {
        currentStatus.hasSync = false;
        return;
    }

    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Check if at tooth 1 (revolution boundary)
    if (toothCurrentCount == 0) {
        handleTooth1();
    } else {
        handleRegularTooth();
    }

    // Flag valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Update timing
    toothLastToothTime = curTime;
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Cam signal resets tooth count for sync
 *
 * PRESERVES: decoders.cpp lines 2022-2026
 *
 * @complexity 2
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_24X(void)
{
    // Reset to beginning of revolution
    toothCurrentCount = 0;
    revolutionOne = 1;
}

/**
 * @brief Get current RPM
 * @details Uses standard RPM calculation at crank speed
 *
 * PRESERVES: decoders.cpp lines 2028-2031
 *
 * @return RPM value
 * @complexity 1
 */
uint16_t getRPM_24X(void)
{
    return stdGetRPM(CRANK_SPEED);
}

/**
 * @brief Get current crank angle
 * @details Calculates angle from tooth position + interpolation
 *
 * PRESERVES: decoders.cpp lines 2032-2060
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 5
 */
int getCrankAngle_24X(void)
{
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount, tempRevolutionOne;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    tempRevolutionOne = revolutionOne;
    lastCrankAngleCalc = micros();
    interrupts();

    int crankAngle;

    // Special case: cam tooth was last seen (tooth 0)
    if (tempToothCurrentCount == 0) {
        crankAngle = 0 + configPage4.triggerAngle;
    } else {
        // Lookup angle from fixed array
        crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;
    }

    // Add interpolation (degrees since last tooth)
    elapsedTime = (lastCrankAngleCalc - tempToothLastToothTime);
    crankAngle += timeToAngleDegPerMicroSec(elapsedTime);

    // Sequential check (add 360 if on second revolution)
    if (tempRevolutionOne == 1) {
        crankAngle += 360;
    }

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
 * @details Not required for 24X (empty implementation)
 *
 * PRESERVES: decoders.cpp lines 2062-2064
 *
 * @complexity 1
 */
void triggerSetEndTeeth_24X(void)
{
    // No end teeth calculation required for 24X
}
