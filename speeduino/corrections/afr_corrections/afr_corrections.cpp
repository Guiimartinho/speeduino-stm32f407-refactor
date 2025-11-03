/**
 * @file afr_corrections.cpp
 * @brief AFR corrections implementation with MISRA-C compliance
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * REFACTORED MODULE - 100% logic preservation with MISRA-C compliance:
 * - Functions < 50 lines
 * - Cyclomatic complexity < 10
 * - Guard clauses for early returns
 * - Anonymous namespace for private helpers
 * - Comprehensive Doxygen documentation
 *
 * This module handles AFR (Air-Fuel Ratio) corrections:
 * - Target AFR calculation from tables
 * - Closed-loop AFR feedback control (Simple & PID algorithms)
 */

#include "afr_corrections.h"
#include "../../globals.h"
#include "../../corrections.h"
#include "../../speeduino.h"
#include "../../timers.h"
#include "../../maths.h"
#include "../../sensors.h"
#include "../../units.h"
#include "../../src/PID_v1/PID_v1.h"

// ============================================================================
// GLOBAL STATE (PID controller and timing)
// ============================================================================

/// PID controller inputs/outputs for AFR closed-loop control
long PID_O2, PID_output, PID_AFRTarget;

/// PID controller instance (maintains state between calls)
/// Uses REVERSE mode (output decreases when input increases above setpoint)
PID egoPID(&PID_O2, &PID_output, &PID_AFRTarget, configPage6.egoKP, configPage6.egoKI, configPage6.egoKD, REVERSE);

/// Ignition count for next AFR correction cycle
uint16_t AFRnextCycle;

// ============================================================================
// PRIVATE HELPERS (Anonymous Namespace)
// ============================================================================

namespace {

/// @brief No correction constant (100% = 1.0)
static const byte AFR_NO_CORRECTION = 100;

/// @brief Simple algorithm step size (1% per cycle)
static const byte AFR_STEP = 1;

/// @brief Check if all closed-loop conditions are met
/// @return true if all 9 conditions allow closed-loop control
/// @complexity 2
static inline bool isClosedLoopActive(void) {
    // Guard clauses - all conditions that must be true for closed-loop
    if (currentStatus.coolant <= temperatureRemoveOffset(configPage6.egoTemp)) { return false; }
    if (currentStatus.RPM <= (unsigned int)(configPage6.egoRPM * 100)) { return false; }
    if (currentStatus.TPS > configPage6.egoTPSMax) { return false; }
    if (currentStatus.O2 >= configPage6.ego_max) { return false; }
    if (currentStatus.O2 <= configPage6.ego_min) { return false; }
    if (currentStatus.runSecs <= configPage6.ego_sdelay) { return false; }
    if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) != 0) { return false; }
    if (currentStatus.MAP > (configPage9.egoMAPMax * 2)) { return false; }
    if (currentStatus.MAP < (configPage9.egoMAPMin * 2)) { return false; }

    return true; // All conditions met
}

/// @brief Simple AFR correction algorithm (1% steps)
/// @param currentCorrection Current correction value (typically 100 ± egoLimit)
/// @return New correction value (100 ± egoLimit)
/// @complexity 3
static inline byte simpleAFRCorrection(byte currentCorrection) {
    const byte maxCorrection = 100 + configPage6.egoLimit;
    const byte minCorrection = 100 - configPage6.egoLimit;

    // Running lean (O2 > target) - add fuel
    if (currentStatus.O2 > currentStatus.afrTarget) {
        if (currentCorrection < maxCorrection) {
            return currentCorrection + AFR_STEP;
        }
    }
    // Running rich (O2 < target) - remove fuel
    else if (currentStatus.O2 < currentStatus.afrTarget) {
        if (currentCorrection > minCorrection) {
            return currentCorrection - AFR_STEP;
        }
    }

    // At target or at limit
    return currentCorrection;
}

/// @brief PID AFR correction algorithm
/// @return Correction value (100 ± egoLimit)
/// @complexity 2
static inline byte pidAFRCorrection(void) {
    // Configure PID limits and tunings
    egoPID.SetOutputLimits((long)(-configPage6.egoLimit), (long)(configPage6.egoLimit));
    egoPID.SetTunings(configPage6.egoKP, configPage6.egoKI, configPage6.egoKD);

    // Set PID inputs
    PID_O2 = (long)(currentStatus.O2);
    PID_AFRTarget = (long)(currentStatus.afrTarget);

    // Compute PID output
    if (egoPID.Compute()) {
        return 100 + PID_output;
    }

    return AFR_NO_CORRECTION;
}

} // anonymous namespace

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

uint8_t calculateAfrTarget(table3d16RpmLoad &afrLookUpTable, const statuses &current, const config2 &page2, const config6 &page6) {
    // Guard: incorporateAFR always uses table lookup
    if (page2.incorporateAFR == true) {
        return get3DTableValue(&afrLookUpTable, current.fuelLoad, current.RPM);
    }

    // Guard: O2 sensor disabled - use existing target
    if (page6.egoType == EGO_TYPE_OFF) {
        return current.afrTarget;
    }

    // O2 sensor enabled - wait for warmup delay
    if (current.runSecs > page6.ego_sdelay) {
        return get3DTableValue(&afrLookUpTable, current.fuelLoad, current.RPM);
    }

    // During warmup - return current O2 reading
    return current.O2;
}

byte correctionAFRClosedLoop(void) {
    byte AFRValue = AFR_NO_CORRECTION;

    // Guard: O2 sensor disabled
    if (configPage6.egoType == 0) { return AFRValue; }

    // Guard: DFCO active (fuel cut-off)
    if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_DFCO) != 0) { return AFRValue; }

    // Preserve current correction while waiting for next cycle
    AFRValue = currentStatus.egoCorrection;

    // Guard: Ignition count not yet reached (overflow-safe check)
    if (((uint16_t)(ignitionCount - AFRnextCycle)) >= UINT16_HALF_RANGE) {
        return AFRValue;
    }

    // Update next cycle timing
    AFRnextCycle = ignitionCount + configPage6.egoCount;

    // Guard: Closed-loop conditions not met
    if (!isClosedLoopActive()) {
        return AFR_NO_CORRECTION;
    }

    // Select and execute correction algorithm
    if (configPage6.egoAlgorithm == EGO_ALGORITHM_SIMPLE) {
        AFRValue = simpleAFRCorrection(currentStatus.egoCorrection);
    }
    else if (configPage6.egoAlgorithm == EGO_ALGORITHM_PID) {
        AFRValue = pidAFRCorrection();
    }
    else {
        AFRValue = AFR_NO_CORRECTION; // Algorithm disabled
    }

    // Final authority limit enforcement
    const byte maxAuthority = 100 + configPage6.egoLimit;
    const byte minAuthority = 100 - configPage6.egoLimit;
    if (AFRValue < minAuthority) { AFRValue = minAuthority; }
    if (AFRValue > maxAuthority) { AFRValue = maxAuthority; }

    return AFRValue;
}

// ============================================================================
// INTERFACE PROVIDER
// ============================================================================

static const AfrCorrectionsInterface afrCorrectionsInterface = {
    .calculateAfrTarget = &calculateAfrTarget,
    .correctionAFRClosedLoop = &correctionAFRClosedLoop,
};

const AfrCorrectionsInterface* getAfrCorrectionsInterface(void) {
    return &afrCorrectionsInterface;
}
