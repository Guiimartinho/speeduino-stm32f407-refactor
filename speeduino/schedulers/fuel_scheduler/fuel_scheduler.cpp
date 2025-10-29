/**
 * @file fuel_scheduler.cpp
 * @brief Fuel scheduling subsystem implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This file is intentionally minimal - it serves as a DOCUMENTATION LAYER.
 * All fuel scheduling implementations remain in original scheduler.cpp (100% preserved).
 *
 * ARCHITECTURE:
 * - No runtime code here (all in scheduler.cpp)
 * - This module organizes documentation
 * - Future expansion point if needed
 *
 * PRESERVES 100% LOGIC from original scheduler.cpp
 */

#include "fuel_scheduler.h"
#include "../../scheduler.h"

// ============================================================================
// FUEL SCHEDULING MODULE
// ============================================================================

/*
 * This compilation unit is intentionally empty.
 *
 * All fuel scheduling functions remain in scheduler.cpp:
 * - setFuelSchedule() - inline in scheduler.h
 * - _setFuelScheduleRunning() - scheduler.cpp:215
 * - _setFuelScheduleNext() - scheduler.cpp:231
 * - disableFuelSchedule() - scheduler.cpp:576
 * - disableAllFuelSchedules() - scheduler.cpp:671
 * - fuelScheduleISR() - scheduler.cpp:327 (inline)
 * - fuelSchedule1-8Interrupt() - scheduler.cpp:370-447
 *
 * WHY:
 * - Performance: Inline functions must stay inline
 * - ISR Timing: <10µs requirement - no wrappers allowed
 * - Logic Preservation: 100% original code unchanged
 *
 * THIS MODULE:
 * - Provides documentation layer (fuel_scheduler.h)
 * - Organizes fuel-specific concepts
 * - Future expansion point
 *
 * USAGE:
 * - Include scheduler.h for actual fuel scheduling
 * - Include fuel_scheduler.h for documentation only
 * - Use coordinator API for convenience wrappers
 */
