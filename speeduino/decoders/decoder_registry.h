/**
 * @file decoder_registry.h
 * @brief Decoder registry interface - table-driven decoder selection
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * Replaces giant switch-case with O(1) table lookup
 * Original: switch(decoderType) with 28 cases
 * Modular: Direct table access by decoder ID
 *
 * @note Thread-safe: Registry is const after initialization
 */

#ifndef DECODER_REGISTRY_H
#define DECODER_REGISTRY_H

#include "decoder_interface.h"

/**
 * @brief Get decoder interface by decoder ID
 * @param decoderID Decoder type (DECODER_MISSING_TOOTH, DECODER_DUAL_WHEEL, etc.)
 * @return Pointer to decoder interface, or NULL if invalid
 * @note O(1) table lookup - very fast
 * @note Returns NULL for invalid decoder ID
 */
const DecoderInterface* decoderRegistryGet(uint8_t decoderID);

/**
 * @brief Get default decoder (Missing Tooth)
 * @return Pointer to default decoder interface
 * @note Never returns NULL
 * @note Used as fallback when decoder ID is invalid
 */
const DecoderInterface* decoderRegistryGetDefault(void);

/**
 * @brief Get total number of registered decoders
 * @return Number of decoders in registry (should be 28)
 * @note Useful for validation and debugging
 */
uint8_t decoderRegistryGetCount(void);

/**
 * @brief Validate decoder ID
 * @param decoderID Decoder type to validate
 * @return true if valid decoder ID, false otherwise
 * @note Use this before calling decoderRegistryGet()
 */
bool decoderRegistryIsValid(uint8_t decoderID);

#endif // DECODER_REGISTRY_H
