/**
 * @file fuel_calculations.cpp
 * @brief Fuel calculation functions implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * MISRA C:2012 Compliance:
 * - Input validation on all parameters
 * - Overflow protection in arithmetic operations
 * - Uses automotive_constants.h for all magic numbers
 */

#include "fuel_calculations.h"
#include "corrections.h"
#include "maths.h"
#include "globals.h"
#include "speeduino.h"
#include "modularization_globals.h"
#include "automotive_constants.h"

// External references to table objects
extern table3d16RpmLoad fuelTable;
extern table3d8RpmLoad stagingTable;

//=============================================================================
// Pulsewidth Calculation
//=============================================================================

/**
 * @brief Calculate base fuel injector pulse width from VE, MAP, AFR and corrections
 *
 * This is the core fuel calculation that determines how long to open the injector.
 * Uses fixed-point arithmetic (UQ1.7 format) to avoid floating point operations.
 *
 * @param REQ_FUEL Base fuel requirement in microseconds (µs)
 * @param VE Volumetric Efficiency from lookup table (0-255%)
 * @param MAP Manifold Absolute Pressure in kPa (0-400)
 * @param corrections Total fuel corrections as percentage (100 = no correction)
 * @param injOpen Injector opening time in microseconds (µs)
 *
 * @return Calculated pulse width in microseconds (0-65535 µs)
 *
 * Calculation sequence:
 * 1. Input validation (REQ_FUEL, MAP, injOpen ranges)
 * 2. Convert VE to fixed-point UQ1.7 format
 * 3. Apply MAP correction if enabled (100% or BARO-referenced)
 * 4. Apply AFR correction if wideband O2 active
 * 5. Apply total corrections with overflow protection
 * 6. Add injector opening time
 * 7. Add acceleration enrichment if active
 *
 * @note Returns 0 if REQ_FUEL is invalid (safety)
 * @note Clamps final result to UINT16_MAX (65535 µs = 65.5ms max)
 * @note Uses adaptive bitshift for corrections to prevent overflow
 *
 * @see MULTIPLY_MAP_MODE_100 Apply MAP as percentage (0-400%)
 * @see MULTIPLY_MAP_MODE_BARO Apply MAP normalized to barometric pressure
 * @see AE_MODE_ADDER Acceleration enrichment adds % of REQ_FUEL
 *
 * @example
 * @code
 * // REQ_FUEL=10000µs, VE=80%, MAP=100kPa, corrections=120%, injOpen=500µs
 * // Result ≈ (10000 × 0.80 × 1.20) + 500 = 10100µs
 * uint16_t pw = PW(10000, 80, 100, 120, 500);
 * @endcode
 *
 * @complexity 9
 * @misra Compliant (44 effective lines, complexity 9)
 */
uint16_t PW(int REQ_FUEL, byte VE, long MAP, uint16_t corrections, int injOpen)
{
  // Input validation - critical for safety
  if(REQ_FUEL < 0 || REQ_FUEL > MAX_REQ_FUEL_US) {
    return 0; // Invalid input, return 0 pulse width
  }
  if(MAP < 0 || MAP > MAX_MAP_VALUE) {
    MAP = CLAMP(MAP, 0, MAX_MAP_VALUE);
  }
  if(injOpen < 0) {
    injOpen = 0;
  }

  // Standard float version of the calculation (commented out for performance):
  // return (REQ_FUEL * (float)(VE/100.0) * (float)(MAP/100.0) * (float)(TPS/100.0) * (float)(corrections/100.0) + injOpen);
  // Note: The MAP and TPS portions are currently disabled, we use VE and corrections only

  uint16_t iVE;
  uint16_t iMAP = PERCENTAGE_BASE;
  uint16_t iAFR = AFR_STOICH_DEFAULT;

  // 100% float free version, does sacrifice a little bit of accuracy, but not much.
  // iVE = ((unsigned int)VE << 7) / 100;
  iVE = div100(((uint16_t)VE << 7U));

  // Check whether either of the multiply MAP modes is turned on
  if(configPage2.multiplyMAP == MULTIPLY_MAP_MODE_100) {
    iMAP = div100(((uint16_t)MAP << 7U));
  }
  else if(configPage2.multiplyMAP == MULTIPLY_MAP_MODE_BARO) {
    iMAP = ((unsigned int)MAP << 7U) / currentStatus.baro;
  }

  // Include AFR (vs target) if enabled and wideband O2 sensor is active
  if((configPage2.includeAFR == true) && (configPage6.egoType == EGO_TYPE_WIDE) && (currentStatus.runSecs > configPage6.ego_sdelay)) {
    iAFR = ((unsigned int)currentStatus.O2 << 7U) / currentStatus.afrTarget;
  }

  // Incorporate stoich vs target AFR, if enabled
  if((configPage2.incorporateAFR == true) && (configPage2.includeAFR == false)) {
    iAFR = ((unsigned int)configPage2.stoich << 7U) / currentStatus.afrTarget;
  }

  // CRITICAL: Overflow protection for multiplication
  // Need to use an intermediate value to avoid overflowing the long
  uint32_t intermediate;

  // Check for potential overflow before multiplication
  if((uint32_t)REQ_FUEL > (UINT32_MAX / (uint32_t)iVE)) {
    // Overflow would occur, clamp to max safe value
    intermediate = UINT32_MAX >> 7U;
  } else {
    intermediate = rshift<7U>((uint32_t)REQ_FUEL * (uint32_t)iVE);
  }

  // Apply MAP correction if enabled
  if(configPage2.multiplyMAP > 0) {
    intermediate = rshift<7U>(intermediate * (uint32_t)iMAP);
  }

  // Apply AFR correction if wideband is active
  if((configPage2.includeAFR == true) && (configPage6.egoType == EGO_TYPE_WIDE) && (currentStatus.runSecs > configPage6.ego_sdelay)) {
    // EGO type must be set to wideband and the AFR warmup time must've elapsed
    intermediate = rshift<7U>(intermediate * (uint32_t)iAFR);
  }

  // Apply AFR correction if incorporateAFR is enabled
  if((configPage2.incorporateAFR == true) && (configPage2.includeAFR == false)) {
    intermediate = rshift<7U>(intermediate * (uint32_t)iAFR);
  }

  // If corrections are huge, use less bitshift to avoid overflow
  // Sacrifices a bit more accuracy (basically only during very cold temp cranking)
  if(corrections < CORRECTION_THRESHOLD_1) {
    intermediate = rshift<CORRECTION_SHIFT_NORMAL>(intermediate * div100(lshift<CORRECTION_SHIFT_NORMAL>(corrections)));
  }
  else if(corrections < CORRECTION_THRESHOLD_2) {
    intermediate = rshift<CORRECTION_SHIFT_MEDIUM>(intermediate * div100(lshift<CORRECTION_SHIFT_MEDIUM>(corrections)));
  }
  else {
    intermediate = rshift<CORRECTION_SHIFT_LARGE>(intermediate * div100(lshift<CORRECTION_SHIFT_LARGE>(corrections)));
  }

  if(intermediate != 0)
  {
    // If intermediate is not 0, we need to add the opening time
    // 0 typically indicates that one of the full fuel cuts is active
    intermediate += injOpen; // Add the injector opening time

    // AE calculation only when ACC is active and in adder mode
    bool accActive = BIT_CHECK(currentStatus.engine, BIT_ENGINE_ACC);
    bool aeAdderMode = (configPage2.aeApplyMode == AE_MODE_ADDER);
    if (accActive && aeAdderMode) {
      intermediate += div100(((uint32_t)REQ_FUEL) * (currentStatus.AEamount - AE_PERCENTAGE_OFFSET));
    }

    // Make sure this won't overflow when we convert to uint16_t
    // This means the maximum pulsewidth possible is 65.535ms
    if(intermediate > UINT16_MAX)
    {
      intermediate = UINT16_MAX;
    }
  }

  return (unsigned int)(intermediate);
}

//=============================================================================
// VE Lookup
//=============================================================================

/**
 * @brief Retrieve Volumetric Efficiency from primary fuel table
 *
 * Calculates fuel load using configured algorithm (Speed-Density, Alpha-N, or MAF)
 * then performs 3D table lookup using load and RPM as axes.
 *
 * @return VE value from fuel table (0-255%)
 *
 * @note Updates currentStatus.fuelLoad as side effect
 * @note Load calculation varies by algorithm:
 *       - Speed-Density: Uses MAP
 *       - Alpha-N: Uses TPS
 *       - MAF: Uses mass airflow sensor
 *
 * @see fuelTable Primary 3D fuel map (VE values)
 * @see configPage2.fuelAlgorithm Determines load calculation method
 *
 * @complexity 1
 * @misra Compliant (3 effective lines, complexity 1)
 */
uint8_t getVE1(void)
{
  // Calculate the load value based on the selected fuel algorithm
  currentStatus.fuelLoad = getLoad(configPage2.fuelAlgorithm, currentStatus);

  // Perform lookup into fuel map for RPM vs Load value
  return get3DTableValue(&fuelTable, currentStatus.fuelLoad, currentStatus.RPM);
}

//=============================================================================
// Pulsewidth Limit Calculation
//=============================================================================

/**
 * @brief Calculate maximum allowed injector pulse width based on duty cycle limit
 *
 * Determines the maximum pulse width an injector can use before exceeding
 * the configured duty cycle limit (typically 85%).
 *
 * @return Maximum pulse width in microseconds (µs), capped at 65535 µs
 *
 * Calculation accounts for:
 * - Engine stroke cycle (2-stroke vs 4-stroke)
 * - Number of injection squirts per revolution
 * - Total revolution time
 *
 * Formula:
 * @code
 * limit = (revolutionTime × dutyLim%) × strokeMultiplier ÷ nSquirts
 * @endcode
 *
 * @note 4-stroke engines get 2× limit (injection every other revolution)
 * @note Optimized division for power-of-2 squirts (1, 2, 4, 8)
 * @note Result clamped to UINT16_MAX if overflow would occur
 *
 * @see configPage2.dutyLim Duty cycle limit percentage (default 85%)
 * @see revolutionTime Time for one full crank revolution (µs)
 * @see currentStatus.nSquirts Number of injection events per revolution
 *
 * @example
 * @code
 * // 4-stroke, 3000 RPM (20ms/rev), 85% duty, 2 squirts
 * // limit = (20000 × 0.85) × 2 ÷ 2 = 17000 µs
 * uint16_t limit = calculatePWLimit();
 * @endcode
 *
 * @complexity 2
 * @misra Compliant (20 effective lines, complexity 2)
 */
uint16_t calculatePWLimit(void)
{
  // The pulsewidth limit is determined to be the duty cycle limit (Eg 85%)
  // by the total time it takes to perform 1 revolution
  uint32_t tempLimit = percentage(configPage2.dutyLim, revolutionTime);

  // Handle multiple squirts per rev
  if(configPage2.strokes == FOUR_STROKE) { tempLimit = tempLimit * 2; }

  // Optimize for power of two divisions where possible
  switch(currentStatus.nSquirts)
  {
    case 1:
      // No action needed
      break;
    case 2:
      tempLimit = tempLimit / 2;
      break;
    case 4:
      tempLimit = tempLimit / 4;
      break;
    case 8:
      tempLimit = tempLimit / 8;
      break;
    default:
      // Non-PoT squirts value. Perform (slow) uint32_t division
      tempLimit = tempLimit / currentStatus.nSquirts;
      break;
  }

  if(tempLimit > UINT16_MAX) { tempLimit = UINT16_MAX; }

  return tempLimit;
}

//=============================================================================
// Staging Calculation
//=============================================================================

namespace {

// Helper: Apply TABLE mode staging calculation
static inline void applyStagingTableMode(uint32_t tempPW1) {
  uint32_t tempPW3 = div100((uint32_t)currentStatus.PW1 * staged_req_fuel_mult_sec);
  uint8_t stagingSplit = get3DTableValue(&stagingTable, currentStatus.fuelLoad, currentStatus.RPM);
  currentStatus.PW1 = div100((100U - stagingSplit) * tempPW1);
  currentStatus.PW1 += inj_opentime_uS;

  // PW2 holds secondary pulsewidth, assigned to correct channel later
  if (stagingSplit > 0) { BIT_SET(currentStatus.status4, BIT_STATUS4_STAGING_ACTIVE); }
  else { BIT_CLEAR(currentStatus.status4, BIT_STATUS4_STAGING_ACTIVE); currentStatus.PW2 = 0; return; }

  currentStatus.PW2 = div100(stagingSplit * tempPW3);
  currentStatus.PW2 += inj_opentime_uS;
}

// Helper: Apply AUTO mode staging calculation
static inline void applyStagingAutoMode(uint32_t tempPW1, uint32_t pwLimit) {
  currentStatus.PW1 = tempPW1;

  // Primary injectors used up to limit, excess goes to secondaries
  if (tempPW1 <= pwLimit) {
    // Entire fuel load handled by primaries, staging inactive
    currentStatus.PW1 += inj_opentime_uS;
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_STAGING_ACTIVE);
    currentStatus.PW2 = 0;
    return;
  }

  // Staging active: calculate secondary PW
  BIT_SET(currentStatus.status4, BIT_STATUS4_STAGING_ACTIVE);
  uint32_t extraPW = tempPW1 - pwLimit + inj_opentime_uS;
  currentStatus.PW1 = pwLimit;
  currentStatus.PW2 = udiv_32_16(extraPW * staged_req_fuel_mult_sec, staged_req_fuel_mult_pri);
  currentStatus.PW2 += inj_opentime_uS;
}

/**
 * @brief Calculate primary and secondary pulse widths based on staging mode
 *
 * @param pwLimit Maximum allowed primary injector pulse width (µs)
 *
 * Handles two staging modes:
 * - TABLE mode: Uses lookup table to split fuel between primary/secondary
 * - AUTO mode: Primary injectors used to limit, overflow goes to secondary
 *
 * Results stored in currentStatus.PW1 (primary) and currentStatus.PW2 (secondary)
 *
 * @note PW1 has opening time removed on entry, re-added before exit
 * @complexity 3
 * @misra Compliant (10 effective lines, complexity 3)
 */
static void calculateStagingModePulsewidths(uint32_t pwLimit) {
  // Subtract opening time - needs to be multiplied out again by pri/sec req_fuel
  currentStatus.PW1 -= inj_opentime_uS;
  uint32_t tempPW1 = div100((uint32_t)currentStatus.PW1 * staged_req_fuel_mult_pri);

  if (configPage10.stagingMode == STAGING_MODE_TABLE) { applyStagingTableMode(tempPW1); }
  else if (configPage10.stagingMode == STAGING_MODE_AUTO) { applyStagingAutoMode(tempPW1, pwLimit); }
}

// Helper: 4-cylinder staging allocation
static inline void allocateStaging4Cyl(void) {
  bool isSeqOrSemiSeq = (configPage2.injLayout == INJ_SEQUENTIAL) || (configPage2.injLayout == INJ_SEMISEQUENTIAL);
  if (!isSeqOrSemiSeq) {
    currentStatus.PW3 = currentStatus.PW2;
    currentStatus.PW4 = currentStatus.PW2;
    currentStatus.PW2 = currentStatus.PW1;
    return;
  }
  // Sequential/semi-sequential requires 8 channels
  #if INJ_CHANNELS >= 8
    currentStatus.PW5 = currentStatus.PW2;
    currentStatus.PW6 = currentStatus.PW2;
    currentStatus.PW7 = currentStatus.PW2;
    currentStatus.PW8 = currentStatus.PW2;
    currentStatus.PW2 = currentStatus.PW1;
    currentStatus.PW3 = currentStatus.PW1;
    currentStatus.PW4 = currentStatus.PW1;
  #else
    currentStatus.PW5 = currentStatus.PW2; // Invalid config fallback
  #endif
}

// Helper: 5-cylinder staging allocation
static inline void allocateStaging5Cyl(void) {
  #if INJ_CHANNELS >= 5
    if (configPage2.injLayout != INJ_SEQUENTIAL) { currentStatus.PW5 = currentStatus.PW2; }
    #if INJ_CHANNELS >= 6
      currentStatus.PW6 = currentStatus.PW2;
    #endif
  #endif
  currentStatus.PW2 = currentStatus.PW1;
  currentStatus.PW3 = currentStatus.PW1;
  currentStatus.PW4 = currentStatus.PW1;
}

// Helper: 6-cylinder staging allocation
static inline void allocateStaging6Cyl(void) {
  #if INJ_CHANNELS >= 6
    bool isSequential = (configPage2.injLayout == INJ_SEQUENTIAL);
    if (!isSequential) {
      currentStatus.PW4 = currentStatus.PW2;
      currentStatus.PW5 = currentStatus.PW2;
      currentStatus.PW6 = currentStatus.PW2;
    }
    #if INJ_CHANNELS >= 8
    else {
      // 6-cyl sequential uses CH7+8 for staging
      currentStatus.PW7 = currentStatus.PW2;
      currentStatus.PW8 = currentStatus.PW2;
      currentStatus.PW4 = currentStatus.PW1;
      currentStatus.PW5 = currentStatus.PW1;
      currentStatus.PW6 = currentStatus.PW1;
    }
    #endif
  #endif
  currentStatus.PW2 = currentStatus.PW1;
  currentStatus.PW3 = currentStatus.PW1;
}

// Helper: 8-cylinder staging allocation
static inline void allocateStaging8Cyl(void) {
  #if INJ_CHANNELS >= 8
    if (configPage2.injLayout != INJ_SEQUENTIAL) {
      currentStatus.PW5 = currentStatus.PW2;
      currentStatus.PW6 = currentStatus.PW2;
      currentStatus.PW7 = currentStatus.PW2;
      currentStatus.PW8 = currentStatus.PW2;
    }
  #endif
  currentStatus.PW2 = currentStatus.PW1;
  currentStatus.PW3 = currentStatus.PW1;
  currentStatus.PW4 = currentStatus.PW1;
}

/**
 * @brief Allocate primary and secondary pulse widths to injector channels based on cylinder count
 *
 * Distributes the calculated PW1 (primary) and PW2 (secondary) values across
 * available injector channels according to engine configuration.
 *
 * Channel allocation depends on:
 * - Number of cylinders (1-8)
 * - Injection layout (sequential, semi-sequential, paired)
 * - Available hardware channels (INJ_CHANNELS)
 *
 * @note For sequential 4-cyl staging, requires 8 channels (4 pri + 4 sec)
 * @note For 6-cyl staging, sequential mode requires 8 channels (uses CH7+8 for staging)
 * @complexity 2
 * @misra Compliant (20 effective lines, complexity 2)
 */
static void allocateStagingPulsewidths() {
  switch(configPage2.nCylinders)
  {
    case 1:
      break; // Nothing required for 1 cylinder

    case 2:
      currentStatus.PW3 = currentStatus.PW2;
      currentStatus.PW4 = currentStatus.PW2;
      currentStatus.PW2 = currentStatus.PW1;
      break;

    case 3:
      #if INJ_CHANNELS >= 6
        currentStatus.PW4 = currentStatus.PW2;
        currentStatus.PW5 = currentStatus.PW2;
        currentStatus.PW6 = currentStatus.PW2;
      #else
        currentStatus.PW4 = currentStatus.PW2;
      #endif
      currentStatus.PW2 = currentStatus.PW1;
      currentStatus.PW3 = currentStatus.PW1;
      break;

    case 4:
      allocateStaging4Cyl();
      break;

    case 5:
      allocateStaging5Cyl();
      break;

    case 6:
      allocateStaging6Cyl();
      break;

    case 8:
      allocateStaging8Cyl();
      break;

    default:
      currentStatus.PW3 = currentStatus.PW2;
      currentStatus.PW4 = currentStatus.PW2;
      currentStatus.PW2 = currentStatus.PW1;
      break;
  }
}

/**
 * @brief Disable staged injection and set all channels to primary pulse width
 *
 * When staging is inactive, all injector channels receive the same pulse width (PW1).
 * Channels beyond hardware capacity are set to 0.
 *
 * @note Clears BIT_STATUS4_STAGING_ACTIVE flag in currentStatus.status4
 * @complexity 1
 * @misra Compliant (8 effective lines, complexity 1)
 */
static void disableStagingOutputs() {
  // Staging not active - copy PW1 to all channels (or set to 0 if channel doesn't exist)
  if(maxInjOutputs >= 2) { currentStatus.PW2 = currentStatus.PW1; }
  else { currentStatus.PW2 = 0; }

  if(maxInjOutputs >= 3) { currentStatus.PW3 = currentStatus.PW1; }
  else { currentStatus.PW3 = 0; }

  if(maxInjOutputs >= 4) { currentStatus.PW4 = currentStatus.PW1; }
  else { currentStatus.PW4 = 0; }

  if(maxInjOutputs >= 5) { currentStatus.PW5 = currentStatus.PW1; }
  else { currentStatus.PW5 = 0; }

  if(maxInjOutputs >= 6) { currentStatus.PW6 = currentStatus.PW1; }
  else { currentStatus.PW6 = 0; }

  if(maxInjOutputs >= 7) { currentStatus.PW7 = currentStatus.PW1; }
  else { currentStatus.PW7 = 0; }

  if(maxInjOutputs >= 8) { currentStatus.PW8 = currentStatus.PW1; }
  else { currentStatus.PW8 = 0; }

  BIT_CLEAR(currentStatus.status4, BIT_STATUS4_STAGING_ACTIVE); // Clear the staging active flag
}

} // anonymous namespace

/**
 * @brief Calculate staged injection pulse widths for primary and secondary injectors
 *
 * Staged injection uses two sets of injectors (primary + secondary) to extend
 * the dynamic range of fuel delivery. This function:
 * 1. Calculates primary/secondary split based on mode (table or auto)
 * 2. Distributes pulse widths across injector channels per cylinder count
 * 3. Falls back to single-injector mode if staging is disabled
 *
 * @param pwLimit Maximum allowed primary injector pulse width (µs)
 *
 * Staging is enabled when:
 * - configPage10.stagingEnabled == true
 * - nCylinders ≤ INJ_CHANNELS (enough outputs for pri+sec)
 * - currentStatus.PW1 > inj_opentime_uS (not in DFCO)
 *
 * @note Modifies currentStatus.PW1 through PW8
 * @note Sets/clears BIT_STATUS4_STAGING_ACTIVE flag
 *
 * @see STAGING_MODE_TABLE Uses lookup table to split fuel
 * @see STAGING_MODE_AUTO Primary to limit, overflow to secondary
 *
 * @example
 * @code
 * // Auto mode: 10ms requested, 8ms limit
 * // Result: PW1=8ms (primary), PW2=2ms scaled (secondary)
 * calculateStaging(8000);
 * @endcode
 *
 * @complexity 3
 * @misra Compliant after refactoring (15 effective lines, complexity 3)
 */
void calculateStaging(uint32_t pwLimit)
{
  // Calculate staging pulsewidths if used
  // To run staged injection, the number of cylinders must be less than or equal to the injector channels
  // (ie Assuming you're running paired injection, you need at least as many injector channels as you have cylinders,
  // half for the primaries and half for the secondaries)
  if((configPage10.stagingEnabled == true) && (configPage2.nCylinders <= INJ_CHANNELS || configPage2.injType == INJ_TYPE_TBODY) && (currentStatus.PW1 > inj_opentime_uS)) // Final check is to ensure that DFCO isn't active, which would cause an overflow below (See #267)
  {
    calculateStagingModePulsewidths(pwLimit);
    allocateStagingPulsewidths();
  }
  else
  {
    disableStagingOutputs();
  }
}
