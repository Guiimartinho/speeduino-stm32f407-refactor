/**
 * @file miata_9905.cpp
 * @brief Miata '99-'05 decoder implementation
 * @details REFACTORED from decoders.cpp (lines 2596-2893)
 *
 * Pattern: 4x 70 degree duration teeth at cam speed
 * - Teeth believed to be at same angles as 4G63 decoder
 * - Alternating 70° and 110° tooth spacing
 * - Tooth #1 defined after cam signal falls with crank HIGH
 * - Tooth #1 at 355° ATDC
 * - Sequential mode with VVT support
 *
 * ORIGINAL: 298 lines
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

// ============================================================================
// CONSTANTS (file scope)
// ============================================================================

/** @brief Number of actual physical teeth */
static constexpr uint8_t ACTUAL_TEETH_COUNT = 8U;

/** @brief Tooth count indicating no sync */
static constexpr uint8_t NO_SYNC_TOOTH_COUNT = 99U;

/** @brief Degrees per tooth (nominal, alternates between 70 and 110) */
static constexpr uint8_t NOMINAL_TOOTH_ANGLE_DEG = 90U;

/** @brief Odd tooth angle (teeth 1,3,5,7) */
static constexpr uint8_t ODD_TOOTH_ANGLE_DEG = 70U;

/** @brief Even tooth angle (teeth 2,4,6,8) */
static constexpr uint8_t EVEN_TOOTH_ANGLE_DEG = 110U;

/** @brief Initial filter time in microseconds */
static constexpr uint16_t INITIAL_FILTER_TIME_US = 1500U;

/** @brief Tooth after double cam pulse */
static constexpr uint8_t TOOTH_AFTER_DOUBLE_CAM = 6U;

/** @brief Secondary tooth count for double pulse */
static constexpr uint8_t DOUBLE_CAM_PULSE_COUNT = 2U;

/** @brief Minimum RPM threshold for filter switching */
static constexpr uint16_t FILTER_RPM_THRESHOLD = 1400U;

/** @brief Minimum RPM in microseconds (50 RPM) */
static constexpr uint32_t MIN_RPM_STALL_TIME_US = 366667UL;

/** @brief Crank angle for VVT calculation */
static constexpr int16_t VVT_ANGLE_BASE = 370;

/** @brief Minimum advance for ignition end tooth calculation */
static constexpr int16_t MIN_ADVANCE_FOR_END_TOOTH = 10;

// ============================================================================
// FILTER CONFIGURATION (file scope)
// ============================================================================

struct Miata9905FilterConfig {
    uint8_t filterLevel;
    uint8_t oddToothMult;
    uint8_t oddToothShift;
    uint8_t evenToothMult;
    uint8_t evenToothShift;
};

static const Miata9905FilterConfig miata9905FilterConfigs[4] = {
    {0, 1,  0,  1, 0},
    {1, 1,  0,  3, 3},
    {2, 5,  2,  1, 1},
    {3, 11, 3,  9, 5}
};

// Helper: Find filter config by level (returns nullptr if not found)
static inline const Miata9905FilterConfig* findFilterConfig(uint8_t filterLevel)
{
    for (uint8_t i = 0; i < 4; i++) {
        if (miata9905FilterConfigs[i].filterLevel == filterLevel) { return &miata9905FilterConfigs[i]; }
    }
    return nullptr;
}

// Helper: Calculate filter time from config
static inline uint32_t calcFilterTime(uint32_t curGap, uint8_t mult, uint8_t shift)
{
    if (shift == 0) { return curGap * (uint32_t)mult; }
    return (curGap * (uint32_t)mult) >> shift;
}

// Helper: Apply Miata9905 filter configuration (file scope to reduce nesting)
static inline void applyMiata9905Filter(uint8_t filterLevel, uint8_t toothCount, uint32_t curGap,
                                        volatile uint16_t* pTriggerToothAngle,
                                        volatile uint32_t* pTriggerFilterTime,
                                        volatile uint32_t* pTriggerSecFilterTime)
{
    bool isOddTooth = (toothCount == 1) || (toothCount == 3) || (toothCount == 5) || (toothCount == 7);
    *pTriggerToothAngle = isOddTooth ? ODD_TOOTH_ANGLE_DEG : EVEN_TOOTH_ANGLE_DEG;

    const Miata9905FilterConfig* config = findFilterConfig(filterLevel);
    if (config == nullptr) { return; }

    if (filterLevel == 0) { *pTriggerFilterTime = 0; *pTriggerSecFilterTime = 0; return; }

    uint8_t mult = isOddTooth ? config->oddToothMult : config->evenToothMult;
    uint8_t shift = isOddTooth ? config->oddToothShift : config->evenToothShift;
    *pTriggerFilterTime = calcFilterTime(curGap, mult, shift);
}

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing (file scope)
// ============================================================================

static inline void handleTooth1(void) {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
}

static inline void handleSyncAcquisition(void) {
    bool needsSync = (currentStatus.hasSync == false) || (configPage4.useResync == true);
    bool isDoubleCamPulse = (secondaryToothCount == DOUBLE_CAM_PULSE_COUNT);
    if (needsSync && isDoubleCamPulse) { toothCurrentCount = TOOTH_AFTER_DOUBLE_CAM; currentStatus.hasSync = true; }
}

static inline void handlePerToothTiming(uint8_t toothCount) {
    bool perToothEnabled = (configPage2.perToothIgn == true) && (configPage4.triggerAngle == 0) && (currentStatus.advance > 0);
    if (!perToothEnabled) { return; }

    int16_t crankAngle = ignitionLimits(toothAngles[(toothCount - 1)]);
    bool needsOffset = (configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (toothCount > configPage2.nCylinders);
    uint8_t effectiveToothCount = needsOffset ? (toothCount - configPage2.nCylinders) : toothCount;
    checkPerToothTiming(crankAngle, effectiveToothCount);
}

static inline void handleFixedCrankTiming(uint8_t toothCount) {
    bool crankLockActive = (currentStatus.RPM < (currentStatus.crankRPM + 30)) && (configPage4.ignCranklock);
    if (!crankLockActive) { return; }

    bool isCoil13Tooth = (toothCount == 1) || (toothCount == 5);
    bool isCoil24Tooth = (toothCount == 3) || (toothCount == 7);
    if (isCoil13Tooth) { endCoil1Charge(); endCoil3Charge(); }
    else if (isCoil24Tooth) { endCoil2Charge(); endCoil4Charge(); }
}

// ============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// ============================================================================

/**
 * @brief Setup Miata9905 decoder
 * @details Initializes decoder state and tooth angles
 *
 * PRESERVES: decoders.cpp lines 2596-2632
 *
 * @complexity 3
 */
void triggerSetup_Miata9905(void)
{
    triggerToothAngle = NOMINAL_TOOTH_ANGLE_DEG;
    toothCurrentCount = NO_SYNC_TOOTH_COUNT; // Fake tooth count represents no sync
    triggerActualTeeth = ACTUAL_TEETH_COUNT;

    // Initialize timing based on completion state
    if (currentStatus.initialisationComplete == false) {
        secondaryToothCount = 0;
        toothLastToothTime = micros();
    } else {
        toothLastToothTime = 0;
    }
    toothLastMinusOneToothTime = 0;

    // Set tooth angles (for every rising and falling edge)
    toothAngles[0] = 710; //
    toothAngles[1] = 100; // First crank pulse after SINGLE cam pulse
    toothAngles[2] = 170; //
    toothAngles[3] = 280; //
    toothAngles[4] = 350; //
    toothAngles[5] = 460; // First crank pulse AFTER DOUBLE cam pulse
    toothAngles[6] = 530; //
    toothAngles[7] = 640; //

    // Calculate timing parameters
    MAX_STALL_TIME = ((MICROS_PER_DEG_1_RPM / 50U) * triggerToothAngle);
    triggerFilterTime = INITIAL_FILTER_TIME_US; // 10000 rpm
    triggerSecFilterTime = 0; // Need to figure out something better

    // Set decoder flags
    BIT_CLEAR(decoderState, BIT_DECODER_2ND_DERIV);
    BIT_SET(decoderState, BIT_DECODER_IS_SEQUENTIAL);
    BIT_SET(decoderState, BIT_DECODER_HAS_FIXED_CRANKING);
    BIT_SET(decoderState, BIT_DECODER_TOOTH_ANG_CORRECT);
    BIT_SET(decoderState, BIT_DECODER_HAS_SECONDARY);
}

/**
 * @brief Primary trigger ISR (crank teeth)
 * @details Processes 8 crank teeth with alternating spacing
 *
 * PRESERVES: decoders.cpp lines 2694-2756
 * ORIGINAL: 63 lines, complexity ~8
 * REFACTORED: 49 lines, complexity 6, uses helper functions
 *
 * @complexity 6
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerPri_Miata9905(void)
{
    curTime = micros();
    curGap = curTime - toothLastToothTime;

    // Guard clause: filter check
    if ((curGap < triggerFilterTime) && (currentStatus.startRevolutions != 0)) {
        return;
    }

    toothCurrentCount++;
    BIT_SET(decoderState, BIT_DECODER_VALID_TRIGGER);

    // Handle tooth rollover
    if (toothCurrentCount == (triggerActualTeeth + 1)) {
        handleTooth1();
    } else {
        // Handle sync acquisition
        handleSyncAcquisition();
    }

    // Process synced operations
    if (currentStatus.hasSync == true) {
        // Apply adaptive filter (use LITE when RPM < 1400)
        uint8_t effectiveFilterLevel = ((configPage4.triggerFilter == 1) || (currentStatus.RPM < FILTER_RPM_THRESHOLD))
                                       ? 1 : configPage4.triggerFilter;
        applyMiata9905Filter(effectiveFilterLevel, toothCurrentCount, curGap,
                            &triggerToothAngle, &triggerFilterTime, &triggerSecFilterTime);

        // Handle per-tooth timing (experimental)
        handlePerToothTiming(toothCurrentCount);
    }

    // Update timing history
    toothLastMinusOneToothTime = toothLastToothTime;
    toothLastToothTime = curTime;

    // Handle fixed cranking timing
    handleFixedCrankTiming(toothCurrentCount);

    // Reset secondary tooth count
    secondaryToothCount = 0;
}

/**
 * @brief Secondary trigger ISR (cam signal)
 * @details Processes cam signal for sync
 *
 * PRESERVES: decoders.cpp lines 2758-2782
 *
 * @complexity 3
 * @isr CRITICAL - Must complete in < 10μs
 */
void triggerSec_Miata9905(void)
{
    curTime2 = micros();
    curGap2 = curTime2 - toothLastSecToothTime;

    // Reset filter during cranking or no sync
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) || (currentStatus.hasSync == false)) {
        triggerFilterTime = INITIAL_FILTER_TIME_US;
    }

    // Guard clause: filter check
    if (curGap2 < triggerSecFilterTime) {
        return;
    }

    toothLastSecToothTime = curTime2;
    lastGap = curGap2;
    secondaryToothCount++;

    // Record VVT tooth time (between tooth #1 and single cam tooth)
    if ((toothCurrentCount == 1) && (curTime2 > toothLastToothTime)) {
        lastVVTtime = curTime2 - toothLastToothTime;
    }
}

/**
 * @brief Get current RPM
 * @details Special handling for uneven tooth spacing during cranking
 *
 * PRESERVES: decoders.cpp lines 2784-2814
 *
 * @return RPM value
 * @complexity 4
 */
uint16_t getRPM_Miata9905(void)
{
    uint16_t tempRPM = 0;

    // During cranking: calculate RPM from tooth-to-tooth time
    if ((currentStatus.RPM < currentStatus.crankRPM) && (currentStatus.hasSync == true)) {
        // Guard clause: check valid timing
        if ((toothLastToothTime == 0) || (toothLastMinusOneToothTime == 0)) {
            return 0;
        }

        int tempToothAngle;
        unsigned long toothTime;

        // Atomic read of ISR variables
        noInterrupts();
        tempToothAngle = triggerToothAngle;
        toothTime = (toothLastToothTime - toothLastMinusOneToothTime);
        interrupts();

        // Calculate RPM from tooth time
        toothTime = toothTime * 36;
        tempRPM = ((unsigned long)tempToothAngle * (MICROS_PER_MIN / 10U)) / toothTime;
        SetRevolutionTime((10UL * toothTime) / tempToothAngle);
        MAX_STALL_TIME = MIN_RPM_STALL_TIME_US; // 50RPM
    } else {
        // Normal operation: use standard RPM at cam speed
        tempRPM = stdGetRPM(CAM_SPEED);
        MAX_STALL_TIME = revolutionTime << 1; // Twice current RPM time
        if (MAX_STALL_TIME < MIN_RPM_STALL_TIME_US) {
            MAX_STALL_TIME = MIN_RPM_STALL_TIME_US; // 50rpm minimum
        }
    }

    return tempRPM;
}

/**
 * @brief Get current crank angle
 * @details Calculates angle from tooth position + interpolation
 *
 * PRESERVES: decoders.cpp lines 2816-2842
 *
 * @return Crank angle (0-719 degrees)
 * @complexity 3
 */
int getCrankAngle_Miata9905(void)
{
    unsigned long tempToothLastToothTime;
    int tempToothCurrentCount;

    // Atomic read of ISR variables
    noInterrupts();
    tempToothCurrentCount = toothCurrentCount;
    tempToothLastToothTime = toothLastToothTime;
    lastCrankAngleCalc = micros();
    interrupts();

    // Lookup fixed tooth angle
    int crankAngle = toothAngles[(tempToothCurrentCount - 1)] + configPage4.triggerAngle;

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
 * @brief Get current cam angle (VVT)
 * @details Calculates VVT angle from cam signal timing
 *
 * PRESERVES: decoders.cpp lines 2844-2853
 *
 * @return Cam angle in degrees
 * @complexity 2
 */
int getCamAngle_Miata9905(void)
{
    // lastVVTtime is time between tooth #1 (10° BTDC) and single cam tooth
    // All cam angles in BTDC, so actual advance = 370 - timeToAngleDegPerMicroSec(lastVVTtime) - base
    int16_t curAngle = VVT_ANGLE_BASE - timeToAngleDegPerMicroSec(lastVVTtime) - configPage10.vvtCL0DutyAng;
    currentStatus.vvt1Angle = LOW_PASS_FILTER((curAngle << 1), configPage4.ANGLEFILTER_VVT, currentStatus.vvt1Angle);

    return currentStatus.vvt1Angle;
}

/**
 * @brief Set end teeth for ignition scheduling
 * @details Calculates ignition end teeth based on advance and spark mode
 *
 * PRESERVES: decoders.cpp lines 2855-2893
 *
 * @complexity 3
 */
void triggerSetEndTeeth_Miata9905(void)
{
    if (configPage4.sparkMode == IGN_MODE_SEQUENTIAL) {
        if (currentStatus.advance >= MIN_ADVANCE_FOR_END_TOOTH) {
            ignition1EndTooth = 8;
            ignition2EndTooth = 2;
            ignition3EndTooth = 4;
            ignition4EndTooth = 6;
        } else if (currentStatus.advance > 0) {
            ignition1EndTooth = 1;
            ignition2EndTooth = 3;
            ignition3EndTooth = 5;
            ignition4EndTooth = 7;
        }
    } else {
        if (currentStatus.advance >= MIN_ADVANCE_FOR_END_TOOTH) {
            ignition1EndTooth = 4;
            ignition2EndTooth = 2;
            ignition3EndTooth = 4; // Not used
            ignition4EndTooth = 2; // Not used
        } else if (currentStatus.advance > 0) {
            ignition1EndTooth = 1;
            ignition2EndTooth = 3;
            ignition3EndTooth = 1; // Not used
            ignition4EndTooth = 3; // Not used
        }
    }
}
