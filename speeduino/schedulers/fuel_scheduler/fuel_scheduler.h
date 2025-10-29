/**
 * @file fuel_scheduler.h
 * @brief Fuel scheduling subsystem - injection timing and control
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This subsystem handles fuel injection scheduling:
 * - 8 independent fuel channels (sequential injection)
 * - Hardware timer-based scheduling (16µs resolution)
 * - Queue support for next pulse (high RPM)
 * - ISR-driven start/end callbacks
 *
 * ARCHITECTURE:
 * - Documentation layer for FuelSchedule struct
 * - All implementations in original scheduler.cpp (100% preserved)
 * - NO runtime wrappers (performance)
 *
 * TECHNICAL:
 * - Timer prescale: 256 (16µs tick)
 * - Max period: 1,048,576µs (~1 second)
 * - Precision: ±8µs
 *
 * PRESERVES 100% LOGIC from original scheduler.h/cpp
 */

#ifndef FUEL_SCHEDULER_H
#define FUEL_SCHEDULER_H

#include <stdint.h>
#include "../../globals.h"

// ============================================================================
// FUEL SCHEDULE STRUCT (Documentation Layer)
// ============================================================================

/**
 * @brief FuelSchedule struct documentation
 *
 * Original struct defined in scheduler.h (lines 176-205).
 * This is a DOCUMENTATION LAYER only - struct remains in original location.
 *
 * Struct members:
 * - counter_t &counter           : Timer counter register (e.g. TCNT3)
 * - compare_t &compare           : Compare register (e.g. OCR3A)
 * - void (&pTimerDisable)()      : Timer disable function pointer
 * - void (&pTimerEnable)()       : Timer enable function pointer
 * - volatile unsigned long duration       : Pulse width (microseconds)
 * - volatile ScheduleStatus Status        : OFF, PENDING, RUNNING
 * - volatile COMPARE_TYPE startCompare    : Start counter value
 * - void (*pStartFunction)(void)          : Injection ON callback
 * - void (*pEndFunction)(void)            : Injection OFF callback
 * - COMPARE_TYPE nextStartCompare         : Next pulse start
 * - volatile bool hasNextSchedule         : Queue flag
 *
 * 8 Global instances (scheduler.cpp:35-51):
 * - fuelSchedule1 (INJ1 - Cylinder 1)
 * - fuelSchedule2 (INJ2 - Cylinder 2)
 * - fuelSchedule3 (INJ3 - Cylinder 3)
 * - fuelSchedule4 (INJ4 - Cylinder 4)
 * - fuelSchedule5 (INJ5 - Cylinder 5) [if INJ_CHANNELS >= 5]
 * - fuelSchedule6 (INJ6 - Cylinder 6) [if INJ_CHANNELS >= 6]
 * - fuelSchedule7 (INJ7 - Cylinder 7) [if INJ_CHANNELS >= 7]
 * - fuelSchedule8 (INJ8 - Cylinder 8) [if INJ_CHANNELS >= 8]
 */

// ============================================================================
// FUEL SCHEDULING FUNCTIONS (Documentation Layer)
// ============================================================================

/**
 * @brief Set fuel schedule (INLINE FUNCTION - scheduler.h:210)
 *
 * Schedules fuel injection pulse:
 * @param schedule FuelSchedule reference (e.g. fuelSchedule1)
 * @param timeout Time until injection starts (µs)
 * @param duration Injection pulse width (µs)
 *
 * Behavior:
 * - If schedule NOT RUNNING: calls _setFuelScheduleRunning()
 * - If schedule RUNNING: queues next pulse via _setFuelScheduleNext()
 * - Checks timeout < MAX_TIMER_PERIOD (safety)
 *
 * Performance: INLINE (always_inline) - zero function call overhead
 *
 * Location: scheduler.h:210-224
 * Complexity: 3 (2 if checks)
 * Critical: YES (timing-sensitive)
 */
// inline void setFuelSchedule(FuelSchedule &schedule, unsigned long timeout, unsigned long duration);
// --> USE ORIGINAL IN scheduler.h

/**
 * @brief Set fuel schedule running (scheduler.cpp:215)
 *
 * Starts new fuel pulse:
 * @param schedule FuelSchedule reference
 * @param timeout Time until injection starts (µs)
 * @param duration Injection pulse width (µs)
 *
 * Behavior:
 * - Calculates startCompare from timeout
 * - Sets duration (clipped to MAX_TIMER_PERIOD - 1)
 * - Sets Status to PENDING
 * - Enables timer interrupt
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:215-229
 * Complexity: 2
 */
// void _setFuelScheduleRunning(FuelSchedule &schedule, unsigned long timeout, unsigned long duration);
// --> Remain in scheduler.cpp (not exported)

/**
 * @brief Queue next fuel schedule (scheduler.cpp:231)
 *
 * Queues next pulse while current running:
 * @param schedule FuelSchedule reference
 * @param timeout Time until next injection starts (µs)
 * @param duration Next injection pulse width (µs)
 *
 * Behavior:
 * - Sets nextStartCompare
 * - Updates duration for next pulse
 * - Sets hasNextSchedule flag
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:231-242
 * Complexity: 2
 */
// void _setFuelScheduleNext(FuelSchedule &schedule, unsigned long timeout, unsigned long duration);
// --> Remain in scheduler.cpp (not exported)

/**
 * @brief Disable fuel schedule (scheduler.cpp:576)
 *
 * Disables fuel injection on specific channel:
 * @param channel Channel number (0-7 for channels 1-8)
 *
 * Behavior:
 * - If Status == PENDING: sets Status to OFF
 * - If Status == RUNNING: clears hasNextSchedule flag
 *
 * Safety: Uses noInterrupts() block
 * Location: scheduler.cpp:576-623
 * Complexity: 9 (switch with 8 cases)
 */
// void disableFuelSchedule(byte channel);
// --> USE ORIGINAL IN scheduler.cpp

/**
 * @brief Disable all fuel schedules (scheduler.cpp:671)
 *
 * Disables all 8 fuel channels (safety/emergency):
 *
 * Behavior:
 * - Calls disableFuelSchedule(0-7)
 *
 * Location: scheduler.cpp:671-681
 * Complexity: 1
 */
// void disableAllFuelSchedules(void);
// --> USE ORIGINAL IN scheduler.cpp

// ============================================================================
// FUEL ISR FUNCTIONS (Documentation Layer)
// ============================================================================

/**
 * @brief Fuel schedule ISR dispatcher (scheduler.cpp:327)
 *
 * Handles fuel injection state machine:
 * @param schedule FuelSchedule reference
 *
 * State machine:
 * - PENDING: Call pStartFunction(), set RUNNING, schedule end
 * - RUNNING: Call pEndFunction(), set OFF (or queue next if hasNextSchedule)
 * - OFF: Disable timer
 *
 * Performance: INLINE (always_inline) - inlined into ISR calls
 * Location: scheduler.cpp:327-356
 * Complexity: 4
 * Critical: YES (<10µs requirement)
 */
// static inline __attribute__((always_inline)) void fuelScheduleISR(FuelSchedule &schedule);
// --> Remain in scheduler.cpp (not exported)

/**
 * @brief Fuel schedule 1-8 interrupt handlers (scheduler.cpp:370-447)
 *
 * ISR functions called by hardware timers:
 * - fuelSchedule1Interrupt()  (Timer3 Compare A)
 * - fuelSchedule2Interrupt()  (Timer3 Compare B)
 * - fuelSchedule3Interrupt()  (Timer3 Compare C)
 * - fuelSchedule4Interrupt()  (Timer4 Compare B)
 * - fuelSchedule5Interrupt()  (Timer4 Compare C) [if INJ_CHANNELS >= 5]
 * - fuelSchedule6Interrupt()  (Timer4 Compare A) [if INJ_CHANNELS >= 6]
 * - fuelSchedule7Interrupt()  (Timer5 Compare C) [if INJ_CHANNELS >= 7]
 * - fuelSchedule8Interrupt()  (Timer5 Compare B) [if INJ_CHANNELS >= 8]
 *
 * Behavior: Each calls fuelScheduleISR(fuelScheduleX)
 *
 * Performance: 3-4 lines each, inline call to fuelScheduleISR()
 * Location: scheduler.cpp:370-447
 * Complexity: 1 each
 * Critical: YES (<10µs requirement)
 */
// void fuelSchedule1-8Interrupt(void);
// --> Remain in scheduler.cpp (called by hardware timer ISR)

// ============================================================================
// IMPORTANT NOTES
// ============================================================================

/*
 * ALL FUEL SCHEDULING FUNCTIONS remain in original scheduler.h/cpp:
 *
 * WHY NO WRAPPERS:
 * 1. Performance: Inline functions must stay inline (zero overhead)
 * 2. ISR Timing: <10µs requirement cannot tolerate wrapper overhead
 * 3. Compiler Optimization: Inline + always_inline guarantees best performance
 * 4. Hardware Integration: Direct timer → ISR path is fastest
 *
 * THIS MODULE PROVIDES:
 * - Documentation layer for fuel scheduling
 * - Conceptual organization (fuel vs ignition)
 * - Future expansion point if needed
 *
 * USAGE:
 * - Include original scheduler.h for all fuel scheduling functions
 * - Use coordinator API for convenience wrapper with validation
 * - Direct calls to setFuelSchedule() for maximum performance
 */

#endif // FUEL_SCHEDULER_H
