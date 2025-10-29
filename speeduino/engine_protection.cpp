/**
 * @file engine_protection.cpp
 * @brief Engine protection and rev limiting implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 */

#include "engine_protection.h"
#include "engineProtection.h"
#include "scheduledIO.h"
#include "schedule_calcs.h"
#include "maths.h"
#include "globals.h"
#include "modularization_globals.h"

//=============================================================================
// Maximum RPM Calculation
//=============================================================================

uint16_t calculateMaxAllowedRPM(void)
{
  // The maximum RPM allowed by all the potential limiters
  // checkRevLimit() returns the current maximum RPM allowed (divided by 100)
  // based on either the fixed hard limit or the current coolant temp
  uint16_t maxAllowedRPM = checkRevLimit();

  // Check each of the functions that has an RPM limit
  // Update the max allowed RPM if the function is active and has a lower RPM than already set
  if(checkEngineProtect() && (configPage4.engineProtectMaxRPM < maxAllowedRPM)) {
    maxAllowedRPM = configPage4.engineProtectMaxRPM;
  }

  if((currentStatus.launchingHard == true) && (configPage6.lnchHardLim < maxAllowedRPM)) {
    maxAllowedRPM = configPage6.lnchHardLim;
  }

  // All of the above limits are divided by 100, convert back to RPM
  maxAllowedRPM = maxAllowedRPM * 100;

  // Flat shifting is a special case as the RPM limit is based on when the clutch was engaged
  // It is not divided by 100 as it is set with the actual RPM
  if((currentStatus.flatShiftingHard == true) && (currentStatus.clutchEngagedRPM < maxAllowedRPM)) {
    maxAllowedRPM = currentStatus.clutchEngagedRPM;
  }

  return maxAllowedRPM;
}

//=============================================================================
// Main Engine Protection Logic
//=============================================================================

void applyEngineProtection(uint16_t maxAllowedRPM)
{
  // Check if current RPM exceeds the maximum allowed
  if(currentStatus.RPM >= maxAllowedRPM)
  {
    // Set hard limit status bit
    BIT_SET(currentStatus.status2, BIT_STATUS2_HRDLIM);
  }
  else if(BIT_CHECK(currentStatus.status2, BIT_STATUS2_HRDLIM))
  {
    // Clear hard limit when RPM drops below limit
    revLimitAllowedEndTime = 0;
    BIT_CLEAR(currentStatus.status2, BIT_STATUS2_HRDLIM);
  }

  // Apply the appropriate cut type
  if((configPage2.hardCutType == HARD_CUT_FULL) && BIT_CHECK(currentStatus.status2, BIT_STATUS2_HRDLIM))
  {
    applyHardCut(maxAllowedRPM);
  }
  else if((configPage2.hardCutType == HARD_CUT_ROLLING) && (currentStatus.RPM > (maxAllowedRPM + (configPage15.rollingProtRPMDelta[0] * 10))))
  {
    // Limit for rolling is the max allowed RPM minus the lowest value in the delta table
    // Delta values are negative
    applyRollingCut(maxAllowedRPM);
  }
  else
  {
    // No engine protection active, so turn all the channels on
    currentStatus.engineProtectStatus = 0;

    if(currentStatus.startRevolutions >= configPage4.StgCycles)
    {
      // Enable the fuel and ignition, assuming staging revolutions are complete
      ignitionChannelsOn = 0xff;
      fuelChannelsOn = 0xff;
    }
  }
}

//=============================================================================
// Hard Cut Implementation
//=============================================================================

void applyHardCut(uint16_t maxAllowedRPM)
{
  // Full hard cut turns outputs off completely
  switch(configPage6.engineProtectType)
  {
    case PROTECT_CUT_OFF:
      // Make sure all channels are turned on
      ignitionChannelsOn = 0xFF;
      fuelChannelsOn = 0xFF;
      currentStatus.engineProtectStatus = 0;
      break;

    case PROTECT_CUT_IGN:
      ignitionChannelsOn = 0;
      disableAllIgnSchedules();
      break;

    case PROTECT_CUT_FUEL:
      fuelChannelsOn = 0;
      disableAllFuelSchedules();
      break;

    case PROTECT_CUT_BOTH:
      ignitionChannelsOn = 0;
      fuelChannelsOn = 0;
      disableAllIgnSchedules();
      disableAllFuelSchedules();
      break;

    default:
      ignitionChannelsOn = 0;
      fuelChannelsOn = 0;
      break;
  }
}

//=============================================================================
// Rolling Cut Implementation
//=============================================================================

void applyRollingCut(uint16_t maxAllowedRPM)
{
  uint8_t revolutionsToCut = 1;

  // 4 stroke needs to cut for at least 2 revolutions
  if(configPage2.strokes == FOUR_STROKE) { revolutionsToCut *= 2; }

  // 4 stroke and non-sequential will cut for 4 revolutions minimum
  // This is to ensure no half fuel ignition cycles take place
  if((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) || (configPage2.injLayout != INJ_SEQUENTIAL)) {
    revolutionsToCut *= 2;
  }

  // First time check
  if(rollingCutLastRev == 0) { rollingCutLastRev = currentStatus.startRevolutions; }

  // If current RPM is over the max allowed RPM always cut,
  // otherwise check if the required number of revolutions have passed since the last cut
  if((currentStatus.startRevolutions >= (rollingCutLastRev + revolutionsToCut)) || (currentStatus.RPM > maxAllowedRPM))
  {
    uint8_t cutPercent = 0;
    int16_t rpmDelta = currentStatus.RPM - maxAllowedRPM;

    // If the current RPM is over the max allowed RPM then cut is full (100%)
    if(rpmDelta >= 0) { cutPercent = 100; }
    else { cutPercent = table2D_getValue(&rollingCutTable, (int8_t)(rpmDelta / 10)); }

    // Iterate through all channels and randomly cut based on percentage
    for(uint8_t x = 0; x < max(maxIgnOutputs, maxInjOutputs); x++)
    {
      if((cutPercent == 100) || (random1to100() < cutPercent))
      {
        // Cut this cylinder
        switch(configPage6.engineProtectType)
        {
          case PROTECT_CUT_OFF:
            // Make sure all channels are turned on
            ignitionChannelsOn = 0xFF;
            fuelChannelsOn = 0xFF;
            break;

          case PROTECT_CUT_IGN:
            BIT_CLEAR(ignitionChannelsOn, x); // Turn off this ignition channel
            disableIgnSchedule(x);
            break;

          case PROTECT_CUT_FUEL:
            BIT_CLEAR(fuelChannelsOn, x); // Turn off this fuel channel
            disableFuelSchedule(x);
            break;

          case PROTECT_CUT_BOTH:
            BIT_CLEAR(ignitionChannelsOn, x); // Turn off this ignition channel
            BIT_CLEAR(fuelChannelsOn, x); // Turn off this fuel channel
            disableFuelSchedule(x);
            disableIgnSchedule(x);
            break;

          default:
            BIT_CLEAR(ignitionChannelsOn, x); // Turn off this ignition channel
            BIT_CLEAR(fuelChannelsOn, x); // Turn off this fuel channel
            break;
        }
      }
      else
      {
        // Turn fuel and ignition channels on

        // Special case for non-sequential, 4-stroke where both fuel and ignition are cut
        // The ignition pulses should wait 1 cycle after the fuel channels are turned back on
        if((revolutionsToCut == 4) &&                           // 4 stroke and non-sequential
           (BIT_CHECK(fuelChannelsOn, x) == false) &&           // Fuel on this channel is currently off
           (configPage6.engineProtectType == PROTECT_CUT_BOTH)) // Both fuel and ignition are cut
        {
          BIT_SET(ignitionChannelsPending, x); // Set this ignition channel as pending
        }
        else {
          BIT_SET(ignitionChannelsOn, x); // Turn on this ignition channel
        }

        BIT_SET(fuelChannelsOn, x); // Turn on this fuel channel
      }
    }

    rollingCutLastRev = currentStatus.startRevolutions;
  }

  // Check whether there are any ignition channels waiting for injection pulses
  // This can only occur when at least 2 revolutions have taken place since fuel was turned back on
  // Note: ignitionChannelsPending can only be >0 on 4 stroke, non-sequential fuel when protect type is Both
  if((ignitionChannelsPending > 0) && (currentStatus.startRevolutions >= (rollingCutLastRev + 2)))
  {
    ignitionChannelsOn = fuelChannelsOn;
    ignitionChannelsPending = 0;
  }
}

//=============================================================================
// Launch Control and Flat Shift
//=============================================================================

void checkLaunchAndFlatShift(void)
{
  // Check for launching/flat shift (clutch) based on the current and previous clutch states
  currentStatus.previousClutchTrigger = currentStatus.clutchTrigger;

  // Only check for pinLaunch if any function using it is enabled
  // Else pins might break starting a board
  if(configPage6.flatSEnable || configPage6.launchEnabled)
  {
    if(configPage6.launchHiLo > 0) {
      currentStatus.clutchTrigger = digitalRead(pinLaunch);
    }
    else {
      currentStatus.clutchTrigger = !digitalRead(pinLaunch);
    }

    // Store the value to send to TunerStudio
    BIT_WRITE(currentStatus.status5, BIT_STATUS5_CLUTCH_PRESS, currentStatus.clutchTrigger);
  }

  // Check whether the clutch has been engaged or disengaged and store the current RPM if so
  if(currentStatus.clutchTrigger && (currentStatus.previousClutchTrigger != currentStatus.clutchTrigger)) {
    currentStatus.clutchEngagedRPM = currentStatus.RPM;
  }

  // Default flags to off
  currentStatus.launchingHard = false;
  BIT_CLEAR(currentStatus.status2, BIT_STATUS2_HLAUNCH);
  currentStatus.flatShiftingHard = false;

  // Launch control check
  if(configPage6.launchEnabled && currentStatus.clutchTrigger &&
     (currentStatus.clutchEngagedRPM < ((unsigned int)(configPage6.flatSArm) * 100)) &&
     (currentStatus.TPS >= configPage10.lnchCtrlTPS))
  {
    // Only enable if VSS is not used or if it is, make sure we're not above the speed limit
    if((configPage2.vssMode == 0) || ((configPage2.vssMode > 0) && (currentStatus.vss < configPage10.lnchCtrlVss)))
    {
      // Check whether RPM is above the launch limit
      uint16_t launchRPMLimit = (configPage6.lnchHardLim * 100);

      // Add the rolling cut delta if enabled (Delta is a negative value)
      if((configPage2.hardCutType == HARD_CUT_ROLLING)) {
        launchRPMLimit += (configPage15.rollingProtRPMDelta[0] * 10);
      }

      if(currentStatus.RPM > launchRPMLimit)
      {
        // HardCut rev limit for 2-step launch control
        currentStatus.launchingHard = true;
        BIT_SET(currentStatus.status2, BIT_STATUS2_HLAUNCH);
      }
    }
  }
  else
  {
    // If launch is not active, check whether flat shift should be active
    if(configPage6.flatSEnable && currentStatus.clutchTrigger &&
       (currentStatus.clutchEngagedRPM >= ((unsigned int)(configPage6.flatSArm * 100))))
    {
      uint16_t flatRPMLimit = currentStatus.clutchEngagedRPM;

      // Add the rolling cut delta if enabled (Delta is a negative value)
      if((configPage2.hardCutType == HARD_CUT_ROLLING)) {
        flatRPMLimit += (configPage15.rollingProtRPMDelta[0] * 10);
      }

      if(currentStatus.RPM > flatRPMLimit)
      {
        // Flat shift rev limit
        currentStatus.flatShiftingHard = true;
      }
    }
  }
}
