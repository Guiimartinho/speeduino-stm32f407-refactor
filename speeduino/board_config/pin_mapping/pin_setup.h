/**
 * @file pin_setup.h
 * @brief Pin mode and port pointer configuration
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Extracted from init.cpp setPinMapping() lines 2769-3068
 *
 * Functions:
 * - Configure programmable/selectable pins
 * - Set pinMode() for all outputs
 * - Setup port/mask pointers for direct GPIO access
 */

#ifndef PIN_SETUP_H
#define PIN_SETUP_H

#include "../../globals.h"

/**
 * @brief Configure user-programmable pins from TunerStudio
 * @details Handles pins that can be configured by user:
 *          - Launch, ignition bypass, tacho, fuel pump, fan
 *          - Boost, VVT1/2, baro, EMAP
 *          - Fuel/spark inputs, VSS
 *          - Fuel/oil pressure
 *          - WMI pins
 *          - Idle up, CTPS
 *          - A/C control pins
 *
 * @note PRESERVES 100% original logic from init.cpp lines 2771-2806
 */
void pinSetupConfigureProgrammablePins(void);

/**
 * @brief Configure reset control pin (special case)
 * @details Must set initial state BEFORE pinMode to avoid reset loop
 * @note PRESERVES 100% original logic from init.cpp lines 2808-2819
 */
void pinSetupConfigureResetControl(void);

/**
 * @brief Set pinMode for all output pins
 * @details Configures direction (INPUT/OUTPUT) for all pins
 * @note PRESERVES 100% original logic from init.cpp lines 2822-2857
 */
void pinSetupConfigurePinModes(void);

/**
 * @brief Setup port/mask pointers for ignition outputs
 * @details Fast GPIO access via direct port manipulation
 * @note PRESERVES 100% original logic from init.cpp lines 2859-2875
 */
void pinSetupConfigureIgnitionPorts(void);

/**
 * @brief Setup port/mask pointers for injector outputs
 * @details Fast GPIO access via direct port manipulation
 * @note PRESERVES 100% original logic from init.cpp lines 2877-2925
 */
void pinSetupConfigureInjectorPorts(void);

/**
 * @brief Apply legacy MAP reading mode if configured
 * @details Reverts MAP behavior to pre-201905 firmware
 * @note PRESERVES 100% original logic from init.cpp lines 2837-2838
 */
void pinSetupApplyLegacyMAP(void);

#endif // PIN_SETUP_H
