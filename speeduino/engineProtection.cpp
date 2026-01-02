
#include "globals.h"
#include "engineProtection.h"
#include "maths.h"
#include "utilities.h"
#include "units.h"
#include "crankMaths.h"
#include "corrections/ignition_corrections/ignition_corrections.h"

unsigned long oilProtStartTime = 0;  // Must be unsigned long to prevent overflow (was byte)
static unsigned long coolantCriticalStartTime = 0; // Critical coolant protection timer

// Critical coolant threshold (120°C = 248°F) - engine damage imminent
static constexpr int16_t COOLANT_CRITICAL_TEMP = 120;
static table2D_u8_u8_4 oilPressureProtectTable(&configPage10.oilPressureProtRPM, &configPage10.oilPressureProtMins);
static table2D_u8_u8_6 coolantProtectTable(&configPage9.coolantProtTemp, &configPage9.coolantProtRPM);

byte checkEngineProtect(void)
{
  byte protectActive = 0;

  // CRITICAL: Check critical coolant first - this is a hard cut regardless of other settings
  if (checkCriticalCoolantProtection())
  {
    return 1;  // Immediate protection active
  }

  if(checkBoostLimit() || checkOilPressureLimit() || checkAFRLimit() )
  {
    if( currentStatus.RPMdiv100 > configPage4.engineProtectMaxRPM ) { protectActive = 1; }
  }

  return protectActive;
}

/**
 * @brief Check if fixed rev limit is exceeded
 * @return true if RPM exceeds hard or soft limit
 */
static inline bool isFixedRevLimitExceeded(void)
{
  bool hardLimitHit = (currentStatus.RPMdiv100 >= configPage4.HardRevLim);
  bool softLimitHit = (softLimitTime >= configPage4.SoftLimMax) &&  // Fixed: >= instead of > to activate at limit
                      (currentStatus.RPMdiv100 >= configPage4.SoftRevLim);
  return hardLimitHit || softLimitHit;
}

byte checkRevLimit(void)
{
  // Default to no limit (In case PROTECT_CUT_OFF is selected)
  byte currentLimitRPM = UINT8_MAX;
  BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_RPM);
  BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_COOLANT);

  if (configPage6.engineProtectType == PROTECT_CUT_OFF) { return currentLimitRPM; }

  // Fixed rev limit mode
  if (configPage9.hardRevMode == HARD_REV_FIXED)
  {
    currentLimitRPM = configPage4.HardRevLim;
    if (isFixedRevLimitExceeded()) { BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_RPM); }
    return currentLimitRPM;
  }

  // Coolant-based rev limit mode
  if (configPage9.hardRevMode == HARD_REV_COOLANT)
  {
    currentLimitRPM = (int16_t)(table2D_getValue(&coolantProtectTable, temperatureAddOffset(currentStatus.coolant)));
    if (currentStatus.RPMdiv100 > currentLimitRPM)
    {
      BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_COOLANT);
      BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_RPM);
    }
  }

  return currentLimitRPM;
}

//=============================================================================
// CRITICAL COOLANT PROTECTION
//=============================================================================

/**
 * @brief Check critical coolant temperature - TOTAL fuel/ignition cut
 * @return true if critical coolant protection is active
 *
 * @details When coolant exceeds COOLANT_CRITICAL_TEMP (120°C), this function
 *          activates after a 2-second delay to prevent engine destruction.
 *          Unlike normal coolant protection, this cuts BOTH fuel AND ignition.
 *
 * @note This is a last-resort protection - engine will stall to prevent damage
 * @note Uses 5°C hysteresis to prevent cycling
 */
bool checkCriticalCoolantProtection(void)
{
  static bool criticalCoolantActive = false;
  static constexpr int16_t HYSTERESIS = 5;  // 5°C hysteresis
  static constexpr unsigned long DELAY_MS = 2000UL;  // 2 second delay before cut

  // Guard: protection disabled
  if (configPage6.engineProtectType == PROTECT_CUT_OFF) { return false; }

  int16_t coolant = currentStatus.coolant;

  // Hysteresis: deactivate at lower temp
  if (criticalCoolantActive)
  {
    if (coolant < (COOLANT_CRITICAL_TEMP - HYSTERESIS))
    {
      criticalCoolantActive = false;
      coolantCriticalStartTime = 0;
    }
    return criticalCoolantActive;
  }

  // Check if coolant exceeds critical threshold
  if (coolant < COOLANT_CRITICAL_TEMP)
  {
    coolantCriticalStartTime = 0;
    return false;
  }

  // Start countdown if first time
  if (coolantCriticalStartTime == 0) { coolantCriticalStartTime = millis(); }

  // Check if countdown expired
  if ((millis() - coolantCriticalStartTime) >= DELAY_MS)
  {
    criticalCoolantActive = true;
    // Force BOTH cuts for maximum protection
    BIT_SET(currentStatus.status1, BIT_STATUS1_BOOSTCUT);  // Fuel cut
    BIT_SET(currentStatus.status2, BIT_STATUS2_BOOSTCUT);  // Ignition cut
    BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_COOLANT);
  }

  return criticalCoolantActive;
}

//=============================================================================
// BOOST LIMIT
//=============================================================================

byte checkBoostLimit(void)
{
  byte boostLimitActive = 0;
  BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_MAP);
  BIT_CLEAR(currentStatus.status2, BIT_STATUS2_BOOSTCUT);
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_BOOSTCUT);

  if (configPage6.engineProtectType != PROTECT_CUT_OFF) {
    //Boost cutoff is very similar to launchControl, but with a check against MAP rather than a switch
    if( (configPage6.boostCutEnabled > 0) && (currentStatus.MAP > (configPage6.boostLimit * 2)) ) //The boost limit is divided by 2 to allow a limit up to 511kPa
    {
      boostLimitActive = 1;
      BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_MAP);
      // Both fuel and ignition cut for maximum safety
      BIT_SET(currentStatus.status2, BIT_STATUS2_BOOSTCUT);  // Ignition cut
      BIT_SET(currentStatus.status1, BIT_STATUS1_BOOSTCUT);  // Fuel cut
    }
  }

  return boostLimitActive;
}

//=============================================================================
// OIL PRESSURE PROTECTION (with hysteresis)
//=============================================================================

/**
 * @brief Check oil pressure protection limit with hysteresis
 * @return 1 if oil pressure protection active, 0 otherwise
 *
 * @details Monitors oil pressure vs RPM-based minimum threshold.
 *          Uses 5 PSI hysteresis to prevent cycling when near threshold.
 *          Activates after configurable delay to ignore transients.
 *
 * @note Hysteresis: activates at limit, deactivates at limit+5 PSI
 */
byte checkOilPressureLimit(void)
{
  static constexpr byte OIL_PRESSURE_HYSTERESIS = 5;  // 5 PSI hysteresis
  byte oilProtectActive = 0;
  bool alreadyActive = BIT_CHECK(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_OIL);
  BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_OIL);

  // Guard: protection disabled
  if (configPage6.engineProtectType == PROTECT_CUT_OFF) { return oilProtectActive; }
  if (!configPage10.oilPressureProtEnbl) { return oilProtectActive; }
  if (!configPage10.oilPressureEnable) { return oilProtectActive; }

  byte oilLimit = table2D_getValue(&oilPressureProtectTable, currentStatus.RPMdiv100);

  // Hysteresis: if already active, require higher pressure to deactivate
  byte deactivateLimit = oilLimit;
  if (alreadyActive)
  {
    // Safely add hysteresis without overflow
    if (oilLimit <= (255U - OIL_PRESSURE_HYSTERESIS))
    {
      deactivateLimit = oilLimit + OIL_PRESSURE_HYSTERESIS;
    }
    else
    {
      deactivateLimit = 255U;
    }
  }

  // Oil pressure OK - reset timer and return
  if (currentStatus.oilPressure >= deactivateLimit)
  {
    oilProtStartTime = 0;
    return oilProtectActive;
  }

  // Oil pressure LOW - only start countdown if below activation threshold
  if (currentStatus.oilPressure >= oilLimit)
  {
    // In hysteresis band - maintain current state
    if (alreadyActive)
    {
      BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_OIL);
      return 1;
    }
    return oilProtectActive;
  }

  // Oil pressure critically LOW - start countdown if first time
  if (oilProtStartTime == 0) { oilProtStartTime = millis(); }

  // Check if countdown expired or already active
  // configPage10.oilPressureProtTime is in 100ms units, multiply by 100 for ms
  unsigned long protectionDelayMs = (unsigned long)configPage10.oilPressureProtTime * 100UL;
  bool countdownExpired = (millis() >= (oilProtStartTime + protectionDelayMs));
  if (countdownExpired || alreadyActive)
  {
    BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_OIL);
    oilProtectActive = 1;
  }

  return oilProtectActive;
}

//=============================================================================
// AFR PROTECTION - Private Helpers
//=============================================================================

namespace {

/**
 * @brief Check if AFR protection system is enabled
 * @return true if all enable conditions met (engine protect + AFR protect + wideband O2)
 *
 * @complexity 1 (single guard clause with 3 AND conditions)
 * @misra Compliant: 4 lines, guard clause pattern
 */
static inline bool isAFRProtectionEnabled(void)
{
  return (configPage6.engineProtectType != PROTECT_CUT_OFF) &&
         (configPage9.afrProtectEnabled > 0) &&
         (configPage6.egoType == EGO_TYPE_WIDE);
}

/**
 * @brief Calculate AFR condition based on configured mode
 * @return true if AFR exceeds threshold
 *
 * @details Two modes:
 *          - Mode 1: Fixed AFR value comparison
 *          - Mode 2: Deviation from target table
 *
 * @complexity 2 (switch with 2 active cases)
 * @misra Compliant: 9 lines, simple switch dispatch
 */
static inline bool calculateAFRCondition(void)
{
  switch(configPage9.afrProtectEnabled)
  {
    case 1:
      // Fixed value mode
      return (currentStatus.O2 >= configPage9.afrProtectDeviation);
    case 2:
      // Deviation from target table mode
      return (currentStatus.O2 >= (currentStatus.afrTarget + configPage9.afrProtectDeviation));
    default:
      return false; // Unknown mode
  }
}

/**
 * @brief Evaluate all AFR protection activation conditions
 * @return true if all conditions met (MAP, RPM, TPS, AFR thresholds)
 *
 * @details Conditions:
 *          - MAP above configured minimum (×2 for kPa range)
 *          - RPM above configured minimum
 *          - TPS above configured minimum %
 *          - AFR exceeds threshold (mode-dependent)
 *
 * @complexity 1 (all conditions combined with AND)
 * @misra Compliant: 7 lines, clear boolean logic
 */
static inline bool evaluateAFRConditions(void)
{
  static constexpr char X2_MULTIPLIER = 2;

  bool mapCondition = (currentStatus.MAP >= (configPage9.afrProtectMinMAP * X2_MULTIPLIER));
  bool rpmCondition = (currentStatus.RPMdiv100 >= configPage9.afrProtectMinRPM);
  bool tpsCondition = (currentStatus.TPS >= configPage9.afrProtectMinTPS);
  bool afrCondition = calculateAFRCondition();

  return (mapCondition && rpmCondition && tpsCondition && afrCondition);
}

/**
 * @brief Activate AFR protection after countdown expires
 * @param countEnabled Countdown timer state (ref)
 * @param countStart Countdown start time in ms (ref)
 * @param limitActive Protection active flag (ref)
 *
 * @details Starts countdown on first call, activates protection after
 *          configured delay (configPage9.afrProtectCutTime × 100ms)
 *
 * @complexity 2 (two nested if statements)
 * @misra Compliant: 11 lines, clear state machine
 */
static inline void activateAFRProtection(bool& countEnabled, unsigned long& countStart, bool& limitActive)
{
  static constexpr char X100_MULTIPLIER = 100;

  if (!countEnabled)
  {
    countEnabled = true;
    countStart = millis();
  }

  if (millis() >= (countStart + (configPage9.afrProtectCutTime * X100_MULTIPLIER)))
  {
    limitActive = true;
    BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);
  }
}

/**
 * @brief Deactivate AFR protection when conditions not met
 * @param countEnabled Countdown timer state (ref)
 * @param countStart Countdown start time in ms (ref)
 *
 * @complexity 1 (single if statement)
 * @misra Compliant: 7 lines, simple reset logic
 */
static inline void deactivateAFRProtection(bool& countEnabled, unsigned long& countStart)
{
  if (countEnabled)
  {
    countEnabled = false;
    countStart = 0;
  }
}

/**
 * @brief Check for AFR protection reactivation condition (TPS drop)
 * @param limitActive Protection active flag (ref)
 * @param countEnabled Countdown timer state (ref)
 *
 * @details Reactivates (clears protection) when TPS drops below threshold
 *
 * @complexity 1 (single if with AND condition)
 * @misra Compliant: 8 lines, clear deactivation logic
 */
static inline void checkAFRReactivation(bool& limitActive, bool& countEnabled)
{
  if (limitActive && (currentStatus.TPS <= configPage9.afrProtectReactivationTPS))
  {
    limitActive = false;
    countEnabled = false;
    BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);
  }
}

} // anonymous namespace

//=============================================================================
// AFR PROTECTION - Public API
//=============================================================================

/**
 * @brief Check AFR (Air-Fuel Ratio) protection limit with countdown timer
 * @return 1 if AFR protection active (cutting fuel/ignition), 0 otherwise
 *
 * @details AFR protection prevents engine damage from running too lean.
 *          Requires wideband O2 sensor. When all conditions met:
 *          - MAP above minimum
 *          - RPM above minimum
 *          - TPS above minimum
 *          - AFR exceeds threshold (fixed or target+deviation)
 *
 *          Protection activates after configured delay, cuts fuel/ignition.
 *          Reactivates when TPS drops below reactivation threshold.
 *
 * @note Uses static variables for countdown state (thread-safe in single-threaded ECU)
 * @note State persists across calls until conditions change
 *
 * @complexity 3 (was 21 - reduced 86% via helper extraction)
 * @misra Compliant: 18 lines (was 96 - reduced 81%), clear logic flow
 *
 * @note Refactored FASE EP - reduced complexity via helper extraction
 *       Original: 96 lines, C:21 (ternaries + switch + nested ifs)
 *       Refactored: 18 lines, C:3 (guard + if/else + helper call)
 *
 * @example At WOT, 6000 RPM, AFR 18:1 (target 12:1, deviation +5):
 *          Conditions met → countdown starts → after 2s → protection cuts fuel
 *          Driver lifts throttle (TPS < 5%) → protection reactivates
 */
byte checkAFRLimit(void)
{
  static bool checkAFRLimitActive = false;
  static bool afrProtectCountEnabled = false;
  static unsigned long afrProtectCount = 0;

  // Guard: protection disabled - clear all state and return inactive
  if (!isAFRProtectionEnabled())
  {
    checkAFRLimitActive = false;
    afrProtectCountEnabled = false;
    afrProtectCount = 0;
    BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_AFR);
    return 0;
  }

  if (evaluateAFRConditions())
  {
    activateAFRProtection(afrProtectCountEnabled, afrProtectCount, checkAFRLimitActive);
  }
  else
  {
    deactivateAFRProtection(afrProtectCountEnabled, afrProtectCount);
  }

  checkAFRReactivation(checkAFRLimitActive, afrProtectCountEnabled);

  return checkAFRLimitActive;
}
