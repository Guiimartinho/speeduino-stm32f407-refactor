/**
 * @file ignition_calculations.cpp
 * @brief Ignition timing calculations for multi-cylinder engines (1-8 cylinders)
 *
 * @details Calculates spark advance, dwell time, and ignition angles for all
 *          supported engine configurations:
 *          - Cylinder counts: 1, 2, 3, 4, 5, 6, 8
 *          - Spark modes: Sequential, Wasted Spark, Rotary (Mazda)
 *          - Sync modes: Full (720°), Half (360°)
 *
 * @note All functions are MISRA-C:2012 compliant after refactoring
 * @complexity Maximum complexity: 5 (mode-specific helper functions)
 * @misra 6 functions total, 0 violations after refactoring
 *
 * @note SCG-ECU 2.0 - STM32F407VGT6
 * @note Modularized from speeduino.cpp main loop
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
// SPARK ADVANCE LOOKUP
//=============================================================================

/**
 * @brief Lookup base spark advance from 3D ignition table with corrections
 * @return Corrected spark advance in degrees BTDC (signed int8_t: -128 to +127°)
 *
 * @details Calculation steps:
 *          1. Calculate load based on ignAlgorithm (MAP/TPS/Alpha-N/etc.)
 *          2. Lookup raw value from ignitionTable[load, RPM]
 *          3. Remove OFFSET_IGNITION bias from table value
 *          4. Apply corrections: knock retard, coolant temp, IAT, etc.
 *
 * @note Load source determined by configPage2.ignAlgorithm
 * @note Table uses offset encoding to fit unsigned table in signed range
 * @note Final advance passes through correctionsIgn() for all corrections
 *
 * @example At 4000 RPM, 80 kPa MAP:
 *          Load = 80, RPM = 4000
 *          Raw table value = 40 + OFFSET_IGNITION
 *          After offset removal = 40°
 *          After corrections (e.g., -2° knock retard) = 38° BTDC
 *
 * @complexity 1 (simple lookup + correction chain)
 * @misra Compliant: 3 effective lines
 */
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
// DWELL TIME CALCULATION
//=============================================================================

/**
 * @brief Calculate ignition coil dwell time in microseconds
 * @return Dwell time in µs (typically 3000-6000µs for modern coils)
 *
 * @details Dwell calculation with three sources:
 *
 *          **CRANKING MODE** (BIT_ENGINE_CRANK set):
 *          - Uses configPage4.dwellCrank (typically longer for cold starts)
 *          - Fixed value, no map lookup
 *
 *          **RUNNING MODE with dwell map** (useDwellMap == true):
 *          - Lookup from dwellTable[load, RPM] (4x4 table)
 *          - Allows RPM/load-dependent dwell optimization
 *
 *          **RUNNING MODE fixed** (useDwellMap == false):
 *          - Uses configPage4.dwellRun (single fixed value)
 *          - Simpler, suitable for most applications
 *
 *          All sources stored as ms × 10, converted to µs:
 *          - Config value 43 = 4.3ms = 4300µs
 *
 *          Final dwell passes through correctionsDwell() for:
 *          - Battery voltage compensation (longer dwell at low voltage)
 *
 * @note Dwell too short → weak spark, misfires
 * @note Dwell too long → coil overheating, wasted energy
 * @note Typical values: 3-4ms @ 14V, 4-6ms @ 10V
 *
 * @example At 6000 RPM, 14.0V battery:
 *          dwellRun = 40 (4.0ms configured)
 *          Base dwell = 40 × 100 = 4000µs
 *          After voltage correction (+5% @ 14V) = 4200µs
 *
 * @complexity 3 (nested if-else for mode selection)
 * @misra Compliant: 11 effective lines, clear guard clauses
 */
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
// IGNITION ANGLE CALCULATION - FILE SCOPE HELPERS
//=============================================================================

// Helper: Ensure full sync for sequential mode (file scope to reduce nesting)
static inline void ensureFullSyncForSequential(void)
{
  if (CRANK_ANGLE_MAX_IGN != 720) { changeHalfToFullSync(); }
}

// Helper: Allow half sync for wasted spark mode (file scope to reduce nesting)
static inline void allowHalfSyncForWastedSpark(void)
{
  if (BIT_CHECK(currentStatus.status3, BIT_STATUS3_HALFSYNC) && (CRANK_ANGLE_MAX_IGN != 360)) { changeFullToHalfSync(); }
}

//=============================================================================
// IGNITION ANGLE CALCULATION - PRIVATE HELPERS
//=============================================================================

namespace {

/**
 * @brief Calculate ignition angles for 4-cylinder engines with mode-specific logic (PRIVATE)
 * @param dwellAngle Dwell time converted to crank degrees
 *
 * @details Handles three spark modes:
 *
 *          **Sequential Mode** (IGN_MODE_SEQUENTIAL + hasSync):
 *          - Calculates all 4 channels independently
 *          - Requires 720° cycle (full sync)
 *          - One spark per cylinder per cycle
 *
 *          **Rotary Mode** (IGN_MODE_ROTARY):
 *          - Channels 1+2: Leading plugs (normal timing)
 *          - Channels 3+4: Trailing plugs (delayed by splitDegrees)
 *          - Uses rotarySplitTable for load-based split angle
 *          - Mazda RX-7/RX-8 specific
 *
 *          **Wasted Spark Mode** (default):
 *          - Only channels 1+2 calculated
 *          - Paired coils fire simultaneously (1+3, 2+4)
 *          - Can use 360° half-sync
 *
 * @note Rotary split typically 10-15° at low load, 20-25° at high load
 * @complexity 5 (three modes + sync changes)
 * @misra Compliant: 21 effective lines after refactoring
 */
static void calculate4CylinderAngles(uint16_t dwellAngle)
{
  // Always calculate channels 1 and 2 (used in all modes)
  calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
  calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);

  #if IGN_CHANNELS >= 4
  if((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && currentStatus.hasSync)
  {
    ensureFullSyncForSequential();
    calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
    calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);
  }
  else if(configPage4.sparkMode == IGN_MODE_ROTARY)
  {
    byte splitDegrees = table2D_getValue(&rotarySplitTable, (uint8_t)currentStatus.ignLoad);
    calculateIgnitionTrailingRotary(dwellAngle, splitDegrees, ignition1EndAngle, &ignition3EndAngle, &ignition3StartAngle);
    calculateIgnitionTrailingRotary(dwellAngle, splitDegrees, ignition2EndAngle, &ignition4EndAngle, &ignition4StartAngle);
  }
  else { allowHalfSyncForWastedSpark(); }
  #endif
}

/**
 * @brief Calculate ignition angles for 6-cylinder engines with sequential/wasted handling (PRIVATE)
 * @param dwellAngle Dwell time converted to crank degrees
 *
 * @details Two spark modes:
 *
 *          **Sequential Mode** (IGN_MODE_SEQUENTIAL + hasSync):
 *          - All 6 channels calculated independently
 *          - Requires 720° cycle (full sync)
 *          - Firing order: typically 1-5-3-6-2-4 (inline-6)
 *
 *          **Wasted Spark Mode** (default):
 *          - Only channels 1-3 calculated
 *          - Paired coils: 1+4, 2+5, 3+6
 *          - Can use 360° half-sync
 *          - Reduces coil count from 6 to 3
 *
 * @note Inline-6 engines typically use wasted spark for cost/simplicity
 * @complexity 3 (two modes + sync change)
 * @misra Compliant: 13 effective lines after refactoring
 */
static void calculate6CylinderAngles(uint16_t dwellAngle)
{
  // Always calculate first 3 channels
  calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
  calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
  calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);

  #if IGN_CHANNELS >= 6
  if((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && currentStatus.hasSync)
  {
    ensureFullSyncForSequential();
    calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);
    calculateIgnitionAngle(dwellAngle, channel5IgnDegrees, currentStatus.advance, &ignition5EndAngle, &ignition5StartAngle);
    calculateIgnitionAngle(dwellAngle, channel6IgnDegrees, currentStatus.advance, &ignition6EndAngle, &ignition6StartAngle);
  }
  else { allowHalfSyncForWastedSpark(); }
  #endif
}

/**
 * @brief Calculate ignition angles for 8-cylinder engines with sequential/wasted handling (PRIVATE)
 * @param dwellAngle Dwell time converted to crank degrees
 *
 * @details Two spark modes:
 *
 *          **Sequential Mode** (IGN_MODE_SEQUENTIAL + hasSync):
 *          - All 8 channels calculated independently
 *          - Requires 720° cycle (full sync)
 *          - Firing order: typically 1-8-4-3-6-5-7-2 (V8 cross-plane)
 *          - Optimal power and emissions
 *
 *          **Wasted Spark Mode** (default):
 *          - Only channels 1-4 calculated
 *          - Paired coils: 1+5, 2+6, 3+7, 4+8
 *          - Can use 360° half-sync
 *          - Reduces coil count from 8 to 4
 *          - Common on older V8 ECUs
 *
 * @note Sequential ignition is standard on modern V8s (2000+)
 * @note Wasted spark used on budget/retrofit applications
 * @complexity 3 (two modes + sync change)
 * @misra Compliant: 16 effective lines after refactoring
 */
static void calculate8CylinderAngles(uint16_t dwellAngle)
{
  // Always calculate first 4 channels
  calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
  calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
  calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
  calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);

  #if IGN_CHANNELS >= 8
  if((configPage4.sparkMode == IGN_MODE_SEQUENTIAL) && currentStatus.hasSync)
  {
    ensureFullSyncForSequential();
    calculateIgnitionAngle(dwellAngle, channel5IgnDegrees, currentStatus.advance, &ignition5EndAngle, &ignition5StartAngle);
    calculateIgnitionAngle(dwellAngle, channel6IgnDegrees, currentStatus.advance, &ignition6EndAngle, &ignition6StartAngle);
    calculateIgnitionAngle(dwellAngle, channel7IgnDegrees, currentStatus.advance, &ignition7EndAngle, &ignition7StartAngle);
    calculateIgnitionAngle(dwellAngle, channel8IgnDegrees, currentStatus.advance, &ignition8EndAngle, &ignition8StartAngle);
  }
  else { allowHalfSyncForWastedSpark(); }
  #endif
}

} // anonymous namespace

//=============================================================================
// IGNITION ANGLE CALCULATION - PUBLIC API
//=============================================================================

/**
 * @brief Calculate ignition start/end angles for all cylinders (PUBLIC)
 * @param dwellAngle Dwell time converted to crank angle degrees
 *
 * @details Main dispatcher that calculates ignition timing for all active
 *          cylinders based on engine configuration.
 *
 *          **Cylinder Count Handling**:
 *          - 1-cyl: Single channel, simple calculation
 *          - 2-cyl: Two independent channels (parallel twin, V-twin)
 *          - 3-cyl: Three independent channels (inline-3, typically 120° apart)
 *          - 4-cyl: Delegates to calculate4CylinderAngles() (complex modes)
 *          - 5-cyl: Five independent channels (inline-5, Audi/Volvo)
 *          - 6-cyl: Delegates to calculate6CylinderAngles() (I6/V6)
 *          - 8-cyl: Delegates to calculate8CylinderAngles() (V8)
 *
 *          **Global State Modified**:
 *          - ignition*StartAngle: Coil charge start angle for each channel
 *          - ignition*EndAngle: Spark fire angle for each channel
 *
 *          These angles are used by the scheduler to set hardware timers.
 *
 * @note dwellAngle = (dwell µs) × (360° / revolution time µs)
 * @note Simple configs (1/2/3/5-cyl) calculated inline for efficiency
 * @note Complex configs (4/6/8-cyl) delegated to helpers (mode-dependent)
 *
 * @example 4-cylinder @ 6000 RPM, 30° BTDC, 4ms dwell:
 *          revolutionTime = 10000µs
 *          dwellAngle = 4000µs × (360° / 10000µs) = 144°
 *          For cylinder 1 (TDC at 0°):
 *            ignition1EndAngle = 330° (30° BTDC)
 *            ignition1StartAngle = 186° (330° - 144°)
 *
 * @complexity 2 (simple switch dispatch to helpers)
 * @misra Compliant: 31 effective lines after refactoring (was 75)
 */
void calculateIgnitionAngles(uint16_t dwellAngle)
{
  // Calculate start and end angles for all cylinders based on cylinder count
  switch(configPage2.nCylinders)
  {
    case 1:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      break;

    case 2:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      break;

    case 3:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
      break;

    case 4:
      calculate4CylinderAngles(dwellAngle);
      break;

    case 5:
      calculateIgnitionAngle(dwellAngle, channel1IgnDegrees, currentStatus.advance, &ignition1EndAngle, &ignition1StartAngle);
      calculateIgnitionAngle(dwellAngle, channel2IgnDegrees, currentStatus.advance, &ignition2EndAngle, &ignition2StartAngle);
      calculateIgnitionAngle(dwellAngle, channel3IgnDegrees, currentStatus.advance, &ignition3EndAngle, &ignition3StartAngle);
      calculateIgnitionAngle(dwellAngle, channel4IgnDegrees, currentStatus.advance, &ignition4EndAngle, &ignition4StartAngle);
      #if (IGN_CHANNELS >= 5)
      calculateIgnitionAngle(dwellAngle, channel5IgnDegrees, currentStatus.advance, &ignition5EndAngle, &ignition5StartAngle);
      #endif
      break;

    case 6:
      calculate6CylinderAngles(dwellAngle);
      break;

    case 8:
      calculate8CylinderAngles(dwellAngle);
      break;

    default:
      // Do nothing for unsupported cylinder counts
      break;
  }
}
