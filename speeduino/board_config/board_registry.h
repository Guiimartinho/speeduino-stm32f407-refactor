/**
 * @file board_registry.h
 * @brief Table-driven board configuration registry
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Replaces giant switch-case from init.cpp setPinMapping()
 * Uses function pointer table for O(1) board selection
 *
 * Benefits over switch-case:
 * - Extensible: Add new boards without modifying core logic
 * - Testable: Each board config is independent function
 * - Maintainable: Board-specific code in separate files
 * - Performance: Direct function call vs switch evaluation
 */

#ifndef BOARD_REGISTRY_H
#define BOARD_REGISTRY_H

#include "../globals.h"

/**
 * @brief Board configuration function pointer type
 * @details Each board implements this signature
 */
typedef void (*BoardConfigFunc)(void);

/**
 * @brief Board registry entry
 */
typedef struct {
  byte boardID;                   ///< Board identifier (matches TunerStudio)
  const char* boardName;          ///< Human-readable name
  BoardConfigFunc configureFunc;  ///< Pin configuration function
} BoardRegistryEntry;

/**
 * @brief Get board configuration function by ID
 * @param boardID Board identifier from configuration
 * @return Function pointer to board config, or NULL if not found
 */
BoardConfigFunc boardRegistryGetConfig(byte boardID);

/**
 * @brief Get default board configuration
 * @return Function pointer to default STM32F407 config
 */
BoardConfigFunc boardRegistryGetDefault(void);

#endif // BOARD_REGISTRY_H
