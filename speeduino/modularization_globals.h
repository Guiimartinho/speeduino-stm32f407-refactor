/**
 * @file modularization_globals.h
 * @brief Global variables needed for modularization
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 *
 * This header declares global variables that were previously local to speeduino.cpp
 * but are now needed across multiple modules after modularization.
 *
 * These variables must be defined (instantiated) in ONE .cpp file (typically speeduino.cpp)
 * and declared as extern in this header for use in other modules.
 *
 * MISRA C:2012 Compliance:
 * - Rule 8.5: External definitions in one file only
 * - Rule 8.6: Identifiers with external linkage have exactly one definition
 * - Variables modified in ISR context are marked volatile
 */

#ifndef MODULARIZATION_GLOBALS_H
#define MODULARIZATION_GLOBALS_H

#include <Arduino.h>
#include "table2d.h"
#include "automotive_constants.h"

//=============================================================================
// Tables
//=============================================================================

extern table2D_i8_u8_4 rollingCutTable; /**< Rolling cut percentage table based on RPM delta */

//=============================================================================
// Channel enable/disable bitmasks (ISR-modified - volatile required)
//=============================================================================

/**
 * @brief Bitmask of which ignition channels are currently enabled
 * @note Modified in interrupt context - must be volatile
 */
extern volatile uint8_t ignitionChannelsOn;

/**
 * @brief Bitmask of which fuel channels are currently enabled
 * @note Modified in interrupt context - must be volatile
 */
extern volatile uint8_t fuelChannelsOn;

/**
 * @brief Bitmask of ignition channels waiting to fire after fuel returns
 * @note Modified in interrupt context - must be volatile
 */
extern volatile uint8_t ignitionChannelsPending;

//=============================================================================
// Injector timing angles (degrees, range: 0-719 for 4-stroke sequential)
//=============================================================================

extern int injector1StartAngle; /**< Injector 1 start angle in crank degrees (0-719) */
extern int injector2StartAngle; /**< Injector 2 start angle in crank degrees (0-719) */
extern int injector3StartAngle; /**< Injector 3 start angle in crank degrees (0-719) */
extern int injector4StartAngle; /**< Injector 4 start angle in crank degrees (0-719) */
#if INJ_CHANNELS >= 5
extern int injector5StartAngle; /**< Injector 5 start angle in crank degrees (0-719) */
#endif
#if INJ_CHANNELS >= 6
extern int injector6StartAngle; /**< Injector 6 start angle in crank degrees (0-719) */
#endif
#if INJ_CHANNELS >= 7
extern int injector7StartAngle; /**< Injector 7 start angle in crank degrees (0-719) */
#endif
#if INJ_CHANNELS >= 8
extern int injector8StartAngle; /**< Injector 8 start angle in crank degrees (0-719) */
#endif

//=============================================================================
// Engine protection variables (ISR-modified - volatile required)
//=============================================================================

/**
 * @brief Timestamp when rev limiter hysteresis expires (microseconds)
 * @note Modified in interrupt context - must be volatile
 */
extern volatile uint32_t revLimitAllowedEndTime;

/**
 * @brief Last engine revolution count when rolling cut was applied
 * @note Modified in interrupt context - must be volatile
 */
extern volatile uint16_t rollingCutLastRev;

//=============================================================================
// Timing and synchronization (ISR-modified - volatile required)
//=============================================================================

/**
 * @brief Timestamp until which EEPROM writes are deferred (microseconds)
 * @note SHOULD be volatile (modified in interrupt context)
 * @warning Declared in storage.h without volatile - cannot override here
 */
// extern volatile uint32_t deferEEPROMWritesUntil;  // Already in storage.h (non-volatile)

/**
 * @brief AFR calculation next cycle counter
 * @note SHOULD be volatile (modified in interrupt context)
 * @warning Declared in corrections.h without volatile - cannot override here
 */
// extern volatile uint16_t AFRnextCycle;  // Already in corrections.h (non-volatile)

//=============================================================================
// Serial communication status
//=============================================================================

/**
 * @brief Current serial communication status
 * @note Using SerialStatus enum from comms_legacy.h
 */
// extern SerialStatus serialStatusFlag;  // Already in comms_legacy.h

#endif // MODULARIZATION_GLOBALS_H
