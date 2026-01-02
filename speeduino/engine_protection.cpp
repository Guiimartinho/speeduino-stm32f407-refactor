/**
 * @file engine_protection.cpp
 * @brief Engine protection and rev limiting implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * MISRA C:2012 Compliance:
 * - Uses automotive_constants.h for all magic numbers
 * - Input validation on all critical paths
 * - Safe integer arithmetic with overflow protection
 */

#include "engine_protection.h"
#include "engineProtection.h"
#include "scheduledIO.h"
#include "schedule_calcs.h"
#include "maths.h"
#include "globals.h"
#include "modularization_globals.h"
#include "automotive_constants.h"
#include "limp_mode.h"

// Helper: Cut a cylinder based on engine protect type (file scope to reduce nesting)
static inline void cutCylinder(uint8_t x)
{
  switch(configPage6.engineProtectType)
  {
    case PROTECT_CUT_OFF:
      ignitionChannelsOn = 0xFF;
      fuelChannelsOn = 0xFF;
      break;
    case PROTECT_CUT_IGN:
      BIT_CLEAR(ignitionChannelsOn, x);
      disableIgnSchedule(x);
      break;
    case PROTECT_CUT_FUEL:
      BIT_CLEAR(fuelChannelsOn, x);
      disableFuelSchedule(x);
      break;
    case PROTECT_CUT_BOTH:
      BIT_CLEAR(ignitionChannelsOn, x);
      BIT_CLEAR(fuelChannelsOn, x);
      disableFuelSchedule(x);
      disableIgnSchedule(x);
      break;
    default:
      BIT_CLEAR(ignitionChannelsOn, x);
      BIT_CLEAR(fuelChannelsOn, x);
      break;
  }
}

// Helper: Turn cylinder back on with pending ignition logic (file scope to reduce nesting)
static inline void turnCylinderOn(uint8_t x, uint8_t revolutionsToCut)
{
  bool needsIgnitionPending = (revolutionsToCut == 4) &&
                              (BIT_CHECK(fuelChannelsOn, x) == false) &&
                              (configPage6.engineProtectType == PROTECT_CUT_BOTH);
  if (needsIgnitionPending) { BIT_SET(ignitionChannelsPending, x); }
  else { BIT_SET(ignitionChannelsOn, x); }
  BIT_SET(fuelChannelsOn, x);
}

// Helper: Check launch RPM limit condition (file scope to reduce nesting)
static inline bool checkLaunchRPMLimit(void)
{
  uint16_t launchRPMLimit = (configPage6.lnchHardLim * LAUNCH_RPM_MULTIPLIER);
  if (configPage2.hardCutType == HARD_CUT_ROLLING) { launchRPMLimit += (configPage15.rollingProtRPMDelta[0] * 10); }
  return (currentStatus.RPM > launchRPMLimit);
}

// Helper: Check flat shift RPM limit condition (file scope to reduce nesting)
static inline bool checkFlatShiftRPMLimit(void)
{
  uint16_t flatRPMLimit = currentStatus.clutchEngagedRPM;
  if (configPage2.hardCutType == HARD_CUT_ROLLING) { flatRPMLimit += (configPage15.rollingProtRPMDelta[0] * 10); }
  return (currentStatus.RPM > flatRPMLimit);
}

// Helper: Apply rolling cut to all cylinders (file scope to reduce nesting)
static void applyRollingCutToAllCylinders(uint8_t cutPercent, uint8_t revolutionsToCut)
{
  uint8_t maxChannels = max(maxIgnOutputs, maxInjOutputs);
  if (maxChannels > MAX_ENGINE_CHANNELS) { maxChannels = MAX_ENGINE_CHANNELS; }

  for (uint8_t x = 0; x < maxChannels; x++)
  {
    bool shouldCut = (cutPercent == PERCENTAGE_FULL) || (random1to100() < cutPercent);
    if (shouldCut) { cutCylinder(x); }
    else { turnCylinderOn(x, revolutionsToCut); }
  }
}

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
  if(checkEngineProtect() &&
     (configPage4.engineProtectMaxRPM > 0) &&
     (configPage4.engineProtectMaxRPM < (MAX_SAFE_RPM / LAUNCH_RPM_MULTIPLIER)) &&
     (configPage4.engineProtectMaxRPM < maxAllowedRPM)) {
    maxAllowedRPM = configPage4.engineProtectMaxRPM;
  }

  if((currentStatus.launchingHard == true) && (configPage6.lnchHardLim < maxAllowedRPM)) {
    maxAllowedRPM = configPage6.lnchHardLim;
  }

  // All of the above limits are divided by 100, convert back to RPM
  maxAllowedRPM = maxAllowedRPM * LAUNCH_RPM_MULTIPLIER;

  // Validate result is within safe range
  if(maxAllowedRPM > MAX_SAFE_RPM) {
    maxAllowedRPM = MAX_SAFE_RPM;
  }

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

    // Activate limp mode on engine protection trigger (overtemp flag)
    limpModeSetTrigger(LIMP_TRIGGER_OVERTEMP);
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
  if(configPage2.strokes == FOUR_STROKE) {
    revolutionsToCut *= REVOLUTIONS_PER_CYCLE_4STROKE;
  }

  // 4 stroke and non-sequential will cut for 4 revolutions minimum
  // This is to ensure no half fuel ignition cycles take place
  if((configPage4.sparkMode != IGN_MODE_SEQUENTIAL) || (configPage2.injLayout != INJ_SEQUENTIAL)) {
    revolutionsToCut *= MIN_REVOLUTIONS_NON_SEQUENTIAL;
  }

  // First time check
  if(rollingCutLastRev == 0) {
    rollingCutLastRev = currentStatus.startRevolutions;
  }

  // If current RPM is over the max allowed RPM always cut,
  // otherwise check if the required number of revolutions have passed since the last cut
  if((currentStatus.startRevolutions >= (rollingCutLastRev + revolutionsToCut)) || (currentStatus.RPM > maxAllowedRPM))
  {
    uint8_t cutPercent = 0;

    // CRITICAL: Use signed 32-bit to prevent overflow
    // currentStatus.RPM and maxAllowedRPM are uint16_t
    int32_t rpmDelta = (int32_t)currentStatus.RPM - (int32_t)maxAllowedRPM;

    // If the current RPM is over the max allowed RPM then cut is full (100%)
    if(rpmDelta >= 0) {
      cutPercent = PERCENTAGE_FULL;
    }
    else {
      // Safely convert to int8_t with bounds checking
      int8_t deltaDivided = (int8_t)CLAMP((rpmDelta / 10), INT8_MIN, INT8_MAX);
      cutPercent = table2D_getValue(&rollingCutTable, deltaDivided);
    }

    // Apply rolling cut to all cylinders
    applyRollingCutToAllCylinders(cutPercent, revolutionsToCut);
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

  // Launch control check - flatten conditions
  bool launchBaseConditions = configPage6.launchEnabled && currentStatus.clutchTrigger &&
     (currentStatus.clutchEngagedRPM < ((unsigned int)(configPage6.flatSArm) * LAUNCH_RPM_MULTIPLIER)) &&
     (currentStatus.TPS >= configPage10.lnchCtrlTPS);
  bool vssAllowsLaunch = (configPage2.vssMode == 0) || ((configPage2.vssMode > 0) && (currentStatus.vss < configPage10.lnchCtrlVss));

  if (launchBaseConditions && vssAllowsLaunch && checkLaunchRPMLimit())
  {
    currentStatus.launchingHard = true;
    BIT_SET(currentStatus.status2, BIT_STATUS2_HLAUNCH);
    return;
  }

  // If launch is not active, check whether flat shift should be active
  bool flatShiftBaseConditions = configPage6.flatSEnable && currentStatus.clutchTrigger &&
     (currentStatus.clutchEngagedRPM >= ((unsigned int)(configPage6.flatSArm * LAUNCH_RPM_MULTIPLIER)));

  if (flatShiftBaseConditions && checkFlatShiftRPMLimit()) { currentStatus.flatShiftingHard = true; }
}
