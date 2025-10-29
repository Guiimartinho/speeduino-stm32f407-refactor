/**
 * @file fuel_scheduling.h
 * @brief Fuel injection scheduling for all 8 channels
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * This module handles the scheduling of all 8 fuel injector channels.
 * Each channel is scheduled independently based on:
 * - Injector start angle (configured per cylinder)
 * - Current crank angle
 * - Pulsewidth (PW1-PW8)
 * - Channel enable status (fuelChannelsOn bitmask)
 *
 * The scheduling process:
 * 1. Calculate timeout until injector should open
 * 2. If timeout is valid (>0) and channel is enabled
 * 3. Set the fuel schedule with timeout and pulsewidth
 *
 * Hardware timers used: TIM3 (INJ1-4), TIM5 (INJ5-8)
 */

#ifndef FUEL_SCHEDULING_H
#define FUEL_SCHEDULING_H

#include "globals.h"

/**
 * @brief Schedule fuel injection for all active channels
 *
 * Iterates through all 8 injector channels and schedules them if:
 * - Channel exists (maxInjOutputs >= channel number)
 * - Pulsewidth is valid (PWx >= inj_opentime_uS)
 * - Channel is enabled (fuelChannelsOn bit is set)
 * - Timeout calculation returns valid value (>0)
 *
 * Each channel uses:
 * - calculateInjectorTimeout() to determine when to fire
 * - setFuelSchedule() to queue the injection event
 *
 * Note on tempCrankAngle/tempStartAngle:
 * These variables realign the current crank angle and desired start angle
 * around 0 degrees for the given cylinder, treating each cylinder relative
 * to its own TDC regardless of offset.
 *
 * Example: If cylinder 2 TDC is 180 degrees after cylinder 1 (standard 4-cyl),
 * then tempCrankAngle is current angle minus 180, and tempStartAngle is
 * desired open time minus 180.
 *
 * @param pwLimit Maximum pulsewidth limit (unused in scheduling, only in calculations)
 */
void scheduleFuelInjection(uint16_t pwLimit);

#endif // FUEL_SCHEDULING_H
