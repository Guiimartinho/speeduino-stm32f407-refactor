/**
 * @file table_interface.h
 * @brief Table Access Interface - common structures for table operations
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module defines the common interface for table access operations.
 * The actual table implementations remain in table2d.h/table3d.h (100% preserved).
 *
 * ARCHITECTURE:
 * - Template-based tables (2D and 3D)
 * - Aggressive caching for performance
 * - Fixed-point math (no floating point)
 * - Header-only templates for zero overhead
 *
 * PERFORMANCE REQUIREMENTS:
 * - 2D interpolation: <20µs
 * - 3D interpolation: <50µs
 * - Cache hit: <5µs
 *
 * @note Original table code 100% preserved in:
 *       - table2d.h/cpp
 *       - table3d.h/cpp
 *       - table3d_interpolate.h/cpp
 */

#ifndef TABLE_INTERFACE_H
#define TABLE_INTERFACE_H

#include "../globals.h"
#include <stdint.h>

// ============================================================================
// TABLE TYPES
// ============================================================================

/**
 * @brief Table dimension types
 */
enum TableDimension {
  TABLE_1D = 1,  // Single axis (e.g., CLT calibration)
  TABLE_2D = 2,  // Two axes (e.g., VE table: RPM x Load)
  TABLE_3D = 3   // Future: 3D tables if needed
};

/**
 * @brief Table axis domains (compression factors)
 * @note Axes are compressed to save SRAM:
 *       - RPM stored /100 (2500 RPM -> 25)
 *       - Load stored as-is (0-255%)
 */
enum AxisDomain {
  AXIS_DOMAIN_NONE = 1,     // No compression
  AXIS_DOMAIN_RPM = 100,    // Divide by 100
  AXIS_DOMAIN_LOAD = 2      // Divide by 2
};

/**
 * @brief Table value types
 */
enum TableValueType {
  VALUE_TYPE_U8,    // uint8_t (0-255)
  VALUE_TYPE_U16,   // uint16_t (0-65535)
  VALUE_TYPE_S16,   // int16_t (-32768 to 32767)
  VALUE_TYPE_S8     // int8_t (-128 to 127)
};

/**
 * @brief Table sizes (all tables are square for 3D)
 */
enum TableSize {
  TABLE_SIZE_4 = 4,
  TABLE_SIZE_6 = 6,
  TABLE_SIZE_8 = 8,
  TABLE_SIZE_10 = 10,
  TABLE_SIZE_16 = 16,
  TABLE_SIZE_32 = 32
};

// ============================================================================
// TABLE ACCESS MODES
// ============================================================================

/**
 * @brief Table access modes
 */
enum TableAccessMode {
  ACCESS_MODE_READ,     // Read-only (normal operation)
  ACCESS_MODE_WRITE,    // Write (tuning via serial)
  ACCESS_MODE_ATOMIC    // Atomic read (for thread safety)
};

// ============================================================================
// INTERPOLATION TYPES
// ============================================================================

/**
 * @brief Interpolation methods
 */
enum InterpolationType {
  INTERP_NONE,      // No interpolation (nearest neighbor)
  INTERP_LINEAR,    // Linear interpolation (1D)
  INTERP_BILINEAR   // Bilinear interpolation (2D)
};

// ============================================================================
// TABLE STATISTICS (for debugging/diagnostics)
// ============================================================================

/**
 * @brief Table statistics structure
 * @note Used for debugging and performance monitoring
 */
struct TableStatistics {
  uint32_t accessCount;      // Total accesses
  uint32_t cacheHits;        // Cache hits
  uint32_t cacheMisses;      // Cache misses
  uint32_t maxAccessTime;    // Max access time (µs)
  uint32_t avgAccessTime;    // Average access time (µs)
};

// ============================================================================
// CACHE INVALIDATION
// ============================================================================

/**
 * @brief Cache invalidation reasons
 */
enum CacheInvalidationReason {
  CACHE_TIMEOUT,          // Cache expired (tuning change)
  CACHE_FORCED,           // Forced invalidation
  CACHE_VALUE_CHANGED     // Table value was modified
};

// ============================================================================
// DOCUMENTATION OF ORIGINAL FUNCTIONS
// ============================================================================

/**
 * @brief Template functions preserved 100% in original files
 *
 * These functions remain inline templates for performance.
 * NO wrappers created (would add overhead).
 *
 * 2D TABLES (table2d.h):
 * - table2D_getValue<axis_t, value_t, size>() - Linear interpolation
 * - Inline implementation with aggressive caching
 *
 * 3D TABLES (table3d_interpolate.h):
 * - get3DTableValue<xFactor, yFactor>() - Bilinear interpolation
 * - Inline implementation with 2-level caching
 *
 * AUXILIARY FUNCTIONS:
 * - find_bin_max() - Axis bin search with cache
 * - interpolate_3d_value() - Bilinear math
 * - getCacheTime() - Cache expiration
 * - Iterators: x_begin(), y_begin(), rows_begin()
 */

// ============================================================================
// FIXED POINT MATH (used internally)
// ============================================================================

/**
 * @brief Fixed point format: QU1X8 (1 integer bit, 8 fractional bits)
 * @note Range: [0.0, 1.0] with 1/256 precision
 * @note Used for interpolation weights (avoids float)
 */
typedef uint16_t QU1X8_t;

constexpr uint8_t QU1X8_SHIFT = 8;
constexpr QU1X8_t QU1X8_ONE = 1U << QU1X8_SHIFT;  // 256 = 1.0

// ============================================================================
// CONSTANTS
// ============================================================================

/**
 * @brief Table access constants
 */
constexpr uint8_t TABLE_CACHE_TIMEOUT_SECONDS = 2;  // Cache expires after 2s
constexpr uint8_t MAX_TABLE_SIZE = 32;              // Maximum axis size
constexpr uint16_t TABLE_ACCESS_TIMEOUT_US = 100;   // Max access time warning

#endif // TABLE_INTERFACE_H
