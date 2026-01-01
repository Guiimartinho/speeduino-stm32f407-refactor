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

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================

/**
 * @brief Check if VVT1 should be active
 */
static inline bool isVVT1Active(void)
{
    return (vvt1_pwm_value != 0) && (vvt1_max_pwm == false);
}

/**
 * @brief Check if VVT2 should be active
 */
static inline bool isVVT2Active(void)
{
    return (vvt2_pwm_value != 0) && (vvt2_max_pwm == false);
}

/**
 * @brief Start VVT1 PWM (rising edge)
 */
static inline void startVVT1(void)
{
    if (!isVVT1Active()) { return; }

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
static inline void startVVT2(void)
{
    if (!isVVT2Active()) { return; }

#if defined(CORE_TEENSY41)
    VVT2_PIN_OFF();
#else
    VVT2_PIN_ON();
#endif
    vvt2_pwm_state = true;
}

/**
 * @brief Check if VVT1 is at max (100%)
 */
static inline bool isVVT1AtMax(void)
{
    return vvt1_pwm_value >= (long)vvt_pwm_max_count;
}

/**
 * @brief Check if VVT2 is at max (100%)
 */
static inline bool isVVT2AtMax(void)
{
    return vvt2_pwm_value >= (long)vvt_pwm_max_count;
}

/**
 * @brief Stop VVT1 PWM (falling edge)
 */
static inline void stopVVT1(void)
{
    if (isVVT1AtMax()) { vvt1_max_pwm = true; return; }

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
static inline void stopVVT2(void)
{
    if (isVVT2AtMax()) { vvt2_max_pwm = true; return; }

#if defined(CORE_TEENSY41)
    VVT2_PIN_ON();
#else
    VVT2_PIN_OFF();
#endif
    vvt2_pwm_state = false;
    vvt2_max_pwm = false;
}

/**
 * @brief Check if VVT1 ends first (or both together)
 */
static inline bool vvt1EndsFirst(void)
{
    if (vvt1_pwm_state == false) { return false; }
    if (vvt2_pwm_state == false) { return true; }
    return (vvt1_pwm_value <= vvt2_pwm_value);
}

/**
 * @brief Schedule VVT1 as next event
 */
static inline void scheduleVVT1First(void)
{
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + vvt1_pwm_value);
    vvt1_pwm_cur_value = vvt1_pwm_value;
    vvt2_pwm_cur_value = vvt2_pwm_value;
    nextVVT = (vvt1_pwm_value == vvt2_pwm_value) ? 2 : 0;
}

/**
 * @brief Schedule VVT2 as next event
 */
static inline void scheduleVVT2First(void)
{
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + vvt2_pwm_value);
    vvt1_pwm_cur_value = vvt1_pwm_value;
    vvt2_pwm_cur_value = vvt2_pwm_value;
    nextVVT = 1;
}

/**
 * @brief Schedule next cycle (neither active)
 */
static inline void scheduleNextCycle(void)
{
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + vvt_pwm_max_count);
}

/**
 * @brief Handle BOTH_IDLE state
 */
static void handleBothIdleState(void)
{
    startVVT1();
    startVVT2();

    if (vvt1EndsFirst()) { scheduleVVT1First(); return; }
    if (vvt2_pwm_state == true) { scheduleVVT2First(); return; }
    scheduleNextCycle();
}

/**
 * @brief Handle VVT1_FALLING state
 */
static void handleVVT1FallingState(void)
{
    stopVVT1();
    nextVVT = 1;

    if (vvt2_pwm_state == true) { SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt2_pwm_cur_value - vvt1_pwm_cur_value)); return; }
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt1_pwm_cur_value));
    nextVVT = 2;
}

/**
 * @brief Handle VVT2_FALLING state
 */
static void handleVVT2FallingState(void)
{
    stopVVT2();
    nextVVT = 0;

    if (vvt1_pwm_state == true) { SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt1_pwm_cur_value - vvt2_pwm_cur_value)); return; }
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt2_pwm_cur_value));
    nextVVT = 2;
}

/**
 * @brief Handle VVT1 in BOTH_FALLING state
 */
static inline void handleVVT1InBothFalling(void)
{
    if (isVVT1AtMax()) { vvt1_max_pwm = true; return; }
    stopVVT1();
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt1_pwm_cur_value));
}

/**
 * @brief Handle VVT2 in BOTH_FALLING state
 */
static inline void handleVVT2InBothFalling(void)
{
    if (isVVT2AtMax()) { vvt2_max_pwm = true; return; }
    stopVVT2();
    SET_COMPARE(VVT_TIMER_COMPARE, VVT_TIMER_COUNTER + (vvt_pwm_max_count - vvt2_pwm_cur_value));
}

/**
 * @brief Handle BOTH_FALLING state
 */
static void handleBothFallingState(void)
{
    handleVVT1InBothFalling();
    handleVVT2InBothFalling();
}

/**
 * @brief Check if both VVT channels are idle
 */
static inline bool areBothIdle(void)
{
    bool vvt1Idle = (vvt1_pwm_state == false) || (vvt1_max_pwm == true);
    bool vvt2Idle = (vvt2_pwm_state == false) || (vvt2_max_pwm == true);
    return vvt1Idle && vvt2Idle;
}

/**
 * @brief Dispatch active state handlers
 */
static inline void handleActiveState(void)
{
    if (nextVVT == 0) { handleVVT1FallingState(); return; }
    if (nextVVT == 1) { handleVVT2FallingState(); return; }
    handleBothFallingState();
}

// =============================================================================
// PUBLIC API - Interrupt handler (global scope for ISR)
// =============================================================================

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
    if (areBothIdle()) { handleBothIdleState(); return; }
    handleActiveState();
}
