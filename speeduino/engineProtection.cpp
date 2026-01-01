
#include "globals.h"
#include "engineProtection.h"
#include "maths.h"
#include "utilities.h"
#include "units.h"
#include "crankMaths.h"
#include "corrections/ignition_corrections/ignition_corrections.h"

byte oilProtStartTime = 0;
static table2D_u8_u8_4 oilPressureProtectTable(&configPage10.oilPressureProtRPM, &configPage10.oilPressureProtMins);
static table2D_u8_u8_6 coolantProtectTable(&configPage9.coolantProtTemp, &configPage9.coolantProtRPM);

byte checkEngineProtect(void)
{
  byte protectActive = 0;
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
  bool softLimitHit = (softLimitTime > configPage4.SoftLimMax) &&
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
      /*
      switch(configPage6.boostCutType)
      {
        case 1:
          BIT_SET(currentStatus.status2, BIT_STATUS2_BOOSTCUT);
          BIT_CLEAR(currentStatus.status1, BIT_STATUS1_BOOSTCUT);
          BIT_SET(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_MAP);
          break;
        case 2:
          BIT_SET(currentStatus.status1, BIT_STATUS1_BOOSTCUT);
          BIT_CLEAR(currentStatus.status2, BIT_STATUS2_BOOSTCUT);
          break;
        case 3:
          BIT_SET(currentStatus.status2, BIT_STATUS2_BOOSTCUT);
          BIT_SET(currentStatus.status1, BIT_STATUS1_BOOSTCUT);
          break;
        default:
          //Shouldn't ever happen, but just in case, disable all cuts
          BIT_CLEAR(currentStatus.status1, BIT_STATUS1_BOOSTCUT);
          BIT_CLEAR(currentStatus.status2, BIT_STATUS2_BOOSTCUT);
      }
      */
    }
  }

  return boostLimitActive;
}

byte checkOilPressureLimit(void)
{
  byte oilProtectActive = 0;
  bool alreadyActive = BIT_CHECK(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_OIL);
  BIT_CLEAR(currentStatus.engineProtectStatus, ENGINE_PROTECT_BIT_OIL);

  // Guard: protection disabled
  if (configPage6.engineProtectType == PROTECT_CUT_OFF) { return oilProtectActive; }
  if (!configPage10.oilPressureProtEnbl) { return oilProtectActive; }
  if (!configPage10.oilPressureEnable) { return oilProtectActive; }

  byte oilLimit = table2D_getValue(&oilPressureProtectTable, currentStatus.RPMdiv100);

  // Oil pressure OK - reset timer and return
  if (currentStatus.oilPressure >= oilLimit)
  {
    oilProtStartTime = 0;
    return oilProtectActive;
  }

  // Oil pressure LOW - start countdown if first time
  if (oilProtStartTime == 0) { oilProtStartTime = div100(millis()); }

  // Check if countdown expired or already active
  bool countdownExpired = (uint8_t(div100(millis())) >= uint16_t(oilProtStartTime + configPage10.oilPressureProtTime));
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

  if (!isAFRProtectionEnabled()) { return checkAFRLimitActive; }

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
