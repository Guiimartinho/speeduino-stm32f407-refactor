/**
 * @file aircon_control.cpp
 * @brief Air conditioning control implementation
 */

#include "aircon_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"

namespace speeduino {
namespace aircon {

// Anonymous namespace for private implementation
namespace {

// State variables (encapsulated)
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

/**
 * @brief Check coolant temperature lockout
 * @details Sets/clears BIT_AIRCON_CLT_LOCKOUT with hysteresis
 */
static void checkCoolantLockout(void) {
    const int16_t offTemp = temperatureRemoveOffset(configPage15.airConClTempCut);

    // Check if temperature exceeded threshold
    if (currentStatus.coolant > offTemp) {
        BIT_SET(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT);
        return;
    }

    // Apply hysteresis (2 degree deadband)
    // Prevents oscillation at threshold
    if (currentStatus.coolant < (offTemp - 1)) {
        BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT);
    }
}

/**
 * @brief Check TPS lockout (high throttle cut)
 * @details Disables A/C during WOT with standdown delay
 */
static void checkTPSLockout(void) {
    // Guard clause: TPS above threshold?
    if (currentStatus.TPS > configPage15.airConTPSCut) {
        BIT_SET(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
        state.tpsLockoutDelay = 0U;
        return;
    }

    // TPS below threshold - check if we're currently locked out
    const bool currentlyLocked = BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
    if (!currentlyLocked) {
        state.tpsLockoutDelay = 0U;
        return;
    }

    // Apply standdown delay before clearing lockout
    if (state.tpsLockoutDelay >= configPage15.airConTPSCutTime) {
        BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
    } else {
        state.tpsLockoutDelay++;
    }
}

/**
 * @brief Check RPM lockout (high/low RPM cut)
 * @details Disables A/C outside RPM range with standdown delay
 */
static void checkRPMLockout(void) {
    const uint16_t minRPM = (uint16_t)configPage15.airConMinRPMdiv10 * 10U;
    const uint16_t maxRPM = (uint16_t)configPage15.airConMaxRPMdiv100 * 100U;

    // Guard clause: RPM out of range?
    if ((currentStatus.RPM < minRPM) || (currentStatus.RPMdiv100 > configPage15.airConMaxRPMdiv100)) {
        BIT_SET(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
        state.rpmLockoutDelay = 0U;
        return;
    }

    // RPM in range - check if we're currently locked out
    const bool currentlyLocked = BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
    if (!currentlyLocked) {
        state.rpmLockoutDelay = 0U;
        return;
    }

    // Apply standdown delay before clearing lockout
    if (state.rpmLockoutDelay >= configPage15.airConRPMCutTime) {
        BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
    } else {
        state.rpmLockoutDelay++;
    }
}

/**
 * @brief Check if engine has been running long enough after cranking
 * @details Prevents A/C from starting immediately after engine start
 */
static void checkPostCrankingDelay(void) {
    // Guard clause: engine not running?
    if (!BIT_CHECK(currentStatus.engine, BIT_ENGINE_RUN)) {
        state.afterEngineStartDelay = 0U;
        state.waitedAfterCranking = false;
        return;
    }

    // Engine running - check if delay period complete
    if (state.afterEngineStartDelay >= configPage15.airConAfterStartDelay) {
        state.waitedAfterCranking = true;
    } else {
        state.afterEngineStartDelay++;
    }
}

/**
 * @brief Check all lockout conditions
 * @details Updates status bits for all lockout types
 */
static void updateLockouts(void) {
    checkCoolantLockout();
    checkTPSLockout();
    checkRPMLockout();
}

/**
 * @brief Check if A/C is allowed to run
 * @details Combines all safety checks
 * @return true if OK to run, false if locked out
 */
static bool isAllowedToRun(void) {
    // Must have waited after cranking
    if (!state.waitedAfterCranking) {
        return false;
    }

    // Check all lockouts
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT)) {
        return false;
    }

    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT)) {
        return false;
    }

    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT)) {
        return false;
    }

    return true;
}

/**
 * @brief Control standalone fan (if configured)
 * @param enabled true to turn fan ON, false to turn OFF
 */
static void controlStandaloneFan(bool enabled) {
    // Guard clause: standalone fan not configured?
    if (!state.standAloneFanEnabled) {
        return;
    }

    if (enabled) {
        AIRCON_FAN_ON();
    } else {
        AIRCON_FAN_OFF();
    }
}

/**
 * @brief Control compressor with turn-on delay
 * @details Applies delay before starting compressor
 */
static void controlCompressor(void) {
    // Guard clause: check if allowed to run
    if (!isAllowedToRun()) {
        BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
        controlStandaloneFan(false);
        AIRCON_OFF();
        state.startDelay = 0U;
        return;
    }

    // Set turning-on flag (signals idle system to idle up)
    BIT_SET(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);

    // Turn on standalone fan immediately
    controlStandaloneFan(true);

    // Apply turn-on delay for compressor
    if (state.startDelay >= configPage15.airConCompOnDelay) {
        AIRCON_ON();
    } else {
        state.startDelay++;
    }
}

} // anonymous namespace

// Public interface implementation

bool initialise(void) {
    // Guard clause: A/C not enabled in config?
    if (configPage15.airConEnable != 1U) {
        state.isEnabled = false;
        return false;
    }

    // Guard clause: pins not configured?
    if ((pinAirConRequest == 0U) || (pinAirConComp == 0U)) {
        state.isEnabled = false;
        return false;
    }

    // Initialize state
    state.afterEngineStartDelay = 0U;
    state.waitedAfterCranking = false;
    state.startDelay = 0U;
    state.tpsLockoutDelay = 0U;
    state.rpmLockoutDelay = 0U;

    // Clear all status bits
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_REQUEST);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_COMPRESSOR);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_FAN);

    // Setup request pin (input)
    aircon_req_pin_port = portInputRegister(digitalPinToPort(pinAirConRequest));
    aircon_req_pin_mask = digitalPinToBitMask(pinAirConRequest);

    // Setup compressor pin (output)
    aircon_comp_pin_port = portOutputRegister(digitalPinToPort(pinAirConComp));
    aircon_comp_pin_mask = digitalPinToBitMask(pinAirConComp);

    // Ensure compressor is OFF
    AIRCON_OFF();

    // Setup standalone fan if configured
    if ((configPage15.airConFanEnabled > 0U) && (pinAirConFan != 0U)) {
        aircon_fan_pin_port = portOutputRegister(digitalPinToPort(pinAirConFan));
        aircon_fan_pin_mask = digitalPinToBitMask(pinAirConFan);
        AIRCON_FAN_OFF();
        state.standAloneFanEnabled = true;
    } else {
        state.standAloneFanEnabled = false;
    }

    state.isEnabled = true;
    return true;
}

void update(void) {
    // Guard clause: A/C not enabled?
    if (!state.isEnabled) {
        return;
    }

    // Update post-cranking delay
    checkPostCrankingDelay();

    // Update all lockout conditions
    updateLockouts();

    // Check if A/C requested and allowed to run
    if (isRequested() && isAllowedToRun()) {
        controlCompressor();
    } else {
        // Not requested or locked out - turn off
        BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
        controlStandaloneFan(false);
        AIRCON_OFF();
        state.startDelay = 0U;
    }
}

bool isRequested(void) {
    // Guard clause: A/C not enabled?
    if (!state.isEnabled) {
        return false;
    }

    // Read request pin with polarity handling
    const bool pinHigh = !!(*aircon_req_pin_port & aircon_req_pin_mask);
    const bool requested = (configPage15.airConReqPol == 1U) ? pinHigh : !pinHigh;

    // Update status bit
    BIT_WRITE(currentStatus.airConStatus, BIT_AIRCON_REQUEST, requested);

    return requested;
}

void forceOff(void) {
    // Disable compressor and fan immediately
    AIRCON_OFF();
    controlStandaloneFan(false);

    // Clear control bits
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_TURNING_ON);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_COMPRESSOR);
    BIT_CLEAR(currentStatus.airConStatus, BIT_AIRCON_FAN);

    // Reset delays
    state.startDelay = 0U;
}

bool isEnabled(void) {
    return state.isEnabled;
}

bool isCompressorOn(void) {
    return BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_COMPRESSOR);
}

bool isLockedOut(void) {
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_CLT_LOCKOUT)) {
        return true;
    }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_TPS_LOCKOUT)) {
        return true;
    }
    if (BIT_CHECK(currentStatus.airConStatus, BIT_AIRCON_RPM_LOCKOUT)) {
        return true;
    }
    return false;
}

} // namespace aircon
} // namespace speeduino
