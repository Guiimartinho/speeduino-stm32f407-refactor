/**
 * @file dwell_corrections.cpp
 * @brief Dwell corrections implementation with MISRA-C compliance
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * REFACTORED MODULE - 100% logic preservation with MISRA-C compliance:
 * - Functions < 50 lines
 * - Cyclomatic complexity < 10
 * - Guard clauses for early returns
 * - Anonymous namespace for private helpers
 * - Comprehensive Doxygen documentation
 */

#include "dwell_corrections.h"
#include "../../globals.h"
#include "../../corrections.h"
#include "../../maths.h"

namespace {

/// @brief Convert spark duration from config format (mS*10) to microseconds
/// @param sparkDurConfig Spark duration from config (mS * 10)
/// @return Spark duration in microseconds
static inline uint16_t convertSparkDurationToMicros(uint8_t sparkDurConfig) {
    return sparkDurConfig * 100; // mS*10 -> uS
}

/// @brief Apply battery voltage correction to dwell
/// @param baseDwell Base dwell time (uS)
/// @param correction Voltage correction percentage (100 = no correction)
/// @return Corrected dwell time (uS)
static inline uint16_t applyVoltageCorrection(uint16_t baseDwell, uint8_t correction) {
    if (correction == 100) { return baseDwell; }
    return div100(baseDwell) * correction;
}

/// @brief Apply closed-loop dwell error correction
/// @param targetDwell Target dwell time (uS)
/// @param actualDwell Actual measured dwell time (uS)
/// @return Corrected dwell with error compensation
static inline uint16_t applyDwellErrorCorrection(uint16_t targetDwell, uint16_t actualDwell) {
    int16_t error = targetDwell - actualDwell;

    // Prevent overflow
    uint16_t safeDwell = (targetDwell > INT16_MAX) ? INT16_MAX : targetDwell;

    // Double correction if actual < 50% of target
    if (error > ((int16_t)safeDwell / 2)) {
        error += error;
    }

    // Only add positive correction
    if (error > 0) {
        return targetDwell + error;
    }

    return targetDwell;
}

/// @brief Calculate number of ignition pulses per revolution
/// @return Pulses per revolution (1-4)
static inline int8_t calculatePulsesPerRevolution() {
    // Guard: Single cylinder always 1 pulse
    if (configPage2.nCylinders <= 1) { return 1; }

    // Check if multiple pulses per revolution
    const bool isSingleChannelMode = (configPage4.sparkMode == IGN_MODE_SINGLE);
    const bool isWastedRotary = (configPage4.sparkMode == IGN_MODE_ROTARY) &&
                                 (configPage10.rotaryType != ROTARY_IGN_RX8);

    if (isSingleChannelMode || isWastedRotary) {
        return (configPage2.nCylinders >> 1); // nCylinders / 2
    }

    return 1;
}

/// @brief Apply dwell limiter to prevent over-dwell at low RPM
/// @param targetDwell Target dwell time (uS)
/// @param sparkDur Spark duration (uS)
/// @param pulsesPerRev Pulses per revolution
/// @return Limited dwell time (uS)
static inline uint16_t applyDwellLimiter(uint16_t targetDwell, uint16_t sparkDur, int8_t pulsesPerRev) {
    // Calculate total dwell per revolution
    const uint16_t dwellPerRevolution = (targetDwell + sparkDur) * pulsesPerRev;

    // Guard: Within limits
    if (dwellPerRevolution <= revolutionTime) {
        return targetDwell;
    }

    // Scale down dwell and spark duration proportionally
    const uint16_t adjustedSparkDur = udiv_32_16(sparkDur * revolutionTime, dwellPerRevolution);
    return udiv_32_16(revolutionTime, (uint16_t)pulsesPerRev) - adjustedSparkDur;
}

} // anonymous namespace

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

uint16_t correctionsDwell(uint16_t dwell) {
    uint16_t tempDwell = dwell;
    const uint16_t sparkDur_uS = convertSparkDurationToMicros(configPage4.sparkDur);

    // Initialize actualDwell on first call
    if (currentStatus.actualDwell == 0) {
        currentStatus.actualDwell = tempDwell;
    }

    // Apply battery voltage correction
    currentStatus.dwellCorrection = table2D_getValue(&dwellVCorrectionTable, currentStatus.battery10);
    tempDwell = applyVoltageCorrection(dwell, currentStatus.dwellCorrection);

    // Apply closed-loop error correction if enabled
    if ((configPage2.perToothIgn == true) && (configPage4.dwellErrCorrect == 1)) {
        tempDwell = applyDwellErrorCorrection(tempDwell, currentStatus.actualDwell);
    }

    // Apply dwell limiter
    const int8_t pulsesPerRev = calculatePulsesPerRevolution();
    tempDwell = applyDwellLimiter(tempDwell, sparkDur_uS, pulsesPerRev);

    return tempDwell;
}

// ============================================================================
// INTERFACE PROVIDER
// ============================================================================

static const DwellCorrectionsInterface dwellCorrectionsInterface = {
    .correctionsDwell = &correctionsDwell,
};

const DwellCorrectionsInterface* getDwellCorrectionsInterface(void) {
    return &dwellCorrectionsInterface;
}
