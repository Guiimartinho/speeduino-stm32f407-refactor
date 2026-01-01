/**
 * @file wmi_control.cpp
 * @brief Water-Methanol Injection implementation
 */

#include "wmi_control.h"
#include "../../globals.h"
#include "../../auxiliaries.h"
#include "../../units.h"
#include "../../table3d.h"

// =============================================================================
// PRIVATE IMPLEMENTATION - Static helpers (file scope)
// =============================================================================

/**
 * @brief Check if WMI conditions are not met
 */
static inline bool wmiConditionsNotMet(void)
{
    if (currentStatus.TPS < configPage10.wmiTPS) { return true; }
    if (currentStatus.RPMdiv100 < configPage10.wmiRPM) { return true; }
    if ((currentStatus.MAP / 2) < configPage10.wmiMAP) { return true; }
    if (temperatureAddOffset(currentStatus.IAT) < configPage10.wmiIAT) { return true; }
    return false;
}

/**
 * @brief Calculate WMI PW for simple mode
 */
static inline int calculateSimpleMode(void)
{
    return 200;
}

/**
 * @brief Calculate WMI PW for proportional mode
 */
static inline int calculateProportionalMode(void)
{
    return map(currentStatus.MAP / 2, configPage10.wmiMAP, configPage10.wmiMAP2, 0, 200);
}

/**
 * @brief Calculate WMI PW for open loop mode
 */
static inline int calculateOpenLoopMode(void)
{
    return get3DTableValue(&wmiTable, (uint16_t)currentStatus.MAP, currentStatus.RPM);
}

/**
 * @brief Calculate WMI PW for closed loop mode
 */
static inline int calculateClosedLoopMode(void)
{
    int basePW = max(0, ((int)currentStatus.PW1 + configPage10.wmiOffset));
    int tableFactor = get3DTableValue(&wmiTable, (uint16_t)currentStatus.MAP, currentStatus.RPM);
    return (basePW * tableFactor) / 200;
}

/**
 * @brief Calculate WMI PW based on mode
 */
static int calculateWMIPW(void)
{
    if (wmiConditionsNotMet()) { return 0; }

    int wmiPW = 0;
    switch (configPage10.wmiMode)
    {
        case WMI_MODE_SIMPLE: wmiPW = calculateSimpleMode(); break;
        case WMI_MODE_PROPORTIONAL: wmiPW = calculateProportionalMode(); break;
        case WMI_MODE_OPENLOOP: wmiPW = calculateOpenLoopMode(); break;
        case WMI_MODE_CLOSEDLOOP: wmiPW = calculateClosedLoopMode(); break;
        default: wmiPW = 0; break;
    }

    return (wmiPW > 200) ? 200 : wmiPW;
}

/**
 * @brief Disable VVT timer if VVT is not enabled
 */
static inline void disableVVTTimerIfNeeded(void)
{
    if (configPage6.vvtEnabled == 0U) { DISABLE_VVT_TIMER(); }
}

/**
 * @brief Disable WMI output
 */
static void disableWMIOutput(void)
{
    VVT2_PIN_LOW();
    vvt2_pwm_state = false;
    vvt2_max_pwm = false;
    disableVVTTimerIfNeeded();
    digitalWrite(pinWMIEnabled, LOW);
}

/**
 * @brief Handle tank empty condition
 */
static void handleTankEmpty(void)
{
    BIT_SET(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);
    currentStatus.wmiPW = 0U;
    disableWMIOutput();
}

/**
 * @brief Handle max WMI output (100%)
 */
static inline void handleMaxOutput(void)
{
    digitalWrite(pinWMIEnabled, HIGH);
    VVT2_PIN_HIGH();
    vvt2_pwm_state = true;
    vvt2_max_pwm = true;
    disableVVTTimerIfNeeded();
}

/**
 * @brief Handle partial WMI output (PWM)
 */
static inline void handlePartialOutput(void)
{
    digitalWrite(pinWMIEnabled, HIGH);
    vvt2_max_pwm = false;
    ENABLE_VVT_TIMER();
}

/**
 * @brief Apply WMI output
 */
static void applyWMIOutput(int wmiPW)
{
    currentStatus.wmiPW = wmiPW;
    vvt2_pwm_value = halfPercentage(currentStatus.wmiPW, vvt_pwm_max_count);

    if (wmiPW == 0) { disableWMIOutput(); return; }
    if (wmiPW >= 200) { handleMaxOutput(); return; }
    handlePartialOutput();
}

/**
 * @brief Internal initialise implementation
 */
static bool wmi_initialise_impl(void)
{
    currentStatus.wmiPW = 0U;
    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);
    return true;
}

/**
 * @brief Internal update implementation
 */
static void wmi_update_impl(void)
{
    bool wmiDisabled = (configPage10.vvt2Enabled != 0U) || (configPage10.wmiEnabled == 0U);
    if (wmiDisabled) { return; }

    if (WMI_TANK_IS_EMPTY()) { handleTankEmpty(); return; }

    BIT_CLEAR(currentStatus.status4, BIT_STATUS4_WMI_EMPTY);

    int wmiPW = calculateWMIPW();
    applyWMIOutput(wmiPW);
}

/**
 * @brief Internal disable implementation
 */
static void wmi_disable_impl(void)
{
    currentStatus.wmiPW = 0U;
    VVT2_PIN_LOW();
    vvt2_pwm_state = false;
    vvt2_max_pwm = false;
    digitalWrite(pinWMIEnabled, LOW);
}

// =============================================================================
// PUBLIC API - Namespace wrapper functions
// =============================================================================

namespace speeduino {
namespace wmi {

bool initialise(void) { return wmi_initialise_impl(); }
void update(void) { wmi_update_impl(); }
void disable(void) { wmi_disable_impl(); }
uint8_t getPW(void) { return currentStatus.wmiPW; }
bool isTankEmpty(void) { return BIT_CHECK(currentStatus.status4, BIT_STATUS4_WMI_EMPTY); }

} // namespace wmi
} // namespace speeduino
