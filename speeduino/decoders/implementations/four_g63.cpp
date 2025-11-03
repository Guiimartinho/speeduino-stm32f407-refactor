/**
 * @file four_g63.cpp
 * @brief Mitsubishi 4G63 / 6G72 CAS decoder implementation
 * @details REFACTORED from decoders.cpp (lines 1476-1930)
 *
 * Pattern: 4G63 (4-cyl) or 6G72 (6-cyl) CAS (Crank Angle Sensor)
 * - 4-cylinder: 4 teeth per revolution (70�/110� alternating widths)
 * - 6-cylinder: 6 teeth per revolution (50�/50�/70� pattern)
 * - Cam signal for sync and sequential operation
 * - Trigger on BOTH rising and falling edges
 *
 * ORIGINAL: 454 lines total (largest decoder)
 * REFACTORED: Modular functions with guard clauses and helpers
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

// Reuse table-driven filter config from decoders.cpp
// (Lines 1532-1642 already refactored with data-driven approach)
extern void apply4G63FilterConfig(uint8_t filterLevel, bool isOddTooth, uint8_t nCylinders, uint32_t curGap);

// Anonymous namespace for private implementation
namespace {

// ============================================================================
// CONSTANTS
// ============================================================================

/** @brief Default trigger filter during non-sync (�s) */
constexpr uint16_t DEFAULT_FILTER_NOSYNC = 1500U;

/** @brief Cranking RPM threshold */
constexpr uint16_t CRANKING_RPM_THRESHOLD = 1400U;

/** @brief No sync tooth count marker */
constexpr uint8_t NO_SYNC_TOOTH_COUNT = 99U;

/** @brief Minimum RPM (50 RPM) */
constexpr uint32_t MIN_RPM_STALL_TIME = 366667UL;

// ============================================================================
// HELPER FUNCTIONS - Revolution Handling
// ============================================================================

/**
 * @brief Check if at revolution boundary
 * @param toothCount Current tooth count
 * @param maxTeeth Maximum teeth (8 for 4-cyl, 12 for 6-cyl)
 * @return true if at boundary
 * @complexity 2
 */
static inline bool isAtRevolutionBoundary(uint16_t toothCount, uint16_t maxTeeth) {
    return (toothCount == 1U) || (toothCount > maxTeeth);
}

/**
 * @brief Handle revolution boundary
 * @complexity 2
 */
static inline void handleRevolutionBoundary(void) {
    toothCurrentCount = 1U;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
}

// ============================================================================
// HELPER FUNCTIONS - Cranklock (Forced Spark During Cranking)
// ============================================================================

/**
 * @brief Handle cranklock for 4-cylinder
 * @details Forces wasted spark mode during cranking aligned with crank teeth
 * @param toothCount Current tooth number (1-8)
 * @complexity 3
 */
static inline void handleCranklock4Cyl(uint16_t toothCount) {
    // Teeth 1 or 5: fire coils 1 and 3
    if ((toothCount == 1U) || (toothCount == 5U)) {
        endCoil1Charge();
        endCoil3Charge();
    }
    // Teeth 3 or 7: fire coils 2 and 4
    else if ((toothCount == 3U) || (toothCount == 7U)) {
        endCoil2Charge();
        endCoil4Charge();
    }
}

/**
 * @brief Handle cranklock for 6-cylinder
 * @param toothCount Current tooth number (1-12)
 * @complexity 4
 */
static inline void handleCranklock6Cyl(uint16_t toothCount) {
    // Teeth 1 or 7: fire coil 1
    if ((toothCount == 1U) || (toothCount == 7U)) {
        endCoil1Charge();
    }
    // Teeth 3 or 9: fire coil 2
    else if ((toothCount == 3U) || (toothCount == 9U)) {
        endCoil2Charge();
    }
    // Teeth 5 or 11: fire coil 3
    else if ((toothCount == 5U) || (toothCount == 11U)) {
        endCoil3Charge();
    }
}

/**
 * @brief Handle cranklock if enabled
 * @param toothCount Current tooth number
 * @param nCylinders Number of cylinders (4 or 6)
 * @complexity 5
 */
static inline void handleCranklockIfEnabled(uint16_t toothCount, uint8_t nCylinders) {
    // Guard clause: cranklock not enabled
    if (!configPage4.ignCranklock) {
        return;
    }

    // Guard clause: not cranking
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        return;
    }

    // Guard clause: not past staging cycles
    if (currentStatus.startRevolutions < configPage4.StgCycles) {
        return;
    }

    // Dispatch based on cylinder count
    if (nCylinders == 4U) {
        handleCranklock4Cyl(toothCount);
    } else if (nCylinders == 6U) {
        handleCranklock6Cyl(toothCount);
    }
}

// ============================================================================
// HELPER FUNCTIONS - Trigger Filter Configuration
// ============================================================================

/**
 * @brief Determine filter level based on config and RPM
 * @return Filter level (0=off, 1=lite, 2=medium, 3=aggressive)
 * @complexity 4
 */
static inline uint8_t determineFilterLevel(void) {
    // Lite filter: enabled or low RPM
    if ((configPage4.triggerFilter == 1U) || (currentStatus.RPM < CRANKING_RPM_THRESHOLD)) {
        return 1U;
    }

    // Medium filter
    if (configPage4.triggerFilter == 2U) {
        return 2U;
    }

    // Aggressive filter
    if (configPage4.triggerFilter == 3U) {
        return 3U;
    }

    // Filter off
    return 0U;
}

/**
 * @brief Apply adaptive trigger filter
 * @param toothCount Current tooth number
 * @param nCylinders Number of cylinders
 * @param gap Current tooth gap (�s)
 * @complexity 3
 */
static inline void applyAdaptiveFilter(uint16_t toothCount, uint8_t nCylinders, uint32_t gap) {
    uint8_t filterLevel = determineFilterLevel();
    bool isOddTooth = (toothCount & 1U) != 0U; // Odd teeth: 1,3,5,7,9,11

    // Use data-driven filter config (replaces 104 lines of duplicated code)
    apply4G63FilterConfig(filterLevel, isOddTooth, nCylinders, gap);
}

// ============================================================================
// HELPER FUNCTIONS - Per-Tooth Ignition
// ============================================================================

/**
 * @brief Handle per-tooth ignition timing
 * @param toothCount Current tooth number
 * @param nCylinders Number of cylinders
 * @complexity 5
 */
static inline void handlePerToothIgnition(uint16_t toothCount, uint8_t nCylinders) {
    // Guard clause: per-tooth ignition not enabled
    if (!configPage2.perToothIgn) {
        return;
    }

    // Guard clause: trigger angle not zero (required for per-tooth on 4G63)
    if (configPage4.triggerAngle != 0) {
        return;
    }

    // Guard clause: 4-cylinder only and positive advance
    if ((nCylinders != 4U) || (currentStatus.advance <= 0)) {
        return;
    }

    // Lookup crank angle from fixed angle table
    int16_t crankAngle = ignitionLimits(toothAngles[toothCount - 1U]);

    // Handle non-sequential mode (wrapping tooth count)
    bool isSequential = (configPage4.sparkMode == IGN_MODE_SEQUENTIAL);
    uint16_t logicalTooth = toothCount;

    if (!isSequential && (toothCount > nCylinders)) {
        logicalTooth = toothCount - nCylinders;
    }

    checkPerToothTiming(crankAngle, logicalTooth);
}

// ============================================================================
// HELPER FUNCTIONS - Sync Detection (No Sync State)
// ============================================================================

/**
 * @brief Detect sync from primary and secondary trigger states
 * @details Uses cam signal to determine tooth position
 * @param nCylinders Number of cylinders
 * @complexity 8
 */
static inline void detectSyncFromTriggerStates(uint8_t nCylinders) {
    // Read current trigger states
    bool priHigh = READ_PRI_TRIGGER();
    bool secHigh = READ_SEC_TRIGGER();

    // Crank rising edge: record cam state
    if (priHigh) {
        revolutionOne = secHigh;
        return;
    }

    // Crank falling edge: check combinations
    // Guard clause: revolution tracking not started
    if (!revolutionOne) {
        return;
    }

    // Crank LOW + Cam LOW + started on Cam HIGH
    if (!secHigh) {
        // 4-cylinder: tooth 1 position (5� BTDC)
        if (nCylinders == 4U) {
            toothCurrentCount = 1U;
        }
        return;
    }

    // Crank LOW + Cam HIGH + started on Cam HIGH
    if (nCylinders == 4U) {
        // 4-cylinder: tooth 5 position (5� BTDC on second revolution)
        toothCurrentCount = 5U;
    } else if (nCylinders == 6U) {
        // 6-cylinder: tooth 2 position (45� ATDC)
        toothCurrentCount = 2U;
        currentStatus.hasSync = true;
    }
}

// ============================================================================
// HELPER FUNCTIONS - Secondary Trigger Sync
// ============================================================================

/**
 * @brief Attempt to gain sync from secondary trigger
 * @param nCylinders Number of cylinders
 * @param toothCount Current tooth count
 * @return true if sync gained
 * @complexity 6
 */
static inline bool attemptSecondarySync(uint8_t nCylinders, uint16_t toothCount) {
    bool priHigh = READ_PRI_TRIGGER();

    // 4-cylinder sync points
    if (nCylinders == 4U) {
        if (priHigh) {
            // Crank HIGH: tooth 8 position
            return (toothCount == 8U);
        } else {
            // Crank LOW: tooth 5 position
            return (toothCount == 5U);
        }
    }

    // 6-cylinder sync points
    if (nCylinders == 6U) {
        // Only gain sync when crank HIGH at tooth 7
        if (priHigh) {
            return (toothCount == 7U);
        }
    }

    return false;
}

/**
 * @brief Verify sync from secondary trigger (resync)
 * @param nCylinders Number of cylinders
 * @param toothCount Current tooth count
 * @complexity 5
 */
static inline void verifySecondarySync(uint8_t nCylinders, uint16_t toothCount) {
    // Guard clause: only 4-cylinder supported for resync
    if (nCylinders != 4U) {
        return;
    }

    // Guard clause: no sync
    if (!currentStatus.hasSync) {
        return;
    }

    bool priHigh = READ_PRI_TRIGGER();

    // Verify tooth position matches expected
    if (priHigh) {
        // Should be at tooth 8
        if (toothCount != 8U) {
            // Noise pulse detected - lose sync
            currentStatus.hasSync = false;
            currentStatus.syncLossCounter++;
        } else {
            // Force tooth count (why?)
            toothCurrentCount = 8U;
        }
    }
}

} // anonymous namespace

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup 4G63 decoder
 * @details Initializes decoder for 4G63/6G72 CAS pattern
 *
 * PRESERVES: decoders.cpp lines 1476-1528
 *
 * @complexity 6
 */
void triggerSetup_4G63(void)
{
    // Fixed tooth angle (180� for primary teeth)
    triggerToothAngle = 180U;

    // Initialize tooth count (99 = no sync marker)
    toothCurrentCount = NO_SYNC_TOOTH_COUNT;

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);

    // Set max stall time (minimum 50 RPM, 110� spacing)
    MAX_STALL_TIME = MIN_RPM_STALL_TIME;

    // Set startup value to avoid filter errors
    if (!currentStatus.initialisationComplete) {
        toothLastToothTime = micros();
    }

    // Setup tooth angle arrays (BOTH rising and falling edges)
    if (configPage2.nCylinders == 6U) {
        // 6-cylinder: 12 edges (6 teeth * 2 edges)
        toothAngles[0] = 715;  // Rising edge tooth #1
        toothAngles[1] = 45;   // Falling edge tooth #1
        toothAngles[2] = 115;  // Rising edge tooth #2
        toothAngles[3] = 165;  // Falling edge tooth #2
        toothAngles[4] = 235;  // Rising edge tooth #3
        toothAngles[5] = 285;  // Falling edge tooth #3
        toothAngles[6] = 355;  // Rising edge tooth #4
        toothAngles[7] = 405;  // Falling edge tooth #4
        toothAngles[8] = 475;  // Rising edge tooth #5
        toothAngles[9] = 525;  // Falling edge tooth #5
        toothAngles[10] = 595; // Rising edge tooth #6
        toothAngles[11] = 645; // Falling edge tooth #6

        triggerActualTeeth = 12U; // Both edges over 720�
    } else {
        // 4-cylinder: 8 edges (4 teeth * 2 edges)
        // 70� / 110� alternating pattern
        toothAngles[0] = 715;  // Falling edge tooth #1
        toothAngles[1] = 105;  // Rising edge tooth #2
        toothAngles[2] = 175;  // Falling edge tooth #2
        toothAngles[3] = 285;  // Rising edge tooth #1

        toothAngles[4] = 355;  // Falling edge tooth #1
        toothAngles[5] = 465;  // Rising edge tooth #2
        toothAngles[6] = 535;  // Falling edge tooth #2
        toothAngles[7] = 645;  // Rising edge tooth #1

        triggerActualTeeth = 8U;
    }

    // Set trigger filters
    triggerFilterTime = DEFAULT_FILTER_NOSYNC; // 10000 RPM on both edges
    triggerSecFilterTime = (MICROS_PER_SEC / (MAX_RPM / 60U * 2U)) / 2U;
    triggerSecFilterTime_duration = 4000U;
    secondaryLastToothTime = 0U;
}

/**
 * @brief Primary trigger ISR
 * @details Processes 4G63 CAS crank teeth (both edges)
 *
 * PRESERVES: decoders.cpp lines 1644-1748
 * ORIGINAL: 104 lines, complexity ~15
 * REFACTORED: 58 lines, complexity 8, uses helper functions
 *
 * @complexity 8
 * @isr CRITICAL - Must complete in < 10�s
 */
void triggerPri_4G63(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Guard clause: filter check (allow first pulse on startup)
    if ((curGap < triggerFilterTime) && (currentStatus.startRevolutions != 0U)) {
        return;
    }

    // Valid trigger
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Set basic filter for non-sync state
    triggerFilterTime = curGap >> 2; // 25% filter

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    // Increment tooth counter
    toothCurrentCount++;

    // Check for revolution boundary
    if (isAtRevolutionBoundary(toothCurrentCount, triggerActualTeeth)) {
        handleRevolutionBoundary();
    }

    // Process based on sync state
    if (currentStatus.hasSync) {
        // Have sync - normal operation
        handleCranklockIfEnabled(toothCurrentCount, configPage2.nCylinders);
        applyAdaptiveFilter(toothCurrentCount, configPage2.nCylinders, curGap);
        handlePerToothIgnition(toothCurrentCount, configPage2.nCylinders);
    } else {
        // No sync - attempt to gain sync
        triggerSecFilterTime = 0U;
        detectSyncFromTriggerStates(configPage2.nCylinders);
    }
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Processes cam signal for sync and sequential operation
 *
 * PRESERVES: decoders.cpp lines 1749-1825
 * ORIGINAL: 76 lines, complexity ~12
 * REFACTORED: 42 lines, complexity 7, uses helper functions
 *
 * @complexity 7
 * @isr CRITICAL - Must complete in < 10�s
 */
void triggerSec_4G63(void)
{
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;

    // Guard clause: filter check
    if (curGap2 < triggerSecFilterTime) {
        return;
    }

    // Valid trigger
    toothLastSecToothTime = curTime2;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Update filter (50% of current gap)
    triggerSecFilterTime = curGap2 >> 1;

    // Process based on sync state
    if (!currentStatus.hasSync) {
        // Attempting to gain sync
        triggerFilterTime = DEFAULT_FILTER_NOSYNC;
        triggerSecFilterTime = triggerSecFilterTime >> 1; // 25% filter when cranking

        // Attempt sync
        if (attemptSecondarySync(configPage2.nCylinders, toothCurrentCount)) {
            currentStatus.hasSync = true;
        }
    } else {
        // Have sync - verify if cranking or resync enabled
        if ((currentStatus.RPM < currentStatus.crankRPM) || (configPage4.useResync == 1U)) {
            verifySecondarySync(configPage2.nCylinders, toothCurrentCount);
        }
    }
}

/**
 * @brief Get current RPM
 * @details Special calculation for uneven tooth spacing
 *
 * PRESERVES: decoders.cpp lines 1828-1863
 *
 * @return RPM value
 * @complexity 7
 */
uint16_t getRPM_4G63(void)
{
    uint16_t tempRPM = 0U;

    // Guard clause: no sync
    if (!currentStatus.hasSync) {
        return 0U;
    }

    // Cranking: special calculation for uneven teeth
    if (currentStatus.RPM < currentStatus.crankRPM) {
        // Guard clause: no valid timing
        if ((toothLastToothTime == 0U) || (toothLastMinusOneToothTime == 0U)) {
            return 0U;
        }

        int tempToothAngle;
        unsigned long toothTime;

        noInterrupts();
        tempToothAngle = triggerToothAngle;
        toothTime = toothLastToothTime - toothLastMinusOneToothTime;
        interrupts();

        // Tooth angle changes between 70� and 110� (or 70�/50� for 6-cyl)
        toothTime = toothTime * 36U;
        tempRPM = ((unsigned long)tempToothAngle * (MICROS_PER_MIN / 10U)) / toothTime;
        SetRevolutionTime((10UL * toothTime) / tempToothAngle);
        MAX_STALL_TIME = MIN_RPM_STALL_TIME;
    } else {
        // Running: use standard calculation (cam speed)
        tempRPM = stdGetRPM(CAM_SPEED);

        // Update stall time (twice revolution time)
        MAX_STALL_TIME = revolutionTime << 1;
        if (MAX_STALL_TIME < MIN_RPM_STALL_TIME) {
            MAX_STALL_TIME = MIN_RPM_STALL_TIME;
        }
    }

    return tempRPM;
}

/**
 * @brief Get current crank angle
 * @details Uses fixed angle lookup table
 *
 * PRESERVES: decoders.cpp lines 1865-1890
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 4
 */
int getCrankAngle_4G63(void)
{
    int crankAngle = 0;

    // Guard clause: no sync
    if (!currentStatus.hasSync) {
        return 0;
    }

    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    // Lookup fixed tooth angle from table
    crankAngle = toothAngles[tempToothCurrentCount - 1] + configPage4.triggerAngle;

    // Add interpolation
    elapsedTime = lastCrankAngleCalc - tempToothLastToothTime;
    crankAngle += timeToAngleIntervalTooth(elapsedTime);

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
 * @details Configures end teeth based on cylinder count and mode
 *
 * PRESERVES: decoders.cpp lines 1892-1929
 *
 * @complexity 4
 */
void triggerSetEndTeeth_4G63(void)
{
    if (configPage2.nCylinders == 4U) {
        if (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) {
            // Sequential mode (full 720� cycle)
            ignition1EndTooth = 8U;
            ignition2EndTooth = 2U;
            ignition3EndTooth = 4U;
            ignition4EndTooth = 6U;
        } else {
            // Wasted spark mode
            ignition1EndTooth = 4U;
            ignition2EndTooth = 2U;
            ignition3EndTooth = 4U; // Not used
            ignition4EndTooth = 2U;
        }
    }

    if (configPage2.nCylinders == 6U) {
        if (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) {
            // Sequential not supported for 6-cylinder - use these values anyway
            ignition1EndTooth = 8U;
            ignition2EndTooth = 2U;
            ignition3EndTooth = 4U;
            ignition4EndTooth = 6U;
        } else {
            // Wasted spark mode
            ignition1EndTooth = 6U;
            ignition2EndTooth = 2U;
            ignition3EndTooth = 4U;
            ignition4EndTooth = 2U; // Not used
        }
    }
}
