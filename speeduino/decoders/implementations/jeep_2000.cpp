/**
 * @file jeep_2000.cpp
 * @brief Jeep 2000 decoder implementation
 * @details REFACTORED from decoders.cpp (lines 2075-2171)
 *
 * Pattern: 24 crank teeth in groups of 4 + cam sync
 * - Groups of 4 pulses (each 20° apart)
 * - Each group separated by 60°
 * - Cam signal resets tooth count
 * - Non-sequential mode
 *
 * ORIGINAL: 96 lines
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

/** @brief Largest gap between teeth in degrees */
constexpr uint8_t MAX_TOOTH_GAP_DEG = 60U;

// ============================================================================
// HELPER FUNCTIONS - Tooth Angle Management
// ============================================================================

/**
 * @brief Initialize tooth angles array for Jeep2000
 * @details Sets 12 fixed angles (only need 360 degrees)
 * @complexity 2
 */
static inline void initializeToothAngles(void) {
    toothAngles[0] = 174;
    toothAngles[1] = 194;
    toothAngles[2] = 214;
    toothAngles[3] = 234;
    toothAngles[4] = 294;
    toothAngles[5] = 314;
    toothAngles[6] = 334;
    toothAngles[7] = 354;
    toothAngles[8] = 414;
    toothAngles[9] = 434;
    toothAngles[10] = 454;
    toothAngles[11] = 474;
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
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;
    triggerToothAngle = 60; // Groups are 60 degrees apart
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
 * @brief Setup Jeep2000 decoder
 * @details Initializes tooth angles and decoder state
 *
 * PRESERVES: decoders.cpp lines 2075-2097
 *
 * @complexity 3
 */
void triggerSetup_Jeep2000(void)
{
    triggerToothAngle = 0;

    // Initialize all 12 tooth angles
    initializeToothAngles();

    // Calculate max stall time (minimum 50 RPM, largest gap 60 degrees)
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * MAX_TOOTH_GAP_DEG);

    // Set startup value (only if not initialized)
    if (currentStatus.initialisationComplete == false) {
        toothCurrentCount = 13;
        toothLastToothTime = micros();
    }

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes each of the 12 crank teeth
 *
 * PRESERVES: decoders.cpp lines 2099-2131
 * ORIGINAL: 32 lines, complexity ~6
 * REFACTORED: 28 lines, complexity 5, uses helper functions
 *
 * @complexity 5
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_Jeep2000(void)
{
    // Guard clause: waiting for sync (initial state)
    if (toothCurrentCount == 13) {
        currentStatus.hasSync = false;
        return;
    }

    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Guard clause: filter out noise
    if (curGap < triggerFilterTime) {
        return;
    }

    // Check if at tooth 1 (revolution boundary)
    if (toothCurrentCount == 0) {
        handleTooth1();
    } else {
        handleRegularTooth();
    }

    // Update adaptive filter
    setFilter(curGap);

    // Flag valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Cam signal resets tooth count for sync
 *
 * PRESERVES: decoders.cpp lines 2132-2136
 *
 * @complexity 1
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_Jeep2000(void)
{
    // Reset to beginning of revolution
    toothCurrentCount = 0;
}

/**
 * @brief Get current RPM
 * @details Uses standard RPM calculation at crank speed
 *
 * PRESERVES: decoders.cpp lines 2138-2141
 *
 * @return RPM value
 * @complexity 1
 */
uint16_t getRPM_Jeep2000(void)
{
    return stdGetRPM(CRANK_SPEED);
}

/**
 * @brief Get current crank angle
 * @details Calculates angle from tooth position + interpolation
 *
 * PRESERVES: decoders.cpp lines 2142-2166
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 4
 */
int getCrankAngle_Jeep2000(void)
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

    // Special case: cam tooth was last seen (tooth 0)
    // Use angle 114 (previous crank tooth angle since timing taken on previous tooth)
    if (toothCurrentCount == 0) {
        crankAngle = 114 + configPage4.triggerAngle;
    } else {
        // Lookup angle from fixed array
        crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;
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
 * @details Not required for Jeep2000 (empty implementation)
 *
 * PRESERVES: decoders.cpp lines 2168-2170
 *
 * @complexity 1
 */
void triggerSetEndTeeth_Jeep2000(void)
{
    // No end teeth calculation required for Jeep2000
}
