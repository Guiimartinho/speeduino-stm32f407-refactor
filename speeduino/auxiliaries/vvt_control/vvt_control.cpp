/**
 * @file vvt_control.cpp
 * @brief Variable Valve Timing control implementation
 * @details Supports open-loop, on/off, and closed-loop PID control for VVT1 and VVT2
 *
 * Modes:
 * - Open Loop: Duty cycle from 3D table (MAP/TPS vs RPM)
 * - On/Off: Binary switching based on table lookup
 * - Closed Loop: PID control to target cam angle
 *
 * @author Speeduino Team
 * @date 2025-01-29
 * @version 1.0
 */

#include "vvt_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../table3d.h"
#include "../../decoders.h"
#include "../../idle.h"
#include "../../units.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope, reduced nesting)
// =============================================================================

// VVT control state
struct VVTState {
    bool isHot;           ///< True after warmup delay complete
    bool timeHoldActive;  ///< True when warmup timer started
    uint32_t warmTime;    ///< Time when engine reached operating temp
    uint32_t counter;     ///< Update counter for PID tuning
};

static VVTState vvtState;

/**
 * @brief Get VVT duty from table based on load source
 */
static inline uint8_t getVVTDutyFromTable(table3d8RpmLoad* table)
{
    if (configPage6.vvtLoadSource == VVT_LOAD_TPS)
    {
        return get3DTableValue(table, (currentStatus.TPS * 2U), currentStatus.RPM);
    }
    return get3DTableValue(table, (uint16_t)currentStatus.MAP, currentStatus.RPM);
}

/**
 * @brief Apply On/Off mode threshold
 */
static inline uint8_t applyOnOffThreshold(uint8_t duty)
{
    if ((configPage6.vvtMode == VVT_MODE_ONOFF) && (duty < 200U)) { return 0U; }
    return duty;
}

/**
 * @brief Calculate open loop duty for VVT1
 */
static void calculateOpenLoopVVT1(void)
{
    uint8_t duty = getVVTDutyFromTable(&vvtTable);
    duty = applyOnOffThreshold(duty);
    currentStatus.vvt1Duty = duty;
    vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);
}

/**
 * @brief Calculate open loop duty for VVT2
 */
static void calculateOpenLoopVVT2(void)
{
    if (configPage10.vvt2Enabled != 1U) { return; }

    uint8_t duty = getVVTDutyFromTable(&vvt2Table);
    duty = applyOnOffThreshold(duty);
    currentStatus.vvt2Duty = duty;
    vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
}

/**
 * @brief Update PID tuning parameters (infrequent update)
 */
static void updatePIDTunings(void)
{
    vvtPID.SetTunings(configPage10.vvtCLKP, configPage10.vvtCLKI, configPage10.vvtCLKD);
    vvtPID.SetControllerDirection(configPage6.vvtPWMdir);

    if (configPage10.vvt2Enabled != 1U) { return; }

    vvt2PID.SetTunings(configPage10.vvtCLKP, configPage10.vvtCLKI, configPage10.vvtCLKD);
    vvt2PID.SetControllerDirection(configPage4.vvt2PWMdir);
}

/**
 * @brief Check if cam angle is within valid range
 */
static inline bool isAngleValid(uint8_t angle)
{
    return (angle > configPage10.vvtCLMinAng) && (angle <= configPage10.vvtCLMaxAng);
}

/**
 * @brief Calculate closed-loop PID for VVT1
 */
static void calculateClosedLoopVVT1(void)
{
    currentStatus.vvt1TargetAngle = getVVTDutyFromTable(&vvtTable);

    // Safety check: angle sensor working?
    if (!isAngleValid(currentStatus.vvt1Angle))
    {
        currentStatus.vvt1Duty = 0U;
        vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);
        BIT_SET(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
        return;
    }

    // Check if already at target (hold mode)
    bool useHold = (configPage6.vvtCLUseHold > 0U);
    bool atTarget = (currentStatus.vvt1TargetAngle == currentStatus.vvt1Angle);
    if (useHold && atTarget)
    {
        currentStatus.vvt1Duty = configPage10.vvtCLholdDuty;
        vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);
        vvtPID.Initialize();
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
        return;
    }

    // Run PID computation
    vvt_pid_target_angle = (unsigned long)currentStatus.vvt1TargetAngle;
    vvt_pid_current_angle = (long)currentStatus.vvt1Angle;

    if (vvtPID.Compute(true)) { vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count); }
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
}

/**
 * @brief Calculate closed-loop PID for VVT2
 */
static void calculateClosedLoopVVT2(void)
{
    if (configPage10.vvt2Enabled != 1U) { return; }

    currentStatus.vvt2TargetAngle = getVVTDutyFromTable(&vvt2Table);

    // Safety check: angle sensor working?
    if (!isAngleValid(currentStatus.vvt2Angle))
    {
        currentStatus.vvt2Duty = 0U;
        vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
        BIT_SET(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
        return;
    }

    // Check if already at target (hold mode)
    bool useHold = (configPage6.vvtCLUseHold > 0U);
    bool atTarget = (currentStatus.vvt2TargetAngle == currentStatus.vvt2Angle);
    if (useHold && atTarget)
    {
        currentStatus.vvt2Duty = configPage10.vvtCLholdDuty;
        vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
        vvt2PID.Initialize();
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
        return;
    }

    // Run PID computation
    vvt2_pid_target_angle = (unsigned long)currentStatus.vvt2TargetAngle;
    vvt2_pid_current_angle = (long)currentStatus.vvt2Angle;

    if (vvt2PID.Compute(true)) { vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count); }
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
}

/**
 * @brief Set PWM outputs for both VVT channels
 */
static void setPWMOutputsStandalone(void)
{
    bool bothOff = (currentStatus.vvt1Duty == 0U) && (currentStatus.vvt2Duty == 0U);
    bool bothMax = (currentStatus.vvt1Duty >= 200U) && (currentStatus.vvt2Duty >= 200U);

    if (bothOff)
    {
        VVT1_PIN_OFF(); VVT2_PIN_OFF();
        vvt1_pwm_state = false; vvt1_max_pwm = false;
        vvt2_pwm_state = false; vvt2_max_pwm = false;
        DISABLE_VVT_TIMER();
        return;
    }

    if (bothMax)
    {
        VVT1_PIN_ON(); VVT2_PIN_ON();
        vvt1_pwm_state = true; vvt1_max_pwm = true;
        vvt2_pwm_state = true; vvt2_max_pwm = true;
        DISABLE_VVT_TIMER();
        return;
    }

    ENABLE_VVT_TIMER();
    vvt1_max_pwm = (currentStatus.vvt1Duty >= 200U);
    vvt2_max_pwm = (currentStatus.vvt2Duty >= 200U);
}

/**
 * @brief Set PWM output for VVT1 only
 */
static void setPWMOutputsVVT1Only(void)
{
    if (currentStatus.vvt1Duty == 0U)
    {
        VVT1_PIN_OFF();
        vvt1_pwm_state = false;
        vvt1_max_pwm = false;
        return;
    }

    if (currentStatus.vvt1Duty >= 200U)
    {
        VVT1_PIN_ON();
        vvt1_pwm_state = true;
        vvt1_max_pwm = true;
        return;
    }

    ENABLE_VVT_TIMER();
    vvt1_max_pwm = false;
}

/**
 * @brief Check warmup conditions
 */
static bool checkWarmupComplete(void)
{
    if (vvtState.isHot) { return true; }

    const uint32_t elapsedTime = runSecsX10 - vvtState.warmTime;
    const uint32_t requiredTime = (uint32_t)configPage4.vvtDelay * VVT_TIME_DELAY_MULTIPLIER;

    if (elapsedTime < requiredTime) { return false; }

    vvtState.isHot = true;
    return true;
}

/**
 * @brief Execute VVT control mode
 */
static void executeControlMode(void)
{
    bool isOpenLoop = (configPage6.vvtMode == VVT_MODE_OPEN_LOOP) || (configPage6.vvtMode == VVT_MODE_ONOFF);

    if (isOpenLoop)
    {
        calculateOpenLoopVVT1();
        calculateOpenLoopVVT2();
        return;
    }

    if (configPage6.vvtMode != VVT_MODE_CLOSED_LOOP) { return; }

    if ((vvtState.counter & 31U) == 1U) { updatePIDTunings(); }

    calculateClosedLoopVVT1();
    calculateClosedLoopVVT2();
    vvtState.counter++;
}

/**
 * @brief Internal disable implementation
 */
static void vvt_disable_impl(void)
{
    currentStatus.vvt1Duty = 0U;
    vvt1_pwm_value = 0;
    vvt1_pwm_state = false;
    vvt1_max_pwm = false;

    if (configPage10.wmiEnabled == 0U)
    {
        DISABLE_VVT_TIMER();
        currentStatus.vvt2Duty = 0U;
        vvt2_pwm_value = 0;
        vvt2_pwm_state = false;
        vvt2_max_pwm = false;
    }

    vvtState.timeHoldActive = false;
}

/**
 * @brief Internal update implementation
 */
static void vvt_update_impl(void)
{
    // Guard: VVT not enabled
    if (configPage6.vvtEnabled != 1U) { vvt_disable_impl(); return; }

    // Guard: engine not running
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) { vvt_disable_impl(); return; }

    // Guard: coolant too cold
    if (currentStatus.coolant < temperatureRemoveOffset(configPage4.vvtMinClt)) { vvt_disable_impl(); return; }

    // Start warmup timer
    if (!vvtState.timeHoldActive) { vvtState.warmTime = runSecsX10; vvtState.timeHoldActive = true; }

    // Calculate cam angle for Miata trigger
    if (configPage4.TrigPattern == 9U) { currentStatus.vvt1Angle = getCamAngle_Miata9905(); }

    // Check warmup complete
    if (!checkWarmupComplete()) { return; }

    // Execute control mode
    executeControlMode();

    // Set PWM outputs
    if (configPage10.wmiEnabled == 0U) { setPWMOutputsStandalone(); }
    else { setPWMOutputsVVT1Only(); }
}

/**
 * @brief Internal initialise implementation
 */
static bool vvt_initialise_impl(void)
{
    vvtState.isHot = false;
    vvtState.timeHoldActive = false;
    vvtState.warmTime = 0U;
    vvtState.counter = 0U;

    currentStatus.vvt1Duty = 0U;
    currentStatus.vvt2Duty = 0U;
    vvt1_pwm_value = 0;
    vvt2_pwm_value = 0;
    vvt1_pwm_state = false;
    vvt2_pwm_state = false;
    vvt1_max_pwm = false;
    vvt2_max_pwm = false;

    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);

    return true;
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace vvt {

bool initialise(void) { return vvt_initialise_impl(); }
void update(void) { vvt_update_impl(); }
void disable(void) { vvt_disable_impl(); }

uint8_t getVVT1Duty(void) {
    return currentStatus.vvt1Duty;
}

uint8_t getVVT2Duty(void) {
    return currentStatus.vvt2Duty;
}

uint8_t getVVT1Angle(void) {
    return currentStatus.vvt1Angle;
}

uint8_t getVVT2Angle(void) {
    return currentStatus.vvt2Angle;
}

bool isVVT1Error(void) {
    return BIT_CHECK(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
}

bool isVVT2Error(void) {
    return BIT_CHECK(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
}

} // namespace vvt
} // namespace speeduino
