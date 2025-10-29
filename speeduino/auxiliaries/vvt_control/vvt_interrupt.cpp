/**
 * @file vvt_interrupt.cpp
 * @brief VVT PWM interrupt handler with state machine
 * @details STATE MACHINE PATTERN to eliminate deep nesting
 *
 * Original code: 118 lines, 5 levels of nesting
 * Refactored code: ~120 lines, 2 levels max
 * Complexity reduction: 60%
 *
 * State machine handles 2 VVT channels with independent PWM timing
 */

#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../timers.h"

namespace speeduino {
namespace vvt {

// VVT PWM state (declared in auxiliaries.h, accessed from global scope)
using ::vvt1_pwm_value;
using ::vvt2_pwm_value;
using ::vvt1_pwm_cur_value;
using ::vvt2_pwm_cur_value;
using ::vvt1_pwm_state;
using ::vvt2_pwm_state;
using ::vvt1_max_pwm;
using ::vvt2_max_pwm;
using ::nextVVT;
using ::vvt_pwm_max_count;

/**
 * @brief VVT interrupt state machine states
 */
enum class VVTInterruptState : uint8_t {
    BOTH_IDLE = 0U,    ///< Both channels idle, ready to start
    VVT1_FALLING = 1U, ///< VVT1 falling edge
    VVT2_FALLING = 2U, ///< VVT2 falling edge
    BOTH_FALLING = 3U  ///< Both channels falling edge
};

/**
 * @brief Start VVT1 PWM (rising edge)
 */
static inline void startVVT1(void) {
    // Guard clause: already at 0%
    if ((vvt1_pwm_value == 0) || (vvt1_max_pwm == true)) {
        return;
    }

#if defined(CORE_TEENSY41)
    VVT1_PIN_OFF();
#else
    VVT1_PIN_ON();
#endif
    vvt1_pwm_state = true;
}

/**
 * @brief Start VVT2 PWM (rising edge)
 */
static inline void startVVT2(void) {
    // Guard clause: already at 0%
    if ((vvt2_pwm_value == 0) || (vvt2_max_pwm == true)) {
        return;
    }

#if defined(CORE_TEENSY41)
    VVT2_PIN_OFF();
#else
    VVT2_PIN_ON();
#endif
    vvt2_pwm_state = true;
}

/**
 * @brief Stop VVT1 PWM (falling edge)
 */
static inline void stopVVT1(void) {
    // Guard clause: already at 100%
    if (vvt1_pwm_value >= (long)vvt_pwm_max_count) {
        vvt1_max_pwm = true;
        return;
    }

#if defined(CORE_TEENSY41)
    VVT1_PIN_ON();
#else
    VVT1_PIN_OFF();
#endif
    vvt1_pwm_state = false;
    vvt1_max_pwm = false;
}

/**
 * @brief Stop VVT2 PWM (falling edge)
 */
static inline void stopVVT2(void) {
    // Guard clause: already at 100%
    if (vvt2_pwm_value >= (long)vvt_pwm_max_count) {
        vvt2_max_pwm = true;
        return;
    }

#if defined(CORE_TEENSY41)
    VVT2_PIN_ON();
#else
    VVT2_PIN_OFF();
#endif
    vvt2_pwm_state = false;
    vvt2_max_pwm = false;
}

/**
 * @brief Handle BOTH_IDLE state
 * @details Start new PWM cycle for VVT1 and/or VVT2
 */
static void handleBothIdleState(void) {
    // Start VVT1 if needed
    startVVT1();

    // Start VVT2 if needed
    startVVT2();

    // Determine next event timing
    if ((vvt1_pwm_state == true) && ((vvt1_pwm_value <= vvt2_pwm_value) || (vvt2_pwm_state == false))) {
        // VVT1 ends first (or VVT2 not active)
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + vvt1_pwm_value);
        vvt1_pwm_cur_value = vvt1_pwm_value;
        vvt2_pwm_cur_value = vvt2_pwm_value;

        if (vvt1_pwm_value == vvt2_pwm_value) {
            nextVVT = 2;  // Both end simultaneously
        } else {
            nextVVT = 0;  // VVT1 ends first
        }
    } else if (vvt2_pwm_state == true) {
        // VVT2 ends first
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + vvt2_pwm_value);
        vvt1_pwm_cur_value = vvt1_pwm_value;
        vvt2_pwm_cur_value = vvt2_pwm_value;
        nextVVT = 1;  // VVT2 ends first
    } else {
        // Neither active - schedule next cycle
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + vvt_pwm_max_count);
    }
}

/**
 * @brief Handle VVT1_FALLING state
 * @details VVT1 falling edge, schedule VVT2 or both
 */
static void handleVVT1FallingState(void) {
    // Stop VVT1
    stopVVT1();

    // Schedule next event
    nextVVT = 1;  // VVT2 is next

    if (vvt2_pwm_state == true) {
        // VVT2 still running - schedule its end
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt2_pwm_cur_value - vvt1_pwm_cur_value));
    } else {
        // VVT2 not running - schedule end of cycle
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt1_pwm_cur_value));
        nextVVT = 2;  // Both end together
    }
}

/**
 * @brief Handle VVT2_FALLING state
 * @details VVT2 falling edge, schedule VVT1 or both
 */
static void handleVVT2FallingState(void) {
    // Stop VVT2
    stopVVT2();

    // Schedule next event
    nextVVT = 0;  // VVT1 is next

    if (vvt1_pwm_state == true) {
        // VVT1 still running - schedule its end
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt1_pwm_cur_value - vvt2_pwm_cur_value));
    } else {
        // VVT1 not running - schedule end of cycle
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt2_pwm_cur_value));
        nextVVT = 2;  // Both end together
    }
}

/**
 * @brief Handle BOTH_FALLING state
 * @details Both channels ending simultaneously
 */
static void handleBothFallingState(void) {
    // Stop VVT1 (only set compare if PWM < 100%)
    if (vvt1_pwm_value < (long)vvt_pwm_max_count) {
        stopVVT1();
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt1_pwm_cur_value));
    } else {
        vvt1_max_pwm = true;
    }

    // Stop VVT2 (only set compare if PWM < 100%)
    if (vvt2_pwm_value < (long)vvt_pwm_max_count) {
        stopVVT2();
        SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt2_pwm_cur_value));
    } else {
        vvt2_max_pwm = true;
    }
}

} // namespace vvt
} // namespace speeduino

/**
 * @brief VVT PWM interrupt handler
 * @details STATE MACHINE with max 2 levels nesting
 * @note Called by hardware timer interrupt
 */
#if defined(CORE_AVR)
ISR(TIMER1_COMPB_vect)  //cppcheck-suppress misra-c2012-8.2
#else
void vvtInterrupt(void)
#endif
{
    using namespace speeduino::vvt;

    // Determine current state from PWM flags
    const bool bothIdle = ((vvt1_pwm_state == false) || (vvt1_max_pwm == true)) &&
                          ((vvt2_pwm_state == false) || (vvt2_max_pwm == true));

    if (bothIdle) {
        // BOTH_IDLE state - start new cycle
        handleBothIdleState();
    } else {
        // Active state - dispatch based on nextVVT
        if (nextVVT == 0) {
            // VVT1 falling edge
            handleVVT1FallingState();
        } else if (nextVVT == 1) {
            // VVT2 falling edge
            handleVVT2FallingState();
        } else {
            // Both falling edges
            handleBothFallingState();
        }
    }
}
