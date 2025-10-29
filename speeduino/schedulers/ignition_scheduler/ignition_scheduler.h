/**
 * @file ignition_scheduler.h
 * @brief Ignition scheduling subsystem - dwell and spark timing control
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This subsystem handles ignition scheduling:
 * - 8 independent ignition channels (sequential/wasted spark)
 * - Hardware timer-based scheduling (4µs resolution)
 * - Dwell control (coil charge time)
 * - Spark timing (fire at specific crank angle)
 * - Queue support for next event (high RPM)
 * - ISR-driven start/end callbacks
 *
 * ARCHITECTURE:
 * - Documentation layer for IgnitionSchedule struct
 * - All implementations in original scheduler.cpp (100% preserved)
 * - NO runtime wrappers (performance)
 *
 * TECHNICAL:
 * - Timer prescale: 64 (4µs tick)
 * - Max period: 262,144µs (~0.26 seconds)
 * - Precision: ±2µs
 * - Dwell range: 1-10ms typical
 *
 * PRESERVES 100% LOGIC from original scheduler.h/cpp
 */

#ifndef IGNITION_SCHEDULER_H
#define IGNITION_SCHEDULER_H

#include <stdint.h>
#include "../../globals.h"

// ============================================================================
// IGNITION SCHEDULE STRUCT (Documentation Layer)
// ============================================================================

/**
 * @brief IgnitionSchedule struct documentation
 *
 * Original struct defined in scheduler.h (lines 117-151).
 * This is a DOCUMENTATION LAYER only - struct remains in original location.
 *
 * Struct members:
 * - counter_t &counter                    : Timer counter register (e.g. TCNT5)
 * - compare_t &compare                    : Compare register (e.g. OCR5A)
 * - void (&pTimerDisable)()               : Timer disable function pointer
 * - void (&pTimerEnable)()                : Timer enable function pointer
 * - volatile unsigned long duration       : Dwell time (microseconds)
 * - volatile ScheduleStatus Status        : OFF, PENDING, RUNNING
 * - void (*pStartCallback)(void)          : Dwell start (coil ON) callback
 * - void (*pEndCallback)(void)            : Spark (coil OFF) callback
 * - volatile unsigned long startTime      : Dwell start timestamp (overdwell protection)
 * - volatile COMPARE_TYPE startCompare    : Start counter value
 * - volatile COMPARE_TYPE endCompare      : End counter value
 * - COMPARE_TYPE nextStartCompare         : Next event start
 * - COMPARE_TYPE nextEndCompare           : Next event end
 * - volatile bool hasNextSchedule         : Queue flag
 * - volatile bool endScheduleSetByDecoder : Decoder override flag
 *
 * 8 Global instances (scheduler.cpp:53-67):
 * - ignitionSchedule1 (IGN1 - Cylinder 1/Wasted pair)
 * - ignitionSchedule2 (IGN2 - Cylinder 2/Wasted pair)
 * - ignitionSchedule3 (IGN3 - Cylinder 3/Wasted pair)
 * - ignitionSchedule4 (IGN4 - Cylinder 4/Wasted pair)
 * - ignitionSchedule5 (IGN5 - Cylinder 5) [if IGN_CHANNELS >= 5]
 * - ignitionSchedule6 (IGN6 - Cylinder 6) [if IGN_CHANNELS >= 6]
 * - ignitionSchedule7 (IGN7 - Cylinder 7) [if IGN_CHANNELS >= 7]
 * - ignitionSchedule8 (IGN8 - Cylinder 8) [if IGN_CHANNELS >= 8]
 */

// ============================================================================
// IGNITION SCHEDULING FUNCTIONS (Documentation Layer)
// ============================================================================

/**
 * @brief Set ignition schedule (INLINE FUNCTION - scheduler.h:156)
 *
 * Schedules ignition event (dwell + spark):
 * @param schedule IgnitionSchedule reference (e.g. ignitionSchedule1)
 * @param timeout Time until dwell starts (µs)
 * @param duration Dwell time (µs)
 *
 * Behavior:
 * - If schedule NOT RUNNING: calls _setIgnitionScheduleRunning()
 * - If schedule RUNNING: queues next event via _setIgnitionScheduleNext()
 * - Checks timeout < MAX_TIMER_PERIOD (safety)
 * - Checks angleToTimeMicroSecPerDegree(CRANK_ANGLE_MAX_IGN) < MAX_TIMER_PERIOD
 *
 * Performance: INLINE (always_inline) - zero function call overhead
 *
 * Location: scheduler.h:156-170
 * Complexity: 4 (3 if checks)
 * Critical: YES (timing-sensitive)
 */
// inline void setIgnitionSchedule(IgnitionSchedule &schedule, unsigned long timeout, unsigned long duration);
// --> USE ORIGINAL IN scheduler.h

/**
 * @brief Set ignition schedule running (scheduler.cpp:244)
 *
 * Starts new ignition event (dwell + spark):
 * @param schedule IgnitionSchedule reference
 * @param timeout Time until dwell starts (µs)
 * @param duration Dwell time (µs)
 *
 * Behavior:
 * - Calculates startCompare from timeout
 * - Sets duration (clipped to MAX_TIMER_PERIOD - 1)
 * - Sets Status to PENDING
 * - Enables timer interrupt
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:244-259
 * Complexity: 2
 */
// void _setIgnitionScheduleRunning(IgnitionSchedule &schedule, unsigned long timeout, unsigned long duration);
// --> Remain in scheduler.cpp (not exported)

/**
 * @brief Queue next ignition schedule (scheduler.cpp:261)
 *
 * Queues next event while current running:
 * @param schedule IgnitionSchedule reference
 * @param timeout Time until next dwell starts (µs)
 * @param duration Next dwell time (µs)
 *
 * Behavior:
 * - Sets nextStartCompare
 * - Updates duration for next event
 * - Sets hasNextSchedule flag
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:261-271
 * Complexity: 2
 */
// void _setIgnitionScheduleNext(IgnitionSchedule &schedule, unsigned long timeout, unsigned long duration);
// --> Remain in scheduler.cpp (not exported)

/**
 * @brief Refresh ignition schedule 1 end time (scheduler.cpp:274)
 *
 * Dynamically updates ignition end time (per-tooth timing):
 * @param timeToEnd New time to spark (µs)
 *
 * Behavior:
 * - Only if schedule RUNNING and timeToEnd < duration
 * - Calculates new endCompare
 * - Updates compare register
 *
 * Purpose: Per-tooth timing adjustments from decoder
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:274-285
 * Complexity: 2
 * Critical: YES (timing-sensitive)
 */
// void refreshIgnitionSchedule1(unsigned long timeToEnd);
// --> USE ORIGINAL IN scheduler.cpp

/**
 * @brief Disable ignition schedule (scheduler.cpp:624)
 *
 * Disables ignition on specific channel:
 * @param channel Channel number (0-7 for channels 1-8)
 *
 * Behavior:
 * - If Status == PENDING: sets Status to OFF
 * - If Status == RUNNING: clears hasNextSchedule flag
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:624-669
 * Complexity: 9 (switch with 8 cases)
 */
// void disableIgnSchedule(byte channel);
// --> USE ORIGINAL IN scheduler.cpp

/**
 * @brief Disable all ignition schedules (scheduler.cpp:682)
 *
 * Disables all 8 ignition channels (safety/emergency):
 *
 * Behavior:
 * - Calls disableIgnSchedule(0-7)
 *
 * Location: scheduler.cpp:682-692
 * Complexity: 1
 */
// void disableAllIgnSchedules(void);
// --> USE ORIGINAL IN scheduler.cpp

// ============================================================================
// IGNITION ISR FUNCTIONS (Documentation Layer)
// ============================================================================

/**
 * @brief Ignition schedule ISR dispatcher (scheduler.cpp:453)
 *
 * Handles ignition event state machine:
 * @param schedule IgnitionSchedule reference
 *
 * State machine:
 * - PENDING: Call pStartCallback() (dwell ON), set RUNNING, record startTime, schedule end
 * - RUNNING: Call pEndCallback() (spark), set OFF, update actualDwell (or queue next if hasNextSchedule)
 * - OFF: Disable timer
 *
 * Special features:
 * - Records startTime for overdwell protection
 * - Updates currentStatus.actualDwell (exponential moving average)
 * - Increments global ignitionCount
 * - Handles endScheduleSetByDecoder flag
 *
 * Performance: INLINE (always_inline) - inlined into ISR calls
 * Location: scheduler.cpp:453-488
 * Complexity: 4
 * Critical: YES (<10µs requirement)
 */
// static inline __attribute__((always_inline)) void ignitionScheduleISR(IgnitionSchedule &schedule);
// --> Remain in scheduler.cpp (not exported)

/**
 * @brief Ignition schedule 1-8 interrupt handlers (scheduler.cpp:493-574)
 *
 * ISR functions called by hardware timers:
 * - ignitionSchedule1Interrupt()  (Timer5 Compare A)
 * - ignitionSchedule2Interrupt()  (Timer5 Compare B) [if IGN_CHANNELS >= 2]
 * - ignitionSchedule3Interrupt()  (Timer5 Compare C) [if IGN_CHANNELS >= 3]
 * - ignitionSchedule4Interrupt()  (Timer4 Compare A) [if IGN_CHANNELS >= 4]
 * - ignitionSchedule5Interrupt()  (Timer4 Compare C) [if IGN_CHANNELS >= 5]
 * - ignitionSchedule6Interrupt()  (Timer4 Compare B) [if IGN_CHANNELS >= 6]
 * - ignitionSchedule7Interrupt()  (Timer3 Compare C) [if IGN_CHANNELS >= 7]
 * - ignitionSchedule8Interrupt()  (Timer3 Compare B) [if IGN_CHANNELS >= 8]
 *
 * Behavior: Each calls ignitionScheduleISR(ignitionScheduleX)
 *
 * Performance: 3-4 lines each, inline call to ignitionScheduleISR()
 * Location: scheduler.cpp:493-574
 * Complexity: 1 each
 * Critical: YES (<10µs requirement)
 */
// void ignitionSchedule1-8Interrupt(void);
// --> Remain in scheduler.cpp (called by hardware timer ISR)

// ============================================================================
// IGNITION TIMING FEATURES
// ============================================================================

/**
 * @brief Dwell control
 *
 * Dwell is coil charge time before spark:
 * - Typical range: 2-8ms depending on coil
 * - Too short: Weak spark
 * - Too long: Coil overheating (overdwell)
 *
 * Protection:
 * - startTime records when dwell started
 * - Can be monitored for overdwell condition
 * - actualDwell tracks real dwell time (EMA filtered)
 *
 * Formula: actualDwell = DWELL_AVERAGE(micros() - startTime)
 * Where: DWELL_AVERAGE() is exponential moving average
 * Alpha: 30 (defined in scheduler.h:50)
 */

/**
 * @brief Per-tooth timing
 *
 * Some decoders update ignition end time per tooth:
 * - More accurate spark timing (especially at high RPM)
 * - endScheduleSetByDecoder flag set by decoder
 * - refreshIgnitionSchedule1() updates endCompare
 *
 * Used by:
 * - Missing tooth decoders
 * - Sequential ignition systems
 */

/**
 * @brief Queue system
 *
 * At high RPM, next ignition event can be queued:
 * - Prevents timing gaps
 * - hasNextSchedule flag
 * - nextStartCompare, nextEndCompare
 * - Automatically activated when current event ends
 */

// ============================================================================
// IMPORTANT NOTES
// ============================================================================

/*
 * ALL IGNITION SCHEDULING FUNCTIONS remain in original scheduler.h/cpp:
 *
 * WHY NO WRAPPERS:
 * 1. Performance: Inline functions must stay inline (zero overhead)
 * 2. ISR Timing: <10µs requirement cannot tolerate wrapper overhead
 * 3. Compiler Optimization: Inline + always_inline guarantees best performance
 * 4. Hardware Integration: Direct timer → ISR path is fastest
 * 5. Dwell Timing: Critical for coil safety (no extra overhead)
 *
 * THIS MODULE PROVIDES:
 * - Documentation layer for ignition scheduling
 * - Conceptual organization (fuel vs ignition)
 * - Future expansion point if needed
 *
 * USAGE:
 * - Include original scheduler.h for all ignition scheduling functions
 * - Use coordinator API for convenience wrapper with validation
 * - Direct calls to setIgnitionSchedule() for maximum performance
 */

#endif // IGNITION_SCHEDULER_H
