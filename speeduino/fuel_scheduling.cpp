/**
 * @file fuel_scheduling.cpp
 * @brief Fuel injection scheduling implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 */

#include "fuel_scheduling.h"
#include "scheduledIO.h"
#include "schedule_calcs.h"
#include "decoders.h"
#include "globals.h"
#include "speeduino.h"
#include "modularization_globals.h"

//=============================================================================
// Fuel Injection Scheduling
//=============================================================================

void scheduleFuelInjection(uint16_t pwLimit)
{
  // Get current crank angle for all calculations
  uint16_t crankAngle = getCrankAngle();

  /*-----------------------------------------------------------------------------------------
  | A Note on tempCrankAngle and tempStartAngle:
  |   The use of tempCrankAngle/tempStartAngle is described below. It is then used in the
  |   same way for channels 2, 3 and 4+ on both injectors and ignition
  |
  |   Essentially, these 2 variables are used to realign the current crank angle and the
  |   desired start angle around 0 degrees for the given cylinder/output
  |
  |   Eg: If cylinder 2 TDC is 180 degrees after cylinder 1 (Eg a standard 4 cylinder engine),
  |       then tempCrankAngle is 180* less than the current crank angle and tempStartAngle
  |       is the desired open time less 180*. Thus the cylinder is being treated relative to
  |       its own TDC, regardless of its offset
  |
  |   This is done to avoid problems with very short or very long times until tempStartAngle.
  |------------------------------------------------------------------------------------------
  */

  //-----------------------------------
  // Injector Channel 1
  //-----------------------------------
  #if INJ_CHANNELS >= 1
  if((maxInjOutputs >= 1) && (currentStatus.PW1 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ1_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule1, injector1StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule1, timeOut, (unsigned long)currentStatus.PW1);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 2
  //-----------------------------------
  #if INJ_CHANNELS >= 2
  if((maxInjOutputs >= 2) && (currentStatus.PW2 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ2_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule2, injector2StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule2, timeOut, (unsigned long)currentStatus.PW2);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 3
  //-----------------------------------
  #if INJ_CHANNELS >= 3
  if((maxInjOutputs >= 3) && (currentStatus.PW3 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ3_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule3, injector3StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule3, timeOut, (unsigned long)currentStatus.PW3);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 4
  //-----------------------------------
  #if INJ_CHANNELS >= 4
  if((maxInjOutputs >= 4) && (currentStatus.PW4 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ4_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule4, injector4StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule4, timeOut, (unsigned long)currentStatus.PW4);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 5
  //-----------------------------------
  #if INJ_CHANNELS >= 5
  if((maxInjOutputs >= 5) && (currentStatus.PW5 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ5_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule5, injector5StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule5, timeOut, (unsigned long)currentStatus.PW5);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 6
  //-----------------------------------
  #if INJ_CHANNELS >= 6
  if((maxInjOutputs >= 6) && (currentStatus.PW6 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ6_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule6, injector6StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule6, timeOut, (unsigned long)currentStatus.PW6);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 7
  //-----------------------------------
  #if INJ_CHANNELS >= 7
  if((maxInjOutputs >= 7) && (currentStatus.PW7 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ7_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule7, injector7StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule7, timeOut, (unsigned long)currentStatus.PW7);
    }
  }
  #endif

  //-----------------------------------
  // Injector Channel 8
  //-----------------------------------
  #if INJ_CHANNELS >= 8
  if((maxInjOutputs >= 8) && (currentStatus.PW8 >= inj_opentime_uS) && (BIT_CHECK(fuelChannelsOn, INJ8_CMD_BIT)))
  {
    uint32_t timeOut = calculateInjectorTimeout(fuelSchedule8, injector8StartAngle, crankAngle);
    if(timeOut > 0U)
    {
      setFuelSchedule(fuelSchedule8, timeOut, (unsigned long)currentStatus.PW8);
    }
  }
  #endif
}
