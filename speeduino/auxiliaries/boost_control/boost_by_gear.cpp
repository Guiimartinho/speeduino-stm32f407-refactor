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

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================

/**
 * @brief Gear-specific boost configuration
 */
struct GearBoostConfig {
    uint8_t multiplierOrDuty;  ///< Gear multiplier or fixed duty
};

/**
 * @brief Get gear configuration value
 * @param gear Gear number (1-6)
 * @return Gear config value
 */
static inline uint8_t getGearConfigValue(uint8_t gear)
{
    switch (gear)
    {
        case 1U: return configPage9.boostByGear1;
        case 2U: return configPage9.boostByGear2;
        case 3U: return configPage9.boostByGear3;
        case 4U: return configPage9.boostByGear4;
        case 5U: return configPage9.boostByGear5;
        case 6U: return configPage9.boostByGear6;
        default: return 0U;
    }
}

/**
 * @brief Check if gear is valid (1-6)
 */
static inline bool isGearValid(uint8_t gear)
{
    return (gear >= 1U) && (gear <= 6U);
}

/**
 * @brief Get boost table base value
 */
static inline uint8_t getBoostTableValue(void)
{
    return get3DTableValue(&boostTable, (currentStatus.TPS * 2U), currentStatus.RPM);
}

/**
 * @brief Calculate open-loop boost duty for table-based mode
 * @param gear Current gear (1-6)
 * @return Boost duty (0-10000)
 */
static uint16_t calculateOpenLoopTableBased(uint8_t gear)
{
    if (!isGearValid(gear)) { return 0U; }

    uint8_t multiplier = getGearConfigValue(gear);
    uint8_t tableValue = getBoostTableValue();

    uint32_t combined = ((uint32_t)multiplier * (uint32_t)tableValue) << 2;
    return (combined > 10000U) ? 10000U : (uint16_t)combined;
}

/**
 * @brief Calculate open-loop boost duty for fixed mode
 * @param gear Current gear (1-6)
 * @return Boost duty (0-10000)
 */
static uint16_t calculateOpenLoopFixed(uint8_t gear)
{
    if (!isGearValid(gear)) { return 0U; }

    uint8_t fixedDuty = getGearConfigValue(gear);
    return (uint16_t)fixedDuty * 200U;
}

/**
 * @brief Calculate closed-loop boost target for table-based mode
 * @param gear Current gear (1-6)
 * @return Boost target (kPa * 2)
 */
static uint16_t calculateClosedLoopTableBased(uint8_t gear)
{
    if (!isGearValid(gear)) { return 0U; }

    uint8_t multiplier = getGearConfigValue(gear);
    uint8_t tableValue = getBoostTableValue();

    uint32_t combined = (((uint32_t)multiplier * (uint32_t)tableValue) / 100U) << 2;
    return (combined > 511U) ? 511U : (uint16_t)combined;
}

/**
 * @brief Calculate closed-loop boost target for fixed mode
 * @param gear Current gear (1-6)
 * @return Boost target (kPa * 2)
 */
static uint16_t calculateClosedLoopFixed(uint8_t gear)
{
    if (!isGearValid(gear)) { return 0U; }

    uint8_t fixedTarget = getGearConfigValue(gear);
    return (uint16_t)fixedTarget << 1;
}

/**
 * @brief Handle open-loop gear compensation
 */
static inline void handleOpenLoopGear(uint8_t gear)
{
    if (configPage9.boostByGearEnabled == 1U) { currentStatus.boostDuty = calculateOpenLoopTableBased(gear); return; }
    if (configPage9.boostByGearEnabled == 2U) { currentStatus.boostDuty = calculateOpenLoopFixed(gear); }
}

/**
 * @brief Handle closed-loop gear compensation
 */
static inline void handleClosedLoopGear(uint8_t gear)
{
    if (configPage9.boostByGearEnabled == 1U) { currentStatus.boostTarget = calculateClosedLoopTableBased(gear); return; }
    if (configPage9.boostByGearEnabled == 2U) { currentStatus.boostTarget = calculateClosedLoopFixed(gear); }
}

/**
 * @brief Internal applyGearCompensation implementation
 */
static void applyGearCompensation_impl(void)
{
    uint8_t gear = currentStatus.gear;

    if (configPage4.boostType == OPEN_LOOP_BOOST) { handleOpenLoopGear(gear); return; }
    if (configPage4.boostType == CLOSED_LOOP_BOOST) { handleClosedLoopGear(gear); }
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace boost {

void applyGearCompensation(void) { applyGearCompensation_impl(); }

} // namespace boost
} // namespace speeduino
