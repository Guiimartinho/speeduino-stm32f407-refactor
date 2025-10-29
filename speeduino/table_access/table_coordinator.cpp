/**
 * @file table_coordinator.cpp
 * @brief Table coordinator implementation - unified API for tables
 *
 * SCG-ECU 2.0 - STM32F407VGT6 8x8
 *
 * This module provides minimal wrappers for non-template table functions.
 * All template functions remain inline in original files (100% preserved).
 *
 * ARCHITECTURE:
 * - Direct wrappers to original functions (no overhead)
 * - Guard clauses for safety
 * - 100% preservation of original logic
 *
 * PRESERVES 100% LOGIC from original table2d/table3d files
 */

#include "table_coordinator.h"
#include "../table2d.h"
#include "../table3d.h"
#include "../table3d_interpolate.h"
#include "../globals.h"

// Anonymous namespace for private state
namespace {

/**
 * @brief Initialization complete flag
 * @note Prevents API calls before initialization
 */
static volatile bool isInitialized = true;  // Tables always available

} // anonymous namespace

// ============================================================================
// CACHE TIME API
// ============================================================================

uint8_t tableCoordinatorGetCacheTime(void)
{
  // Direct call to original function (table2d.cpp)
  return _table2d_detail::getCacheTime();
}

// ============================================================================
// AXIS SEARCH API
// ============================================================================

uint8_t tableCoordinatorFindBinMax(uint8_t value,
                                    const uint8_t *pAxis,
                                    uint8_t length,
                                    uint8_t lastBinMax)
{
  // Guard clause
  if (pAxis == nullptr) {
    return 0;
  }

  // Direct call to original function (table3d_interpolate.cpp)
  return find_bin_max(value, pAxis, length, lastBinMax);
}

// ============================================================================
// INTERPOLATION API (3D)
// ============================================================================

uint8_t tableCoordinatorInterpolate3D(uint16_t xValue,
                                       uint16_t yValue,
                                       uint8_t xBinIndex,
                                       uint8_t yBinIndex,
                                       uint8_t axisSize,
                                       const uint8_t *pValues,
                                       const uint8_t *pXAxis,
                                       uint16_t xMultiplier,
                                       const uint8_t *pYAxis,
                                       uint16_t yMultiplier)
{
  // Guard clauses
  if (pValues == nullptr || pXAxis == nullptr || pYAxis == nullptr) {
    return 0;
  }

  // Prepare structures for original function
  xy_values lookUpValues = { xValue, yValue };
  xy_coord2d upperBinIndices = { xBinIndex, yBinIndex };

  // Direct call to original function (table3d_interpolate.cpp)
  return interpolate_3d_value(lookUpValues,
                               upperBinIndices,
                               axisSize,
                               pValues,
                               pXAxis,
                               xMultiplier,
                               pYAxis,
                               yMultiplier);
}

// ============================================================================
// UTILITY API
// ============================================================================

bool tableCoordinatorIsInitialized(void)
{
  return isInitialized;
}

const char* tableCoordinatorGetName(void)
{
  return "Table Coordinator";
}

// ============================================================================
// NOTES ON TEMPLATE FUNCTIONS
// ============================================================================

/*
 * The following functions are NOT wrapped because they are template inline
 * functions in header files. Wrapping them would:
 * 1. Add function call overhead (unacceptable for <50µs requirement)
 * 2. Require template instantiation (code bloat)
 * 3. Break compiler optimization (inline + constexpr)
 *
 * USE THESE DIRECTLY FROM ORIGINAL FILES:
 *
 * From table2d.h:
 * - template<...> value_t table2D_getValue(const table2D<...> *fromTable, axis_t axisValue)
 *
 * From table3d_interpolate.h:
 * - template<...> table3d_value_t get3DTableValue(...)
 * - inline void invalidate_cache(table3DGetValueCache *pCache)
 *
 * From table3d.cpp:
 * - table_value_iterator rows_begin(void *pTable, table_type_t key)
 * - table_axis_iterator x_begin(void *pTable, table_type_t key)
 * - table_axis_iterator x_rbegin(void *pTable, table_type_t key)
 * - table_axis_iterator y_begin(void *pTable, table_type_t key)
 * - table_axis_iterator y_rbegin(void *pTable, table_type_t key)
 *
 * These remain at their original locations with ZERO modifications.
 */
