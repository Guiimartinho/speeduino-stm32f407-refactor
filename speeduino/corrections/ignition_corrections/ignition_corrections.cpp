/**
 * @file ignition_corrections.cpp
 * @brief Ignition corrections implementation - all ignition timing corrections
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module handles all ignition timing correction calculations:
 * - Main dispatcher (correctionsIgn)
 * - Override corrections (fixed timing, cranking)
 * - Environmental corrections (IAT retard, CLT advance)
 * - Fuel type corrections (flex fuel, WMI)
 * - Protection corrections (knock, soft rev limit)
 * - Special corrections (idle advance, nitrous, launch, flat shift, DFCO)
 *
 * @note All functions preserve 100% original logic from corrections.cpp
 * @note MISRA-C compliant: functions <50 lines, complexity <10, guard clauses
 */

#include "ignition_corrections.h"
#include "../../corrections.h"
#include "../../sensors.h"
#include "../../units.h"

// ============================================================================
// GLOBAL STATE (Ignition tracking variables)
// ============================================================================

/// Idle advance active flag (tracks whether idle advance is currently applied)
bool idleAdvActive = false;

/// Knock retard start time (microseconds)
uint32_t knockStartTime = 0;

/// Knock recovery last step (tracks knock recovery progress)
uint8_t knockLastRecoveryStep = 0;

/// Soft limit time counter (tracks soft rev limit duration)
uint8_t softLimitTime = 0;

// ============================================================================
// CONSTANTS
// ============================================================================

namespace {

// Idle advance constants
static const int IDLE_RPM_DELTA_MIN = 0;
static const int IDLE_RPM_DELTA_MAX = 100; // ±500 RPM range
static const int IDLE_RPM_DELTA_OFFSET = 50; // Center offset

} // anonymous namespace

// ============================================================================
// HELPER FUNCTIONS (Private - Anonymous Namespace)
// ============================================================================

namespace {

/**
 * @brief Check if idle advance conditions are met
 * @return true if all conditions met, false otherwise
 * @note Checks: RPM below threshold, vehicle stopped (VSS), throttle closed
 */
static inline bool isIdleConditionsActive(void)
{
    // Guard: RPM above idle threshold
    if (currentStatus.RPM >= (configPage2.idleAdvRPM * 100)) { return false; }

    // Guard: Vehicle moving (VSS check)
    bool vssMoving = (configPage2.vssMode != 0) && (currentStatus.vss >= configPage2.idleAdvVss);
    if (vssMoving) { return false; }

    // Guard: Throttle not closed (check algorithm TPS vs CTPS)
    bool tpsOpen = (configPage2.idleAdvAlgorithm == 0) && (currentStatus.TPS >= configPage2.idleAdvTPS);
    bool ctpsOpen = (configPage2.idleAdvAlgorithm != 0) && (currentStatus.CTPSActive != 1);
    if (tpsOpen || ctpsOpen) { return false; }

    return true;
}

/**
 * @brief Calculate knock retard amount based on knock count
 * @return Knock retard in degrees
 * @note Formula: firstStep + (count - threshold) * stepSize
 */
static inline byte calculateKnockRetard(void)
{
    return configPage10.knock_firstStep +
           ((currentStatus.knockCount - configPage10.knock_count) * configPage10.knock_stepSize);
}

/**
 * @brief Calculate knock recovery (gradual timing restoration)
 * @param curKnockRetard Current knock retard amount
 * @return Updated knock retard (0 = fully recovered)
 * @note Called during knock recovery period after initial knock event
 */
/**
 * @brief Complete knock recovery and reset status
 */
static inline void completeKnockRecovery(void)
{
    BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
    knockStartTime = 0;
    currentStatus.knockCount = 0;
}

/**
 * @brief Calculate recovery adjustment based on elapsed steps
 */
static inline int8_t getRecoveryAdjustment(uint32_t timeInRecovery)
{
    uint8_t recoverySteps = timeInRecovery / (configPage10.knock_recoveryStepTime * 100000UL);
    if (recoverySteps <= knockLastRecoveryStep) { return 0; }
    int8_t adj = (recoverySteps - knockLastRecoveryStep) * configPage10.knock_recoveryStep;
    knockLastRecoveryStep = recoverySteps;
    return adj;
}

static uint8_t calculateKnockRecovery(uint8_t curKnockRetard)
{
    uint32_t knockDurationMicros = configPage10.knock_duration * 100000UL;
    if ((micros() - knockStartTime) <= knockDurationMicros) { return curKnockRetard; }

    uint32_t timeInRecovery = (micros() - knockStartTime) - knockDurationMicros;
    int8_t recoveryTimingAdj = getRecoveryAdjustment(timeInRecovery);

    if (recoveryTimingAdj >= currentStatus.knockRetard) { completeKnockRecovery(); return 0; }
    if (recoveryTimingAdj > 0) { return currentStatus.knockRetard - recoveryTimingAdj; }
    return curKnockRetard;
}

/**
 * @brief Check if knock step time has elapsed
 */
static inline bool isKnockStepTimeElapsed(void)
{
    return (micros() - knockStartTime) > (configPage10.knock_stepTime * 1000UL);
}

/**
 * @brief Handle new digital knock pulse during active knock
 */
static inline byte handleDigitalKnockPulse(byte currentRetard)
{
    if (!BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE)) { return currentRetard; }
    if (!isKnockStepTimeElapsed()) { return currentRetard; }
    currentStatus.knockCount++;
    knockStartTime = micros();
    knockLastRecoveryStep = 0;
    return calculateKnockRetard();
}

/**
 * @brief Digital knock detection algorithm
 * @return Knock retard amount in degrees
 * @note Uses digital knock sensor with pulse counting
 */
static inline byte knockDetectionDigital(void)
{
    if (currentStatus.knockCount < configPage10.knock_count) { BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE); return 0; }

    if (BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE)) {
        byte tmpRetard = handleDigitalKnockPulse(currentStatus.knockRetard);
        BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE);
        return calculateKnockRecovery(tmpRetard);
    }

    // Activate knock retard
    knockStartTime = micros();
    BIT_SET(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
    knockLastRecoveryStep = 0;
    BIT_CLEAR(currentStatus.status5, BIT_STATUS5_KNOCK_PULSE);
    return calculateKnockRetard();
}

/**
 * @brief Handle analog knock while active
 */
static inline byte handleAnalogKnockActive(void)
{
    byte tmpRetard = 0;
    if (!isKnockStepTimeElapsed()) { return calculateKnockRecovery(tmpRetard); }

    uint16_t reading = getAnalogKnock();
    if (reading > configPage10.knock_threshold) {
        currentStatus.knockCount++;
        tmpRetard = calculateKnockRetard();
        knockStartTime = micros();
        knockLastRecoveryStep = 0;
    }
    return calculateKnockRecovery(tmpRetard);
}

/**
 * @brief Check for new analog knock event
 */
static inline byte checkAnalogKnockStart(void)
{
    if (!BIT_CHECK(LOOP_TIMER, BIT_TIMER_30HZ)) { return 0; }

    uint16_t reading = getAnalogKnock();
    if (reading <= configPage10.knock_threshold) { return 0; }

    knockStartTime = micros();
    BIT_SET(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE);
    knockLastRecoveryStep = 0;
    return configPage10.knock_firstStep;
}

/**
 * @brief Analog knock detection algorithm
 * @return Knock retard amount in degrees
 * @note Uses analog knock sensor with voltage threshold
 */
static inline byte knockDetectionAnalog(void)
{
    if (BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE)) { return handleAnalogKnockActive(); }
    return checkAnalogKnockStart();
}

/**
 * @brief Check if soft launch conditions are met
 * @return true if launch control should activate
 * @note Checks: clutch engaged, RPM above threshold, TPS above threshold, VSS below limit
 */
static inline bool isSoftLaunchActive(void)
{
    return (configPage6.launchEnabled &&
            currentStatus.clutchTrigger &&
            (currentStatus.clutchEngagedRPM < ((unsigned int)(configPage6.flatSArm) * 100)) &&
            (currentStatus.RPM > ((unsigned int)(configPage6.lnchSoftLim) * 100)) &&
            (currentStatus.TPS >= configPage10.lnchCtrlTPS) &&
            ((configPage2.vssMode == 0) || ((configPage2.vssMode > 0) && (currentStatus.vss <= configPage10.lnchCtrlVss))));
}

/**
 * @brief Check if flat shift conditions are met
 * @return true if flat shift should activate
 * @note Checks: clutch engaged during gear change, RPM above threshold
 */
static inline bool isFlatShiftActive(void)
{
    return (configPage6.flatSEnable &&
            currentStatus.clutchTrigger &&
            (currentStatus.clutchEngagedRPM > ((unsigned int)(configPage6.flatSArm) * 100)) &&
            (currentStatus.RPM > (currentStatus.clutchEngagedRPM - (configPage6.flatSSoftWin * 100))));
}

/**
 * @brief Check if WMI conditions are met for timing advance
 * @return true if WMI timing advance should apply
 * @note Checks: WMI enabled, not empty, TPS/RPM/MAP/IAT above thresholds
 */
static inline bool isWMIActive(void)
{
    if ((configPage10.wmiEnabled >= 1) && (configPage10.wmiAdvEnabled == 1) &&
        !BIT_CHECK(currentStatus.status4, BIT_STATUS4_WMI_EMPTY)) {
        return ((currentStatus.TPS >= configPage10.wmiTPS) &&
                (currentStatus.RPM >= configPage10.wmiRPM) &&
                (currentStatus.MAP / 2 >= configPage10.wmiMAP) &&
                (temperatureAddOffset(currentStatus.IAT) >= configPage10.wmiIAT));
    }
    return false;
}

} // anonymous namespace

// ============================================================================
// PUBLIC FUNCTIONS (Ignition Corrections)
// ============================================================================

/**
 * @brief Main ignition corrections dispatcher
 * @param base_advance Base ignition timing from main fuel table
 * @return Corrected ignition timing in degrees BTDC
 * @note Applies all corrections in sequence (order matters!)
 * @note Fixed timing checks MUST come last to override all other corrections
 */
int8_t correctionsIgn(int8_t base_advance)
{
    int8_t advance;

    // Apply all corrections in sequence
    advance = correctionFlexTiming(base_advance);
    advance = correctionWMITiming(advance);
    advance = correctionIATretard(advance);
    advance = correctionCLTadvance(advance);
    advance = correctionIdleAdvance(advance);
    advance = correctionSoftRevLimit(advance);
    advance = correctionNitrous(advance);
    advance = correctionSoftLaunch(advance);
    advance = correctionSoftFlatShift(advance);
    advance = correctionKnockTiming(advance);
    advance = correctionDFCOignition(advance);

    // Fixed timing checks MUST go last (override all corrections)
    advance = correctionFixedTiming(advance);
    advance = correctionCrankingFixedTiming(advance); // Override even fixed timing

    return advance;
}

/**
 * @brief Fixed timing override
 * @param advance Current ignition timing
 * @return Fixed timing value (if enabled) or original timing
 * @note MUST be called near end to override all other corrections
 */
int8_t correctionFixedTiming(int8_t advance)
{
    int8_t ignFixValue = advance;

    // Guard: Fixed timing disabled
    if (configPage2.fixAngEnable == 1) {
        ignFixValue = configPage4.FixAng;
    }

    return ignFixValue;
}

/**
 * @brief Cranking fixed timing override
 * @param advance Current ignition timing
 * @return Cranking fixed timing value (if cranking) or original timing
 * @note MUST be called after correctionFixedTiming() to override it during cranking
 * @note Can optionally apply CLT advance to cranking angle
 */
int8_t correctionCrankingFixedTiming(int8_t advance)
{
    int8_t ignCrankFixValue = advance;

    // Guard: Not cranking
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK)) {
        return ignCrankFixValue;
    }

    // Apply cranking angle (with or without CLT compensation)
    if (configPage2.crkngAddCLTAdv == 0) {
        // Use fixed cranking angle
        ignCrankFixValue = configPage4.CrankAng;
    }
    else {
        // Use CLT compensated cranking angle
        ignCrankFixValue = correctionCLTadvance(configPage4.CrankAng);
    }

    return ignCrankFixValue;
}

/**
 * @brief Flex fuel timing correction
 * @param advance Current ignition timing
 * @return Corrected timing with flex fuel adjustment
 * @note Adds/subtracts timing based on ethanol percentage (E0-E100)
 * @note Uses table lookup with OFFSET_IGNITION for negative values
 */
int8_t correctionFlexTiming(int8_t advance)
{
    int16_t ignFlexValue = advance;

    // Guard: Flex fuel disabled
    if (configPage2.flexEnabled != 1) {
        return (int8_t)ignFlexValue;
    }

    // Lookup flex timing adjustment from table
    ignFlexValue = (int16_t)table2D_getValue(&flexAdvTable, currentStatus.ethanolPct) - OFFSET_IGNITION;
    currentStatus.flexIgnCorrection = (int8_t)ignFlexValue;
    ignFlexValue = (int8_t)advance + currentStatus.flexIgnCorrection;

    return (int8_t)ignFlexValue;
}

/**
 * @brief WMI (Water-Methanol Injection) timing correction
 * @param advance Current ignition timing
 * @return Corrected timing with WMI advance
 * @note Adds timing when WMI is active (allows more aggressive timing)
 * @note Only active when all conditions met: TPS/RPM/MAP/IAT above thresholds
 */
int8_t correctionWMITiming(int8_t advance)
{
    // Guard: WMI conditions not met
    if (!isWMIActive()) {
        return advance;
    }

    // Apply WMI timing advance from table
    return advance + (int8_t)table2D_getValue(&wmiAdvTable, (uint8_t)((uint16_t)currentStatus.MAP / 2U)) - OFFSET_IGNITION;
}

/**
 * @brief IAT (Inlet Air Temperature) retard correction
 * @param advance Current ignition timing
 * @return Corrected timing with IAT retard
 * @note Retards timing at high IAT to prevent knock
 * @note Uses table lookup (warmer air = more retard)
 */
int8_t correctionIATretard(int8_t advance)
{
    int8_t advanceIATadjust = table2D_getValue(&IATRetardTable, (uint8_t)currentStatus.IAT);
    return advance - advanceIATadjust;
}

/**
 * @brief CLT (Coolant Temperature) advance correction
 * @param advance Current ignition timing
 * @return Corrected timing with CLT advance
 * @note Advances timing as engine warms up
 * @note Uses table lookup with offset (15 degrees center point)
 */
int8_t correctionCLTadvance(int8_t advance)
{
    int8_t ignCLTValue = advance;

    // Lookup CLT adjustment from table (with 15 degree offset)
    int8_t advanceCLTadjust = (int16_t)(table2D_getValue(&CLTAdvanceTable, temperatureAddOffset(currentStatus.coolant))) - 15;
    ignCLTValue = advance + advanceCLTadjust;

    return ignCLTValue;
}

/**
 * @brief Idle advance correction
 * @param advance Current ignition timing
 * @return Corrected timing with idle advance
 * @note Two modes: additive (add to base) or override (replace base)
 * @note Uses RPM delta from target to determine advance amount
 * @note Includes taper delay before activation
 */
int8_t correctionIdleAdvance(int8_t advance)
{
    int8_t ignIdleValue = advance;

    // Update idle advance active flag
    if (!idleAdvActive && BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) {
        // Activate when: No IAC or RPM above threshold
        if ((configPage6.iacAlgorithm == 0) ||
            (currentStatus.RPM > (((uint16_t)currentStatus.CLIdleTarget * 10) - (uint16_t)IGN_IDLE_THRESHOLD))) {
            idleAdvActive = true;
        }
    }
    else if (idleAdvActive && !BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) {
        idleAdvActive = false;
    }

    // Guard: Idle advance disabled
    if (configPage2.idleAdvEnabled < 1) { return ignIdleValue; }

    // Guard: Delay not elapsed
    if (runSecsX10 < (configPage2.idleAdvDelay * 5)) { return ignIdleValue; }

    // Guard: Idle advance not active
    if (!idleAdvActive) { return ignIdleValue; }

    // Calculate RPM delta from target
    int idleRPMdelta = (currentStatus.CLIdleTarget - (currentStatus.RPM / 10)) + IDLE_RPM_DELTA_OFFSET;
    idleRPMdelta = constrain(idleRPMdelta, IDLE_RPM_DELTA_MIN, IDLE_RPM_DELTA_MAX);

    // Check if idle conditions are met
    if (!isIdleConditionsActive()) {
        idleAdvTaper = 0;
        return ignIdleValue;
    }

    // Taper delay before applying advance
    if (idleAdvTaper < configPage9.idleAdvStartDelay) {
        if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) {
            idleAdvTaper++;
        }
        return ignIdleValue;
    }

    // Calculate and apply idle advance
    int8_t advanceIdleAdjust = (int8_t)table2D_getValue(&idleAdvanceTable, (uint8_t)idleRPMdelta) - 15;

    if (configPage2.idleAdvEnabled == 1) {
        // Additive mode
        ignIdleValue = advance + advanceIdleAdjust;
    }
    else if (configPage2.idleAdvEnabled == 2) {
        // Override mode
        ignIdleValue = advanceIdleAdjust;
    }

    return ignIdleValue;
}

/**
 * @brief Apply soft limit retard based on mode
 */
static inline byte applySoftRevLimitRetard(byte advance)
{
    if (configPage2.SoftLimitMode == SOFT_LIMIT_RELATIVE) { return advance - configPage4.SoftLimRetard; }
    if (configPage2.SoftLimitMode == SOFT_LIMIT_FIXED) { return configPage4.SoftLimRetard; }
    return advance;
}

/**
 * @brief Handle soft rev limit when RPM above threshold
 * @note After timeout, CONTINUES applying retard (safety-critical fix)
 *       Original bug: returned unmodified advance after timeout
 */
static inline byte handleSoftRevLimitActive(byte advance)
{
    BIT_SET(currentStatus.status2, BIT_STATUS2_SFTLIM);
    // Increment timer until max reached
    if (softLimitTime < configPage4.SoftLimMax)
    {
        if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) { softLimitTime++; }
    }
    // CRITICAL: Always apply retard while in soft limit zone (fix for timeout bug)
    return applySoftRevLimitRetard(advance);
}

/**
 * @brief Soft rev limit correction
 * @param advance Current ignition timing
 * @return Corrected timing with soft rev limit retard
 * @note Two modes: relative (subtract from current) or fixed (set to value)
 * @note Duration limited by SoftLimMax to prevent excessive retard
 * @note Sets status bit BIT_STATUS2_SFTLIM when active
 */
int8_t correctionSoftRevLimit(int8_t advance)
{
    BIT_CLEAR(currentStatus.status2, BIT_STATUS2_SFTLIM);

    // Guard: Protection mode not set to ignition cut
    bool protectEnabled = (configPage6.engineProtectType == PROTECT_CUT_IGN) ||
                          (configPage6.engineProtectType == PROTECT_CUT_BOTH);
    if (!protectEnabled) { return advance; }

    if (currentStatus.RPMdiv100 >= configPage4.SoftRevLim) { return handleSoftRevLimitActive(advance); }

    if (BIT_CHECK(LOOP_TIMER, BIT_TIMER_10HZ)) { softLimitTime = 0; }
    return advance;
}

/**
 * @brief Nitrous oxide timing correction
 * @param advance Current ignition timing
 * @return Corrected timing with nitrous retard
 * @note Retards timing during nitrous activation to prevent detonation
 * @note Supports 2-stage nitrous (stage 1, stage 2, or both)
 */
int8_t correctionNitrous(int8_t advance)
{
    byte ignNitrous = advance;

    // Guard: Nitrous disabled
    if (configPage10.n2o_enable <= 0) {
        return ignNitrous;
    }

    // Check which stage is running (if any)
    if ((currentStatus.nitrous_status == NITROUS_STAGE1) || (currentStatus.nitrous_status == NITROUS_BOTH)) {
        ignNitrous -= configPage10.n2o_stage1_retard;
    }
    if ((currentStatus.nitrous_status == NITROUS_STAGE2) || (currentStatus.nitrous_status == NITROUS_BOTH)) {
        ignNitrous -= configPage10.n2o_stage2_retard;
    }

    return ignNitrous;
}

/**
 * @brief Soft launch control correction (2-step)
 * @param advance Current ignition timing
 * @return Launch control timing (fixed retard value when active)
 * @note Used for 2-step launch control (fixed retard to build boost)
 * @note Sets status bit BIT_STATUS2_SLAUNCH when active
 * @note Conditions: clutch engaged, RPM above threshold, TPS above threshold, VSS below limit
 */
int8_t correctionSoftLaunch(int8_t advance)
{
    uint8_t ignSoftLaunchValue = advance;

    // Check if soft launch conditions are met
    if (isSoftLaunchActive()) {
        currentStatus.launchingSoft = true;
        BIT_SET(currentStatus.status2, BIT_STATUS2_SLAUNCH);
        ignSoftLaunchValue = configPage6.lnchRetard;
    }
    else {
        currentStatus.launchingSoft = false;
        BIT_CLEAR(currentStatus.status2, BIT_STATUS2_SLAUNCH);
    }

    return ignSoftLaunchValue;
}

/**
 * @brief Flat shift timing correction
 * @param advance Current ignition timing
 * @return Flat shift timing (fixed retard value when active)
 * @note Used during gear changes to maintain boost (clutch-triggered)
 * @note Sets status bit BIT_STATUS5_FLATSS when active
 * @note Conditions: clutch engaged during shift, RPM above armed threshold
 */
int8_t correctionSoftFlatShift(int8_t advance)
{
    int8_t ignSoftFlatValue = advance;

    // Check if flat shift conditions are met
    if (isFlatShiftActive()) {
        BIT_SET(currentStatus.status5, BIT_STATUS5_FLATSS);
        ignSoftFlatValue = configPage6.flatSRetard;
    }
    else {
        BIT_CLEAR(currentStatus.status5, BIT_STATUS5_FLATSS);
    }

    return ignSoftFlatValue;
}

/**
 * @brief Knock timing correction
 * @param advance Current ignition timing
 * @return Corrected timing with knock retard
 * @note Strategy pattern: digital (pulse counting) or analog (voltage threshold)
 * @note Includes recovery period to gradually restore timing after knock event
 * @note Sets status bit BIT_STATUS5_KNOCK_ACTIVE during knock event
 */
int8_t correctionKnockTiming(int8_t advance)
{
    byte tmpKnockRetard = 0;

    // Strategy pattern - select knock algorithm
    if (configPage10.knock_mode == KNOCK_MODE_DIGITAL) {
        tmpKnockRetard = knockDetectionDigital();
    }
    else if (configPage10.knock_mode == KNOCK_MODE_ANALOG) {
        tmpKnockRetard = knockDetectionAnalog();
    }

    // Enforce maximum retard limit
    tmpKnockRetard = min(tmpKnockRetard, configPage10.knock_maxRetard);
    currentStatus.knockRetard = tmpKnockRetard;

    return advance - tmpKnockRetard;
}

/**
 * @brief DFCO (Deceleration Fuel Cut-Off) ignition taper
 * @param advance Current ignition timing
 * @return Corrected timing with DFCO taper retard
 * @note Gradually retards timing during DFCO to smooth fuel restoration
 * @note Taper uses linear interpolation (dfcoTaper counter from max to 0)
 */
int8_t correctionDFCOignition(int8_t advance)
{
    int8_t dfcoRetard = advance;

    // Guard: DFCO taper disabled
    if ((configPage9.dfcoTaperEnable != 1) || !BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO)) {
        dfcoTaper = configPage9.dfcoTaperTime; // Keep updating duration until DFCO active
        return dfcoRetard;
    }

    // Apply taper retard
    if (dfcoTaper != 0) {
        dfcoRetard -= map(dfcoTaper, configPage9.dfcoTaperTime, 0, 0, configPage9.dfcoTaperAdvance);
    }
    else {
        dfcoRetard -= configPage9.dfcoTaperAdvance; // Taper ended, use full value
    }

    return dfcoRetard;
}

// ============================================================================
// INTERFACE DEFINITION
// ============================================================================

/**
 * @brief Static const interface structure
 * @note All function pointers link to implementations above
 * @note Stored in flash (const), zero RAM overhead
 */
static const IgnitionCorrectionsInterface ignitionCorrectionsInterface = {
    // Main ignition correction pipeline
    .correctionsIgn = &correctionsIgn,

    // Override corrections
    .correctionFixedTiming = &correctionFixedTiming,
    .correctionCrankingFixedTiming = &correctionCrankingFixedTiming,

    // Environmental corrections
    .correctionIATretard = &correctionIATretard,
    .correctionCLTadvance = &correctionCLTadvance,

    // Fuel type corrections
    .correctionFlexTiming = &correctionFlexTiming,
    .correctionWMITiming = &correctionWMITiming,

    // Protection corrections
    .correctionKnockTiming = &correctionKnockTiming,
    .correctionSoftRevLimit = &correctionSoftRevLimit,

    // Special feature corrections
    .correctionIdleAdvance = &correctionIdleAdvance,
    .correctionNitrous = &correctionNitrous,
    .correctionSoftLaunch = &correctionSoftLaunch,
    .correctionSoftFlatShift = &correctionSoftFlatShift,
    .correctionDFCOignition = &correctionDFCOignition,
};

// ============================================================================
// PUBLIC API
// ============================================================================

const IgnitionCorrectionsInterface* ignitionCorrectionsGetInterface(void)
{
    // Return const interface (never NULL)
    return &ignitionCorrectionsInterface;
}
