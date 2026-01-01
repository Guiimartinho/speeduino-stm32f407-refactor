/**
 * @file auxiliary_coordinator.cpp
 * @brief Main coordinator implementation for all auxiliary systems
 */

#include "auxiliary_coordinator.h"
#include "air_conditioning/aircon_control.h"
#include "fan_control/fan_control.h"
#include "boost_control/boost_control.h"
#include "vvt_control/vvt_control.h"
#include "nitrous_control/nitrous_control.h"
#include "wmi_control/wmi_control.h"
#include "../globals.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================

// Initialization state tracking
static bool initialized = false;

/**
 * @brief Internal initialiseAll implementation
 */
static bool initialiseAll_impl(void)
{
    bool success = true;

    // Initialize each module - track failures for critical systems
    speeduino::aircon::initialise();  // Non-critical
    if (!speeduino::fan::initialise()) { success = false; }
    if (!speeduino::boost::initialise()) { success = false; }
    if (!speeduino::vvt::initialise()) { success = false; }
    speeduino::nitrous::initialise();  // Non-critical
    speeduino::wmi::initialise();      // Non-critical

    initialized = success;
    return success;
}

/**
 * @brief Internal updateAll implementation
 */
static void updateAll_impl(void)
{
    if (!initialized) { return; }

    speeduino::aircon::update();
    speeduino::fan::update();
    speeduino::boost::update();
    speeduino::vvt::update();
    speeduino::nitrous::update();
    speeduino::wmi::update();
}

/**
 * @brief Internal disableAll implementation
 */
static void disableAll_impl(void)
{
    speeduino::aircon::forceOff();
    speeduino::fan::forceOff();
    speeduino::boost::disable();
    speeduino::vvt::disable();
    speeduino::nitrous::disable();
    speeduino::wmi::disable();
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace auxiliaries {

bool initialiseAll(void) { return initialiseAll_impl(); }
void updateAll(void) { updateAll_impl(); }
void disableAll(void) { disableAll_impl(); }
void initialiseAuxPWM(void) { initialiseAll_impl(); }

} // namespace auxiliaries
} // namespace speeduino

// Legacy global functions for backward compatibility
void initialiseAirCon(void) { speeduino::aircon::initialise(); }
void airConControl(void) { speeduino::aircon::update(); }
void initialiseFan(void) { speeduino::fan::initialise(); }
void fanControl(void) { speeduino::fan::update(); }
void initialiseAuxPWM(void) { speeduino::auxiliaries::initialiseAuxPWM(); }
void boostControl(void) { speeduino::boost::update(); }
void vvtControl(void) { speeduino::vvt::update(); }
void nitrousControl(void) { speeduino::nitrous::update(); }
void wmiControl(void) { speeduino::wmi::update(); }
void boostDisable(void) { speeduino::boost::disable(); }
