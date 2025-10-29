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

namespace speeduino {
namespace auxiliaries {

// Initialization state tracking
static bool initialized = false;

bool initialiseAll(void) {
    // Initialize each module
    bool success = true;

    // Air Conditioning
    if (!aircon::initialise()) {
        // A/C init failed (likely disabled in config)
    }

    // Fan Control
    if (!fan::initialise()) {
        success = false;
    }

    // Boost Control
    if (!boost::initialise()) {
        success = false;
    }

    // VVT Control
    if (!vvt::initialise()) {
        success = false;
    }

    // Nitrous Control
    if (!nitrous::initialise()) {
        // Nitrous init failed (likely disabled)
    }

    // WMI Control
    if (!wmi::initialise()) {
        // WMI init failed (likely disabled)
    }

    initialized = success;
    return success;
}

void updateAll(void) {
    // Guard clause: not initialized
    if (!initialized) {
        return;
    }

    // Update each active module
    // These functions have internal guards for enabled/disabled state

    aircon::update();
    fan::update();
    boost::update();
    vvt::update();
    nitrous::update();
    wmi::update();
}

void disableAll(void) {
    // Force all auxiliaries off
    aircon::forceOff();
    fan::forceOff();
    boost::disable();
    vvt::disable();
    nitrous::disable();
    wmi::disable();
}

void initialiseAuxPWM(void) {
    // Legacy interface - calls new modular init
    initialiseAll();
}

} // namespace auxiliaries
} // namespace speeduino

// Legacy global functions for backward compatibility
void initialiseAirCon(void) {
    speeduino::aircon::initialise();
}

void airConControl(void) {
    speeduino::aircon::update();
}

void initialiseFan(void) {
    speeduino::fan::initialise();
}

void fanControl(void) {
    speeduino::fan::update();
}

void initialiseAuxPWM(void) {
    speeduino::auxiliaries::initialiseAuxPWM();
}

void boostControl(void) {
    speeduino::boost::update();
}

void vvtControl(void) {
    speeduino::vvt::update();
}

void nitrousControl(void) {
    speeduino::nitrous::update();
}

void wmiControl(void) {
    speeduino::wmi::update();
}

void boostDisable(void) {
    speeduino::boost::disable();
}
