/**
 * @file communication_handler.h
 * @brief Communication handling for Serial, CAN, and Secondary Serial
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 * Modularized from speeduino.cpp main loop
 */

#ifndef COMMUNICATION_HANDLER_H
#define COMMUNICATION_HANDLER_H

#include "globals.h"

/**
 * @brief Handle primary serial communication (TunerStudio)
 * Checks for pending transmits and incoming data
 */
void handleSerialComms(void);

/**
 * @brief Handle secondary serial communication
 * Only active if configPage9.enable_secondarySerial is enabled
 */
void handleSecondarySerial(void);

/**
 * @brief Handle CAN bus communication
 * Reads CAN messages and processes commands
 * Only active if NATIVE_CAN_AVAILABLE and configPage9.enable_intcan
 */
void handleCANComms(void);

#endif // COMMUNICATION_HANDLER_H
