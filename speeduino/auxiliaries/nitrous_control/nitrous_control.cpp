/**
 * @file nitrous_control.cpp
 * @brief Nitrous oxide injection control implementation
 */

#include "nitrous_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================

/**
 * @brief Setup nitrous pins
 */
static void setupNitrousPins(void)
{
    n2o_stage1_pin_port = portOutputRegister(digitalPinToPort(configPage10.n2o_stage1_pin));
    n2o_stage1_pin_mask = digitalPinToBitMask(configPage10.n2o_stage1_pin);
    n2o_stage2_pin_port = portOutputRegister(digitalPinToPort(configPage10.n2o_stage2_pin));
    n2o_stage2_pin_mask = digitalPinToBitMask(configPage10.n2o_stage2_pin);
    n2o_arming_pin_port = portInputRegister(digitalPinToPort(configPage10.n2o_arming_pin));
    n2o_arming_pin_mask = digitalPinToBitMask(configPage10.n2o_arming_pin);

    uint8_t pinModeVal = (configPage10.n2o_pin_polarity == 1U) ? INPUT_PULLUP : INPUT;
    pinMode(configPage10.n2o_arming_pin, pinModeVal);
}

/**
 * @brief Check if nitrous is armed
 */
static inline bool isNitrousArmed(void)
{
    bool isArmed = READ_N2O_ARM_PIN();
    return (configPage10.n2o_pin_polarity == 1U) ? !isArmed : isArmed;
}

/**
 * @brief Check safety conditions for nitrous
 */
static inline bool safetyConditionsFailed(void)
{
    if (!isNitrousArmed()) { return true; }
    if (currentStatus.coolant <= temperatureRemoveOffset(configPage10.n2o_minCLT)) { return true; }
    if (currentStatus.TPS <= configPage10.n2o_minTPS) { return true; }
    if (currentStatus.O2 >= configPage10.n2o_maxAFR) { return true; }
    if (currentStatus.MAP >= ((uint16_t)configPage10.n2o_maxMAP * 2U)) { return true; }
    return false;
}

/**
 * @brief Check if RPM is in stage 1 range
 */
static inline bool isStage1RPMValid(void)
{
    uint16_t minRPM = (uint16_t)configPage10.n2o_stage1_minRPM * 100U;
    uint16_t maxRPM = (uint16_t)configPage10.n2o_stage1_maxRPM * 100U;
    return (currentStatus.RPM > minRPM) && (currentStatus.RPM < maxRPM);
}

/**
 * @brief Check if RPM is in stage 2 range
 */
static inline bool isStage2RPMValid(void)
{
    uint16_t minRPM = (uint16_t)configPage10.n2o_stage2_minRPM * 100U;
    uint16_t maxRPM = (uint16_t)configPage10.n2o_stage2_maxRPM * 100U;
    return (currentStatus.RPM > minRPM) && (currentStatus.RPM < maxRPM);
}

/**
 * @brief Activate stage 1 nitrous
 */
static inline void activateStage1(void)
{
    currentStatus.nitrous_status += NITROUS_STAGE1;
    BIT_SET(currentStatus.status3, BIT_STATUS3_NITROUS);
    N2O_STAGE1_PIN_HIGH();
}

/**
 * @brief Activate stage 2 nitrous
 */
static inline void activateStage2(void)
{
    currentStatus.nitrous_status += NITROUS_STAGE2;
    BIT_SET(currentStatus.status3, BIT_STATUS3_NITROUS);
    N2O_STAGE2_PIN_HIGH();
}

/**
 * @brief Internal disable implementation
 */
static void nitrous_disable_impl(void)
{
    currentStatus.nitrous_status = NITROUS_OFF;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_NITROUS);

    if (configPage10.n2o_enable > 0U) { N2O_STAGE1_PIN_LOW(); N2O_STAGE2_PIN_LOW(); }
}

/**
 * @brief Internal initialise implementation
 */
static bool nitrous_initialise_impl(void)
{
    if (configPage10.n2o_minTPS == 255U) { configPage10.n2o_enable = 0U; }
    if (configPage10.n2o_enable > 0U) { setupNitrousPins(); }

    currentStatus.nitrous_status = NITROUS_OFF;
    return true;
}

/**
 * @brief Internal update implementation
 */
static void nitrous_update_impl(void)
{
    if (configPage10.n2o_enable == 0U) { nitrous_disable_impl(); return; }
    if (safetyConditionsFailed()) { nitrous_disable_impl(); return; }

    currentStatus.nitrous_status = NITROUS_OFF;
    bool nitrousOn = false;

    if (isStage1RPMValid()) { activateStage1(); nitrousOn = true; }

    bool stage2Enabled = (configPage10.n2o_enable == NITROUS_STAGE2);
    if (stage2Enabled && isStage2RPMValid()) { activateStage2(); nitrousOn = true; }

    if (!nitrousOn) { nitrous_disable_impl(); }
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace nitrous {

bool initialise(void) { return nitrous_initialise_impl(); }
void update(void) { nitrous_update_impl(); }
void disable(void) { nitrous_disable_impl(); }
bool isActive(void) { return (currentStatus.nitrous_status != NITROUS_OFF); }
uint8_t getStatus(void) { return currentStatus.nitrous_status; }

} // namespace nitrous
} // namespace speeduino
