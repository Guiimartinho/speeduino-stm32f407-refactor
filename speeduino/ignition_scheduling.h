/**
 * @file ignition_scheduling.h
 * @brief Ignition scheduling for all 8 channels
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * This module handles the scheduling of all 8 ignition coil channels.
 * Each channel is scheduled independently based on:
 * - Ignition start angle (when dwell begins)
 * - Current crank angle
 * - Dwell time (coil charging time)
 * - Channel enable status (ignitionChannelsOn bitmask)
 *
 * The scheduling process:
 * 1. Calculate timeout until dwell should begin
 * 2. If timeout is valid (>0) and channel is enabled
 * 3. Set the ignition schedule with timeout and dwell time
 *
 * Special features:
 * - Fixed cranking override for extended dwell during cranking
 * - Ignition refresh (USE_IGN_REFRESH) for dynamic timing correction
 * - Per-tooth ignition for precise timing control
 *
 * Hardware timers used: TIM2 (IGN1-4), TIM4 (IGN5-8)
 */

#ifndef IGNITION_SCHEDULING_H
#define IGNITION_SCHEDULING_H

#include "globals.h"

/**
 * @brief Schedule ignition for all active channels
 *
 * Main ignition scheduling function that:
 * 1. Checks for fixed cranking override mode (ignCranklock)
 * 2. Applies fixed dwell extension during cranking (3x normal dwell)
 * 3. Adjusts start angles slightly at very low RPM (<250) to prevent timing issues
 * 4. Iterates through all 8 ignition channels and schedules them
 *
 * Each channel is scheduled if:
 * - Channel exists (maxIgnOutputs >= channel number)
 * - Channel is enabled (ignitionChannelsOn bit is set)
 * - Timeout calculation returns valid value (>0)
 * - ignitionChannelsOn > 0 (global enable)
 *
 * Fixed Cranking Override:
 * Available only on basic distributor and 4g63 decoders when ignCranklock is enabled.
 * Extends dwell by 3x during cranking so decoder can trigger spark upon seeing a
 * specific tooth. Start angles are moved forward 5 degrees at very low RPM (<250)
 * to prevent start time occurring after target tooth pulse.
 *
 * Ignition Refresh (if USE_IGN_REFRESH defined):
 * For channel 1 only, dynamically recalculates timing if schedule is already running
 * and spark hasn't fired yet. Prevents timing drift during acceleration/deceleration.
 *
 * Each channel uses:
 * - calculateIgnitionTimeout() to determine when to start dwell
 * - setIgnitionSchedule() to queue the ignition event
 */
void scheduleIgnition(void);

#endif // IGNITION_SCHEDULING_H
