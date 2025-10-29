/**
 * @file table_coordinator.h
 * @brief Table Coordinator - unified API for table access
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module provides a unified interface to all table operations.
 * Original implementations remain 100% preserved in table2d/table3d files.
 *
 * DESIGN PHILOSOPHY:
 * - NO wrappers for template functions (zero overhead requirement)
 * - Direct use of original functions (no performance penalty)
 * - Documentation layer only
 * - Future RTOS migration path
 *
 * PERFORMANCE:
 * - All critical functions remain inline templates
 * - Cache hit: <5µs
 * - 2D lookup: <20µs
 * - 3D lookup: <50µs
 *
 * @note This is a DOCUMENTATION module, not a runtime wrapper
 * @note All table code remains in original files (100% preserved)
 */

#ifndef TABLE_COORDINATOR_H
#define TABLE_COORDINATOR_H

#include "table_interface.h"
#include "../table2d.h"
#include "../table3d.h"
#include "../table3d_interpolate.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// Original functions (no wrappers - use directly)
// These are documented here for reference but remain in original files

// ============================================================================
// TABLE 2D API (from table2d.h/cpp)
// ============================================================================

/**
 * @brief Get interpolated value from 2D table
 * @note TEMPLATE INLINE - Use original function directly
 * @note Location: table2d.h
 *
 * Example usage:
 * @code
 *   table2D_u8_u8_10 myTable(&axisArray, &valueArray);
 *   uint8_t result = table2D_getValue(&myTable, lookupValue);
 * @endcode
 *
 * @tparam axis_t Axis type (uint8_t, uint16_t, int8_t, int16_t)
 * @tparam value_t Value type (uint8_t, uint16_t, int8_t, int16_t)
 * @tparam sizeT Table size (4, 6, 8, 10, 32)
 * @param fromTable Pointer to table structure
 * @param axisValue Axis value to lookup
 * @return Interpolated value
 *
 * Performance: <20µs typical, <5µs on cache hit
 */
// template <typename axis_t, typename value_t, uint8_t sizeT>
// value_t table2D_getValue(const table2D<axis_t, value_t, sizeT> *fromTable, axis_t axisValue);
// --> USE ORIGINAL IN table2d.h

/**
 * @brief Get cache expiration time
 * @note Location: table2d.cpp
 * @return Current cache time (seconds)
 */
uint8_t tableCoordinatorGetCacheTime(void);

// ============================================================================
// TABLE 3D API (from table3d.h/cpp + table3d_interpolate.h/cpp)
// ============================================================================

/**
 * @brief Get interpolated value from 3D table
 * @note TEMPLATE INLINE - Use original function directly
 * @note Location: table3d_interpolate.h
 *
 * Example usage:
 * @code
 *   table3d16RpmLoad myTable;  // 16x16 table, RPM x Load
 *   uint8_t result = get3DTableValue(&myTable, rpmValue, loadValue);
 * @endcode
 *
 * @tparam xFactor X-axis compression factor (1, 2, 100)
 * @tparam yFactor Y-axis compression factor (1, 2, 100)
 * @param pValueCache Pointer to cache structure
 * @param axisSize Table axis size (4, 6, 8, 16)
 * @param pValues Pointer to value array
 * @param pXAxis Pointer to X-axis array
 * @param pYAxis Pointer to Y-axis array
 * @param lookupValues X and Y lookup values
 * @return Interpolated value
 *
 * Performance: <50µs typical, <5µs on cache hit
 */
// template <uint16_t xFactor, uint16_t yFactor>
// table3d_value_t get3DTableValue(...);
// --> USE ORIGINAL IN table3d_interpolate.h

/**
 * @brief Find axis bin that contains value
 * @note Location: table3d_interpolate.cpp
 * @param value Value to search for
 * @param pAxis Pointer to axis array (ordered max to min)
 * @param length Axis length
 * @param lastBinMax Last bin index (for cache)
 * @return Upper bin index
 *
 * Performance: <10µs typical, <2µs on cache hit
 */
uint8_t tableCoordinatorFindBinMax(uint8_t value,
                                    const uint8_t *pAxis,
                                    uint8_t length,
                                    uint8_t lastBinMax);

/**
 * @brief Interpolate value from 4 corners (bilinear)
 * @note Location: table3d_interpolate.cpp
 * @param lookUpValues X and Y lookup values
 * @param upperBinIndices X and Y axis bin indices
 * @param axisSize Axis size
 * @param pValues Pointer to value array
 * @param pXAxis Pointer to X-axis
 * @param xMultiplier X-axis multiplier
 * @param pYAxis Pointer to Y-axis
 * @param yMultiplier Y-axis multiplier
 * @return Interpolated value
 *
 * Performance: <30µs typical
 */
uint8_t tableCoordinatorInterpolate3D(uint16_t xValue,
                                       uint16_t yValue,
                                       uint8_t xBinIndex,
                                       uint8_t yBinIndex,
                                       uint8_t axisSize,
                                       const uint8_t *pValues,
                                       const uint8_t *pXAxis,
                                       uint16_t xMultiplier,
                                       const uint8_t *pYAxis,
                                       uint16_t yMultiplier);

// ============================================================================
// TABLE ITERATORS (from table3d.cpp)
// ============================================================================

/**
 * @brief Get iterator to start of table rows
 * @note Location: table3d.cpp
 * @param pTable Pointer to table
 * @param key Table type key
 * @return Iterator to first row
 */
// table_value_iterator rows_begin(void *pTable, table_type_t key);
// --> USE ORIGINAL IN table3d.cpp

/**
 * @brief Get iterator to start of X-axis
 * @note Location: table3d.cpp
 * @param pTable Pointer to table
 * @param key Table type key
 * @return Iterator to X-axis start
 */
// table_axis_iterator x_begin(void *pTable, table_type_t key);
// --> USE ORIGINAL IN table3d.cpp

/**
 * @brief Get reverse iterator to X-axis
 * @note Location: table3d.cpp
 * @param pTable Pointer to table
 * @param key Table type key
 * @return Reverse iterator to X-axis
 */
// table_axis_iterator x_rbegin(void *pTable, table_type_t key);
// --> USE ORIGINAL IN table3d.cpp

/**
 * @brief Get iterator to start of Y-axis
 * @note Location: table3d.cpp
 * @param pTable Pointer to table
 * @param key Table type key
 * @return Iterator to Y-axis start
 */
// table_axis_iterator y_begin(void *pTable, table_type_t key);
// --> USE ORIGINAL IN table3d.cpp

/**
 * @brief Get reverse iterator to Y-axis
 * @note Location: table3d.cpp
 * @param pTable Pointer to table
 * @param key Table type key
 * @return Reverse iterator to Y-axis
 */
// table_axis_iterator y_rbegin(void *pTable, table_type_t key);
// --> USE ORIGINAL IN table3d.cpp

// ============================================================================
// CACHE MANAGEMENT
// ============================================================================

/**
 * @brief Invalidate 3D table cache
 * @note Location: table3d_interpolate.h (inline)
 * @param pCache Pointer to cache structure
 */
// void invalidate_cache(table3DGetValueCache *pCache);
// --> USE ORIGINAL IN table3d_interpolate.h

// ============================================================================
// UTILITY API
// ============================================================================

/**
 * @brief Check if table coordinator is initialized
 * @return true if initialized
 */
bool tableCoordinatorIsInitialized(void);

/**
 * @brief Get table coordinator name (for debugging)
 * @return "Table Coordinator"
 */
const char* tableCoordinatorGetName(void);

// ============================================================================
// USAGE EXAMPLES
// ============================================================================

/**
 * @example Using 2D tables
 * @code
 *   // Define table (typically in globals)
 *   uint8_t cltAxisArray[10] = {0, 20, 40, 60, 80, 100, 120, 140, 160, 180};
 *   uint8_t cltValueArray[10] = {150, 145, 140, 130, 120, 110, 105, 102, 100, 100};
 *   table2D_u8_u8_10 cltCalibrationTable(&cltAxisArray, &cltValueArray);
 *
 *   // Lookup value
 *   uint8_t temperature = 75;  // degrees C
 *   uint8_t correction = table2D_getValue(&cltCalibrationTable, temperature);
 *   // Returns: 125 (interpolated between 120 and 130)
 * @endcode
 */

/**
 * @example Using 3D tables
 * @code
 *   // Define table (typically in globals)
 *   table3d16RpmLoad veTable;  // 16x16 VE table
 *
 *   // Lookup value
 *   uint16_t rpm = 2500;   // RPM
 *   uint16_t load = 80;    // Load %
 *   uint8_t ve = get3DTableValue(&veTable, rpm, load);
 *   // Returns: VE value interpolated from 4 nearest cells
 * @endcode
 */

/**
 * @example Cache invalidation
 * @code
 *   // After tuning change via serial
 *   invalidate_cache(&veTable.get_value_cache);
 *   // Forces fresh lookup on next access
 * @endcode
 */

#endif // TABLE_COORDINATOR_H
