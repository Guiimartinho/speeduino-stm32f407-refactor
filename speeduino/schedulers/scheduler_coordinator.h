/**
 * @file scheduler_coordinator.h
 * @brief Scheduler coordinator - unified API for fuel and ignition scheduling
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module coordinates all scheduling subsystems:
 * - Fuel scheduling (8 channels, injection timing)
 * - Ignition scheduling (8 channels, dwell + spark timing)
 *
 * ARCHITECTURE:
 * - Direct wrapper pattern (like Module 5 - Sensors)
 * - NO function pointer dispatch (schedulers are fixed 8 channels)
 * - Guard clauses for safety (except ISRs)
 * - ISRs preserved 100% original (inline, no wrappers)
 * - 100% preservation of original logic
 * - RTOS-ready (thread-safe after initialization)
 *
 * CRITICAL PERFORMANCE:
 * - ISR timing: <10µs requirement (5µs ideal)
 * - Inline functions preserved (setFuelSchedule, setIgnitionSchedule)
 * - Zero overhead after compiler optimization
 *
 * PRESERVES 100% LOGIC from original scheduler.h/cpp
 */

#ifndef SCHEDULER_COORDINATOR_H
#define SCHEDULER_COORDINATOR_H

#include <stdint.h>
#include "../globals.h"

// ============================================================================
// INITIALIZATION API
// ============================================================================

/**
 * @brief Initialize all schedulers (fuel + ignition)
 * @details Calls original initialiseSchedulers()
 * @post All 16 schedules (8 fuel + 8 ignition) reset to OFF state
 * @note NOT timing-critical (called once during ECU init)
 */
void schedulerCoordinatorInitialize(void);

/**
 * @brief Begin injector priming pulses
 * @details Calls original beginInjectorPriming()
 * @note Priming pulse based on coolant temperature
 * @note Only runs if TPS <= floodClear threshold
 */
void schedulerCoordinatorBeginPriming(void);

// ============================================================================
// FUEL SCHEDULING API
// ============================================================================

/**
 * @brief Schedule fuel injection on specific channel
 * @param scheduleNum Schedule number (0-7 for channels 1-8)
 * @param timeout Time until injection starts (microseconds)
 * @param duration Injection pulse width (microseconds)
 * @note Calls original setFuelSchedule() inline function
 * @note Guard clause for channel validation
 */
void schedulerCoordinatorSetFuel(uint8_t scheduleNum,
                                  uint32_t timeout,
                                  uint32_t duration);

/**
 * @brief Disable fuel schedule on specific channel
 * @param channel Channel number (0-7 for channels 1-8)
 * @note Calls original disableFuelSchedule()
 * @note Guard clause for channel validation
 */
void schedulerCoordinatorDisableFuel(uint8_t channel);

/**
 * @brief Disable all fuel schedules (emergency/safety)
 * @note Calls original disableAllFuelSchedules()
 * @note Disables all 8 fuel channels
 */
void schedulerCoordinatorDisableAllFuel(void);

// ============================================================================
// IGNITION SCHEDULING API
// ============================================================================

/**
 * @brief Schedule ignition on specific channel
 * @param scheduleNum Schedule number (0-7 for channels 1-8)
 * @param timeout Time until dwell starts (microseconds)
 * @param duration Dwell time (microseconds)
 * @note Calls original setIgnitionSchedule() inline function
 * @note Guard clause for channel validation
 */
void schedulerCoordinatorSetIgnition(uint8_t scheduleNum,
                                      uint32_t timeout,
                                      uint32_t duration);

/**
 * @brief Refresh ignition schedule 1 end time
 * @param timeToEnd New time to ignition end (microseconds)
 * @note Calls original refreshIgnitionSchedule1()
 * @note Used for per-tooth timing adjustments
 * @warning Timing-sensitive - uses noInterrupts()
 */
void schedulerCoordinatorRefreshIgnition1(uint32_t timeToEnd);

/**
 * @brief Disable ignition schedule on specific channel
 * @param channel Channel number (0-7 for channels 1-8)
 * @note Calls original disableIgnSchedule()
 * @note Guard clause for channel validation
 */
void schedulerCoordinatorDisableIgnition(uint8_t channel);

/**
 * @brief Disable all ignition schedules (emergency/safety)
 * @note Calls original disableAllIgnSchedules()
 * @note Disables all 8 ignition channels
 */
void schedulerCoordinatorDisableAllIgnition(void);

// ============================================================================
// UTILITY API
// ============================================================================

/**
 * @brief Check if scheduler coordinator is initialized
 * @return true if initialized, false otherwise
 */
bool schedulerCoordinatorIsInitialized(void);

/**
 * @brief Get scheduler coordinator name
 * @return Coordinator name string
 */
const char* schedulerCoordinatorGetName(void);

// ============================================================================
// IMPORTANT NOTES
// ============================================================================

/*
 * ISR FUNCTIONS ARE NOT WRAPPED:
 *
 * The following ISR functions remain 100% in original scheduler.cpp:
 * - fuelSchedule1-8Interrupt()      (called by hardware timer ISR)
 * - ignitionSchedule1-8Interrupt()  (called by hardware timer ISR)
 *
 * WHY NOT WRAPPED:
 * - ISR timing is CRITICAL (<10µs requirement)
 * - Functions are marked "always_inline" for zero overhead
 * - Any wrapper would add unacceptable overhead
 * - Direct hardware timer → ISR call is fastest path
 *
 * INLINE FUNCTIONS ARE NOT WRAPPED:
 *
 * The following inline functions remain 100% in original scheduler.h:
 * - setFuelSchedule()       (always_inline, ~15 lines)
 * - setIgnitionSchedule()   (always_inline, ~15 lines)
 *
 * WHY NOT WRAPPED:
 * - Marked "always_inline" for performance
 * - Compiler inlines them at call site (zero overhead)
 * - Wrapping would force function call overhead
 * - Used extensively in timing-critical code
 *
 * COORDINATOR PROVIDES:
 * - Convenience API with channel validation
 * - Unified interface for fuel + ignition
 * - Guard clauses for safety
 * - Documentation layer
 *
 * For maximum performance, advanced users can still call:
 * - setFuelSchedule(fuelSchedule1, timeout, duration) directly
 * - setIgnitionSchedule(ignitionSchedule1, timeout, duration) directly
 */

#endif // SCHEDULER_COORDINATOR_H
