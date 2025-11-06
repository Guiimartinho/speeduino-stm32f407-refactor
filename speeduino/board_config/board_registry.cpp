/**
 * @file board_registry.cpp
 * @brief Board configuration registry implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Table-driven board selection - replaces switch-case
 */

#include "board_registry.h"
#include "pin_mapping/stm32f407_pins.h"
#include "pin_mapping/stm32f407_scg_ecu_pins.h"

// Anonymous namespace for private data
namespace {

/**
 * @brief Board configuration registry table
 * @note To add new board: Add entry here + create board-specific file
 *
 * Example:
 * {BOARD_ID_ARDUINO_MEGA, "Arduino Mega 2560", &arduinoMegaConfigurePins},
 */
static const BoardRegistryEntry boardRegistry[] = {
  // STM32F407 Black F407VE (Generic board)
  {60U, "STM32F407 Black F407VE", &stm32f407ConfigurePins},

  // SCG-ECU 2.0 - Custom STM32F407VGT6 8x8 board
  {61U, "SCG-ECU 2.0 STM32F407", &stm32f407ScgEcuConfigurePins},

  // Add additional boards here following same pattern
  // {<board_id>, "<name>", &<config_function>},
};

static const size_t boardRegistrySize = sizeof(boardRegistry) / sizeof(BoardRegistryEntry);

} // anonymous namespace

BoardConfigFunc boardRegistryGetConfig(byte boardID)
{
  // Linear search through registry
  // Note: For 60+ boards, could optimize to hash table or binary search
  // Current implementation is O(n) but registry is small (~5-10 entries)
  for (size_t i = 0; i < boardRegistrySize; ++i) {
    if (boardRegistry[i].boardID == boardID) {
      return boardRegistry[i].configureFunc;
    }
  }

  // Board not found
  return NULL;
}

BoardConfigFunc boardRegistryGetDefault(void)
{
  // Default to SCG-ECU 2.0 (board ID 61)
  return &stm32f407ScgEcuConfigurePins;
}
