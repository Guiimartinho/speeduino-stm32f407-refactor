/**
 * @file decoder_interface.h
 * @brief Common interface for all trigger decoders
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * MODULARIZED from decoders.cpp (6242 lines)
 * Original code: 28 decoders in single file
 * Modular: Function pointer table + separate implementation files
 *
 * This module defines the standard interface that ALL decoders must implement.
 * Each decoder provides 6 functions for trigger handling and timing.
 *
 * @note CRITICAL: ISR timing-sensitive code - DO NOT add overhead
 * @note Each decoder implementation must preserve 100% original logic
 */

#ifndef DECODER_INTERFACE_H
#define DECODER_INTERFACE_H

#include "../globals.h"
#include <stdint.h>

/**
 * @brief Decoder function pointers - standard interface for all decoders
 *
 * Every decoder must implement these 6 functions:
 * - setup: Initialize decoder state and configuration
 * - primaryISR: Handle primary trigger interrupt (CRITICAL timing)
 * - secondaryISR: Handle secondary trigger interrupt (CRITICAL timing)
 * - thirdISR: Handle third trigger interrupt (CRITICAL timing - rare)
 * - getRPM: Calculate current engine RPM
 * - getCrankAngle: Get current crankshaft angle
 */
typedef struct {
  /**
   * @brief Initialize decoder configuration
   * @details Called once during ECU initialization
   * @post Decoder state ready for operation
   * @note NOT timing-critical (called during init)
   */
  void (*setup)(void);

  /**
   * @brief Primary trigger ISR handler
   * @details CRITICAL: Called from hardware interrupt
   * @warning Must execute in <10µs for reliability
   * @note NO heavy processing - set flags only
   */
  void (*primaryISR)(void);

  /**
   * @brief Secondary trigger ISR handler
   * @details CRITICAL: Called from hardware interrupt
   * @warning Must execute in <10µs for reliability
   * @note Some decoders don't use secondary - can be NULL/empty
   */
  void (*secondaryISR)(void);

  /**
   * @brief Third trigger ISR handler
   * @details CRITICAL: Called from hardware interrupt (rare)
   * @warning Must execute in <10µs for reliability
   * @note Very few decoders use third trigger
   */
  void (*thirdISR)(void);

  /**
   * @brief Calculate engine RPM
   * @return Current RPM (0-65535)
   * @note Called from main loop (NOT ISR)
   * @note May return stale value if engine not running
   */
  uint16_t (*getRPM)(void);

  /**
   * @brief Get current crankshaft angle
   * @return Crank angle in degrees (0-719 for 4-stroke, 0-359 for 2-stroke)
   * @note Called from main loop (NOT ISR)
   * @note Used for injection/ignition timing calculations
   */
  int (*getCrankAngle)(void);

  /**
   * @brief Decoder name for debugging/logging
   * @note NULL-terminated string, max 32 chars
   */
  const char* name;

  /**
   * @brief Decoder ID (matches DECODER_* defines)
   */
  uint8_t decoderID;

} DecoderInterface;

/**
 * @brief Helper function to set end teeth (used by some decoders)
 * @details Some decoders need to calculate end teeth for ignition scheduling
 * @note Not all decoders need this - can be NULL
 */
typedef void (*TriggerSetEndTeethFunc)(void);

#endif // DECODER_INTERFACE_H
