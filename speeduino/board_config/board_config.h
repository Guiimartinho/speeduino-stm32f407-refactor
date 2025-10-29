/**
 * @file board_config.h
 * @brief Board configuration interface for Speeduino ECU
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Modularized from init.cpp (setPinMapping function)
 * Original: 1853 lines in single function
 * Modular: Table-driven architecture with board-specific modules
 *
 * MISRA C:2012 Compliance:
 * - Guard clauses for validation
 * - Named constants instead of magic numbers
 * - Input validation on all parameters
 * - Single Responsibility Principle
 *
 * Architecture:
 * - board_registry.cpp: Table-driven board selection
 * - pin_mapping/: Pin configuration modules per board
 * - initialization/: Initialization sequence modules
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "../globals.h"

/**
 * @brief Board ID constants
 * @note These match TunerStudio speeduino.ini pinLayout values
 */
#define BOARD_ID_STM32F407_BLACK_F407VE     60U
#define BOARD_ID_DEFAULT_STM32F407          255U

/**
 * @brief Configure pin mapping based on board ID
 * @param boardID Board identifier from configuration (0-255)
 * @details This is the main entry point for board configuration.
 *          Replaces the giant switch-case from original setPinMapping()
 *
 * @note PRESERVES 100% of original logic from init.cpp setPinMapping()
 */
void boardConfigSetPinMapping(byte boardID);

/**
 * @brief Configure pin modes for all outputs
 * @details Sets pinMode() for all configured pins
 * @note Called after pin mapping is set
 */
void boardConfigSetupPinModes(void);

/**
 * @brief Configure port/mask pointers for direct GPIO access
 * @details Sets up fast port manipulation variables
 * @note Called after pinMode configuration
 */
void boardConfigSetupPortPointers(void);

/**
 * @brief Configure programmable/selectable pins
 * @details Handles pins that can be configured by user in TunerStudio
 * @note Called after base pin mapping
 */
void boardConfigSetupProgrammablePins(void);

#endif // BOARD_CONFIG_H
