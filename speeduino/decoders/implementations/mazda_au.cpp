/**
 * @file mazda_au.cpp
 * @brief Mazda AU decoder implementation
 * @details REFACTORED from decoders.cpp (lines 2902-3043)
 *
 * Pattern: 4 crank teeth + cam tooth
 * - Alternating 72° and 108° spacing
 * - Tooth #2 defined after single cam tooth
 * - Tooth #1 at 348° ATDC
 * - Sequential mode
 *
 * ORIGINAL: 142 lines
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

/** @brief Maximum tooth angle (108 degrees) */
constexpr uint8_t MAX_TOOTH_ANGLE_DEG = 108U;

/** @brief Small tooth angle (72 degrees) */
constexpr uint8_t SMALL_TOOTH_ANGLE_DEG = 72U;

/** @brief Tooth count indicating no sync */
constexpr uint8_t NO_SYNC_TOOTH_COUNT = 99U;

/** @brief Initial filter time in microseconds */
constexpr uint16_t INITIAL_FILTER_TIME_US = 1500U;

/** @brief Secondary tooth count for sync detection */
constexpr uint8_t SYNC_SECONDARY_COUNT = 2U;

/** @brief Total crank teeth per revolution */
constexpr uint8_t TOTAL_CRANK_TEETH = 4U;

/** @brief Cam teeth for secondary filter */
constexpr uint8_t CAM_TEETH_COUNT = 2U;

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing
// ============================================================================

/**
 * @brief Handle tooth 1 or 5 (revolution boundary)
 * @details Resets counters and updates timing
 * @complexity 2
 */
static inline void handleRevolutionBoundary(void) {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.hasSync = true;
    currentStatus.startRevolutions++;
}

/**
 * @brief Handle fixed cranking timing
 * @details Ends coil charge at specific teeth
 * @param toothCount Current tooth count
 * @complexity 2
 */
static inline void handleFixedCrankTiming(uint8_t toothCount) {
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && configPage4.ignCranklock) {
        if (toothCount == 1) {
            endCoil1Charge();
        } else if (toothCount == 3) {
            endCoil2Charge();
        }
    }
}

/**
 * @brief Update trigger filter based on tooth position
 * @details Adjusts angle and filter for uneven tooth spacing
 * @param toothCount Current tooth count
 * @complexity 2
 */
static inline void updateTriggerFilter(uint8_t toothCount) {
    if ((toothCount == 1) || (toothCount == 3)) {
        // 72 degree gap, next trigger is 108 degrees away
        triggerToothAngle = SMALL_TOOTH_ANGLE_DEG;
        triggerFilterTime = curGap;
    } else {
        // 108 degree gap, next trigger is 72 degrees away
        triggerToothAngle = MAX_TOOTH_ANGLE_DEG;
        triggerFilterTime = rshift<3>(curGap * 3UL); // (108*3)/8=40 degrees
    }
}

/**
 * @brief Detect sync from cam signal
 * @details Looks for close-together teeth pattern
 * @return true if sync achieved
 * @complexity 3
 */
static inline bool detectSync(void) {
    if (secondaryToothCount == SYNC_SECONDARY_COUNT) {
        toothCurrentCount = 1;
        currentStatus.hasSync = true;
        return true;
    }

    // Look for 2 teeth close together
    triggerFilterTime = INITIAL_FILTER_TIME_US;
    unsigned long targetGap = lastGap >> 1; // Half of last gap

    // If current gap < half of previous gap, we're at extra cam tooth
    // This tooth at 421 crank degrees, so last crank tooth was #1 (at 350°)
    if (curGap2 < targetGap) {
        secondaryToothCount = SYNC_SECONDARY_COUNT;
    }

    return false;
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup MazdaAU decoder
 * @details Initializes decoder state and tooth angles
 *
 * PRESERVES: decoders.cpp lines 2902-2920
 *
 * @complexity 2
 */
void triggerSetup_MazdaAU(void)
{
    // Maximum gap is 108 degrees
    triggerToothAngle = MAX_TOOTH_ANGLE_DEG;
    toothCurrentCount = NO_SYNC_TOOTH_COUNT; // No sync initially
    secondaryToothCount = 0;

    // Set tooth angles
    toothAngles[0] = 348; // tooth #1
    toothAngles[1] = 96;  // tooth #2
    toothAngles[2] = 168; // tooth #3
    toothAngles[3] = 276; // tooth #4

    // Calculate timing parameters
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle);
    triggerFilterTime = INITIAL_FILTER_TIME_US; // 10000 rpm
    triggerSecFilterTime = (int)(MICROS_PER_SEC / (MAX_RPM / 60U * CAM_TEETH_COUNT)) / 2U;

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes 4 crank teeth with uneven spacing
 *
 * PRESERVES: decoders.cpp lines 2922-2957
 * ORIGINAL: 36 lines, complexity ~5
 * REFACTORED: 26 lines, complexity 4, uses helper functions
 *
 * @complexity 4
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_MazdaAU(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Guard clause: filter check
    if (curGap < triggerFilterTime) {
        return;
    }

    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    toothCurrentCount++;

    // Handle revolution boundary (tooth 1 or 5)
    // Trigger on CHANGE, so 4 pulses = 1 crank rev
    if ((toothCurrentCount == 1) || (toothCurrentCount == 5)) {
        handleRevolutionBoundary();
    }

    // Process synced operations
    if (currentStatus.hasSync == true) {
        // Handle fixed cranking timing (locked at 12° BTDC)
        handleFixedCrankTiming(toothCurrentCount);

        // Update filter for uneven tooth pattern
        updateTriggerFilter(toothCurrentCount);

        // Update timing history
        toothLastMinusOneToothTime = toothLastToothTime;
        toothLastToothTime = curTime;
    }
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Processes cam signal for sync detection
 *
 * PRESERVES: decoders.cpp lines 2959-2988
 *
 * @complexity 3
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_MazdaAU(void)
{
    curTime2 = micros();
    lastGap = curGap2;
    curGap2 = curTime2 - toothLastSecToothTime;
    toothLastSecToothTime = curTime2;

    // Only look for sync when not synced
    if (currentStatus.hasSync == false) {
        detectSync();
        secondaryToothCount++;
    }
}

/**
 * @brief Get current RPM
 * @details Special handling for uneven tooth spacing during cranking
 *
 * PRESERVES: decoders.cpp lines 2991-3011
 *
 * @return RPM value
 * @complexity 3
 */
uint16_t getRPM_MazdaAU(void)
{
    uint16_t tempRPM = 0;

    if (currentStatus.hasSync == true) {
        // During cranking: special calculation for uneven teeth
        if (currentStatus.RPM < currentStatus.crankRPM) {
            int tempToothAngle;

            noInterrupts();
            tempToothAngle = triggerToothAngle;
            // Tooth angle changes between 72 and 108 degrees
            SetRevolutionTime(36 * (toothLastToothTime - toothLastMinusOneToothTime));
            interrupts();

            tempRPM = (tempToothAngle * MICROS_PER_MIN) / revolutionTime;
        } else {
            // Normal operation: standard RPM at crank speed
            tempRPM = stdGetRPM(CRANK_SPEED);
        }
    }

    return tempRPM;
}

/**
 * @brief Get current crank angle
 * @details Calculates angle from tooth position + interpolation
 *
 * PRESERVES: decoders.cpp lines 3013-3039
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 3
 */
int getCrankAngle_MazdaAU(void)
{
    int crankAngle = 0;

    if (currentStatus.hasSync == true) {
        unsigned long tempToothLastToothTime;
        int tempToothCurrentCount;

        // Atomic read of ISR variables
        noInterrupts();
        tempToothCurrentCount = toothCurrentCount;
        tempToothLastToothTime = toothLastToothTime;
        lastCrankAngleCalc = micros();
        interrupts();

        // Lookup fixed tooth angle
        crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;

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
    }

    return crankAngle;
}

/**
 * @brief Set end teeth for ignition scheduling
 * @details Not required for MazdaAU (empty implementation)
 *
 * PRESERVES: decoders.cpp lines 3041-3043
 *
 * @complexity 1
 */
void triggerSetEndTeeth_MazdaAU(void)
{
    // No end teeth calculation required for MazdaAU
}
