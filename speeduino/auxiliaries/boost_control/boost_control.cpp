/**
 * @file boost_control.cpp
 * @brief Boost control main implementation
 */

#include "boost_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../table3d.h"
#include "../../timers.h"
#include "../../src/PID_v1/PID_v1.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================
// Note: Uses global PWM variables from auxiliaries.h:
// - boost_pwm_target_value, boost_pwm_state, boost_pwm_cur_value, boost_pwm_max_count
// - boostCounter

/**
 * @brief Calculate duty from table or gear compensation
 */
static inline void calculateBoostDuty(void)
{
    bool gearBoostEnabled = (configPage9.boostByGearEnabled > 0U) && (configPage2.vssMode > 1U);

    if (gearBoostEnabled) { speeduino::boost::applyGearCompensation(); return; }

    uint8_t tableDuty = get3DTableValue(&boostTable, (currentStatus.TPS * 2U), currentStatus.RPM);
    currentStatus.boostDuty = (uint16_t)tableDuty * 200U;
}

/**
 * @brief Clamp boost duty to maximum
 */
static inline void clampBoostDuty(void)
{
    if (currentStatus.boostDuty > 10000U) { currentStatus.boostDuty = 10000U; }
}

/**
 * @brief Update PWM target value from duty
 */
static inline void updatePWMTarget(void)
{
    boost_pwm_target_value = ((unsigned long)(currentStatus.boostDuty) * boost_pwm_max_count) / 10000UL;
}

/**
 * @brief Update open-loop boost control
 */
static void updateOpenLoop(void)
{
    calculateBoostDuty();
    clampBoostDuty();

    if (currentStatus.boostDuty == 0U) { DISABLE_BOOST_TIMER(); BOOST_PIN_LOW(); return; }

    updatePWMTarget();
}

/**
 * @brief Update boost target from table or gear compensation
 */
static void updateBoostTarget(void)
{
    bool gearBoostEnabled = (configPage9.boostByGearEnabled > 0U) && (configPage2.vssMode > 1U);

    if (gearBoostEnabled) { speeduino::boost::applyGearCompensation(); return; }

    currentStatus.boostTarget = get3DTableValue(&boostTable, (currentStatus.TPS * 2U), currentStatus.RPM) << 1;
}

/**
 * @brief Apply flex fuel correction to boost target
 */
static void applyFlexCorrection(void)
{
    if (configPage2.flexEnabled != 1U) { currentStatus.flexBoostCorrection = 0; return; }

    int16_t flexAdder = table2D_getValue(&flexBoostTable, currentStatus.ethanolPct);
    currentStatus.flexBoostCorrection = flexAdder;
    currentStatus.boostTarget += flexAdder;
    if (currentStatus.boostTarget > 511U) { currentStatus.boostTarget = 511U; }
}

/**
 * @brief Check if boost control should be active
 */
static inline bool isBoostControlEnabled(void)
{
    bool baroMode = (configPage15.boostControlEnable == EN_BOOST_CONTROL_BARO) &&
                    (currentStatus.MAP >= currentStatus.baro);
    bool fixedMode = (configPage15.boostControlEnable == EN_BOOST_CONTROL_FIXED) &&
                     (currentStatus.MAP >= configPage15.boostControlEnableThreshold);
    return baroMode || fixedMode;
}

/**
 * @brief Update PID tunings
 */
static inline void updatePIDTunings(void)
{
    boostPID.SetOutputLimits(configPage2.boostMinDuty, configPage2.boostMaxDuty);

    bool simpleMode = (configPage6.boostMode == BOOST_MODE_SIMPLE);
    if (simpleMode) { boostPID.SetTunings(SIMPLE_BOOST_P, SIMPLE_BOOST_I, SIMPLE_BOOST_D); }
    else { boostPID.SetTunings(configPage6.boostKP, configPage6.boostKI, configPage6.boostKD); }
}

/**
 * @brief Handle control disabled state
 */
static void handleControlDisabled(void)
{
    boostPID.Initialize();
    currentStatus.boostDuty = (uint16_t)configPage15.boostDCWhenDisabled * 100U;
    updatePWMTarget();
    ENABLE_BOOST_TIMER();
    if (currentStatus.boostDuty == 0U) { speeduino::boost::disable(); }
}

/**
 * @brief Update closed-loop boost control
 */
static void updateClosedLoop(void)
{
    // Update target every 8 cycles
    if ((boostCounter & 7U) == 1U) { updateBoostTarget(); applyFlexCorrection(); }

    // Guard: control not enabled
    if (!isBoostControlEnabled()) { handleControlDisabled(); return; }

    // Guard: no target set
    if (currentStatus.boostTarget == 0U) { speeduino::boost::disable(); return; }

    // Update PID tunings every 16 cycles
    if ((boostCounter & 15U) == 1U) { updatePIDTunings(); }

    // Compute PID
    uint8_t feedforward = get3DTableValue(&boostTableLookupDuty, currentStatus.boostTarget, currentStatus.RPM) * 50U;
    bool computed = boostPID.Compute(feedforward);

    // Handle zero duty
    if (currentStatus.boostDuty == 0U) { DISABLE_BOOST_TIMER(); BOOST_PIN_LOW(); return; }

    // Update PWM if computed
    if (computed) { updatePWMTarget(); }
}

/**
 * @brief Internal initialise implementation
 */
static bool boost_initialise_impl(void)
{
    boost_pin_port = portOutputRegister(digitalPinToPort(pinBoost));
    boost_pin_mask = digitalPinToBitMask(pinBoost);

    boostPID.SetOutputLimits(configPage2.boostMinDuty, configPage2.boostMaxDuty);

    bool simpleMode = (configPage6.boostMode == BOOST_MODE_SIMPLE);
    if (simpleMode) { boostPID.SetTunings(SIMPLE_BOOST_P, SIMPLE_BOOST_I, SIMPLE_BOOST_D); }
    else { boostPID.SetTunings(configPage6.boostKP, configPage6.boostKI, configPage6.boostKD); }

    currentStatus.boostDuty = 0U;
    boostCounter = 0U;

    return true;
}

/**
 * @brief Internal disable implementation
 */
static void boost_disable_impl(void)
{
    boostPID.Initialize();
    currentStatus.boostDuty = 0U;
    DISABLE_BOOST_TIMER();
    BOOST_PIN_LOW();
}

/**
 * @brief Internal update implementation
 */
static void boost_update_impl(void)
{
    // Guard: boost not enabled
    if (configPage6.boostEnabled != 1U) { DISABLE_BOOST_TIMER(); currentStatus.flexBoostCorrection = 0; return; }

    // Dispatch based on control mode
    if (configPage4.boostType == OPEN_LOOP_BOOST) { updateOpenLoop(); }
    else if (configPage4.boostType == CLOSED_LOOP_BOOST) { updateClosedLoop(); }

    // Handle duty output
    if (currentStatus.boostDuty >= 10000U) { DISABLE_BOOST_TIMER(); BOOST_PIN_HIGH(); }
    else if (currentStatus.boostDuty > 0U) { ENABLE_BOOST_TIMER(); }

    boostCounter++;
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace boost {

bool initialise(void) { return boost_initialise_impl(); }
void update(void) { boost_update_impl(); }
void disable(void) { boost_disable_impl(); }
uint16_t getDuty(void) { return currentStatus.boostDuty; }
uint16_t getTarget(void) { return currentStatus.boostTarget; }
bool isEnabled(void) { return (configPage6.boostEnabled == 1U); }

} // namespace boost
} // namespace speeduino

// Boost PWM interrupt handler (global scope for ISR)
#if defined(CORE_AVR)
ISR(TIMER1_COMPA_vect)  //cppcheck-suppress misra-c2012-8.2
#else
void boostInterrupt(void)
#endif
{
    if (boost_pwm_state == true) {
#if defined(CORE_TEENSY41)
        BOOST_PIN_HIGH();
#else
        BOOST_PIN_LOW();
#endif
        SET_COMPARE(BOOST_TIMER_COMPARE,
                    BOOST_TIMER_COUNTER + (boost_pwm_max_count - boost_pwm_cur_value));
        boost_pwm_state = false;
    } else {
#if defined(CORE_TEENSY41)
        BOOST_PIN_LOW();
#else
        BOOST_PIN_HIGH();
#endif
        SET_COMPARE(BOOST_TIMER_COMPARE,
                    BOOST_TIMER_COUNTER + boost_pwm_target_value);
        boost_pwm_cur_value = boost_pwm_target_value;
        boost_pwm_state = true;
    }
}
