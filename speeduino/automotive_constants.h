/**
 * @file automotive_constants.h
 * @brief Automotive ECU constants and configuration limits
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 *
 * This header centralizes all magic numbers and hardcoded values
 * to improve code maintainability and MISRA C:2012 compliance.
 *
 * MISRA C:2012 Compliance:
 * - Rule 2.5: Unused macros are documented
 * - Rule 5.1: External identifiers are unique within 31 characters
 * - Rule 8.14: All constants are defined once
 */

#ifndef AUTOMOTIVE_CONSTANTS_H
#define AUTOMOTIVE_CONSTANTS_H

#include <stdint.h>

//=============================================================================
// Engine Limits
//=============================================================================

/** Maximum safe RPM for any engine configuration */
#define MAX_SAFE_RPM                    20000U

/** Minimum valid RPM for running engine */
#define MIN_RUNNING_RPM                 100U

/** Maximum allowed load value (percentage * 10) */
#define MAX_LOAD_VALUE                  2550U

/** Maximum MAP sensor reading (kPa * 10) */
#define MAX_MAP_VALUE                   2550U

/** Minimum advance value in raw table units */
#define MIN_ADVANCE_RAW                 -400

/** Maximum advance value in raw table units */
#define MAX_ADVANCE_RAW                 900

//=============================================================================
// Fuel System Constants
//=============================================================================

/** Maximum required fuel value (microseconds) */
#define MAX_REQ_FUEL_US                 25500U

/** Percentage base value (100%) */
#define PERCENTAGE_BASE                 100U

/** Stoichiometric AFR * 10 (14.7 * 10) */
#define AFR_STOICH_DEFAULT              147U

/** Acceleration enrichment percentage offset */
#define AE_PERCENTAGE_OFFSET            100U

/** Maximum pulse width in microseconds (65.535ms) */
#define MAX_PULSE_WIDTH_US              UINT16_MAX

/** Minimum injector opening time */
#define MIN_INJECTOR_OPEN_TIME_US       50U

//=============================================================================
// Ignition System Constants
//=============================================================================

/** Ignition table offset value */
#define IGNITION_TABLE_OFFSET           OFFSET_IGNITION

/** Dwell configuration multiplier (ms * 10 -> us) */
#define DWELL_CONFIG_MULTIPLIER         100U

/** Maximum safe dwell time (milliseconds * 10) */
#define MAX_SAFE_DWELL_MS               100U

/** Fixed cranking dwell multiplier */
#define FIXED_CRANKING_DWELL_MULTIPLIER 3U

/** Fixed cranking angle advance (degrees) */
#define FIXED_CRANKING_ANGLE_ADVANCE    5

/** Fixed cranking RPM threshold */
#define FIXED_CRANKING_RPM_THRESHOLD    250U

//=============================================================================
// Engine Protection Constants
//=============================================================================

/** Revolutions per cycle for 4-stroke engine */
#define REVOLUTIONS_PER_CYCLE_4STROKE   2U

/** Minimum revolutions for non-sequential cut */
#define MIN_REVOLUTIONS_NON_SEQUENTIAL  2U

/** Percentage value representing 100% */
#define PERCENTAGE_FULL                 100U

/** Launch control RPM multiplier */
#define LAUNCH_RPM_MULTIPLIER           100U

/** Maximum number of engine channels (injectors or ignition) */
#define MAX_ENGINE_CHANNELS             8U

//=============================================================================
// Sensor Polling Constants
//=============================================================================

/** CAN broadcast frequency - 50Hz */
#define CAN_BROADCAST_50HZ              50U

/** CAN broadcast frequency - 30Hz */
#define CAN_BROADCAST_30HZ              30U

/** CAN broadcast frequency - 15Hz */
#define CAN_BROADCAST_15HZ              15U

/** CAN broadcast frequency - 10Hz */
#define CAN_BROADCAST_10HZ              10U

/** SD card sync minimum RPM threshold - defined in SD_logger.h */
// #define SD_SYNC_RPM_THRESHOLD           2000U  // Use value from SD_logger.h

/** Maximum auxiliary input channels */
#define MAX_AUX_CHANNELS                16U

/** Maximum CAN input channels */
#define MAX_CAN_CHANNELS                16U

/** Temperature range minimum (degrees C + offset) */
#define TEMPERATURE_RANGE_MIN           0

/** Temperature range maximum (degrees C + offset) */
#define TEMPERATURE_RANGE_MAX           250

//=============================================================================
// Communication Constants
//=============================================================================

/** Maximum serial buffer size (bytes) */
#define SERIAL_BUFFER_MAX               512U

/** Serial operation success code */
#define SERIAL_SUCCESS                  0

/** Serial operation error code */
#define SERIAL_ERROR                    -1

//=============================================================================
// Scheduling Constants
//=============================================================================

/** Schedule operation success code */
#define SCHEDULE_SUCCESS                0

/** Schedule operation error code */
#define SCHEDULE_ERROR                  -1

/** Maximum timeout value for scheduling (microseconds) */
#define MAX_TIMEOUT_VALUE               UINT32_MAX

/** Maximum ignition timeout value (microseconds) */
#define MAX_IGNITION_TIMEOUT            100000U

//=============================================================================
// PW Calculation Correction Shift Constants
//=============================================================================

/** Correction shift amount for normal range */
#define CORRECTION_SHIFT_NORMAL         7U

/** Correction shift amount for medium range */
#define CORRECTION_SHIFT_MEDIUM         6U

/** Correction shift amount for large range */
#define CORRECTION_SHIFT_LARGE          5U

/** Correction threshold 1 for shift selection */
#define CORRECTION_THRESHOLD_1          512U

/** Correction threshold 2 for shift selection */
#define CORRECTION_THRESHOLD_2          1024U

//=============================================================================
// Loop Counter Constants
//=============================================================================

/** Main loop counter maximum value before saturation */
#define MAIN_LOOP_COUNT_MAX             (UINT16_MAX - 1U)

//=============================================================================
// Reset Control Constants
//=============================================================================

/** Reset control prevent mode active value */
#define RESET_CONTROL_PREVENT_ACTIVE    1U

//=============================================================================
// Configuration Write Constants
//=============================================================================

/** Configuration write success code */
#define WRITE_SUCCESS                   0

/** Configuration write error code */
#define WRITE_ERROR                     -1

//=============================================================================
// Type Definitions for Better Type Safety
//=============================================================================

/**
 * @brief Serial communication status enumeration
 * @note Already defined as SerialStatus in comms_legacy.h - use that instead
 */
// typedef enum {
//     SERIAL_INACTIVE = 0,  /**< No serial communication active */
//     SERIAL_TRANSMIT = 1,  /**< Serial transmission in progress */
//     SERIAL_RECEIVE  = 2   /**< Serial reception in progress */
// } SerialStatus_t;

/**
 * @brief Error codes for critical operations
 */
typedef enum {
    ERROR_NONE              = 0,    /**< No error */
    ERROR_OVERFLOW          = -1,   /**< Arithmetic overflow detected */
    ERROR_INVALID_PARAM     = -2,   /**< Invalid parameter value */
    ERROR_OUT_OF_RANGE      = -3,   /**< Value out of valid range */
    ERROR_NULL_POINTER      = -4,   /**< Null pointer detected */
    ERROR_TIMEOUT           = -5,   /**< Operation timeout */
    ERROR_HARDWARE          = -6    /**< Hardware error */
} ErrorCode_t;

//=============================================================================
// Safety Macros
//=============================================================================

/**
 * @brief Safe increment macro with saturation
 * @param counter Counter variable to increment
 * @param max Maximum value before saturation
 */
#define SAFE_INCREMENT(counter, max) \
    do { \
        if((counter) < (max)) { \
            (counter)++; \
        } \
    } while(0)

/**
 * @brief Validate value is within range
 * @param val Value to check
 * @param min Minimum valid value
 * @param max Maximum valid value
 * @return true if in range, false otherwise
 */
#define IS_IN_RANGE(val, min, max) \
    (((val) >= (min)) && ((val) <= (max)))

/**
 * @brief Clamp value to range
 * @param val Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 */
#define CLAMP(val, min, max) \
    (((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val)))

#endif // AUTOMOTIVE_CONSTANTS_H
