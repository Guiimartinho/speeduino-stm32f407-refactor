/**
 * @file ignition_scheduling.cpp
 * @brief Ignition scheduling implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 */

#include "ignition_scheduling.h"
#include "scheduledIO.h"
#include "schedule_calcs.h"
#include "decoders.h"
#include "maths.h"
#include "globals.h"
#include "modularization_globals.h"

//=============================================================================
// Ignition Scheduling
//=============================================================================

void scheduleIgnition(void)
{
  uint32_t fixedCrankingOverride = 0;

  //***********************************************************************************************
  //| BEGIN IGNITION SCHEDULES
  //***********************************************************************************************

  // fixedCrankingOverride is used to extend the dwell during cranking so that the decoder
  // can trigger the spark upon seeing a certain tooth. Currently only available on the
  // basic distributor and 4g63 decoders.
  if(configPage4.ignCranklock && BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK) && (BIT_CHECK(decoderState, BIT_DECODER_HAS_FIXED_CRANKING)))
  {
    fixedCrankingOverride = currentStatus.dwell * 3;

    // This is a safety step to prevent the ignition start time occurring AFTER the target
    // tooth pulse has already occurred. It simply moves the start time forward a little,
    // which is compensated for by the increase in the dwell time
    if(currentStatus.RPM < 250)
    {
      ignition1StartAngle -= 5;
      ignition2StartAngle -= 5;
      ignition3StartAngle -= 5;
      ignition4StartAngle -= 5;
      #if IGN_CHANNELS >= 5
      ignition5StartAngle -= 5;
      #endif
      #if IGN_CHANNELS >= 6
      ignition6StartAngle -= 5;
      #endif
      #if IGN_CHANNELS >= 7
      ignition7StartAngle -= 5;
      #endif
      #if IGN_CHANNELS >= 8
      ignition8StartAngle -= 5;
      #endif
    }
  }
  else {
    fixedCrankingOverride = 0;
  }

  if(ignitionChannelsOn > 0)
  {
    // Refresh the current crank angle info
    uint16_t crankAngle = ignitionLimits(getCrankAngle());

    //-----------------------------------
    // Ignition Channel 1
    //-----------------------------------
    #if IGN_CHANNELS >= 1
    uint32_t timeOut = calculateIgnitionTimeout(ignitionSchedule1, ignition1StartAngle, channel1IgnDegrees, crankAngle);
    if((timeOut > 0U) && (BIT_CHECK(ignitionChannelsOn, IGN1_CMD_BIT)))
    {
      setIgnitionSchedule(ignitionSchedule1, timeOut, currentStatus.dwell + fixedCrankingOverride);
    }
    #endif

    //-----------------------------------
    // Ignition Refresh for Channel 1
    // (Dynamic timing correction during acceleration/deceleration)
    //-----------------------------------
    #if defined(USE_IGN_REFRESH)
    if((ignitionSchedule1.Status == RUNNING) && (ignition1EndAngle > crankAngle) && (configPage4.StgCycles == 0) && (configPage2.perToothIgn != true))
    {
      unsigned long uSToEnd = 0;

      // Refresh the crank angle info
      crankAngle = ignitionLimits(getCrankAngle());

      // Calculate time until spark should fire
      if(ignition1EndAngle > crankAngle) {
        uSToEnd = angleToTimeMicroSecPerDegree((ignition1EndAngle - crankAngle));
      }
      else {
        uSToEnd = angleToTimeMicroSecPerDegree((360 + ignition1EndAngle - crankAngle));
      }

      refreshIgnitionSchedule1(uSToEnd + fixedCrankingOverride);
    }
    #endif

    //-----------------------------------
    // Ignition Channel 2
    //-----------------------------------
    #if IGN_CHANNELS >= 2
    if(maxIgnOutputs >= 2)
    {
      unsigned long ignition2StartTime = calculateIgnitionTimeout(ignitionSchedule2, ignition2StartAngle, channel2IgnDegrees, crankAngle);

      if((ignition2StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN2_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule2, ignition2StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

    //-----------------------------------
    // Ignition Channel 3
    //-----------------------------------
    #if IGN_CHANNELS >= 3
    if(maxIgnOutputs >= 3)
    {
      unsigned long ignition3StartTime = calculateIgnitionTimeout(ignitionSchedule3, ignition3StartAngle, channel3IgnDegrees, crankAngle);

      if((ignition3StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN3_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule3, ignition3StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

    //-----------------------------------
    // Ignition Channel 4
    //-----------------------------------
    #if IGN_CHANNELS >= 4
    if(maxIgnOutputs >= 4)
    {
      unsigned long ignition4StartTime = calculateIgnitionTimeout(ignitionSchedule4, ignition4StartAngle, channel4IgnDegrees, crankAngle);

      if((ignition4StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN4_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule4, ignition4StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

    //-----------------------------------
    // Ignition Channel 5
    //-----------------------------------
    #if IGN_CHANNELS >= 5
    if(maxIgnOutputs >= 5)
    {
      unsigned long ignition5StartTime = calculateIgnitionTimeout(ignitionSchedule5, ignition5StartAngle, channel5IgnDegrees, crankAngle);

      if((ignition5StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN5_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule5, ignition5StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

    //-----------------------------------
    // Ignition Channel 6
    //-----------------------------------
    #if IGN_CHANNELS >= 6
    if(maxIgnOutputs >= 6)
    {
      unsigned long ignition6StartTime = calculateIgnitionTimeout(ignitionSchedule6, ignition6StartAngle, channel6IgnDegrees, crankAngle);

      if((ignition6StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN6_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule6, ignition6StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

    //-----------------------------------
    // Ignition Channel 7
    //-----------------------------------
    #if IGN_CHANNELS >= 7
    if(maxIgnOutputs >= 7)
    {
      unsigned long ignition7StartTime = calculateIgnitionTimeout(ignitionSchedule7, ignition7StartAngle, channel7IgnDegrees, crankAngle);

      if((ignition7StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN7_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule7, ignition7StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

    //-----------------------------------
    // Ignition Channel 8
    //-----------------------------------
    #if IGN_CHANNELS >= 8
    if(maxIgnOutputs >= 8)
    {
      unsigned long ignition8StartTime = calculateIgnitionTimeout(ignitionSchedule8, ignition8StartAngle, channel8IgnDegrees, crankAngle);

      if((ignition8StartTime > 0) && (BIT_CHECK(ignitionChannelsOn, IGN8_CMD_BIT)))
      {
        setIgnitionSchedule(ignitionSchedule8, ignition8StartTime, currentStatus.dwell + fixedCrankingOverride);
      }
    }
    #endif

  } // ignitionChannelsOn > 0
}
