/**
 * @file nitrous_control.cpp
 * @brief Nitrous oxide injection control implementation
 */

#include "nitrous_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"

namespace speeduino {
namespace nitrous {

bool initialise(void) {
    if (configPage10.n2o_minTPS == 255U) {
        configPage10.n2o_enable = 0U;
    }

    if (configPage10.n2o_enable > 0U) {
        n2o_stage1_pin_port = portOutputRegister(digitalPinToPort(configPage10.n2o_stage1_pin));
        n2o_stage1_pin_mask = digitalPinToBitMask(configPage10.n2o_stage1_pin);
        n2o_stage2_pin_port = portOutputRegister(digitalPinToPort(configPage10.n2o_stage2_pin));
        n2o_stage2_pin_mask = digitalPinToBitMask(configPage10.n2o_stage2_pin);
        n2o_arming_pin_port = portInputRegister(digitalPinToPort(configPage10.n2o_arming_pin));
        n2o_arming_pin_mask = digitalPinToBitMask(configPage10.n2o_arming_pin);

        if (configPage10.n2o_pin_polarity == 1U) {
            pinMode(configPage10.n2o_arming_pin, INPUT_PULLUP);
        } else {
            pinMode(configPage10.n2o_arming_pin, INPUT);
        }
    }

    currentStatus.nitrous_status = NITROUS_OFF;
    return true;
}

void update(void) {
    bool nitrousOn = false;

    if (configPage10.n2o_enable == 0U) {
        disable();
        return;
    }

    bool isArmed = READ_N2O_ARM_PIN();
    if (configPage10.n2o_pin_polarity == 1U) {
        isArmed = !isArmed;
    }

    if (!isArmed ||
        currentStatus.coolant <= temperatureRemoveOffset(configPage10.n2o_minCLT) ||
        currentStatus.TPS <= configPage10.n2o_minTPS ||
        currentStatus.O2 >= configPage10.n2o_maxAFR ||
        currentStatus.MAP >= ((uint16_t)configPage10.n2o_maxMAP * 2U)) {
        disable();
        return;
    }

    const uint16_t stage1MinRPM = (uint16_t)configPage10.n2o_stage1_minRPM * 100U;
    const uint16_t stage1MaxRPM = (uint16_t)configPage10.n2o_stage1_maxRPM * 100U;
    const uint16_t stage2MinRPM = (uint16_t)configPage10.n2o_stage2_minRPM * 100U;
    const uint16_t stage2MaxRPM = (uint16_t)configPage10.n2o_stage2_maxRPM * 100U;

    currentStatus.nitrous_status = NITROUS_OFF;

    if ((currentStatus.RPM > stage1MinRPM) && (currentStatus.RPM < stage1MaxRPM)) {
        currentStatus.nitrous_status += NITROUS_STAGE1;
        BIT_SET(currentStatus.status3, BIT_STATUS3_NITROUS);
        N2O_STAGE1_PIN_HIGH();
        nitrousOn = true;
    }

    if (configPage10.n2o_enable == NITROUS_STAGE2) {
        if ((currentStatus.RPM > stage2MinRPM) && (currentStatus.RPM < stage2MaxRPM)) {
            currentStatus.nitrous_status += NITROUS_STAGE2;
            BIT_SET(currentStatus.status3, BIT_STATUS3_NITROUS);
            N2O_STAGE2_PIN_HIGH();
            nitrousOn = true;
        }
    }

    if (!nitrousOn) {
        disable();
    }
}

void disable(void) {
    currentStatus.nitrous_status = NITROUS_OFF;
    BIT_CLEAR(currentStatus.status3, BIT_STATUS3_NITROUS);

    if (configPage10.n2o_enable > 0U) {
        N2O_STAGE1_PIN_LOW();
        N2O_STAGE2_PIN_LOW();
    }
}

bool isActive(void) {
    return (currentStatus.nitrous_status != NITROUS_OFF);
}

uint8_t getStatus(void) {
    return currentStatus.nitrous_status;
}

} // namespace nitrous
} // namespace speeduino
