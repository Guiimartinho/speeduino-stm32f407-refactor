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
 */

#ifndef MODULARIZATION_GLOBALS_H
#define MODULARIZATION_GLOBALS_H

#include <Arduino.h>
#include "table2d.h"

//=============================================================================
// Tables
//=============================================================================

extern table2D_i8_u8_4 rollingCutTable; /**< Rolling cut percentage table based on RPM delta */

//=============================================================================
// Channel enable/disable bitmasks
//=============================================================================

extern uint8_t ignitionChannelsOn; /**< Bitmask of which ignition channels are currently enabled */
extern uint8_t fuelChannelsOn; /**< Bitmask of which fuel channels are currently enabled */
extern uint8_t ignitionChannelsPending; /**< Bitmask of ignition channels waiting to fire after fuel returns */

//=============================================================================
// Injector timing angles (degrees)
//=============================================================================

extern int injector1StartAngle;
extern int injector2StartAngle;
extern int injector3StartAngle;
extern int injector4StartAngle;
#if INJ_CHANNELS >= 5
extern int injector5StartAngle;
#endif
#if INJ_CHANNELS >= 6
extern int injector6StartAngle;
#endif
#if INJ_CHANNELS >= 7
extern int injector7StartAngle;
#endif
#if INJ_CHANNELS >= 8
extern int injector8StartAngle;
#endif

//=============================================================================
// Engine protection variables
//=============================================================================

extern uint32_t revLimitAllowedEndTime; /**< Time when rev limiter hysteresis expires */
extern uint16_t rollingCutLastRev; /**< Last revolution when rolling cut was applied */

//=============================================================================
// Timing and synchronization
//=============================================================================

extern uint32_t deferEEPROMWritesUntil; /**< Time until which EEPROM writes are deferred */
extern uint16_t AFRnextCycle; /**< AFR next cycle counter */

//=============================================================================
// Serial communication status
//=============================================================================

extern uint8_t serialStatusFlag; /**< Current serial communication status */

// Serial status values
#define SERIAL_INACTIVE 0
#define SERIAL_TRANSMIT 1
#define SERIAL_RECEIVE  2

#endif // MODULARIZATION_GLOBALS_H
