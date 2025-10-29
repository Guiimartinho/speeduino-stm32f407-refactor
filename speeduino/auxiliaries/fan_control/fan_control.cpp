/**
 * @file fan_control.cpp
 * @brief Cooling fan control implementation
 */

#include "fan_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"
#include "../../table2d.h"

namespace speeduino {
namespace fan {

// Anonymous namespace for private implementation
namespace {

// Fan PWM state (only used on platforms with PWM_FAN_AVAILABLE)
#if defined(PWM_FAN_AVAILABLE)
struct FanPWMState {
    volatile bool pwm_state;
    uint16_t pwm_max_count;
    volatile uint16_t pwm_cur_value;
    long pwm_value;
};

static FanPWMState pwm_state;
#endif

// Fan table (defined in auxiliaries.cpp originally)
extern table2D_u8_u8_4 fanPWMTable;

/**
 * @brief Check if fan is permitted to run
 * @details Checks engine running status and fanWhenOff config
 * @return true if fan allowed to run
 */
static bool isFanPermitted(void) {
    // Fan when off enabled?
    if (configPage2.fanWhenOff == 1U) {
        return true;
    }

    // Otherwise requires engine running
    return BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN);
}

/**
 * @brief Check if A/C requires fan activation
 * @details A/C can force fan on for radiator airflow
 * @return true if A/C requests fan
 */
static bool acRequestsFan(void) {
    // Guard clause: A/C fan integration disabled?
    if (configPage15.airConTurnsFanOn != 1U) {
        return false;
    }

    // Check if A/C is turning on
    return BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
}

/**
 * @brief Turn fan on (digital mode)
 */
static void turnFanOn(void) {
    FAN_ON();
    BIT_SET(currentStatus.status4, BIT_STATUS4_FAN);
}

/**
 * @brief Turn fan off (digital mode)
 */
static void turnFanOff(void) {
    FAN_OFF();
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
}

/**
 * @brief Handle digital (on/off) fan control
 * @details Uses temperature thresholds with hysteresis
 */
static void updateDigitalMode(void) {
    const int16_t onTemp = temperatureRemoveOffset(configPage6.fanSP);
    const int16_t offTemp = onTemp - configPage6.fanHyster;

    // Guard clause: fan not permitted?
    if (!isFanPermitted()) {
        turnFanOff();
        return;
    }

    // Check if fan should be on
    const bool tempRequiresFan = (currentStatus.coolant >= onTemp);
    const bool acRequiresFan = acRequestsFan();

    // Guard clause: neither temp nor A/C requires fan?
    if (!tempRequiresFan && !acRequiresFan) {
        // Check if below off temperature
        if (currentStatus.coolant <= offTemp) {
            turnFanOff();
        }
        return;
    }

    // Fan needed - check cranking disable
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) &&
        (configPage2.fanWhenCranking == 0U)) {
        // User disabled fan during cranking
        turnFanOff();
        return;
    }

    // All checks passed - turn fan on
    turnFanOn();
}

/**
 * @brief Handle PWM fan control
 * @details Duty cycle from table or A/C minimum
 */
static void updatePWMMode(void) {
    // Guard clause: fan not permitted?
    if (!isFanPermitted()) {
        currentStatus.fanDuty = 0U;
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
#if defined(PWM_FAN_AVAILABLE)
        DISABLE_FAN_TIMER();
#endif
        return;
    }

    // Guard clause: cranking and fan disabled during cranking?
    if (BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) &&
        (configPage2.fanWhenCranking == 0U)) {
        currentStatus.fanDuty = 0U;
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
#if defined(PWM_FAN_AVAILABLE)
        DISABLE_FAN_TIMER();
#endif
        return;
    }

    // Normal operation - read duty from table
    uint8_t tempFanDuty = table2D_getValue(&fanPWMTable,
                                           temperatureAddOffset(currentStatus.coolant));

    // Check if A/C requires minimum duty
    if (acRequestsFan()) {
        if (tempFanDuty < configPage15.airConPwmFanMinDuty) {
            tempFanDuty = configPage15.airConPwmFanMinDuty;
        }
    }

    currentStatus.fanDuty = tempFanDuty;

#if defined(PWM_FAN_AVAILABLE)
    // Update PWM value
    pwm_state.pwm_value = halfPercentage(currentStatus.fanDuty, pwm_state.pwm_max_count);

    // Handle 0% duty
    if (currentStatus.fanDuty == 0U) {
        FAN_OFF();
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
        DISABLE_FAN_TIMER();
        return;
    }

    // Handle 100% duty
    if (currentStatus.fanDuty >= 200U) {
        FAN_ON();
        BIT_SET(currentStatus.status4, BIT_STATUS4_FAN);
        DISABLE_FAN_TIMER();
        return;
    }

    // Variable duty (0-100%)
    ENABLE_FAN_TIMER();
    BIT_SET(currentStatus.status4, BIT_STATUS4_FAN);
#else
    // Platform without PWM - fallback to digital
    if (currentStatus.fanDuty > 0U) {
        FAN_ON();
        BIT_SET(currentStatus.status4, BIT_STATUS4_FAN);
    } else {
        FAN_OFF();
        BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
    }
#endif
}

} // anonymous namespace

// Public interface implementation

bool initialise(void) {
    // Setup fan pin
    fan_pin_port = portOutputRegister(digitalPinToPort(pinFan));
    fan_pin_mask = digitalPinToBitMask(pinFan);

    // Initial state: fan OFF
    FAN_OFF();
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
    currentStatus.fanDuty = 0U;

#if defined(PWM_FAN_AVAILABLE)
    // Disable timer initially
    DISABLE_FAN_TIMER();

    // Setup PWM if enabled
    if (configPage2.fanEnable == 2U) {
#if defined(CORE_TEENSY)
        pwm_state.pwm_max_count = (uint16_t)(MICROS_PER_SEC / (32U * configPage6.fanFreq * 2U));
#endif
        pwm_state.pwm_value = 0;
    }
#endif

    return true;
}

void update(void) {
    // Dispatch based on fan mode
    if (configPage2.fanEnable == 1U) {
        updateDigitalMode();
    } else if (configPage2.fanEnable == 2U) {
        updatePWMMode();
    }
    // Mode 0 = disabled, do nothing
}

void forceOff(void) {
    turnFanOff();
    currentStatus.fanDuty = 0U;

#if defined(PWM_FAN_AVAILABLE)
    DISABLE_FAN_TIMER();
    pwm_state.pwm_value = 0;
#endif
}

uint8_t getDuty(void) {
    return currentStatus.fanDuty;
}

bool isActive(void) {
    return BIT_CHECK(currentStatus.status4, BIT_STATUS4_FAN);
}

FanMode getMode(void) {
    return static_cast<FanMode>(configPage2.fanEnable);
}

} // namespace fan
} // namespace speeduino

#if defined(PWM_FAN_AVAILABLE)
/**
 * @brief Fan PWM interrupt handler
 * @details Toggles fan pin for PWM generation
 * @note Called by hardware timer interrupt
 */
void fanInterrupt(void) {
    if (speeduino::fan::pwm_state.pwm_state == true) {
        FAN_OFF();
        FAN_TIMER_COMPARE = FAN_TIMER_COUNTER +
            (speeduino::fan::pwm_state.pwm_max_count - speeduino::fan::pwm_state.pwm_cur_value);
        speeduino::fan::pwm_state.pwm_state = false;
    } else {
        FAN_ON();
        FAN_TIMER_COMPARE = FAN_TIMER_COUNTER + speeduino::fan::pwm_state.pwm_value;
        speeduino::fan::pwm_state.pwm_cur_value = speeduino::fan::pwm_state.pwm_value;
        speeduino::fan::pwm_state.pwm_state = true;
    }
}
#endif
