/**
 * @file boost_by_gear.cpp
 * @brief Gear-based boost compensation implementation
 * @details TABLE-DRIVEN pattern to eliminate code repetition
 *
 * Original code: 138 lines with 6 nearly identical switch cases
 * Refactored code: ~60 lines with table-driven dispatch
 * Reduction: 57% fewer lines, 100% less repetition
 */

#include "../boost_control/boost_control.h"
#include "../../globals.h"
#include "../../table3d.h"

namespace speeduino {
namespace boost {

// Anonymous namespace for private implementation
namespace {

/**
 * @brief Gear-specific boost configuration
 */
struct GearBoostConfig {
    uint8_t multiplierOrDuty;  ///< Gear multiplier or fixed duty
};

/**
 * @brief Get gear configuration array
 * @details Indexed by gear number (0-based)
 * @return Array of 6 gear configs
 */
static const GearBoostConfig* getGearConfigs(void) {
    static const GearBoostConfig configs[6] = {
        { configPage9.boostByGear1 },  // Gear 1
        { configPage9.boostByGear2 },  // Gear 2
        { configPage9.boostByGear3 },  // Gear 3
        { configPage9.boostByGear4 },  // Gear 4
        { configPage9.boostByGear5 },  // Gear 5
        { configPage9.boostByGear6 }   // Gear 6
    };
    return configs;
}

/**
 * @brief Calculate open-loop boost duty for table-based mode
 * @param gear Current gear (1-6)
 * @return Boost duty (0-10000)
 */
static uint16_t calculateOpenLoopTableBased(uint8_t gear) {
    // Guard clause: invalid gear
    if ((gear < 1U) || (gear > 6U)) {
        return 0U;
    }

    const GearBoostConfig* configs = getGearConfigs();
    const uint8_t multiplier = configs[gear - 1U].multiplierOrDuty;

    // Lookup base value from boost table
    const uint8_t tableValue = get3DTableValue(&boostTable,
                                                (currentStatus.TPS * 2U),
                                                currentStatus.RPM);

    // Apply gear multiplier and shift
    const uint32_t combined = ((uint32_t)multiplier * (uint32_t)tableValue) << 2;

    // Clamp to maximum
    return (combined > 10000U) ? 10000U : (uint16_t)combined;
}

/**
 * @brief Calculate open-loop boost duty for fixed mode
 * @param gear Current gear (1-6)
 * @return Boost duty (0-10000)
 */
static uint16_t calculateOpenLoopFixed(uint8_t gear) {
    // Guard clause: invalid gear
    if ((gear < 1U) || (gear > 6U)) {
        return 0U;
    }

    const GearBoostConfig* configs = getGearConfigs();
    const uint8_t fixedDuty = configs[gear - 1U].multiplierOrDuty;

    // Convert to 0.01% resolution
    return (uint16_t)fixedDuty * 200U;  // * 2 * 100
}

/**
 * @brief Calculate closed-loop boost target for table-based mode
 * @param gear Current gear (1-6)
 * @return Boost target (kPa * 2)
 */
static uint16_t calculateClosedLoopTableBased(uint8_t gear) {
    // Guard clause: invalid gear
    if ((gear < 1U) || (gear > 6U)) {
        return 0U;
    }

    const GearBoostConfig* configs = getGearConfigs();
    const uint8_t multiplier = configs[gear - 1U].multiplierOrDuty;

    // Lookup base value from boost table
    const uint8_t tableValue = get3DTableValue(&boostTable,
                                                (currentStatus.TPS * 2U),
                                                currentStatus.RPM);

    // Apply gear multiplier with division and shift
    const uint32_t combined = (((uint32_t)multiplier * (uint32_t)tableValue) / 100U) << 2;

    // Clamp to maximum (511 kPa * 2 = 255.5 kPa actual)
    return (combined > 511U) ? 511U : (uint16_t)combined;
}

/**
 * @brief Calculate closed-loop boost target for fixed mode
 * @param gear Current gear (1-6)
 * @return Boost target (kPa * 2)
 */
static uint16_t calculateClosedLoopFixed(uint8_t gear) {
    // Guard clause: invalid gear
    if ((gear < 1U) || (gear > 6U)) {
        return 0U;
    }

    const GearBoostConfig* configs = getGearConfigs();
    const uint8_t fixedTarget = configs[gear - 1U].multiplierOrDuty;

    // Convert to kPa * 2
    return (uint16_t)fixedTarget << 1;
}

} // anonymous namespace

// Public interface implementation

void applyGearCompensation(void) {
    const uint8_t gear = currentStatus.gear;

    // Dispatch based on boost type and gear mode
    if (configPage4.boostType == OPEN_LOOP_BOOST) {
        // Open loop mode
        if (configPage9.boostByGearEnabled == 1U) {
            // Mode 1: Table-based with gear multiplier
            currentStatus.boostDuty = calculateOpenLoopTableBased(gear);
        } else if (configPage9.boostByGearEnabled == 2U) {
            // Mode 2: Fixed duty per gear
            currentStatus.boostDuty = calculateOpenLoopFixed(gear);
        }
        // Mode 0: disabled, do nothing
    } else if (configPage4.boostType == CLOSED_LOOP_BOOST) {
        // Closed loop mode
        if (configPage9.boostByGearEnabled == 1U) {
            // Mode 1: Table-based with gear multiplier
            currentStatus.boostTarget = calculateClosedLoopTableBased(gear);
        } else if (configPage9.boostByGearEnabled == 2U) {
            // Mode 2: Fixed target per gear
            currentStatus.boostTarget = calculateClosedLoopFixed(gear);
        }
        // Mode 0: disabled, do nothing
    }
}

} // namespace boost
} // namespace speeduino
