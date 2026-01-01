/**
 * @file basic_distributor.cpp
 * @brief Basic Distributor decoder implementation
 * @details REFACTORED from decoders.cpp - Function complexity reduced
 *
 * ORIGINAL: Mixed complexity functions, some >50 lines
 * REFACTORED: All functions < 50 lines, complexity < 10
 *
 * Pattern: Simple distributor with N cylinders
 * - One pulse per cylinder firing (e.g., 4-cyl = 4 pulses/rev)
 * - No missing tooth - just regular spacing
 * - Used for traditional distributor-based ignition
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

// ============================================================================
// FILE SCOPE HELPERS (moved from namespace to reduce nesting depth)
// ============================================================================

/**
 * @brief Handle regular tooth increment
 * @details Increments tooth counter with overflow detection
 * @return true if valid increment, false if overflow detected
 */
static inline bool handleToothIncrement(void) {
    if (toothCurrentCount < triggerActualTeeth) {
        toothCurrentCount++;
        return true;
    }
    // Overflow detected - sync loss
    bool hadSync = currentStatus.hasSync;
    if (hadSync) { currentStatus.syncLossCounter++; }
    currentStatus.hasSync = false;
    return false;
}

// Anonymous namespace for private implementation
namespace {

// ============================================================================
// CONSTANTS
// ============================================================================

/** @brief Minimum RPM for low-cylinder engines (1-4 cyl) */
constexpr uint16_t MIN_RPM_LOW_CYL = 90U;

/** @brief Minimum RPM for high-cylinder engines (5+ cyl) */
constexpr uint16_t MIN_RPM_HIGH_CYL = 50U;

/** @brief RPM threshold for cranking detection */
constexpr uint16_t CRANKING_RPM_THRESHOLD = 1500U;

/** @brief Safety margin divisor for trigger filter */
constexpr uint8_t FILTER_SAFETY_MARGIN = 2U;

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing
// ============================================================================

/**
 * @brief Handle tooth counter at revolution boundary
 * @details Resets counter and updates timing when back at tooth 1
 * @complexity 3
 */
static inline void handleRevolutionBoundary(void) {
    toothCurrentCount = 1U;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;
}

// handleToothIncrement moved to file scope above

/**
 * @brief Update adaptive trigger filter
 * @details Updates filter based on current gap or clears if no sync
 * @param gap Current tooth gap (�s)
 * @complexity 2
 */
static inline void updateTriggerFilter(uint32_t gap) {
    if (currentStatus.hasSync) {
        setFilter(gap);
    } else {
        // No sync - ensure filter won't block valid pulses
        triggerFilterTime = 0U;
    }
}

/**
 * @brief Handle ignition coil cranklock
 * @details Ends all coil charges during cranking if cranklock enabled
 * @complexity 2
 */
static inline void handleCranklock(void) {
    // Guard clause: cranklock not enabled or not cranking
    if (!configPage4.ignCranklock || !BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        return;
    }

    // End all coil charges
    endCoil1Charge();
    endCoil2Charge();
    endCoil3Charge();
    endCoil4Charge();
}

/**
 * @brief Handle per-tooth ignition timing
 * @param toothCount Current tooth number
 * @param toothAngle Degrees per tooth
 * @complexity 4
 */
static inline void handlePerToothIgnition(uint16_t toothCount, uint16_t toothAngle) {
    // Calculate crank angle
    int16_t crankAngle = ((toothCount - 1U) * toothAngle) + configPage4.triggerAngle;
    crankAngle = ignitionLimits(crankAngle);

    // Calculate logical tooth number (wraps after half revolution for distributor)
    uint16_t currentTooth = toothCount;
    if (toothCount > (triggerActualTeeth / 2U)) {
        currentTooth = toothCount - (triggerActualTeeth / 2U);
    }

    checkPerToothTiming(crankAngle, currentTooth);
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup basic distributor decoder
 * @details Initializes decoder for simple distributor pattern
 * @complexity 5
 */
void triggerSetup_BasicDistributor(void)
{
    // Calculate actual teeth (one per cylinder)
    triggerActualTeeth = configPage2.nCylinders;
    if (triggerActualTeeth == 0U) {
        triggerActualTeeth = 1U;
    }

    // Calculate degrees per tooth based on stroke type
    if (configPage2.strokes == FOUR_STROKE) {
        triggerToothAngle = 720U / triggerActualTeeth; // 720� / teeth
    } else {
        triggerToothAngle = 360U / triggerActualTeeth; // 360� / teeth (2-stroke)
    }

    // Calculate minimum time between teeth (at MAX_RPM)
    triggerFilterTime = MICROS_PER_MIN / MAX_RPM / configPage2.nCylinders;
    triggerFilterTime = triggerFilterTime / FILTER_SAFETY_MARGIN; // Safety margin
    triggerFilterTime = 0U; // Disabled by default

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_CLEAR(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_CLEAR(decoderState, BIT_DECODER_HAS_SECONDARY);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);

    // Initialize state
    toothCurrentCount = 0U;

    // Calculate max stall time based on cylinder count
    // Low-cylinder engines (1-4): Use 90 RPM minimum (faster stall detection)
    // High-cylinder engines (5+): Use 50 RPM minimum
    if (configPage2.nCylinders <= 4U) {
        MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / MIN_RPM_LOW_CYL) * triggerToothAngle);
    } else {
        MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / MIN_RPM_HIGH_CYL) * triggerToothAngle);
    }
}

/**
 * @brief Primary trigger ISR (REFACTORED)
 * @details Processes distributor teeth
 *
 * ORIGINAL: 55 lines, complexity ~8, moderate nesting
 * REFACTORED: 38 lines, complexity 5, max nesting 2
 *
 * @complexity 5
 * @isr CRITICAL - Must complete in < 10�s
 */
void triggerPri_BasicDistributor(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Guard clause: filter out noise
    if (curGap < triggerFilterTime) {
        return;
    }

    // Update adaptive filter
    updateTriggerFilter(curGap);

    // Check if at revolution boundary (back to tooth 1)
    if ((toothCurrentCount == triggerActualTeeth) || !currentStatus.hasSync) {
        handleRevolutionBoundary();
    } else {
        // Not at boundary - increment tooth counter
        handleToothIncrement();
    }

    // Flag valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Handle cranklock (end coil charges during cranking)
    handleCranklock();

    // Handle per-tooth ignition if enabled
    if (configPage2.perToothIgn) {
        handlePerToothIgnition(toothCurrentCount, triggerToothAngle);
    }

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;
}

/**
 * @brief Secondary trigger ISR
 * @details Not used for basic distributor
 */
void triggerSec_BasicDistributor(void)
{
    // Not required for basic distributor
    return;
}

/**
 * @brief Get current RPM
 * @details Uses cranking or running RPM calculation
 * @return RPM value
 * @complexity 6
 */
uint16_t getRPM_BasicDistributor(void)
{
    uint16_t tempRPM;

    // Determine distributor speed (cam or crank)
    uint8_t distributorSpeed = CAM_SPEED; // Default: cam speed
    if (configPage2.strokes == TWO_STROKE) {
        distributorSpeed = CRANK_SPEED; // 2-stroke: crank speed
    }

    // Use cranking RPM calculation if below threshold
    if ((currentStatus.RPM < currentStatus.crankRPM) ||
        (currentStatus.RPM < CRANKING_RPM_THRESHOLD)) {
        tempRPM = crankingGetRPM(triggerActualTeeth, distributorSpeed);
    } else {
        tempRPM = stdGetRPM(distributorSpeed);
    }

    // Update stall time (twice current revolution time)
    MAX_STALL_TIME = revolutionTime << 1;

    // Special case: 1-cylinder engines (1 pulse per 720�)
    if (triggerActualTeeth == 1U) {
        MAX_STALL_TIME = revolutionTime << 1;
    }

    // Enforce minimum stall time (50 RPM)
    if (MAX_STALL_TIME < 366667UL) {
        MAX_STALL_TIME = 366667UL;
    }

    return tempRPM;
}

/**
 * @brief Get current crank angle
 * @details Calculates angle with interpolation between teeth
 * @return Crank angle (0-719 degrees)
 * @complexity 4
 */
int getCrankAngle_BasicDistributor(void)
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
    int crankAngle = ((tempToothCurrentCount - 1) * triggerToothAngle) +
                     configPage4.triggerAngle;

    // Estimate degrees since last tooth
    elapsedTime = lastCrankAngleCalc - tempToothLastToothTime;
    crankAngle += timeToAngleIntervalTooth(elapsedTime);

    // Wrap angle to valid range
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
 * @details Uses shared table-driven implementation
 * @complexity 3
 */
void triggerSetEndTeeth_BasicDistributor(void)
{
    // Calculate end angle
    int tempEndAngle = ignition1EndAngle - configPage4.triggerAngle;
    tempEndAngle = ignitionLimits(tempEndAngle);

    // Map 3-cylinder to 6-cylinder pattern
    uint8_t cylinders = configPage2.nCylinders;
    if (cylinders == 3U) {
        cylinders = 6U;
    }

    // Use shared data-driven implementation (declared in decoders.h, defined in decoders.cpp)
    setEndTeethFromDistributorConfig(tempEndAngle, cylinders);
}
