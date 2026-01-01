/**
 * @file fan_control.cpp
 * @brief Cooling fan control implementation
 */

#include "fan_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"
#include "../../table2d.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================
// Note: Uses global PWM variables from auxiliaries.h:
// - fan_pwm_state, fan_pwm_max_count, fan_pwm_cur_value, fan_pwm_value

/**
 * @brief Check if fan is permitted to run
 */
static inline bool isFanPermitted(void)
{
    if (configPage2.fanWhenOff == 1U) { return true; }
    return BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN);
}

/**
 * @brief Check if A/C requires fan activation
 */
static inline bool acRequestsFan(void)
{
    if (configPage15.airConTurnsFanOn != 1U) { return false; }
    return BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
}

/**
 * @brief Turn fan on (digital mode)
 */
static inline void turnFanOn(void)
{
    FAN_ON();
    BIT_SET(currentStatus.status4, BIT_STATUS4_FAN);
}

/**
 * @brief Turn fan off (digital mode)
 */
static inline void turnFanOff(void)
{
    FAN_OFF();
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
}

/**
 * @brief Check if cranking with fan disabled
 */
static inline bool isCrankingFanDisabled(void)
{
    return BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && (configPage2.fanWhenCranking == 0U);
}

/**
 * @brief Disable fan with zero duty (PWM mode)
 */
static inline void disableFanPWM(void)
{
    currentStatus.fanDuty = 0U;
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
#if defined(PWM_FAN_AVAILABLE)
    DISABLE_FAN_TIMER();
#endif
}

/**
 * @brief Apply A/C minimum duty if required
 */
static inline uint8_t applyACMinDuty(uint8_t duty)
{
    if (!acRequestsFan()) { return duty; }
    return (duty < configPage15.airConPwmFanMinDuty) ? configPage15.airConPwmFanMinDuty : duty;
}

/**
 * @brief Handle digital (on/off) fan control
 */
static void updateDigitalMode(void)
{
    int16_t onTemp = temperatureRemoveOffset(configPage6.fanSP);
    int16_t offTemp = onTemp - configPage6.fanHyster;

    if (!isFanPermitted()) { turnFanOff(); return; }

    bool tempRequiresFan = (currentStatus.coolant >= onTemp);
    bool acRequiresFan = acRequestsFan();

    if (!tempRequiresFan && !acRequiresFan)
    {
        if (currentStatus.coolant <= offTemp) { turnFanOff(); }
        return;
    }

    if (isCrankingFanDisabled()) { turnFanOff(); return; }

    turnFanOn();
}

/**
 * @brief Handle PWM fan output states
 */
static void handlePWMOutput(void)
{
#if defined(PWM_FAN_AVAILABLE)
    fan_pwm_value = halfPercentage(currentStatus.fanDuty, fan_pwm_max_count);

    if (currentStatus.fanDuty == 0U) { FAN_OFF(); BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN); DISABLE_FAN_TIMER(); return; }
    if (currentStatus.fanDuty >= 200U) { FAN_ON(); BIT_SET(currentStatus.status4, BIT_STATUS4_FAN); DISABLE_FAN_TIMER(); return; }

    ENABLE_FAN_TIMER();
    BIT_SET(currentStatus.status4, BIT_STATUS4_FAN);
#else
    if (currentStatus.fanDuty > 0U) { FAN_ON(); BIT_SET(currentStatus.status4, BIT_STATUS4_FAN); }
    else { FAN_OFF(); BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN); }
#endif
}

/**
 * @brief Handle PWM fan control
 */
static void updatePWMMode(void)
{
    if (!isFanPermitted()) { disableFanPWM(); return; }
    if (isCrankingFanDisabled()) { disableFanPWM(); return; }

    uint8_t tempFanDuty = table2D_getValue(&fanPWMTable, temperatureAddOffset(currentStatus.coolant));
    currentStatus.fanDuty = applyACMinDuty(tempFanDuty);

    handlePWMOutput();
}

/**
 * @brief Internal initialise implementation
 */
static bool fan_initialise_impl(void)
{
    fan_pin_port = portOutputRegister(digitalPinToPort(pinFan));
    fan_pin_mask = digitalPinToBitMask(pinFan);

    FAN_OFF();
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_FAN);
    currentStatus.fanDuty = 0U;

#if defined(PWM_FAN_AVAILABLE)
    DISABLE_FAN_TIMER();
    if (configPage2.fanEnable == 2U)
    {
#if defined(CORE_TEENSY)
        fan_pwm_max_count = (uint16_t)(MICROS_PER_SEC / (32U * configPage6.fanFreq * 2U));
#endif
        fan_pwm_value = 0;
    }
#endif

    return true;
}

/**
 * @brief Internal update implementation
 */
static void fan_update_impl(void)
{
    if (configPage2.fanEnable == 1U) { updateDigitalMode(); }
    else if (configPage2.fanEnable == 2U) { updatePWMMode(); }
}

/**
 * @brief Internal forceOff implementation
 */
static void fan_forceOff_impl(void)
{
    turnFanOff();
    currentStatus.fanDuty = 0U;
#if defined(PWM_FAN_AVAILABLE)
    DISABLE_FAN_TIMER();
    fan_pwm_value = 0;
#endif
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace fan {

bool initialise(void) { return fan_initialise_impl(); }
void update(void) { fan_update_impl(); }
void forceOff(void) { fan_forceOff_impl(); }
uint8_t getDuty(void) { return currentStatus.fanDuty; }
bool isActive(void) { return BIT_CHECK(currentStatus.status4, BIT_STATUS4_FAN); }
FanMode getMode(void) { return static_cast<FanMode>(configPage2.fanEnable); }

} // namespace fan
} // namespace speeduino

#if defined(PWM_FAN_AVAILABLE)
/**
 * @brief Fan PWM interrupt handler
 * @details Toggles fan pin for PWM generation
 * @note Called by hardware timer interrupt
 */
void fanInterrupt(void) {
    if (fan_pwm_state == true) {
        FAN_OFF();
        FAN_TIMER_COMPARE = FAN_TIMER_COUNTER + (fan_pwm_max_count - fan_pwm_cur_value);
        fan_pwm_state = false;
    } else {
        FAN_ON();
        FAN_TIMER_COMPARE = FAN_TIMER_COUNTER + fan_pwm_value;
        fan_pwm_cur_value = fan_pwm_value;
        fan_pwm_state = true;
    }
}
#endif
