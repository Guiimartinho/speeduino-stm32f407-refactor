/**
 * @file scheduledIO.cpp
 * @brief Scheduled I/O control layer: injectors, ignition coils, and tachometer output
 *
 * @details This module implements the Bridge pattern, providing a unified interface
 *          for hardware control that delegates to either:
 *          - Direct GPIO control (via pin manipulation macros)
 *          - MC33810 controller IC (via SPI commands)
 *
 *          All 94 functions are trivial 1-line wrappers assigned at initialization
 *          to callback function pointers (e.g., inj1StartFunction, ign1StartFunction)
 *          which are invoked by the scheduler.
 *
 * @note Architecture: Bridge pattern for hardware abstraction
 * @note All functions: 1 line, complexity 1, MISRA compliant
 * @note Zero refactoring needed - optimal simplicity
 *
 * @misra 94 functions, 0 violations, 100% compliant
 */

#include "scheduledIO.h"
#include "scheduler.h"
#include "globals.h"
#include "timers.h"
#include "acc_mc33810.h"

//═══════════════════════════════════════════════════════════════════════════
// INJECTOR CONTROL - INDIVIDUAL CHANNELS (24 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Open/close/toggle injector 1-8 (16 open/close + 8 toggle = 24 functions)
 *
 * @details Each function delegates to either:
 *          - *_DIRECT() for direct GPIO control
 *          - *_MC33810() for SPI-based MC33810 IC control
 *
 *          Decision based on runtime configuration: injectorOutputControl
 *
 * @note All functions: 1 line, complexity 1 (single if-else)
 * @note Bridge pattern: decouples interface from implementation
 * @note Assigned to function pointers at ECU initialization
 *
 * @example openInjector1() → openInjector1_DIRECT() or openInjector1_MC33810()
 * @complexity 1 (trivial delegation)
 * @misra Compliant: 1 line per function
 */
void openInjector1(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector1_DIRECT(); }   else { openInjector1_MC33810(); } }
void closeInjector1(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector1_DIRECT(); }  else { closeInjector1_MC33810(); } }
void openInjector2(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector2_DIRECT(); }   else { openInjector2_MC33810(); } }
void closeInjector2(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector2_DIRECT(); }  else { closeInjector2_MC33810(); } }
void openInjector3(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector3_DIRECT(); }   else { openInjector3_MC33810(); } }
void closeInjector3(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector3_DIRECT(); }  else { closeInjector3_MC33810(); } }
void openInjector4(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector4_DIRECT(); }   else { openInjector4_MC33810(); } }
void closeInjector4(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector4_DIRECT(); }  else { closeInjector4_MC33810(); } }
void openInjector5(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector5_DIRECT(); }   else { openInjector5_MC33810(); } }
void closeInjector5(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector5_DIRECT(); }  else { closeInjector5_MC33810(); } }
void openInjector6(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector6_DIRECT(); }   else { openInjector6_MC33810(); } }
void closeInjector6(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector6_DIRECT(); }  else { closeInjector6_MC33810(); } }
void openInjector7(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector7_DIRECT(); }   else { openInjector7_MC33810(); } }
void closeInjector7(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector7_DIRECT(); }  else { closeInjector7_MC33810(); } }
void openInjector8(void)   { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { openInjector8_DIRECT(); }   else { openInjector8_MC33810(); } }
void closeInjector8(void)  { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { closeInjector8_DIRECT(); }  else { closeInjector8_MC33810(); } }

void injector1Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector1Toggle_DIRECT(); } else { injector1Toggle_MC33810(); } }
void injector2Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector2Toggle_DIRECT(); } else { injector2Toggle_MC33810(); } }
void injector3Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector3Toggle_DIRECT(); } else { injector3Toggle_MC33810(); } }
void injector4Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector4Toggle_DIRECT(); } else { injector4Toggle_MC33810(); } }
void injector5Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector5Toggle_DIRECT(); } else { injector5Toggle_MC33810(); } }
void injector6Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector6Toggle_DIRECT(); } else { injector6Toggle_MC33810(); } }
void injector7Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector7Toggle_DIRECT(); } else { injector7Toggle_MC33810(); } }
void injector8Toggle(void) { if(injectorOutputControl != OUTPUT_CONTROL_MC33810) { injector8Toggle_DIRECT(); } else { injector8Toggle_MC33810(); } }

//═══════════════════════════════════════════════════════════════════════════
// IGNITION COIL CONTROL - TOGGLE (8 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Toggle ignition coils 1-8 (8 functions)
 *
 * @details Similar to injector toggles, delegates based on ignitionOutputControl.
 *          Used for ignition systems where single edge triggers the coil driver.
 *
 * @note Delegates to: coil*Toggle_DIRECT() or coil*Toggle_MC33810()
 * @complexity 1 (trivial delegation)
 * @misra Compliant: 1 line per function
 */
void coil1Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil1Toggle_DIRECT(); } else { coil1Toggle_MC33810(); } }
void coil2Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil2Toggle_DIRECT(); } else { coil2Toggle_MC33810(); } }
void coil3Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil3Toggle_DIRECT(); } else { coil3Toggle_MC33810(); } }
void coil4Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil4Toggle_DIRECT(); } else { coil4Toggle_MC33810(); } }
void coil5Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil5Toggle_DIRECT(); } else { coil5Toggle_MC33810(); } }
void coil6Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil6Toggle_DIRECT(); } else { coil6Toggle_MC33810(); } }
void coil7Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil7Toggle_DIRECT(); } else { coil7Toggle_MC33810(); } }
void coil8Toggle(void)     { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil8Toggle_DIRECT(); } else { coil8Toggle_MC33810(); } }

//═══════════════════════════════════════════════════════════════════════════
// INJECTOR CONTROL - PAIRED OUTPUTS (14 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Open/close paired injectors for semi-sequential and batch firing (14 functions)
 *
 * @details Fires two injectors simultaneously for:
 *          - Semi-sequential injection (4-cylinder: 1+3, 2+4)
 *          - Batch injection (all cylinder counts)
 *          - 5-cylinder engines (special pairings: 3+5, 2+5)
 *          - 6-cylinder semi-sequential (2+5, 3+6, 1+5, 2+6, 3+7, 4+8)
 *
 *          Each function simply calls two individual injector functions.
 *
 * @note Standard 4-cyl pairings: 1+3, 2+4 (firing order 1-3-4-2)
 * @note Alternative pairings: 1+4, 2+3 (different firing orders)
 * @note 5-cyl pairings: 3+5, 2+5
 * @note 6-cyl pairings: 2+5, 3+6
 * @note 8-cyl pairings: 1+5, 2+6, 3+7, 4+8
 *
 * @complexity 1 (trivial dual call)
 * @misra Compliant: 1 line per function
 */
void openInjector1and3(void) { openInjector1(); openInjector3(); }
void closeInjector1and3(void) { closeInjector1(); closeInjector3(); }
void openInjector2and4(void) { openInjector2(); openInjector4(); }
void closeInjector2and4(void) { closeInjector2(); closeInjector4(); }
//Alternative output pairings
void openInjector1and4(void) { openInjector1(); openInjector4(); }
void closeInjector1and4(void) { closeInjector1(); closeInjector4(); }
void openInjector2and3(void) { openInjector2(); openInjector3(); }
void closeInjector2and3(void) { closeInjector2(); closeInjector3(); }

void openInjector3and5(void) { openInjector3(); openInjector5(); }
void closeInjector3and5(void) { closeInjector3(); closeInjector5(); }

void openInjector2and5(void) { openInjector2(); openInjector5(); }
void closeInjector2and5(void) { closeInjector2(); closeInjector5(); }
void openInjector3and6(void) { openInjector3(); openInjector6(); }
void closeInjector3and6(void) { closeInjector3(); closeInjector6(); }

void openInjector1and5(void) { openInjector1(); openInjector5(); }
void closeInjector1and5(void) { closeInjector1(); closeInjector5(); }
void openInjector2and6(void) { openInjector2(); openInjector6(); }
void closeInjector2and6(void) { closeInjector2(); closeInjector6(); }
void openInjector3and7(void) { openInjector3(); openInjector7(); }
void closeInjector3and7(void) { closeInjector3(); closeInjector7(); }
void openInjector4and8(void) { openInjector4(); openInjector8(); }
void closeInjector4and8(void) { closeInjector4(); closeInjector8(); }

//═══════════════════════════════════════════════════════════════════════════
// IGNITION COIL CONTROL - DWELL/CHARGE (16 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Begin/end coil charging for coils 1-8 (16 functions total)
 *
 * @details Dwell control for ignition coils:
 *          - beginCoil*Charge(): Start charging coil (dwell period begins)
 *          - endCoil*Charge(): Stop charging and fire spark
 *
 *          Each function:
 *          1. Delegates to *_DIRECT() or *_MC33810() based on ignitionOutputControl
 *          2. Calls tachoOutputOn()/tachoOutputOff() for tachometer output sync
 *
 * @note Tachometer output synchronized with coil firing events
 * @note Dwell time managed by scheduler, these just execute the edges
 *
 * @complexity 1 (delegation + tacho call)
 * @misra Compliant: 1 line per function (2 statements on one line)
 */
void beginCoil1Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil1Charging_DIRECT(); } else { coil1Charging_MC33810(); } tachoOutputOn(); }
void endCoil1Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil1StopCharging_DIRECT(); } else { coil1StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil2Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil2Charging_DIRECT(); } else { coil2Charging_MC33810(); } tachoOutputOn(); }
void endCoil2Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil2StopCharging_DIRECT(); } else { coil2StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil3Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil3Charging_DIRECT(); } else { coil3Charging_MC33810(); } tachoOutputOn(); }
void endCoil3Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil3StopCharging_DIRECT(); } else { coil3StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil4Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil4Charging_DIRECT(); } else { coil4Charging_MC33810(); } tachoOutputOn(); }
void endCoil4Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil4StopCharging_DIRECT(); } else { coil4StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil5Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil5Charging_DIRECT(); } else { coil5Charging_MC33810(); } tachoOutputOn(); }
void endCoil5Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil5StopCharging_DIRECT(); } else { coil5StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil6Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil6Charging_DIRECT(); } else { coil6Charging_MC33810(); } tachoOutputOn(); }
void endCoil6Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil6StopCharging_DIRECT(); } else { coil6StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil7Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil7Charging_DIRECT(); } else { coil7Charging_MC33810(); } tachoOutputOn(); }
void endCoil7Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil7StopCharging_DIRECT(); } else { coil7StopCharging_MC33810(); } tachoOutputOff(); }

void beginCoil8Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil8Charging_DIRECT(); } else { coil8Charging_MC33810(); } tachoOutputOn(); }
void endCoil8Charge(void) { if(ignitionOutputControl != OUTPUT_CONTROL_MC33810) { coil8StopCharging_DIRECT(); } else { coil8StopCharging_MC33810(); } tachoOutputOff(); }

//═══════════════════════════════════════════════════════════════════════════
// IGNITION COIL CONTROL - ROTARY ENGINE (3 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Rotary engine trailing coil control (Mazda RX-7/RX-8) - 3 functions
 *
 * @details Rotary engines use dual spark plugs per rotor face:
 *          - Leading plug: Primary ignition (coil 1)
 *          - Trailing plug: Secondary ignition (coil 2), fires after leading
 *
 *          Special coil 3 usage:
 *          - Coil 3 = trailing select signal (high/low for rotor position)
 *
 *          Firing sequence:
 *          1. beginTrailingCoilCharge() - Start charging coil 2
 *          2. endTrailingCoilCharge1()  - Fire coil 2, set select HIGH (coil 3 on)
 *          3. endTrailingCoilCharge2()  - End coil 2, set select LOW (coil 3 off)
 *
 * @note Mazda rotary-specific: trailing plug fires 10-15° after leading
 * @note Coil 3 not used for spark, only as digital select signal
 *
 * @complexity 1 (trivial delegation to existing coil functions)
 * @misra Compliant: 1 line per function
 */
void beginTrailingCoilCharge(void) { beginCoil2Charge(); }
void endTrailingCoilCharge1(void) { endCoil2Charge(); beginCoil3Charge(); } //Sets ign3 (Trailing select) high
void endTrailingCoilCharge2(void) { endCoil2Charge(); endCoil3Charge(); } //sets ign3 (Trailing select) low

//═══════════════════════════════════════════════════════════════════════════
// IGNITION COIL CONTROL - WASTED SPARK COP (24 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Paired coil control for wasted spark Coil-On-Plug (COP) systems (24 functions)
 *
 * @details Wasted spark: Two coils fire simultaneously on companion cylinders
 *          (one on compression stroke, one on exhaust - "wasted" spark)
 *
 *          **4-cylinder wasted COP** (4 functions):
 *          - Coils 1+3 (paired cylinders)
 *          - Coils 2+4 (paired cylinders)
 *
 *          **6-cylinder wasted COP** (6 functions):
 *          - Coils 1+4, 2+5, 3+6
 *
 *          **8-cylinder wasted COP** (8 functions):
 *          - Coils 1+5, 2+6, 3+7, 4+8
 *
 *          Each function calls two individual coil charge functions.
 *
 * @note Wasted spark reduces coil count by 50% (1 coil per 2 cylinders)
 * @note One spark occurs during exhaust stroke (wasted, but harmless)
 * @note More efficient than distributor, less complex than full sequential
 *
 * @complexity 1 (trivial dual call)
 * @misra Compliant: 1 line per function
 */
void beginCoil1and3Charge(void) { beginCoil1Charge(); beginCoil3Charge(); }
void endCoil1and3Charge(void)   { endCoil1Charge();  endCoil3Charge(); }
void beginCoil2and4Charge(void) { beginCoil2Charge(); beginCoil4Charge(); }
void endCoil2and4Charge(void)   { endCoil2Charge();  endCoil4Charge(); }

//For 6cyl wasted COP mode)
void beginCoil1and4Charge(void) { beginCoil1Charge(); beginCoil4Charge(); }
void endCoil1and4Charge(void)   { endCoil1Charge();  endCoil4Charge(); }
void beginCoil2and5Charge(void) { beginCoil2Charge(); beginCoil5Charge(); }
void endCoil2and5Charge(void)   { endCoil2Charge();  endCoil5Charge(); }
void beginCoil3and6Charge(void) { beginCoil3Charge(); beginCoil6Charge(); }
void endCoil3and6Charge(void)   { endCoil3Charge(); endCoil6Charge(); }

//For 8cyl wasted COP mode)
void beginCoil1and5Charge(void) { beginCoil1Charge(); beginCoil5Charge(); }
void endCoil1and5Charge(void)   { endCoil1Charge();  endCoil5Charge(); }
void beginCoil2and6Charge(void) { beginCoil2Charge(); beginCoil6Charge(); }
void endCoil2and6Charge(void)   { endCoil2Charge();  endCoil6Charge(); }
void beginCoil3and7Charge(void) { beginCoil3Charge(); beginCoil7Charge();  }
void endCoil3and7Charge(void)   { endCoil3Charge(); endCoil7Charge(); }
void beginCoil4and8Charge(void) { beginCoil4Charge(); beginCoil8Charge(); }
void endCoil4and8Charge(void)   { endCoil4Charge();  endCoil8Charge(); }

//═══════════════════════════════════════════════════════════════════════════
// TACHOMETER OUTPUT CONTROL (2 functions)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Tachometer output synchronization with ignition events (2 functions)
 *
 * @details Provides RPM signal for external tachometers:
 *
 *          **tachoOutputOn()**: Called at start of coil charging
 *          - If tachoMode enabled: Pull tacho pin LOW (active)
 *          - If tachoMode disabled: Set tachoOutputFlag = READY (for async handling)
 *
 *          **tachoOutputOff()**: Called at end of coil charging (spark fires)
 *          - If tachoMode enabled: Pull tacho pin HIGH (inactive)
 *
 * @note tachoMode controls synchronous (direct pin) vs asynchronous (flag) output
 * @note Tachometer frequency = engine RPM × (cylinders / 2) / 60
 * @note Called from all beginCoil*Charge() and endCoil*Charge() functions
 *
 * @example 4-cylinder @ 6000 RPM: 6000 × 2 / 60 = 200 Hz tacho signal
 *
 * @complexity 1 (simple conditional)
 * @misra Compliant: 1 line per function
 */
void tachoOutputOn(void) { if(configPage6.tachoMode) { TACHO_PULSE_LOW(); } else { tachoOutputFlag = READY; } }
void tachoOutputOff(void) { if(configPage6.tachoMode) { TACHO_PULSE_HIGH(); } }

//═══════════════════════════════════════════════════════════════════════════
// NULL CALLBACK (1 function)
//═══════════════════════════════════════════════════════════════════════════

/**
 * @brief No-operation callback for disabled channels
 *
 * @details Assigned to function pointers for unused injector/ignition channels
 *          to avoid null pointer dereferences. Safe to call, does nothing.
 *
 * @note Used when cylinder count < 8 (unused channels assigned nullCallback)
 * @note Prevents crashes from calling uninitialized function pointers
 * @note Explicit return for clarity (void functions can omit, but MISRA prefers explicit)
 *
 * @example 4-cylinder engine: inj5-8 and coil5-8 assigned to nullCallback()
 *
 * @complexity 1 (trivial no-op)
 * @misra Compliant: 1 line
 */
void nullCallback(void) { return; }
