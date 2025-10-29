/**
 * @file wmi_control.cpp
 * @brief Water-Methanol Injection implementation
 */

#include "wmi_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"
#include "../../table3d.h"

namespace speeduino {
namespace wmi {

extern uint16_t vvt_pwm_max_count;
extern long vvt2_pwm_value;
extern volatile bool vvt2_pwm_state;
extern volatile bool vvt2_max_pwm;

bool initialise(void) {
    currentStatus.wmiPW = 0U;
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);
    return true;
}

void update(void) {
    int wmiPW = 0;

    if ((configPage10.vvt2Enabled != 0U) || (configPage10.wmiEnabled == 0U)) {
        return;
    }

    if (WMI_TANK_IS_EMPTY()) {
        BIT_SET(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);
        currentStatus.wmiPW = 0U;
        VVT2_PIN_LOW();
        vvt2_pwm_state = false;
        vvt2_max_pwm = false;
        if (configPage6.vvtEnabled == 0U) {
            DISABLE_VVT_TIMER();
        }
        digitalWrite(pinWMIEnabled, LOW);
        return;
    }

    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);

    if ((currentStatus.TPS < configPage10.wmiTPS) ||
        (currentStatus.RPMdiv100 < configPage10.wmiRPM) ||
        ((currentStatus.MAP / 2) < configPage10.wmiMAP) ||
        (temperatureAddOffset(currentStatus.IAT) < configPage10.wmiIAT)) {
        wmiPW = 0;
    } else {
        switch (configPage10.wmiMode) {
            case WMI_MODE_SIMPLE:
                wmiPW = 200;
                break;
            case WMI_MODE_PROPORTIONAL:
                wmiPW = map(currentStatus.MAP / 2, configPage10.wmiMAP, configPage10.wmiMAP2, 0, 200);
                break;
            case WMI_MODE_OPENLOOP:
                wmiPW = get3DTableValue(&wmiTable, (uint16_t)currentStatus.MAP, currentStatus.RPM);
                break;
            case WMI_MODE_CLOSEDLOOP:
                wmiPW = max(0, ((int)currentStatus.PW1 + configPage10.wmiOffset)) *
                        get3DTableValue(&wmiTable, (uint16_t)currentStatus.MAP, currentStatus.RPM) / 200;
                break;
            default:
                wmiPW = 0;
                break;
        }

        if (wmiPW > 200) {
            wmiPW = 200;
        }
    }

    currentStatus.wmiPW = wmiPW;
    vvt2_pwm_value = halfPercentage(currentStatus.wmiPW, vvt_pwm_max_count);

    if (wmiPW == 0) {
        VVT2_PIN_LOW();
        vvt2_pwm_state = false;
        vvt2_max_pwm = false;
        if (configPage6.vvtEnabled == 0U) {
            DISABLE_VVT_TIMER();
        }
        digitalWrite(pinWMIEnabled, LOW);
    } else {
        digitalWrite(pinWMIEnabled, HIGH);
        if (wmiPW >= 200) {
            VVT2_PIN_HIGH();
            vvt2_pwm_state = true;
            vvt2_max_pwm = true;
            if (configPage6.vvtEnabled == 0U) {
                DISABLE_VVT_TIMER();
            }
        } else {
            vvt2_max_pwm = false;
            ENABLE_VVT_TIMER();
        }
    }
}

void disable(void) {
    currentStatus.wmiPW = 0U;
    VVT2_PIN_LOW();
    vvt2_pwm_state = false;
    vvt2_max_pwm = false;
    digitalWrite(pinWMIEnabled, LOW);
}

uint8_t getPW(void) {
    return currentStatus.wmiPW;
}

bool isTankEmpty(void) {
    return BIT_CHECK(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);
}

} // namespace wmi
} // namespace speeduino
