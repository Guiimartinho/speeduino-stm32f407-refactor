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

namespace speeduino {
namespace boost {

// PID controller and table (declared in auxiliaries.h, accessed from global scope)
using ::boostPID;
using ::flexBoostTable;

// Boost PWM state
static long boost_pwm_target_value = 0;
static volatile bool boost_pwm_state = false;
static volatile uint16_t boost_pwm_cur_value = 0U;
static uint16_t boost_pwm_max_count = 0U;

static uint8_t boostCounter = 0U;

/**
 * @brief Update open-loop boost control
 * @details Table-based duty cycle or gear-compensated
 */
static void updateOpenLoop(void) {
    // Check if gear-based boost is enabled
    const bool gearBoostEnabled = (configPage9.boostByGearEnabled > 0U) &&
                                   (configPage2.vssMode > 1U);

    if (gearBoostEnabled) {
        applyGearCompensation();
    } else {
        // Standard table lookup
        const uint8_t tableDuty = get3DTableValue(&boostTable,
                                                   (currentStatus.TPS * 2U),
                                                   currentStatus.RPM);
        currentStatus.boostDuty = (uint16_t)tableDuty * 200U;  // * 2 * 100
    }

    // Safety clamp
    if (currentStatus.boostDuty > 10000U) {
        currentStatus.boostDuty = 10000U;
    }

    // Handle zero duty
    if (currentStatus.boostDuty == 0U) {
        DISABLE_BOOST_TIMER();
        BOOST_PIN_LOW();
        return;
    }

    // Convert duty to PWM count
    boost_pwm_target_value = ((unsigned long)(currentStatus.boostDuty) * boost_pwm_max_count) / 10000UL;
}

/**
 * @brief Update closed-loop boost control
 * @details PID control to target pressure
 */
static void updateClosedLoop(void) {
    // Update target every 8 cycles
    if ((boostCounter & 7U) == 1U) {
        // Check if gear-based boost is enabled
        const bool gearBoostEnabled = (configPage9.boostByGearEnabled > 0U) &&
                                       (configPage2.vssMode > 1U);

        if (gearBoostEnabled) {
            applyGearCompensation();
        } else {
            // Standard table lookup (kPa * 2)
            currentStatus.boostTarget = get3DTableValue(&boostTable,
                                                        (currentStatus.TPS * 2U),
                                                        currentStatus.RPM) << 1;
        }

        // Apply flex fuel correction if enabled
        if (configPage2.flexEnabled == 1U) {
            const int16_t flexAdder = table2D_getValue(&flexBoostTable, currentStatus.ethanolPct);
            currentStatus.flexBoostCorrection = flexAdder;
            currentStatus.boostTarget += flexAdder;
            // Clamp to maximum
            if (currentStatus.boostTarget > 511U) {
                currentStatus.boostTarget = 511U;
            }
        } else {
            currentStatus.flexBoostCorrection = 0;
        }
    }

    // Check if boost control should be active
    const bool controlEnabled =
        ((configPage15.boostControlEnable == EN_BOOST_CONTROL_BARO) &&
         (currentStatus.MAP >= currentStatus.baro)) ||
        ((configPage15.boostControlEnable == EN_BOOST_CONTROL_FIXED) &&
         (currentStatus.MAP >= configPage15.boostControlEnableThreshold));

    // Guard clause: control not enabled (below threshold)
    if (!controlEnabled) {
        // Reset PID to prevent windup
        boostPID.Initialize();
        // High duty when disabled (close wastegate to build boost)
        currentStatus.boostDuty = (uint16_t)configPage15.boostDCWhenDisabled * 100U;
        boost_pwm_target_value = ((unsigned long)(currentStatus.boostDuty) * boost_pwm_max_count) / 10000UL;
        ENABLE_BOOST_TIMER();

        if (currentStatus.boostDuty == 0U) {
            disable();
        }
        return;
    }

    // Guard clause: no target set
    if (currentStatus.boostTarget == 0U) {
        disable();
        return;
    }

    // Update PID tunings every 16 cycles
    if ((boostCounter & 15U) == 1U) {
        boostPID.SetOutputLimits(configPage2.boostMinDuty, configPage2.boostMaxDuty);

        if (configPage6.boostMode == BOOST_MODE_SIMPLE) {
            boostPID.SetTunings(SIMPLE_BOOST_P, SIMPLE_BOOST_I, SIMPLE_BOOST_D);
        } else {
            boostPID.SetTunings(configPage6.boostKP, configPage6.boostKI, configPage6.boostKD);
        }
    }

    // Compute PID
    const uint8_t feedforward = get3DTableValue(&boostTableLookupDuty,
                                                 currentStatus.boostTarget,
                                                 currentStatus.RPM) * 50U;  // * 100 / 2
    const bool computed = boostPID.Compute(feedforward);

    // Handle zero duty
    if (currentStatus.boostDuty == 0U) {
        DISABLE_BOOST_TIMER();
        BOOST_PIN_LOW();
        return;
    }

    // Update PWM if computed
    if (computed) {
        boost_pwm_target_value = ((unsigned long)(currentStatus.boostDuty) * boost_pwm_max_count) / 10000UL;
    }
}

bool initialise(void) {
    // Setup GPIO
    boost_pin_port = portOutputRegister(digitalPinToPort(pinBoost));
    boost_pin_mask = digitalPinToBitMask(pinBoost);

    // Configure PID
    boostPID.SetOutputLimits(configPage2.boostMinDuty, configPage2.boostMaxDuty);

    if (configPage6.boostMode == BOOST_MODE_SIMPLE) {
        boostPID.SetTunings(SIMPLE_BOOST_P, SIMPLE_BOOST_I, SIMPLE_BOOST_D);
    } else {
        boostPID.SetTunings(configPage6.boostKP, configPage6.boostKI, configPage6.boostKD);
    }

    // Initial state
    currentStatus.boostDuty = 0U;
    boostCounter = 0U;

    return true;
}

void update(void) {
    // Guard clause: boost not enabled
    if (configPage6.boostEnabled != 1U) {
        DISABLE_BOOST_TIMER();
        currentStatus.flexBoostCorrection = 0;
        return;
    }

    // Dispatch based on control mode
    if (configPage4.boostType == OPEN_LOOP_BOOST) {
        updateOpenLoop();
    } else if (configPage4.boostType == CLOSED_LOOP_BOOST) {
        updateClosedLoop();
    }

    // Check for 100% duty cycle
    if (currentStatus.boostDuty >= 10000U) {
        DISABLE_BOOST_TIMER();
        BOOST_PIN_HIGH();
    } else if (currentStatus.boostDuty > 0U) {
        ENABLE_BOOST_TIMER();
    }

    boostCounter++;
}

void disable(void) {
    boostPID.Initialize();
    currentStatus.boostDuty = 0U;
    DISABLE_BOOST_TIMER();
    BOOST_PIN_LOW();
}

uint16_t getDuty(void) {
    return currentStatus.boostDuty;
}

uint16_t getTarget(void) {
    return currentStatus.boostTarget;
}

bool isEnabled(void) {
    return (configPage6.boostEnabled == 1U);
}

} // namespace boost
} // namespace speeduino

// Boost PWM interrupt handler (global scope for ISR)
#if defined(CORE_AVR)
ISR(TIMER1_COMPA_vect)  //cppcheck-suppress misra-c2012-8.2
#else
void boostInterrupt(void)
#endif
{
    if (speeduino::boost::boost_pwm_state == true) {
#if defined(CORE_TEENSY41)
        BOOST_PIN_HIGH();
#else
        BOOST_PIN_LOW();
#endif
        SET_COMPARE(BOOST_TIMER_COMPARE,
                    BOOST_TIMER_COUNTER + (speeduino::boost::boost_pwm_max_count - speeduino::boost::boost_pwm_cur_value));
        speeduino::boost::boost_pwm_state = false;
    } else {
#if defined(CORE_TEENSY41)
        BOOST_PIN_LOW();
#else
        BOOST_PIN_HIGH();
#endif
        SET_COMPARE(BOOST_TIMER_COMPARE,
                    BOOST_TIMER_COUNTER + speeduino::boost::boost_pwm_target_value);
        speeduino::boost::boost_pwm_cur_value = speeduino::boost::boost_pwm_target_value;
        speeduino::boost::boost_pwm_state = true;
    }
}
