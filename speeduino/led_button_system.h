/**
 * @file led_button_system.h
 * @brief LED + Button interactive system for SCG-ECU 2.0
 *
 * Hardware:
 * - LED1 (PC10) - Status LED (Green)
 * - LED2 (PC11) - Error LED (Red)
 * - LED3 (PC12) - Activity LED (Blue)
 * - BTN1 (PB2)  - Mode button (BOOT1 - safe to use!)
 *
 * Features:
 * - 5 operation modes (Normal, Shift Light, Error Codes, Tuning, Diagnostic)
 * - Interactive configuration via button
 * - EEPROM persistence
 * - Advanced configuration menu
 * - Startup self-test sequence
 *
 * @date 2025-11-06
 * @author Claude Code - SCG-ECU 2.0 Project
 */

#ifndef LED_BUTTON_SYSTEM_H
#define LED_BUTTON_SYSTEM_H

#include <Arduino.h>

// ============================================================================
// HARDWARE PINS
// ============================================================================

#if defined(CORE_STM32) && defined(BOARD_SCG_ECU_20)
  #define LED_STATUS   PC10  // LED1 (Green)
  #define LED_ERROR    PC11  // LED2 (Red)
  #define LED_ACTIVITY PC12  // LED3 (Blue)
  #define BTN_MODE     PB2   // BOOT1 (safe to use as GPIO!)
#else
  // Fallback for testing on other boards
  #define LED_STATUS   13
  #define LED_ERROR    12
  #define LED_ACTIVITY 11
  #define BTN_MODE     10
#endif

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================

#define BTN_DEBOUNCE_MS         10    // Debounce time
#define BTN_SHORT_PRESS_MS      500   // Short press threshold
#define BTN_LONG_PRESS_MS       1000  // Long press threshold (1-3s)
#define BTN_VLONG_PRESS_MS      3000  // Very long press threshold (>3s)
#define BTN_DOUBLE_CLICK_MS     500   // Double click window
#define BTN_TRIPLE_CLICK_MS     500   // Triple click window

// ============================================================================
// LED TIMING CONFIGURATION
// ============================================================================

#define LED_UPDATE_RATE_MS      10    // LED update rate (100Hz)

// Blink rates (period in ms)
#define LED_BLINK_VERY_SLOW     2000  // 0.5Hz
#define LED_BLINK_SLOW          1000  // 1Hz
#define LED_BLINK_NORMAL        500   // 2Hz
#define LED_BLINK_FAST          250   // 4Hz
#define LED_BLINK_VERY_FAST     100   // 10Hz

// Pulse durations
#define LED_PULSE_SHORT         50    // Short pulse (50ms)
#define LED_PULSE_LONG          500   // Long pulse (500ms)

// ============================================================================
// BUTTON STATES
// ============================================================================

typedef enum {
  BTN_STATE_IDLE = 0,
  BTN_STATE_PRESSED,
  BTN_STATE_SHORT_PRESS,
  BTN_STATE_LONG_PRESS,
  BTN_STATE_VLONG_PRESS,
  BTN_STATE_DOUBLE_CLICK,
  BTN_STATE_TRIPLE_CLICK
} ButtonState_t;

// ============================================================================
// LED MODES
// ============================================================================

typedef enum {
  LED_MODE_NORMAL = 0,      // Heartbeat + Error + Activity
  LED_MODE_SHIFT_LIGHT,     // Shift light + Status
  LED_MODE_ERROR_CODES,     // Blink error codes
  LED_MODE_TUNING,          // AFR + Knock + Datalogging
  LED_MODE_DIAGNOSTIC,      // Sensor status loop
  LED_MODE_COUNT
} LedMode_t;

// ============================================================================
// LED PATTERNS
// ============================================================================

typedef enum {
  LED_PATTERN_OFF = 0,
  LED_PATTERN_ON,
  LED_PATTERN_BLINK_VERY_SLOW,
  LED_PATTERN_BLINK_SLOW,
  LED_PATTERN_BLINK_NORMAL,
  LED_PATTERN_BLINK_FAST,
  LED_PATTERN_BLINK_VERY_FAST,
  LED_PATTERN_PULSE_SHORT,
  LED_PATTERN_PULSE_LONG,
  LED_PATTERN_CUSTOM        // For error codes, etc.
} LedPattern_t;

// ============================================================================
// ERROR CODES (for blink patterns)
// ============================================================================

#define LED_ERROR_NONE              0x00
#define LED_ERROR_CRANK_SENSOR      0x11  // 1-1
#define LED_ERROR_CAM_SENSOR        0x12  // 1-2
#define LED_ERROR_TPS_RANGE         0x21  // 2-1
#define LED_ERROR_MAP_SENSOR        0x22  // 2-2
#define LED_ERROR_CLT_SENSOR        0x31  // 3-1
#define LED_ERROR_IAT_SENSOR        0x32  // 3-2
#define LED_ERROR_O2_SENSOR         0x41  // 4-1
#define LED_ERROR_BATTERY_VOLTAGE   0x42  // 4-2
#define LED_ERROR_SYNC_LOSS         0x51  // 5-1
#define LED_ERROR_COMM_TIMEOUT      0x52  // 5-2

// ============================================================================
// CONFIGURATION MENU OPTIONS
// ============================================================================

typedef enum {
  CONFIG_MENU_LED_BRIGHTNESS = 0,
  CONFIG_MENU_AFR_TARGET,
  CONFIG_MENU_SENSOR_TIMEOUT,
  CONFIG_MENU_SHIFT_LIGHT_ENABLE,
  CONFIG_MENU_KNOCK_SENSITIVITY,
  CONFIG_MENU_ERROR_DISPLAY_TIME,
  CONFIG_MENU_FACTORY_RESET,
  CONFIG_MENU_COUNT
} ConfigMenuOption_t;

// ============================================================================
// SYSTEM STATE STRUCTURE
// ============================================================================

struct LedButtonSystem {
  // ========== Mode Management ==========
  LedMode_t mode;              // Current active mode
  LedMode_t mode_saved;        // Mode saved in EEPROM
  bool mode_changed;           // Flag to indicate mode change

  // ========== Button State ==========
  ButtonState_t btn_state;
  bool btn_current;            // Current button reading (after debounce)
  bool btn_last;               // Previous button reading
  unsigned long btn_press_start;
  unsigned long btn_last_click;
  uint8_t btn_click_count;
  unsigned long btn_debounce_time;

  // ========== LED State ==========
  bool led1_state;             // Current LED1 output state
  bool led2_state;             // Current LED2 output state
  bool led3_state;             // Current LED3 output state

  LedPattern_t led1_pattern;   // Current LED1 pattern
  LedPattern_t led2_pattern;   // Current LED2 pattern
  LedPattern_t led3_pattern;   // Current LED3 pattern

  unsigned long led_last_update;
  unsigned long led_blink_counter;
  unsigned long led_pulse_start;
  bool led_pulse_active;

  // ========== Configuration (saved to EEPROM) ==========
  struct Config {
    uint8_t led_brightness;           // 0-255 (PWM duty cycle)
    uint16_t shift_light_threshold[3]; // RPM thresholds [warn, shift, critical]
    bool shift_light_enabled;
    float afr_target;                 // Target AFR for tuning mode
    uint16_t sensor_timeout_ms;       // Sensor validation timeout
    uint8_t knock_sensitivity;        // 0-10
    uint8_t error_display_time_sec;   // Seconds to display each error
    uint16_t eeprom_version;          // Config version (for migration)
  } config;

  // ========== Error Management ==========
  uint8_t error_codes[10];     // Array of active error codes
  uint8_t error_count;         // Number of active errors
  uint8_t error_current_index; // Current error being displayed
  unsigned long error_display_start;

  // ========== Shift Light State ==========
  uint8_t shift_light_zone;    // 0=off, 1=warn, 2=shift, 3=critical

  // ========== Diagnostic Mode State ==========
  uint8_t diagnostic_sensor_index;  // Which sensor is being tested
  bool diagnostic_locked;           // Locked on current sensor
  unsigned long diagnostic_last_switch;

  // ========== Configuration Menu State ==========
  bool in_config_menu;
  ConfigMenuOption_t config_menu_option;
  bool config_menu_editing;
  uint8_t config_menu_value;

  // ========== Startup Sequence ==========
  bool startup_complete;
  uint8_t startup_step;
  unsigned long startup_last_step;

  // ========== Statistics (for debugging) ==========
  #ifdef LED_SYSTEM_DEBUG
  uint32_t led_update_count;
  uint32_t btn_press_count;
  uint32_t mode_change_count;
  #endif
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

extern LedButtonSystem ledSystem;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize LED + Button system
 *
 * Sets up GPIO pins, loads configuration from EEPROM, runs startup sequence
 *
 * @note Call once during setup()
 */
void ledButtonInit(void);

/**
 * @brief Update LED + Button system
 *
 * Must be called frequently (100Hz recommended)
 * Handles button debounce, LED patterns, mode logic
 *
 * @note Call in main loop()
 */
void ledButtonUpdate(void);

/**
 * @brief Set LED mode
 *
 * @param mode New mode to activate
 */
void ledSetMode(LedMode_t mode);

/**
 * @brief Get current LED mode
 *
 * @return Current active mode
 */
LedMode_t ledGetMode(void);

/**
 * @brief Add error code to system
 *
 * @param error_code Error code to add (e.g., LED_ERROR_TPS_RANGE)
 */
void ledAddError(uint8_t error_code);

/**
 * @brief Clear specific error code
 *
 * @param error_code Error code to clear
 */
void ledClearError(uint8_t error_code);

/**
 * @brief Clear all error codes
 */
void ledClearAllErrors(void);

/**
 * @brief Check if specific error is active
 *
 * @param error_code Error code to check
 * @return true if error is active, false otherwise
 */
bool ledHasError(uint8_t error_code);

/**
 * @brief Flash LED3 for activity indication (e.g., serial packet received)
 */
void ledFlashActivity(void);

/**
 * @brief Set shift light thresholds
 *
 * @param warn_rpm Warning threshold (RPM)
 * @param shift_rpm Shift threshold (RPM)
 * @param critical_rpm Critical threshold (RPM)
 */
void ledSetShiftLightThresholds(uint16_t warn_rpm, uint16_t shift_rpm, uint16_t critical_rpm);

/**
 * @brief Save configuration to EEPROM
 */
void ledSaveConfig(void);

/**
 * @brief Load configuration from EEPROM
 */
void ledLoadConfig(void);

/**
 * @brief Factory reset (restore default configuration)
 */
void ledFactoryReset(void);

/**
 * @brief Run startup self-test sequence
 *
 * @note Called automatically by ledButtonInit()
 */
void ledStartupSequence(void);

// ============================================================================
// INTERNAL FUNCTIONS (used by ledButtonUpdate)
// ============================================================================

void ledButtonUpdateState(void);
void ledButtonHandlePress(void);
void ledUpdatePatterns(void);
void ledUpdateModeNormal(void);
void ledUpdateModeShiftLight(void);
void ledUpdateModeErrorCodes(void);
void ledUpdateModeTuning(void);
void ledUpdateModeDiagnostic(void);
void ledUpdateConfigMenu(void);
void ledSetPattern(uint8_t led, LedPattern_t pattern);
void ledApplyPattern(uint8_t led, LedPattern_t pattern, bool *state);
void ledWriteOutput(void);

// ============================================================================
// ERROR CODE HELPERS
// ============================================================================

/**
 * @brief Get number of blinks for error code digit
 *
 * @param code Error code (e.g., 0x21)
 * @param digit 0 = tens, 1 = ones
 * @return Number of blinks (1-5)
 */
uint8_t ledGetErrorCodeDigit(uint8_t code, uint8_t digit);

/**
 * @brief Check sensors and update error codes automatically
 *
 * @note Called by ledButtonUpdate() in background
 */
void ledCheckSensors(void);

// ============================================================================
// EEPROM ADDRESSES
// ============================================================================

#define LED_EEPROM_BASE_ADDR    1000  // Start address in EEPROM
#define LED_EEPROM_VERSION      0x0001 // Config version

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

#define LED_DEFAULT_BRIGHTNESS          255   // Full brightness
#define LED_DEFAULT_SHIFT_WARN_RPM      4000
#define LED_DEFAULT_SHIFT_SHIFT_RPM     5500
#define LED_DEFAULT_SHIFT_CRITICAL_RPM  6500
#define LED_DEFAULT_SHIFT_ENABLED       true
#define LED_DEFAULT_AFR_TARGET          14.7f
#define LED_DEFAULT_SENSOR_TIMEOUT      1000  // ms
#define LED_DEFAULT_KNOCK_SENSITIVITY   5     // 0-10
#define LED_DEFAULT_ERROR_DISPLAY_TIME  3     // seconds

#endif // LED_BUTTON_SYSTEM_H
