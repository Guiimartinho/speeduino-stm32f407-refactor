/**
 * @file sensor_polling.h
 * @brief Sensor polling functions organized by frequency
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 *
 * This module handles all sensor reads at different frequencies using the LOOP_TIMER
 * mechanism. Each function is called when its corresponding timer bit is set.
 *
 * Timing frequencies:
 * - 1 kHz: MAP (every ~1ms)
 * - 200 Hz: ADC interrupt re-enable
 * - 50 Hz: CAN broadcast
 * - 30 Hz: Boost, VVT, WMI, O2, TPS
 * - 15 Hz: TPS alternate, Launch/Flat shift
 * - 10 Hz: Idle, A/C, VSS, Gear, Programmable IO
 * - 4 Hz: CLT, IAT, BAT, Nitrous, Fuel/Oil pressure, Aux inputs
 * - 1 Hz: System temp, Baro, SD sync, WMI indicator
 */

#ifndef SENSOR_POLLING_H
#define SENSOR_POLLING_H

#include "globals.h"

/**
 * @brief Poll sensors at 1 kHz (every ~1ms)
 * Reads MAP sensor for speed density calculations
 */
void pollSensors1KHz(void);

/**
 * @brief Poll sensors at 200 Hz (every 5ms)
 * Re-enables ADC interrupt for free-running mode
 */
void pollSensors200Hz(void);

/**
 * @brief Poll sensors at 50 Hz (every 20ms)
 * Sends CAN broadcast if enabled
 */
void pollSensors50Hz(void);

/**
 * @brief Poll sensors at 30 Hz (every 33ms)
 * Control loops: Boost, VVT, WMI
 * Sensor reads: O2, TPS (if configured)
 * EEPROM writes if pending
 */
void pollSensors30Hz(void);

/**
 * @brief Poll sensors at 15 Hz (every 66ms)
 * TPS read (if configured)
 * Launch control and flat shift checks
 * Tooth log ready check
 */
void pollSensors15Hz(void);

/**
 * @brief Poll sensors at 10 Hz (every 100ms)
 * Idle control
 * Air conditioning control
 * VSS and gear calculation
 * Programmable IO checks
 */
void pollSensors10Hz(void);

/**
 * @brief Poll sensors at 4 Hz (every 250ms)
 * Slow sensor reads: CLT, IAT, BAT
 * Nitrous control
 * Fuel and oil pressure
 * Auxiliary CAN inputs (16 channels)
 * Idle target lookup
 */
void pollSensors4Hz(void);

/**
 * @brief Poll sensors at 1 Hz (every second)
 * System temperature
 * Barometric pressure
 * SD card sync
 * WMI indicator flashing
 */
void pollSensors1Hz(void);

/**
 * @brief Read all 16 auxiliary CAN input channels
 * Called from pollSensors4Hz if auxiliary inputs are enabled
 */
void readAuxiliaryInputs(void);

/**
 * @brief Handle WMI empty indicator flashing
 * Called from pollSensors1Hz if WMI is enabled
 */
void handleWMIIndicator(void);

/**
 * @brief Handle all engine stop procedures
 * Resets all status flags and turns off outputs
 */
void handleEngineStop(void);

#endif // SENSOR_POLLING_H
