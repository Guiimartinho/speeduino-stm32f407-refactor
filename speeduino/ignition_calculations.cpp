/**
 * @file ignition_calculations.cpp
 * @brief Ignition calculation functions implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 */

#include "ignition_calculations.h"
#include "corrections.h"
#include "schedule_calcs.h"
#include "decoders.h"
#include "globals.h"
#include "init.h"

// External references to table objects
extern table3d16RpmLoad ignitionTable;
extern table2D_u8_u8_8 rotarySplitTable;

//=============================================================================
// Advance Lookup
//=============================================================================

int8_t getAdvance1(void)
{
  // Calculate the load value based on the selected ignition algorithm
  currentStatus.ignLoad = getLoad(configPage2.ignAlgorithm, currentStatus);

  // Perform lookup into ignition map for RPM vs Load value
  // The raw table value has an offset that needs to be removed
  // Then pass through corrections for knock, coolant temp, etc.
  return correctionsIgn((int16_t)get3DTableValue(&ignitionTable, currentStatus.ignLoad, currentStatus.RPM) - INT16_C(OFFSET_IGNITION));
}

//=============================================================================
// Dwell Calculation
//=============================================================================

uint16_t calculateDwell(void)
{
  uint16_t dwell;

  // Dwell is stored as ms * 10. ie Dwell of 4.3ms would be 43 in configPage4
  // This number therefore needs to be multiplied by 100 to get dwell in uS

  if(BIT_CHECK(currentStatus.engine, BIT_ENGINE_CRANK))
  {
    // Use cranking dwell (typically longer for reliable cold starts)
    dwell = (configPage4.dwellCrank * 100U);
  }
  else
  {
    if(configPage2.useDwellMap == true)
    {
      // Use running dwell from 4x4 map (table3d4RpmLoad dwellTable from globals.h)
      dwell = (get3DTableValue(&dwellTable, currentStatus.ignLoad, currentStatus.RPM) * 100U);
    }
    else
    {
      // Use fixed running dwell
      dwell = (configPage4.dwellRun * 100U);
    }
  }

  // Apply corrections (primarily battery voltage compensation)
  dwell = correctionsDwell(dwell);

  return dwell;
}

//=============================================================================
// Ignition Angle Calculation
//=============================================================================

void calculateIgnitionAngles(uint16_t dwellAngle)
{
  // Calculate start and end angles for all cylinders based on cylinder count
  switch(configPage2.nCylinders)
  {
    //-----------------------------------
    // 1 cylinder
    //-----------------------------------
    case 1:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      break;

    //-----------------------------------
    // 2 cylinders
    //-----------------------------------
    case 2:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      break;

    //-----------------------------------
    // 3 cylinders
    //-----------------------------------
    case 3:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
      break;

    //-----------------------------------
    // 4 cylinders
    //-----------------------------------
    case 4:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);

      #if IGN_CHANNELS >= 4
      if((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && currentStatus.hasSync)
      {
        // Sequential mode requires full 720 degree cycle
        if(CRANK_ANGLE_MAX_IGN != 720) { changeHalfToFullSync(); }

        calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
        calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);
      }
      else if(configPage4.sparkMode == IGN_MODE_ROTARY)
      {
        // Rotary mode: Calculate trailing spark split from leading
        byte splitDegrees = 0;
        splitDegrees = table2D_getValue(&rotarySplitTable, (uint8_t)currentStatus.ignLoad);

        // The trailing angles are set relative to the leading ones
        calculateIgnitionTrailingRotary(dwellAngle, splitDegrees, ignition1EndAngle, &ignition3EndAngle, &ignition3StartAngle);
        calculateIgnitionTrailingRotary(dwellAngle, splitDegrees, ignition2EndAngle, &ignition4EndAngle, &ignition4StartAngle);
      }
      else
      {
        // Wasted spark mode can use half-sync (360 degrees)
        if(BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC) && (CRANK_ANGLE_MAX_IGN != 360)) {
          changeFullToHalfSync();
        }
      }
      #endif
      break;

    //-----------------------------------
    // 5 cylinders
    //-----------------------------------
    case 5:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
      calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);
      #if (IGN_CHANNELS >= 5)
      calculateIgnitionAngle(dwellAngle, channel5IgnDegrees, currentStatus.advance, &ignition5EndAngle, &ignition5StartAngle);
      #endif
      break;

    //-----------------------------------
    // 6 cylinders
    //-----------------------------------
    case 6:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);

      #if IGN_CHANNELS >= 6
      if((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && currentStatus.hasSync)
      {
        // Sequential mode requires full 720 degree cycle
        if(CRANK_ANGLE_MAX_IGN != 720) { changeHalfToFullSync(); }

        calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);
        calculateIgnitionAngle(dwellAngle, channel5IgnDegrees, currentStatus.advance, &ignition5EndAngle, &ignition5StartAngle);
        calculateIgnitionAngle(dwellAngle, channel6IgnDegrees, currentStatus.advance, &ignition6EndAngle, &ignition6StartAngle);
      }
      else
      {
        // Wasted spark mode can use half-sync (360 degrees)
        if(BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC) && (CRANK_ANGLE_MAX_IGN != 360)) {
          changeFullToHalfSync();
        }
      }
      #endif
      break;

    //-----------------------------------
    // 8 cylinders (SCG-ECU 2.0 target)
    //-----------------------------------
    case 8:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
      calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);

      #if IGN_CHANNELS >= 8
      if((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && currentStatus.hasSync)
      {
        // Sequential mode requires full 720 degree cycle
        if(CRANK_ANGLE_MAX_IGN != 720) { changeHalfToFullSync(); }

        calculateIgnitionAngle(dwellAngle, channel5IgnDegrees, currentStatus.advance, &ignition5EndAngle, &ignition5StartAngle);
        calculateIgnitionAngle(dwellAngle, channel6IgnDegrees, currentStatus.advance, &ignition6EndAngle, &ignition6StartAngle);
        calculateIgnitionAngle(dwellAngle, channel7IgnDegrees, currentStatus.advance, &ignition7EndAngle, &ignition7StartAngle);
        calculateIgnitionAngle(dwellAngle, channel8IgnDegrees, currentStatus.advance, &ignition8EndAngle, &ignition8StartAngle);
      }
      else
      {
        // Wasted spark mode can use half-sync (360 degrees)
        if(BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC) && (CRANK_ANGLE_MAX_IGN != 360)) {
          changeFullToHalfSync();
        }
      }
      #endif
      break;

    //-----------------------------------
    // Default case (>8 cylinders)
    //-----------------------------------
    default:
      // Do nothing for unsupported cylinder counts
      break;
  }
}
