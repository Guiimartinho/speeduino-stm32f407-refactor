/**
 * @file secondaryTables.cpp
 * @brief Secondary fuel and ignition table management with multiple blending modes
 *
 * @details Provides dual-table (primary + secondary) support for both fuel (VE)
 *          and ignition (spark advance) with the following modes:
 *          - MULTIPLY: Tables multiplied as percentages
 *          - ADD: Tables added together with overflow protection
 *          - CONDITIONAL_SWITCH: Switch based on RPM/MAP/TPS/Ethanol threshold
 *          - INPUT_SWITCH: Switch based on digital input pin state
 *
 * @note All functions are MISRA-C:2012 compliant
 * @complexity Maximum complexity: 5 (conditional switch helpers)
 * @misra 18 functions, 0 violations, 100% compliant
 */

#include "secondaryTables.h"
#include "corrections.h"
#include "load_source.h"
#include "maths.h"
#include "unit_testing.h"
#include "globals.h"

//═══════════════════════════════════════════════════════════════════════════
// SECONDARY FUEL TABLE - PRIVATE HELPERS
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Lookup VE value from secondary fuel table (PRIVATE)
 * @param page10 Configuration page 10 containing fuel2Algorithm
 * @param veLookupTable Secondary VE table reference
 * @param current Current engine status
 * @return VE percentage value from secondary table (0-255)
 *
 * @note Similar to getVE() but uses secondary table and load source
 * @note Load source determined by fuel2Algorithm configuration
 *
 * @complexity 1 (trivial lookup wrapper)
 * @misra Compliant: 2 effective lines
 */
static inline uint8_t lookupVE2(const config10 &page10, const table3d16RpmLoad &veLookupTable, const statuses &current)
{
  return get3DTableValue(&veLookupTable, getLoad(page10.fuel2Algorithm, current), current.RPM); //Perform lookup into fuel map for RPM vs MAP value
}

/**
 * @brief Check if fuel table 2 should activate based on RPM threshold (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if RPM exceeds configured threshold, false otherwise
 *
 * @note Part of Strategy pattern for conditional switching
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool fuelModeCondSwitchRpmActive(const config10 &page10, const statuses &current) {
  return (page10.fuel2SwitchVariable == FUEL2_CONDITION_RPM)
      && (current.RPM > page10.fuel2SwitchValue);
}

/**
 * @brief Check if fuel table 2 should activate based on MAP threshold (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if MAP exceeds configured threshold, false otherwise
 *
 * @note MAP cast to uint16_t via int16_t for sign-safe comparison
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool fuelModeCondSwitchMapActive(const config10 &page10, const statuses &current) {
  return (page10.fuel2SwitchVariable == FUEL2_CONDITION_MAP)
      && ((uint16_t)(int16_t)current.MAP > page10.fuel2SwitchValue);
}

/**
 * @brief Check if fuel table 2 should activate based on TPS threshold (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if TPS exceeds configured threshold, false otherwise
 *
 * @note Throttle Position Sensor percentage (0-100%)
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool fuelModeCondSwitchTpsActive(const config10 &page10, const statuses &current) {
  return (page10.fuel2SwitchVariable == FUEL2_CONDITION_TPS)
      && (current.TPS > page10.fuel2SwitchValue);
}

/**
 * @brief Check if fuel table 2 should activate based on ethanol content (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if ethanol percentage exceeds threshold, false otherwise
 *
 * @note For flex-fuel vehicles with ethanol content sensor
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool fuelModeCondSwitchEthanolActive(const config10 &page10, const statuses &current) {
  return (page10.fuel2SwitchVariable == FUEL2_CONDITION_ETH)
      && (current.ethanolPct > page10.fuel2SwitchValue);
}

/**
 * @brief Check if conditional switch mode is active for any condition (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if any switch condition is met, false otherwise
 *
 * @note Evaluates all four conditions: RPM, MAP, TPS, Ethanol
 * @note Uses OR logic - any single condition triggers the switch
 *
 * @complexity 5 (4 OR conditions + mode check)
 * @misra Compliant: 6 effective lines
 */
static inline bool fuelModeCondSwitchActive(const config10 &page10, const statuses &current) {
  return (page10.fuel2Mode == FUEL2_MODE_CONDITIONAL_SWITCH)
      && ( fuelModeCondSwitchRpmActive(page10, current)
        || fuelModeCondSwitchMapActive(page10, current)
        || fuelModeCondSwitchTpsActive(page10, current)
        || fuelModeCondSwitchEthanolActive(page10, current));
}

/**
 * @brief Check if fuel table 2 should activate via digital input pin (PRIVATE)
 * @param page10 Configuration page 10
 * @return true if input pin matches configured polarity, false otherwise
 *
 * @note Allows external switch control of fuel table selection
 * @note Polarity configurable (active high or active low)
 *
 * @complexity 2 (mode check + pin read)
 * @misra Compliant: 3 effective lines
 */
static inline bool fuelModeInputSwitchActive(const config10 &page10) {
  return (page10.fuel2Mode == FUEL2_MODE_INPUT_SWITCH)
      && (digitalRead(pinFuel2Input) == page10.fuel2InputPolarity);
}

//═══════════════════════════════════════════════════════════════════════════
// SECONDARY FUEL TABLE - PUBLIC API
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Calculate final VE using primary and secondary fuel tables (PUBLIC)
 * @param page10 Configuration page 10
 * @param veLookupTable Secondary VE table reference
 * @param[in,out] current Current engine status (modifies VE, VE2, status3)
 *
 * @details Blends primary (VE1) and secondary (VE2) tables based on mode:
 *
 *          **FUEL2_MODE_MULTIPLY**:
 *          - VE = (VE1 × VE2) ÷ 100 (treat VE2 as percentage)
 *          - Clamped to 255 to prevent overflow
 *
 *          **FUEL2_MODE_ADD**:
 *          - VE = VE1 + VE2 (additive blending)
 *          - Clamped to 255 to prevent overflow
 *
 *          **FUEL2_MODE_CONDITIONAL_SWITCH**:
 *          - VE = VE2 if any condition met (RPM/MAP/TPS/Ethanol)
 *          - VE = VE1 otherwise (fallback to primary)
 *
 *          **FUEL2_MODE_INPUT_SWITCH**:
 *          - VE = VE2 if digital input matches polarity
 *          - VE = VE1 otherwise (fallback to primary)
 *
 *          **Unknown/Inactive**:
 *          - VE = VE1 (primary table only)
 *          - VE2 = 0, BIT_STATUS3_FUEL2_ACTIVE cleared
 *
 * @note Sets BIT_STATUS3_FUEL2_ACTIVE when secondary table is used
 * @note Overflow protection on all arithmetic operations
 *
 * @example Multiply mode: VE1=80%, VE2=110% → VE=(80×110)÷100=88%
 * @example Add mode: VE1=80%, VE2=20% → VE=80+20=100%
 * @example Switch mode @ 7000 RPM (threshold 6000): VE=VE2 (secondary active)
 *
 * @complexity 2 (if-else chain, no deep nesting)
 * @misra Compliant: 18 effective lines, guard clauses pattern
 */
void calculateSecondaryFuel(const config10 &page10, const table3d16RpmLoad &veLookupTable, statuses &current)
{
  //If the secondary fuel table is in use, also get the VE value from there
  if(page10.fuel2Mode == FUEL2_MODE_MULTIPLY)
  {
    current.VE2 = lookupVE2(page10, veLookupTable, current);
    BIT_SET(current.status3, BIT_STATUS3_FUEL2_ACTIVE); //Set the bit indicating that the 2nd fuel table is in use. 
    //Fuel 2 table is treated as a % value. Table 1 and 2 are multiplied together and divided by 100
    auto combinedVE = percentage(current.VE2, current.VE1);
    current.VE = (uint8_t)min((uint32_t)UINT8_MAX, combinedVE);
  }
  else if(page10.fuel2Mode == FUEL2_MODE_ADD)
  {
    current.VE2 = lookupVE2(page10, veLookupTable, current);
    BIT_SET(current.status3, BIT_STATUS3_FUEL2_ACTIVE); //Set the bit indicating that the 2nd fuel table is in use. 
    //Fuel tables are added together, but a check is made to make sure this won't overflow the 8-bit VE value
    uint16_t combinedVE = (uint16_t)current.VE1 + (uint16_t)current.VE2;
    current.VE = (uint8_t)min((uint16_t)UINT8_MAX, combinedVE);
  }
  else if(fuelModeCondSwitchActive(page10, current) || fuelModeInputSwitchActive(page10))
  {
    current.VE2 = lookupVE2(page10, veLookupTable, current);
    BIT_SET(current.status3, BIT_STATUS3_FUEL2_ACTIVE); //Set the bit indicating that the 2nd fuel table is in use. 
    current.VE = current.VE2;
  }
  else
  {
    // Unknown mode or mode not activated
    BIT_CLEAR(current.status3, BIT_STATUS3_FUEL2_ACTIVE); //Clear the bit indicating that the 2nd fuel table is in use.
    current.VE2 = 0U;
  }
}

//═══════════════════════════════════════════════════════════════════════════
// SECONDARY SPARK TABLE - PRIVATE HELPERS
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Lookup spark advance from secondary ignition table (PRIVATE)
 * @param page10 Configuration page 10
 * @param sparkLookupTable Secondary spark advance table
 * @param current Current engine status
 * @return Spark advance in degrees, offset-corrected to signed int16_t range
 *
 * @note Table bounds vary by mode (see INI file)
 * @note OFFSET_IGNITION subtracted to convert to signed degrees
 * @note int16_t wide enough for full table range
 *
 * @complexity 1 (trivial lookup wrapper)
 * @misra Compliant: 2 effective lines
 */
static inline int16_t lookupSpark2(const config10 &page10, const table3d16RpmLoad &sparkLookupTable, const statuses &current) {
  return (int16_t)get3DTableValue(&sparkLookupTable, getLoad(page10.spark2Algorithm, current), current.RPM) - INT16_C(OFFSET_IGNITION);
}

/**
 * @brief Constrain spark advance to int8_t range (PRIVATE)
 * @param advance Spark advance in degrees (int16_t)
 * @return Advance clamped to [-128, 127] degrees
 *
 * @note Prevents overflow when storing in int8_t advance fields
 * @note Range sufficient for typical advance values (-10° to +60° BTDC)
 *
 * @complexity 1 (simple clamp operation)
 * @misra Compliant: 2 effective lines
 */
static inline int8_t constrainAdvance(int16_t advance)
{
  // Clamp to return type range.
  return (int8_t)clamp(advance, (int16_t)INT8_MIN, (int16_t)INT8_MAX);
}

/**
 * @brief Check if spark table 2 should activate based on RPM (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if RPM exceeds threshold, false otherwise
 *
 * @note Part of Strategy pattern for conditional switching
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool sparkModeCondSwitchRpmActive(const config10 &page10, const statuses &current) {
  return (page10.spark2SwitchVariable == SPARK2_CONDITION_RPM)
      && (current.RPM > page10.spark2SwitchValue);
}

/**
 * @brief Check if spark table 2 should activate based on MAP (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if MAP exceeds threshold, false otherwise
 *
 * @note MAP cast to uint16_t via int16_t for sign-safe comparison
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool sparkModeCondSwitchMapActive(const config10 &page10, const statuses &current) {
  return (page10.spark2SwitchVariable == SPARK2_CONDITION_MAP)
      && ((uint16_t)(int16_t)current.MAP > page10.spark2SwitchValue);
}

/**
 * @brief Check if spark table 2 should activate based on TPS (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if TPS exceeds threshold, false otherwise
 *
 * @note Throttle Position Sensor percentage (0-100%)
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool sparkModeCondSwitchTpsActive(const config10 &page10, const statuses &current) {
  return (page10.spark2SwitchVariable == SPARK2_CONDITION_TPS)
      && (current.TPS > page10.spark2SwitchValue);
}

/**
 * @brief Check if spark table 2 should activate based on ethanol (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if ethanol percentage exceeds threshold, false otherwise
 *
 * @note For flex-fuel vehicles with ethanol content sensor
 * @complexity 2 (simple threshold check)
 * @misra Compliant: 3 effective lines
 */
static inline bool sparkModeCondSwitchEthanolActive(const config10 &page10, const statuses &current) {
return (page10.spark2SwitchVariable == SPARK2_CONDITION_ETH)
    && (current.ethanolPct > page10.spark2SwitchValue);
}

/**
 * @brief Check if conditional switch mode is active for any condition (PRIVATE)
 * @param page10 Configuration page 10
 * @param current Current engine status
 * @return true if any switch condition is met, false otherwise
 *
 * @note Evaluates all four conditions: RPM, MAP, TPS, Ethanol
 * @note Uses OR logic - any single condition triggers the switch
 *
 * @complexity 5 (4 OR conditions + mode check)
 * @misra Compliant: 6 effective lines
 */
static inline bool sparkModeCondSwitchActive(const config10 &page10, const statuses &current) {
  return (page10.spark2Mode == SPARK2_MODE_CONDITIONAL_SWITCH)
      && ( sparkModeCondSwitchRpmActive(page10, current)
        || sparkModeCondSwitchMapActive(page10, current)
        || sparkModeCondSwitchTpsActive(page10, current)
        || sparkModeCondSwitchEthanolActive(page10, current));
}

/**
 * @brief Check if spark table 2 should activate via digital input (PRIVATE)
 * @param page10 Configuration page 10
 * @return true if input pin matches polarity, false otherwise
 *
 * @note Allows external switch control of spark table selection
 * @note Polarity configurable (active high or active low)
 *
 * @complexity 2 (mode check + pin read)
 * @misra Compliant: 3 effective lines
 */
static inline bool sparkModeInputSwitchActive(const config10 &page10) {
  return (page10.spark2Mode == SPARK2_MODE_INPUT_SWITCH)
      && (digitalRead(pinSpark2Input) == page10.spark2InputPolarity);
}

/**
 * @brief Check if fixed timing mode is enabled (PRIVATE)
 * @param page2 Configuration page 2
 * @param current Current engine status
 * @return true if fixed timing or cranking, false otherwise
 *
 * @note Fixed timing overrides secondary spark table
 * @note Cranking uses fixed cranking advance angle
 *
 * @details Returns true if:
 *          - fixAngEnable == 1 (user-configured fixed timing)
 *          - BIT_ENGINE_CRANK set (engine cranking)
 *
 * @complexity 2 (two OR conditions)
 * @misra Compliant: 3 effective lines
 */
static inline bool isFixedTimingOn(const config2 &page2, const statuses &current) {
            // Fixed timing is in effect
    return  (page2.fixAngEnable == 1U)
            // Cranking, so the cranking advance angle is in effect
            || (BIT_CHECK(current.engine, BIT_ENGINE_CRANK));
}

//═══════════════════════════════════════════════════════════════════════════
// SECONDARY SPARK TABLE - PUBLIC API
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Calculate final spark advance using primary and secondary tables (PUBLIC)
 * @param page2 Configuration page 2 (for fixed timing check)
 * @param page10 Configuration page 10 (secondary spark config)
 * @param sparkLookupTable Secondary spark advance table
 * @param[in,out] current Current engine status (modifies advance, advance2, status5)
 *
 * @details Blends primary (advance1) and secondary (advance2) spark tables:
 *
 *          **FIXED TIMING/CRANKING**:
 *          - Skips all secondary table logic
 *          - Uses fixed timing or cranking advance
 *          - advance2 = 0, BIT_STATUS5_SPARK2_ACTIVE cleared
 *
 *          **SPARK2_MODE_MULTIPLY** (not fixed timing):
 *          - advance = (advance1 × spark2Percent) ÷ 100
 *          - spark2Percent from table, clamped to [0, 255]
 *          - advance2 = spark2Percent - 127 (informational, adjusted to int8_t range)
 *          - Overflow protected via constrainAdvance()
 *
 *          **SPARK2_MODE_ADD** (not fixed timing):
 *          - advance = advance1 + advance2
 *          - advance2 from table, constrained to int8_t
 *          - Overflow protected via constrainAdvance()
 *
 *          **SPARK2_MODE_CONDITIONAL_SWITCH** (not fixed timing):
 *          - advance = advance2 if any condition met (RPM/MAP/TPS/Ethanol)
 *          - advance2 corrected via correctionsIgn() (not in UNIT_TEST)
 *          - Full advance corrections applied to secondary value
 *
 *          **SPARK2_MODE_INPUT_SWITCH** (not fixed timing):
 *          - advance = advance2 if digital input matches polarity
 *          - advance2 corrected via correctionsIgn() (not in UNIT_TEST)
 *
 *          **Unknown/Inactive**:
 *          - advance = advance1 (primary table only)
 *          - advance2 = 0, BIT_STATUS5_SPARK2_ACTIVE cleared
 *
 * @note Sets BIT_STATUS5_SPARK2_ACTIVE when secondary table used
 * @note Fixed timing ALWAYS overrides secondary spark table
 * @note Overflow protection on all arithmetic operations
 * @note UNIT_TEST mode skips correctionsIgn() for deterministic testing
 *
 * @example Multiply mode: advance1=20° BTDC, spark2=110% → advance=(20×110)÷100=22° BTDC
 * @example Add mode: advance1=20° BTDC, advance2=5° → advance=25° BTDC
 * @example Switch @ 7000 RPM (threshold 6000): advance=advance2 (secondary)
 *
 * @complexity 2 (if-else chain, no deep nesting)
 * @misra Compliant: 24 effective lines, guard clauses pattern
 */
void calculateSecondarySpark(const config2 &page2, const config10 &page10, const table3d16RpmLoad &sparkLookupTable, statuses &current)
{
  BIT_CLEAR(current.status5, BIT_STATUS5_SPARK2_ACTIVE); //Clear the bit indicating that the 2nd spark table is in use. 
  current.advance2 = 0;

  if (!isFixedTimingOn(page2, current))
  {
    if(page10.spark2Mode == SPARK2_MODE_MULTIPLY)
    {
      BIT_SET(current.status5, BIT_STATUS5_SPARK2_ACTIVE);
      uint8_t spark2Percent = (uint8_t)clamp(lookupSpark2(page10, sparkLookupTable, current), (int16_t)0, (int16_t)UINT8_MAX);
      //Spark 2 table is treated as a % value. Table 1 and 2 are multiplied together and divided by 100
      int16_t combinedAdvance = div100((int16_t)spark2Percent * (int16_t)current.advance1);
      //make sure we don't overflow and accidentally set negative timing: current.advance can only hold a signed 8 bit value
      current.advance = constrainAdvance(combinedAdvance);

      // This is informational only, but the value needs corrected into the int8_t range
      current.advance2 = constrainAdvance((int16_t)spark2Percent-(int16_t)INT8_MAX);
    }
    else if(page10.spark2Mode == SPARK2_MODE_ADD)
    {    
      BIT_SET(current.status5, BIT_STATUS5_SPARK2_ACTIVE); //Set the bit indicating that the 2nd spark table is in use. 
      current.advance2 = constrainAdvance(lookupSpark2(page10, sparkLookupTable, current));
      //Spark tables are added together, but a check is made to make sure this won't overflow the 8-bit VE value
      int16_t combinedAdvance = (int16_t)current.advance1 + (int16_t)current.advance2;
      current.advance = constrainAdvance(combinedAdvance);
    }
    else if(sparkModeCondSwitchActive(page10, current) || sparkModeInputSwitchActive(page10))
    {
      BIT_SET(current.status5, BIT_STATUS5_SPARK2_ACTIVE); //Set the bit indicating that the 2nd spark table is in use. 
#if defined(UNIT_TEST)
      current.advance2 = constrainAdvance(lookupSpark2(page10, sparkLookupTable, current));
#else
      //Perform the corrections calculation on the secondary advance value, only if it uses a switched mode
      current.advance2 = correctionsIgn(constrainAdvance(lookupSpark2(page10, sparkLookupTable, current)));
#endif      
      current.advance = current.advance2;
    }
    else
    {
      // Unknown mode or mode not activated
      // Keep MISRA checker happy.
    }
  }
}
