/**
 * @file decoder_coordinator.h
 * @brief Decoder coordinator - main API for decoder management
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module orchestrates decoder selection and execution.
 * Replaces direct function calls with registry-based dispatch.
 *
 * Original API preserved:
 * - Maintains compatibility with existing code
 * - Same function names and signatures
 * - 100% behavioral equivalence
 *
 * @note This is the ONLY module that should be included by init.cpp/speeduino.ino
 */

#ifndef DECODER_COORDINATOR_H
#define DECODER_COORDINATOR_H

#include "../globals.h"

/**
 * @brief Initialize decoder system
 * @details Selects decoder based on configPage4.TrigPattern and calls setup
 * @post Decoder ready for operation
 * @note Called once during ECU initialization
 * @note Thread-safe: Called only from init sequence
 */
void decoderCoordinatorInitialize(void);

/**
 * @brief Primary trigger ISR handler (dispatcher)
 * @details Routes to appropriate decoder's primaryISR
 * @warning CRITICAL: Called from hardware interrupt
 * @warning Must execute in <5µs (dispatcher overhead must be minimal)
 * @note Uses function pointer - overhead is 1-2 CPU cycles
 */
void decoderCoordinatorPrimaryISR(void);

/**
 * @brief Secondary trigger ISR handler (dispatcher)
 * @details Routes to appropriate decoder's secondaryISR
 * @warning CRITICAL: Called from hardware interrupt
 * @warning Must execute in <5µs
 */
void decoderCoordinatorSecondaryISR(void);

/**
 * @brief Third trigger ISR handler (dispatcher)
 * @details Routes to appropriate decoder's thirdISR
 * @warning CRITICAL: Called from hardware interrupt (rarely used)
 * @warning Must execute in <5µs
 */
void decoderCoordinatorThirdISR(void);

/**
 * @brief Get current engine RPM
 * @return RPM value (0-65535)
 * @note Called from main loop (NOT ISR)
 * @note Returns 0 if engine not running or decoder not initialized
 */
uint16_t decoderCoordinatorGetRPM(void);

/**
 * @brief Get current crankshaft angle
 * @return Crank angle in degrees (0-719 for 4-stroke)
 * @note Called from main loop (NOT ISR)
 * @note Used for injection/ignition timing
 */
int decoderCoordinatorGetCrankAngle(void);

/**
 * @brief Get current decoder name (for debugging)
 * @return Decoder name string, or "Unknown" if not initialized
 * @note Useful for diagnostics and logging
 */
const char* decoderCoordinatorGetName(void);

/**
 * @brief Check if decoder is initialized
 * @return true if decoder ready, false otherwise
 * @note Used to prevent ISR calls before initialization complete
 */
bool decoderCoordinatorIsInitialized(void);

#endif // DECODER_COORDINATOR_H
