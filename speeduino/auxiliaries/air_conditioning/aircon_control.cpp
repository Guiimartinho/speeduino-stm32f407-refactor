/**
 * @file aircon_control.cpp
 * @brief Air conditioning control implementation
 */

#include "aircon_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================

// State variables
struct AirConState {
    bool isEnabled;
    bool standAloneFanEnabled;
    uint8_t startDelay;
    uint8_t tpsLockoutDelay;
    uint8_t rpmLockoutDelay;
    uint8_t afterEngineStartDelay;
    bool waitedAfterCranking;
};

static AirConState state;

// Forward declarations for functions used before defined
static bool aircon_isRequested_impl(void);

/**
 * @brief Check coolant temperature lockout
 */
static void checkCoolantLockout(void)
{
    const int16_t offTemp = temperatureRemoveOffset(configPage15.airConClTempCut);

    if (currentStatus.coolant > offTemp)
    {
        BIT_SET(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT);
        return;
    }

    // Hysteresis: 2 degree deadband
    if (currentStatus.coolant < (offTemp - 1)) { BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT); }
}

/**
 * @brief Check TPS lockout (high throttle cut)
 */
static void checkTPSLockout(void)
{
    if (currentStatus.TPS > configPage15.airConTPSCut)
    {
        BIT_SET(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
        state.tpsLockoutDelay = 0U;
        return;
    }

    bool currentlyLocked = BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
    if (!currentlyLocked) { state.tpsLockoutDelay = 0U; return; }

    bool delayComplete = (state.tpsLockoutDelay >= configPage15.airConTPSCutTime);
    if (delayComplete) { BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT); }
    else { state.tpsLockoutDelay++; }
}

/**
 * @brief Check RPM lockout (high/low RPM cut)
 */
static void checkRPMLockout(void)
{
    const uint16_t minRPM = (uint16_t)configPage15.airConMinRPMdiv10 * 10U;
    bool rpmOutOfRange = (currentStatus.RPM < minRPM) || (currentStatus.RPMdiv100 > configPage15.airConMaxRPMdiv100);

    if (rpmOutOfRange)
    {
        BIT_SET(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
        state.rpmLockoutDelay = 0U;
        return;
    }

    bool currentlyLocked = BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
    if (!currentlyLocked) { state.rpmLockoutDelay = 0U; return; }

    bool delayComplete = (state.rpmLockoutDelay >= configPage15.airConRPMCutTime);
    if (delayComplete) { BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT); }
    else { state.rpmLockoutDelay++; }
}

/**
 * @brief Check if engine has been running long enough after cranking
 */
static void checkPostCrankingDelay(void)
{
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN))
    {
        state.afterEngineStartDelay = 0U;
        state.waitedAfterCranking = false;
        return;
    }

    bool delayComplete = (state.afterEngineStartDelay >= configPage15.airConAfterStartDelay);
    if (delayComplete) { state.waitedAfterCranking = true; }
    else { state.afterEngineStartDelay++; }
}

/**
 * @brief Check all lockout conditions
 */
static inline void updateLockouts(void)
{
    checkCoolantLockout();
    checkTPSLockout();
    checkRPMLockout();
}

/**
 * @brief Check if A/C is allowed to run
 */
static bool isAllowedToRun(void)
{
    if (!state.waitedAfterCranking) { return false; }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT)) { return false; }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT)) { return false; }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT)) { return false; }
    return true;
}

/**
 * @brief Control standalone fan (if configured)
 */
static void controlStandaloneFan(bool enabled)
{
    if (!state.standAloneFanEnabled) { return; }
    if (enabled) { AIRCON_FAN_ON(); }
    else { AIRCON_FAN_OFF(); }
}

/**
 * @brief Control compressor with turn-on delay
 */
static void controlCompressor(void)
{
    if (!isAllowedToRun())
    {
        BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
        controlStandaloneFan(false);
        AIRCON_OFF();
        state.startDelay = 0U;
        return;
    }

    BIT_SET(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
    controlStandaloneFan(true);

    bool delayComplete = (state.startDelay >= configPage15.airConCompOnDelay);
    if (delayComplete) { AIRCON_ON(); }
    else { state.startDelay++; }
}

/**
 * @brief Internal initialise implementation
 */
static bool aircon_initialise_impl(void)
{
    if (configPage15.airConEnable != 1U) { state.isEnabled = false; return false; }
    if ((pinAirConRequest == 0U) || (pinAirConComp == 0U)) { state.isEnabled = false; return false; }

    state.afterEngineStartDelay = 0U;
    state.waitedAfterCranking = false;
    state.startDelay = 0U;
    state.tpsLockoutDelay = 0U;
    state.rpmLockoutDelay = 0U;

    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_REQUEST);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_COMPRESSOR);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_FAN);

    aircon_req_pin_port = portInputRegister(digitalPinToPort(pinAirConRequest));
    aircon_req_pin_mask = digitalPinToBitMask(pinAirConRequest);

    aircon_comp_pin_port = portOutputRegister(digitalPinToPort(pinAirConComp));
    aircon_comp_pin_mask = digitalPinToBitMask(pinAirConComp);

    AIRCON_OFF();

    bool fanConfigured = (configPage15.airConFanEnabled > 0U) && (pinAirConFan != 0U);
    if (fanConfigured)
    {
        aircon_fan_pin_port = portOutputRegister(digitalPinToPort(pinAirConFan));
        aircon_fan_pin_mask = digitalPinToBitMask(pinAirConFan);
        AIRCON_FAN_OFF();
        state.standAloneFanEnabled = true;
    }
    else { state.standAloneFanEnabled = false; }

    state.isEnabled = true;
    return true;
}

/**
 * @brief Internal update implementation
 */
static void aircon_update_impl(void)
{
    if (!state.isEnabled) { return; }

    checkPostCrankingDelay();
    updateLockouts();

    bool shouldRun = aircon_isRequested_impl() && isAllowedToRun();
    if (shouldRun) { controlCompressor(); return; }

    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
    controlStandaloneFan(false);
    AIRCON_OFF();
    state.startDelay = 0U;
}

/**
 * @brief Internal forceOff implementation
 */
static void aircon_forceOff_impl(void)
{
    AIRCON_OFF();
    controlStandaloneFan(false);

    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_COMPRESSOR);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_FAN);

    state.startDelay = 0U;
}

/**
 * @brief Internal isRequested implementation
 */
static bool aircon_isRequested_impl(void)
{
    if (!state.isEnabled) { return false; }

    bool pinHigh = !!(*aircon_req_pin_port & aircon_req_pin_mask);
    bool requested = (configPage15.airConReqPol == 1U) ? pinHigh : !pinHigh;

    BIT_WRITE(currentStatus.airConStatus, BIT_AIRCON_REQUEST, requested);
    return requested;
}

/**
 * @brief Internal isLockedOut implementation
 */
static bool aircon_isLockedOut_impl(void)
{
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT)) { return true; }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT)) { return true; }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT)) { return true; }
    return false;
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace aircon {

bool initialise(void) { return aircon_initialise_impl(); }
void update(void) { aircon_update_impl(); }
void forceOff(void) { aircon_forceOff_impl(); }
bool isEnabled(void) { return state.isEnabled; }
bool isCompressorOn(void) { return BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_COMPRESSOR); }
bool isRequested(void) { return aircon_isRequested_impl(); }
bool isLockedOut(void) { return aircon_isLockedOut_impl(); }

} // namespace aircon
} // namespace speeduino
