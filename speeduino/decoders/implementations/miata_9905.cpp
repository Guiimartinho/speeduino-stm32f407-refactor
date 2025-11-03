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

// Anonymous namespace for private implementation
namespace {

// ============================================================================
// CONSTANTS
// ============================================================================

/** @brief Number of actual physical teeth */
constexpr uint8_t ACTUAL_TEETH_COUNT = 8U;

/** @brief Tooth count indicating no sync */
constexpr uint8_t NO_SYNC_TOOTH_COUNT = 99U;

/** @brief Degrees per tooth (nominal, alternates between 70 and 110) */
constexpr uint8_t NOMINAL_TOOTH_ANGLE_DEG = 90U;

/** @brief Odd tooth angle (teeth 1,3,5,7) */
constexpr uint8_t ODD_TOOTH_ANGLE_DEG = 70U;

/** @brief Even tooth angle (teeth 2,4,6,8) */
constexpr uint8_t EVEN_TOOTH_ANGLE_DEG = 110U;

/** @brief Initial filter time in microseconds */
constexpr uint16_t INITIAL_FILTER_TIME_US = 1500U;

/** @brief Tooth after double cam pulse */
constexpr uint8_t TOOTH_AFTER_DOUBLE_CAM = 6U;

/** @brief Secondary tooth count for double pulse */
constexpr uint8_t DOUBLE_CAM_PULSE_COUNT = 2U;

/** @brief Minimum RPM threshold for filter switching */
constexpr uint16_t FILTER_RPM_THRESHOLD = 1400U;

/** @brief Minimum RPM in microseconds (50 RPM) */
constexpr uint32_t MIN_RPM_STALL_TIME_US = 366667UL;

/** @brief Crank angle for VVT calculation */
constexpr int16_t VVT_ANGLE_BASE = 370;

/** @brief Minimum advance for ignition end tooth calculation */
constexpr int16_t MIN_ADVANCE_FOR_END_TOOTH = 10;

// ============================================================================
// FILTER CONFIGURATION
// ============================================================================

/**
 * @brief Filter configuration for Miata9905 decoder
 * @details Data-driven approach for trigger filtering
 */
struct Miata9905FilterConfig {
    uint8_t filterLevel;    ///< Filter level (0=OFF, 1=LITE, 2=MEDIUM, 3=AGGRESSIVE)
    uint8_t oddToothMult;   ///< Multiplier for odd teeth (70° spacing)
    uint8_t oddToothShift;  ///< Right shift for odd teeth filter
    uint8_t evenToothMult;  ///< Multiplier for even teeth (110° spacing)
    uint8_t evenToothShift; ///< Right shift for even teeth filter
};

/** @brief Filter configurations for all filter levels */
static const Miata9905FilterConfig miata9905FilterConfigs[4] = {
    {0, 1,  0,  1, 0},  // OFF: No filtering
    {1, 1,  0,  3, 3},  // LITE: odd=curGap, even=(curGap*3)>>3 (41.25°)
    {2, 5,  2,  1, 1},  // MEDIUM: odd=(curGap*5)>>2 (87.5°), even=curGap>>1 (55°)
    {3, 11, 3,  9, 5}   // AGGRESSIVE: odd=(curGap*11)>>3 (96.26°), even=(curGap*9)>>5 (61.87°)
};

// ============================================================================
// HELPER FUNCTIONS - Filter Application
// ============================================================================

/**
 * @brief Apply Miata9905 filter configuration
 * @details Configures filter based on tooth position and filter level
 * @param filterLevel Filter level (0-3)
 * @param toothCount Current tooth count
 * @param curGap Current gap time
 * @param pTriggerToothAngle Pointer to triggerToothAngle
 * @param pTriggerFilterTime Pointer to triggerFilterTime
 * @param pTriggerSecFilterTime Pointer to triggerSecFilterTime
 * @complexity 4
 */
static inline void applyMiata9905Filter(uint8_t filterLevel, uint8_t toothCount, uint32_t curGap,
                                        volatile uint16_t* pTriggerToothAngle,
                                        volatile uint32_t* pTriggerFilterTime,
                                        volatile uint32_t* pTriggerSecFilterTime)
{
    // Determine if odd tooth (1,3,5,7 have 70° spacing, even 2,4,6,8 have 110°)
    bool isOddTooth = (toothCount == 1) || (toothCount == 3) || (toothCount == 5) || (toothCount == 7);

    // Find and apply matching filter configuration
    for (uint8_t i = 0; i < 4; i++) {
        const Miata9905FilterConfig* config = &miata9905FilterConfigs[i];
        if (config->filterLevel == filterLevel) {
            // Set tooth angle based on odd/even position
            *pTriggerToothAngle = isOddTooth ? ODD_TOOTH_ANGLE_DEG : EVEN_TOOTH_ANGLE_DEG;

            // Special case: filter OFF sets both filter times to 0
            if (filterLevel == 0) {
                *pTriggerFilterTime = 0;
                *pTriggerSecFilterTime = 0;
            } else {
                // Apply filter calculation based on tooth position
                uint8_t mult = isOddTooth ? config->oddToothMult : config->evenToothMult;
                uint8_t shift = isOddTooth ? config->oddToothShift : config->evenToothShift;

                // Calculate filter time: (curGap * multiplier) >> shift
                if (shift == 0) {
                    *pTriggerFilterTime = curGap * (uint32_t)mult;
                } else {
                    *pTriggerFilterTime = (curGap * (uint32_t)mult) >> shift;
                }
            }
            return;
        }
    }
}

// ============================================================================
// HELPER FUNCTIONS - Tooth Processing
// ============================================================================

/**
 * @brief Handle tooth 1 (revolution boundary)
 * @details Resets counters and updates timing
 * @complexity 2
 */
static inline void handleTooth1(void) {
    toothCurrentCount = 1;
    toothOneMinusOneTime = toothOneTime;
    toothOneTime = curTime;
    currentStatus.startRevolutions++;
}

/**
 * @brief Check and handle sync acquisition
 * @details Uses secondary tooth count to determine sync position
 * @complexity 2
 */
static inline void handleSyncAcquisition(void) {
    if ((currentStatus.hasSync == false) || (configPage4.useResync == true)) {
        if (secondaryToothCount == DOUBLE_CAM_PULSE_COUNT) {
            toothCurrentCount = TOOTH_AFTER_DOUBLE_CAM;
            currentStatus.hasSync = true;
        }
    }
}

/**
 * @brief Handle per-tooth timing if enabled
 * @details Experimental per-tooth ignition timing
 * @param toothCount Current tooth count
 * @complexity 3
 */
static inline void handlePerToothTiming(uint8_t toothCount) {
    // Only available when trigger angle is 0 and advance > 0
    if ((configPage2.perToothIgn == true) &&
        (configPage4.triggerAngle == 0) &&
        (currentStatus.advance > 0)) {

        int16_t crankAngle = ignitionLimits(toothAngles[(toothCount - 1)]);

        // Handle non-sequential tooth counts
        if ((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) && (toothCount > configPage2.nCylinders)) {
            checkPerToothTiming(crankAngle, (toothCount - configPage2.nCylinders));
        } else {
            checkPerToothTiming(crankAngle, toothCount);
        }
    }
}

/**
 * @brief Handle fixed cranking timing
 * @details Ends coil charge at specific teeth during cranking
 * @param toothCount Current tooth count
 * @complexity 2
 */
static inline void handleFixedCrankTiming(uint8_t toothCount) {
    if ((currentStatus.RPM < (currentStatus.crankRPM + 30)) && (configPage4.ignCranklock)) {
        if ((toothCount == 1) || (toothCount == 5)) {
            endCoil1Charge();
            endCoil3Charge();
        } else if ((toothCount == 3) || (toothCount == 7)) {
            endCoil2Charge();
            endCoil4Charge();
        }
    }
}

} // anonymous namespace

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
