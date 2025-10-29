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

namespace speeduino {
namespace vvt {

// VVT PWM state (declared in auxiliaries.h, accessed from global scope)
using ::vvt1_pwm_value;
using ::vvt2_pwm_value;
using ::vvt1_pwm_state;
using ::vvt2_pwm_state;
using ::vvt1_max_pwm;
using ::vvt2_max_pwm;
using ::vvt_pwm_max_count;

// PID input/output variables (declared in auxiliaries.h, accessed from global scope)
using ::vvt_pid_target_angle;
using ::vvt_pid_current_angle;
using ::vvt2_pid_target_angle;
using ::vvt2_pid_current_angle;

// Anonymous namespace for private implementation
namespace {

// VVT control state
struct VVTState {
    bool isHot;           ///< True after warmup delay complete
    bool timeHoldActive;  ///< True when warmup timer started
    uint32_t warmTime;    ///< Time when engine reached operating temp
    uint32_t counter;     ///< Update counter for PID tuning
};

static VVTState state;

/**
 * @brief Calculate duty cycle from table (open loop)
 * @details Lookup based on MAP or TPS vs RPM
 * @param channelNum VVT channel (1 or 2)
 */
static void calculateOpenLoopDuty(uint8_t channelNum) {
    uint8_t duty = 0U;

    if (channelNum == 1U) {
        // VVT1 table lookup
        if (configPage6.vvtLoadSource == VVT_LOAD_TPS) {
            duty = get3DTableValue(&vvtTable, (currentStatus.TPS * 2U), currentStatus.RPM);
        } else {
            duty = get3DTableValue(&vvtTable, (uint16_t)currentStatus.MAP, currentStatus.RPM);
        }

        // On/Off mode: force to 0 if below threshold
        if ((configPage6.vvtMode == VVT_MODE_ONOFF) && (duty < 200U)) {
            duty = 0U;
        }

        currentStatus.vvt1Duty = duty;
        vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);

    } else if (channelNum == 2U) {
        // VVT2 table lookup
        if (configPage10.vvt2Enabled != 1U) {
            return;
        }

        if (configPage6.vvtLoadSource == VVT_LOAD_TPS) {
            duty = get3DTableValue(&vvt2Table, (currentStatus.TPS * 2U), currentStatus.RPM);
        } else {
            duty = get3DTableValue(&vvt2Table, (uint16_t)currentStatus.MAP, currentStatus.RPM);
        }

        // On/Off mode: force to 0 if below threshold
        if ((configPage6.vvtMode == VVT_MODE_ONOFF) && (duty < 200U)) {
            duty = 0U;
        }

        currentStatus.vvt2Duty = duty;
        vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
    }
}

/**
 * @brief Update PID tuning parameters (infrequent update)
 * @details Called once every 32 updates (~1 second)
 */
static void updatePIDTunings(void) {
    // Update VVT1 PID
    vvtPID.SetTunings(configPage10.vvtCLKP, configPage10.vvtCLKI, configPage10.vvtCLKD);
    vvtPID.SetControllerDirection(configPage6.vvtPWMdir);

    // Update VVT2 PID if enabled
    if (configPage10.vvt2Enabled == 1U) {
        vvt2PID.SetTunings(configPage10.vvtCLKP, configPage10.vvtCLKI, configPage10.vvtCLKD);
        vvt2PID.SetControllerDirection(configPage4.vvt2PWMdir);
    }
}

/**
 * @brief Check if cam angle is within valid range
 * @param angle Current cam angle
 * @return true if angle is valid, false if out of range
 */
static bool isAngleValid(uint8_t angle) {
    if (angle <= configPage10.vvtCLMinAng) {
        return false;
    }
    if (angle > configPage10.vvtCLMaxAng) {
        return false;
    }
    return true;
}

/**
 * @brief Calculate duty cycle using closed-loop PID (VVT1)
 * @details PID control to reach target cam angle
 */
static void calculateClosedLoopVVT1(void) {
    // Lookup target angle from table
    if (configPage6.vvtLoadSource == VVT_LOAD_TPS) {
        currentStatus.vvt1TargetAngle = get3DTableValue(&vvtTable, (currentStatus.TPS * 2U), currentStatus.RPM);
    } else {
        currentStatus.vvt1TargetAngle = get3DTableValue(&vvtTable, (uint16_t)currentStatus.MAP, currentStatus.RPM);
    }

    // Safety check: angle sensor working?
    if (!isAngleValid(currentStatus.vvt1Angle)) {
        currentStatus.vvt1Duty = 0U;
        vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);
        BIT_SET(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
        return;
    }

    // Check if already at target (hold mode)
    if ((configPage6.vvtCLUseHold > 0U) && (currentStatus.vvt1TargetAngle == currentStatus.vvt1Angle)) {
        currentStatus.vvt1Duty = configPage10.vvtCLholdDuty;
        vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);
        vvtPID.Initialize();
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
        return;
    }

    // Run PID computation
    vvt_pid_target_angle = (unsigned long)currentStatus.vvt1TargetAngle;
    vvt_pid_current_angle = (long)currentStatus.vvt1Angle;

    bool pidCompute = vvtPID.Compute(true);
    if (pidCompute == true) {
        vvt1_pwm_value = halfPercentage(currentStatus.vvt1Duty, vvt_pwm_max_count);
    }
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT1_ERROR);
}

/**
 * @brief Calculate duty cycle using closed-loop PID (VVT2)
 * @details PID control to reach target cam angle
 */
static void calculateClosedLoopVVT2(void) {
    // Guard clause: VVT2 not enabled
    if (configPage10.vvt2Enabled != 1U) {
        return;
    }

    // Lookup target angle from table
    if (configPage6.vvtLoadSource == VVT_LOAD_TPS) {
        currentStatus.vvt2TargetAngle = get3DTableValue(&vvt2Table, (currentStatus.TPS * 2U), currentStatus.RPM);
    } else {
        currentStatus.vvt2TargetAngle = get3DTableValue(&vvt2Table, (uint16_t)currentStatus.MAP, currentStatus.RPM);
    }

    // Safety check: angle sensor working?
    if (!isAngleValid(currentStatus.vvt2Angle)) {
        currentStatus.vvt2Duty = 0U;
        vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
        BIT_SET(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
        return;
    }

    // Check if already at target (hold mode)
    if ((configPage6.vvtCLUseHold > 0U) && (currentStatus.vvt2TargetAngle == currentStatus.vvt2Angle)) {
        currentStatus.vvt2Duty = configPage10.vvtCLholdDuty;
        vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
        vvt2PID.Initialize();
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
        return;
    }

    // Run PID computation
    vvt2_pid_target_angle = (unsigned long)currentStatus.vvt2TargetAngle;
    vvt2_pid_current_angle = (long)currentStatus.vvt2Angle;

    bool pidCompute = vvt2PID.Compute(true);
    if (pidCompute == true) {
        vvt2_pwm_value = halfPercentage(currentStatus.vvt2Duty, vvt_pwm_max_count);
    }
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_VVT2_ERROR);
}

/**
 * @brief Set PWM outputs based on duty cycles (no WMI sharing)
 * @details Controls VVT1 and VVT2 pins when WMI not using VVT2 pin
 */
static void setPWMOutputsStandalone(void) {
    // Both channels at 0%
    if ((currentStatus.vvt1Duty == 0U) && (currentStatus.vvt2Duty == 0U)) {
        VVT1_PIN_OFF();
        VVT2_PIN_OFF();
        vvt1_pwm_state = false;
        vvt1_max_pwm = false;
        vvt2_pwm_state = false;
        vvt2_max_pwm = false;
        DISABLE_VVT_TIMER();
        return;
    }

    // Both channels at 100%
    if ((currentStatus.vvt1Duty >= 200U) && (currentStatus.vvt2Duty >= 200U)) {
        VVT1_PIN_ON();
        VVT2_PIN_ON();
        vvt1_pwm_state = true;
        vvt1_max_pwm = true;
        vvt2_pwm_state = true;
        vvt2_max_pwm = true;
        DISABLE_VVT_TIMER();
        return;
    }

    // Variable duty: enable timer
    ENABLE_VVT_TIMER();
    if (currentStatus.vvt1Duty < 200U) {
        vvt1_max_pwm = false;
    }
    if (currentStatus.vvt2Duty < 200U) {
        vvt2_max_pwm = false;
    }
}

/**
 * @brief Set PWM output for VVT1 only (WMI using VVT2 pin)
 * @details Controls only VVT1 pin when VVT2 pin shared with WMI
 */
static void setPWMOutputsVVT1Only(void) {
    // VVT1 at 0%
    if (currentStatus.vvt1Duty == 0U) {
        VVT1_PIN_OFF();
        vvt1_pwm_state = false;
        vvt1_max_pwm = false;
        return;
    }

    // VVT1 at 100%
    if (currentStatus.vvt1Duty >= 200U) {
        VVT1_PIN_ON();
        vvt1_pwm_state = true;
        vvt1_max_pwm = true;
        return;
    }

    // VVT1 variable duty: enable timer
    ENABLE_VVT_TIMER();
    if (currentStatus.vvt1Duty < 200U) {
        vvt1_max_pwm = false;
    }
}

} // anonymous namespace

// Public interface implementation

bool initialise(void) {
    state.isHot = false;
    state.timeHoldActive = false;
    state.warmTime = 0U;
    state.counter = 0U;

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

void update(void) {
    // Guard clause: VVT not enabled
    if (configPage6.vvtEnabled != 1U) {
        disable();
        return;
    }

    // Guard clause: engine not running or coolant too cold
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) {
        disable();
        return;
    }

    if (currentStatus.coolant < temperatureRemoveOffset(configPage4.vvtMinClt)) {
        disable();
        return;
    }

    // Start warmup timer
    if (state.timeHoldActive == false) {
        state.warmTime = runSecsX10;
        state.timeHoldActive = true;
    }

    // Calculate current cam angle for Miata trigger
    if (configPage4.TrigPattern == 9U) {
        currentStatus.vvt1Angle = getCamAngle_Miata9905();
    }

    // Check if warmup delay complete
    if (state.isHot == false) {
        const uint32_t elapsedTime = runSecsX10 - state.warmTime;
        const uint32_t requiredTime = (uint32_t)configPage4.vvtDelay * VVT_TIME_DELAY_MULTIPLIER;

        if (elapsedTime < requiredTime) {
            return;  // Still warming up
        }

        state.isHot = true;
    }

    // Execute control mode
    if ((configPage6.vvtMode == VVT_MODE_OPEN_LOOP) || (configPage6.vvtMode == VVT_MODE_ONOFF)) {
        // Open loop mode
        calculateOpenLoopDuty(1U);  // VVT1
        calculateOpenLoopDuty(2U);  // VVT2 (if enabled)

    } else if (configPage6.vvtMode == VVT_MODE_CLOSED_LOOP) {
        // Closed loop mode

        // Update PID tunings every 32 calls (~1 second)
        if ((state.counter & 31U) == 1U) {
            updatePIDTunings();
        }

        calculateClosedLoopVVT1();
        calculateClosedLoopVVT2();

        state.counter++;
    }

    // Set PWM outputs
    if (configPage10.wmiEnabled == 0U) {
        // No WMI: control both VVT1 and VVT2
        setPWMOutputsStandalone();
    } else {
        // WMI enabled: only control VVT1 (VVT2 pin shared with WMI)
        setPWMOutputsVVT1Only();
    }
}

void disable(void) {
    // Disable VVT1
    currentStatus.vvt1Duty = 0U;
    vvt1_pwm_value = 0;
    vvt1_pwm_state = false;
    vvt1_max_pwm = false;

    // Disable VVT2 (if not used by WMI)
    if (configPage10.wmiEnabled == 0U) {
        DISABLE_VVT_TIMER();
        currentStatus.vvt2Duty = 0U;
        vvt2_pwm_value = 0;
        vvt2_pwm_state = false;
        vvt2_max_pwm = false;
    }

    // Reset state
    state.timeHoldActive = false;
}

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
