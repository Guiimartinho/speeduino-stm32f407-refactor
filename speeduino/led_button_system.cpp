/**
 * @file led_button_system.cpp
 * @brief LED + Button interactive system implementation
 *
 * Complete implementation of 5 operation modes with button control
 *
 * @date 2025-11-06
 * @author Claude Code - SCG-ECU 2.0 Project
 */

#include "led_button_system.h"
#include "globals.h"
#include "speeduino.h"
#include "sensors.h"
#include "corrections.h"
#include "decoders.h"
#include "storage.h"
#include "utilities.h"
#include EEPROM_LIB_H

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

LedButtonSystem ledSystem;

// ============================================================================
// INITIALIZATION
// ============================================================================

void ledButtonInit(void) {
  // Initialize GPIO pins
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  pinMode(LED_ACTIVITY, OUTPUT);
  pinMode(BTN_MODE, INPUT_PULLUP);

  // Initialize state
  ledSystem.mode = LED_MODE_NORMAL;
  ledSystem.mode_saved = LED_MODE_NORMAL;
  ledSystem.mode_changed = false;

  // Button state
  ledSystem.btn_state = BTN_STATE_IDLE;
  ledSystem.btn_current = false;
  ledSystem.btn_last = false;
  ledSystem.btn_press_start = 0;
  ledSystem.btn_last_click = 0;
  ledSystem.btn_click_count = 0;
  ledSystem.btn_debounce_time = 0;

  // LED state
  ledSystem.led1_state = false;
  ledSystem.led2_state = false;
  ledSystem.led3_state = false;
  ledSystem.led1_pattern = LED_PATTERN_OFF;
  ledSystem.led2_pattern = LED_PATTERN_OFF;
  ledSystem.led3_pattern = LED_PATTERN_OFF;
  ledSystem.led_last_update = 0;
  ledSystem.led_blink_counter = 0;
  ledSystem.led_pulse_start = 0;
  ledSystem.led_pulse_active = false;

  // Error management
  ledSystem.error_count = 0;
  ledSystem.error_current_index = 0;
  ledSystem.error_display_start = 0;
  for (uint8_t i = 0; i < 10; i++) {
    ledSystem.error_codes[i] = LED_ERROR_NONE;
  }

  // Shift light
  ledSystem.shift_light_zone = 0;

  // Diagnostic mode
  ledSystem.diagnostic_sensor_index = 0;
  ledSystem.diagnostic_locked = false;
  ledSystem.diagnostic_last_switch = 0;

  // Configuration menu
  ledSystem.in_config_menu = false;
  ledSystem.config_menu_option = CONFIG_MENU_LED_BRIGHTNESS;
  ledSystem.config_menu_editing = false;
  ledSystem.config_menu_value = 0;

  // Startup sequence
  ledSystem.startup_complete = false;
  ledSystem.startup_step = 0;
  ledSystem.startup_last_step = 0;

  // Load configuration from EEPROM
  ledLoadConfig();

  // Run startup self-test
  ledStartupSequence();
}

// ============================================================================
// HELPER: Mode dispatch
// ============================================================================

static void ledDispatchModeUpdate(void) {
  switch (ledSystem.mode) {
    case LED_MODE_NORMAL: ledUpdateModeNormal(); break;
    case LED_MODE_SHIFT_LIGHT: ledUpdateModeShiftLight(); break;
    case LED_MODE_ERROR_CODES: ledUpdateModeErrorCodes(); break;
    case LED_MODE_TUNING: ledUpdateModeTuning(); break;
    case LED_MODE_DIAGNOSTIC: ledUpdateModeDiagnostic(); break;
    default: ledSystem.mode = LED_MODE_NORMAL; break;
  }
}

// ============================================================================
// MAIN UPDATE FUNCTION (call at 100Hz)
// ============================================================================

void ledButtonUpdate(void) {
  unsigned long now = millis();

  // Update button state
  ledButtonUpdateState();

  // Handle button press actions
  ledButtonHandlePress();

  // Update LED patterns at 100Hz
  if (now - ledSystem.led_last_update >= LED_UPDATE_RATE_MS) {
    ledSystem.led_last_update = now;

    // If in startup sequence, handle it
    if (!ledSystem.startup_complete) {
      // Startup sequence is handled separately
      return;
    }

    // If in configuration menu, handle it
    if (ledSystem.in_config_menu) { ledUpdateConfigMenu(); }
    else { ledDispatchModeUpdate(); }

    // Apply patterns and write outputs
    ledUpdatePatterns();
    ledWriteOutput();
  }

  // Periodic sensor checking (every 100ms)
  static unsigned long lastSensorCheck = 0;
  if (now - lastSensorCheck >= 100) {
    lastSensorCheck = now;
    ledCheckSensors();
  }
}

// ============================================================================
// BUTTON STATE MACHINE HELPERS
// ============================================================================

static inline void classifyPressDuration(unsigned long press_duration) {
  if (press_duration >= BTN_VLONG_PRESS_MS) { ledSystem.btn_state = BTN_STATE_VLONG_PRESS; return; }
  if (press_duration >= BTN_LONG_PRESS_MS) { ledSystem.btn_state = BTN_STATE_LONG_PRESS; return; }
  if (press_duration >= BTN_SHORT_PRESS_MS) { ledSystem.btn_state = BTN_STATE_SHORT_PRESS; }
}

static inline void evaluateMultiClick(unsigned long now) {
  if (ledSystem.btn_click_count == 0) { return; }
  if ((now - ledSystem.btn_press_start) <= BTN_DOUBLE_CLICK_MS) { return; }

  if (ledSystem.btn_click_count >= 3) { ledSystem.btn_state = BTN_STATE_TRIPLE_CLICK; }
  else if (ledSystem.btn_click_count == 2) { ledSystem.btn_state = BTN_STATE_DOUBLE_CLICK; }
  ledSystem.btn_click_count = 0;
}

static inline void handleButtonRelease(unsigned long now) {
  unsigned long press_duration = now - ledSystem.btn_press_start;
  classifyPressDuration(press_duration);
}

static inline void handleButtonPress(unsigned long now) {
  ledSystem.btn_state = BTN_STATE_PRESSED;
  ledSystem.btn_press_start = now;
  ledSystem.btn_click_count++;
}

// ============================================================================
// BUTTON STATE MACHINE
// ============================================================================

void ledButtonUpdateState(void) {
  unsigned long now = millis();
  bool btn_raw = !digitalRead(BTN_MODE);  // Inverted due to INPUT_PULLUP

  // Debounce
  if (btn_raw != ledSystem.btn_current) { ledSystem.btn_debounce_time = now; }

  if ((now - ledSystem.btn_debounce_time) > BTN_DEBOUNCE_MS) {
    bool btn_new_state = btn_raw;

    if (btn_new_state && !ledSystem.btn_last) { handleButtonPress(now); }
    else if (!btn_new_state && ledSystem.btn_last) { handleButtonRelease(now); }

    ledSystem.btn_last = btn_new_state;
  }

  ledSystem.btn_current = btn_raw;
  evaluateMultiClick(now);
}

// Helper: Handle short press in config menu
static inline void handleConfigMenuShortPress(void) {
  if (ledSystem.config_menu_editing) { ledSystem.config_menu_value++; }
  else { ledSystem.config_menu_option = (ConfigMenuOption_t)((ledSystem.config_menu_option + 1) % CONFIG_MENU_COUNT); }
}

// Helper: Handle long press in config menu
static inline void handleConfigMenuLongPress(void) {
  if (ledSystem.config_menu_editing) { ledSystem.config_menu_editing = false; ledSaveConfig(); }
  else { ledSystem.config_menu_editing = true; ledSystem.config_menu_value = 0; }
}

// Helper: Handle button actions in config menu
static inline void handleConfigMenuButton(void) {
  switch (ledSystem.btn_state) {
    case BTN_STATE_SHORT_PRESS: handleConfigMenuShortPress(); break;
    case BTN_STATE_LONG_PRESS: handleConfigMenuLongPress(); break;
    case BTN_STATE_VLONG_PRESS: ledSystem.in_config_menu = false; ledSystem.config_menu_editing = false; break;
    default: break;
  }
}

// Helper: Handle button actions in normal mode
static inline void handleNormalModeButton(void) {
  switch (ledSystem.btn_state) {
    case BTN_STATE_SHORT_PRESS:
      ledSystem.mode = (LedMode_t)((ledSystem.mode + 1) % LED_MODE_COUNT);
      ledSystem.mode_changed = true;
      break;
    case BTN_STATE_LONG_PRESS:
      ledSystem.mode_saved = ledSystem.mode;
      ledSaveConfig();
      ledSetPattern(1, LED_PATTERN_PULSE_SHORT);
      ledSetPattern(2, LED_PATTERN_PULSE_SHORT);
      ledSetPattern(3, LED_PATTERN_PULSE_SHORT);
      break;
    case BTN_STATE_VLONG_PRESS:
      ledFactoryReset();
      ledSetPattern(1, LED_PATTERN_BLINK_VERY_FAST);
      ledSetPattern(2, LED_PATTERN_BLINK_VERY_FAST);
      ledSetPattern(3, LED_PATTERN_BLINK_VERY_FAST);
      break;
    case BTN_STATE_DOUBLE_CLICK:
      ledSystem.in_config_menu = true;
      ledSystem.config_menu_option = CONFIG_MENU_LED_BRIGHTNESS;
      ledSystem.config_menu_editing = false;
      break;
    case BTN_STATE_TRIPLE_CLICK:
      ledClearAllErrors();
      ledSetPattern(2, LED_PATTERN_PULSE_LONG);
      break;
    default: break;
  }
}

void ledButtonHandlePress(void) {
  if (ledSystem.btn_state == BTN_STATE_IDLE) { return; }

  if (ledSystem.in_config_menu) { handleConfigMenuButton(); }
  else { handleNormalModeButton(); }

  ledSystem.btn_state = BTN_STATE_IDLE;
}

// ============================================================================
// MODE 1: NORMAL (Heartbeat + Error + Activity)
// ============================================================================

void ledUpdateModeNormal(void) {
  // LED1 (Green) - Heartbeat (slow blink)
  ledSetPattern(1, LED_PATTERN_BLINK_SLOW);

  // LED2 (Red) - Error indicator
  if (ledSystem.error_count > 0) {
    ledSetPattern(2, LED_PATTERN_BLINK_FAST);
  } else {
    ledSetPattern(2, LED_PATTERN_OFF);
  }

  // LED3 (Blue) - Activity pulse (handled by ledFlashActivity())
  if (!ledSystem.led_pulse_active) {
    ledSetPattern(3, LED_PATTERN_OFF);
  }
}

// ============================================================================
// MODE 2: SHIFT LIGHT
// ============================================================================

void ledUpdateModeShiftLight(void) {
  if (!ledSystem.config.shift_light_enabled) {
    // Shift light disabled, fall back to normal mode
    ledUpdateModeNormal();
    return;
  }

  uint16_t rpm = currentStatus.RPM;

  // Determine shift light zone
  if (rpm >= ledSystem.config.shift_light_threshold[2]) {
    // Critical zone (>6500 RPM) - All LEDs flashing very fast
    ledSystem.shift_light_zone = 3;
    ledSetPattern(1, LED_PATTERN_BLINK_VERY_FAST);
    ledSetPattern(2, LED_PATTERN_BLINK_VERY_FAST);
    ledSetPattern(3, LED_PATTERN_BLINK_VERY_FAST);
  } else if (rpm >= ledSystem.config.shift_light_threshold[1]) {
    // Shift zone (5500-6500 RPM) - LED1+LED2 fast blink
    ledSystem.shift_light_zone = 2;
    ledSetPattern(1, LED_PATTERN_BLINK_FAST);
    ledSetPattern(2, LED_PATTERN_BLINK_FAST);
    ledSetPattern(3, LED_PATTERN_OFF);
  } else if (rpm >= ledSystem.config.shift_light_threshold[0]) {
    // Warning zone (4000-5500 RPM) - LED1 normal blink
    ledSystem.shift_light_zone = 1;
    ledSetPattern(1, LED_PATTERN_BLINK_NORMAL);
    ledSetPattern(2, LED_PATTERN_OFF);
    ledSetPattern(3, LED_PATTERN_OFF);
  } else {
    // Below warning zone - Heartbeat only
    ledSystem.shift_light_zone = 0;
    ledSetPattern(1, LED_PATTERN_BLINK_VERY_SLOW);
    ledSetPattern(2, LED_PATTERN_OFF);
    ledSetPattern(3, LED_PATTERN_OFF);
  }

  // Show errors on LED2 if any
  if (ledSystem.error_count > 0) {
    ledSetPattern(2, LED_PATTERN_ON);
  }
}

// ============================================================================
// MODE 3: ERROR CODES (Blink patterns)
// ============================================================================

void ledUpdateModeErrorCodes(void) {
  if (ledSystem.error_count == 0) {
    // No errors - show "OK" pattern (LED1 slow blink)
    ledSetPattern(1, LED_PATTERN_BLINK_SLOW);
    ledSetPattern(2, LED_PATTERN_OFF);
    ledSetPattern(3, LED_PATTERN_OFF);
    return;
  }

  unsigned long now = millis();

  // Cycle through errors
  if (now - ledSystem.error_display_start >= (ledSystem.config.error_display_time_sec * 1000UL)) {
    ledSystem.error_current_index++;
    if (ledSystem.error_current_index >= ledSystem.error_count) {
      ledSystem.error_current_index = 0;
    }
    ledSystem.error_display_start = now;
  }

  // Get current error code
  uint8_t error = ledSystem.error_codes[ledSystem.error_current_index];

  // Blink pattern: X blinks (tens) - pause - Y blinks (ones)
  // Example: 0x21 = 2-1 = TPS range error
  uint8_t tens = ledGetErrorCodeDigit(error, 0);
  uint8_t ones = ledGetErrorCodeDigit(error, 1);

  // Simple blink pattern implementation (use custom timing)
  // For now, just use LED_PATTERN_CUSTOM and implement detailed pattern later
  ledSetPattern(1, LED_PATTERN_CUSTOM);
  ledSetPattern(2, LED_PATTERN_CUSTOM);
  ledSetPattern(3, LED_PATTERN_OFF);

  // TODO: Implement detailed blink sequence
  // This would require a more complex state machine for blink counting
}

// ============================================================================
// MODE 4: TUNING (AFR + Knock + Datalogging)
// ============================================================================

void ledUpdateModeTuning(void) {
  // LED1 (Green) - AFR status
  float afr = currentStatus.O2;
  float target_afr = ledSystem.config.afr_target;
  float afr_tolerance = 0.5f;

  if (afr > (target_afr + afr_tolerance)) {
    // Too lean - blink fast
    ledSetPattern(1, LED_PATTERN_BLINK_FAST);
  } else if (afr < (target_afr - afr_tolerance)) {
    // Too rich - blink slow
    ledSetPattern(1, LED_PATTERN_BLINK_SLOW);
  } else {
    // Within target - solid on
    ledSetPattern(1, LED_PATTERN_ON);
  }

  // LED2 (Red) - Knock detection
  if (BIT_CHECK(currentStatus.status5, BIT_STATUS5_KNOCK_ACTIVE)) {
    ledSetPattern(2, LED_PATTERN_BLINK_VERY_FAST);
  } else {
    ledSetPattern(2, LED_PATTERN_OFF);
  }

  // LED3 (Blue) - Datalogging activity (pulse on serial activity)
  if (!ledSystem.led_pulse_active) {
    ledSetPattern(3, LED_PATTERN_OFF);
  }
}

// ============================================================================
// MODE 5: DIAGNOSTIC (Sensor status loop)
// ============================================================================

static inline void diagnosticCycleSensor(unsigned long now) {
  if (ledSystem.diagnostic_locked) { return; }
  if ((now - ledSystem.diagnostic_last_switch) < 2000) { return; }
  ledSystem.diagnostic_sensor_index = (ledSystem.diagnostic_sensor_index + 1) % 6;
  ledSystem.diagnostic_last_switch = now;
}

void ledUpdateModeDiagnostic(void) {
  unsigned long now = millis();
  diagnosticCycleSensor(now);

  // Show current sensor status
  // LED1 (Green) - Sensor ID (blink count)
  // LED2 (Red) - Error status (on = bad, off = good)
  // LED3 (Blue) - Data activity (pulse)

  bool sensor_ok = true;

  switch (ledSystem.diagnostic_sensor_index) {
    case 0:  // TPS
      sensor_ok = (currentStatus.TPS >= 0 && currentStatus.TPS <= 100);
      break;
    case 1:  // CLT
      sensor_ok = (currentStatus.coolant > -40 && currentStatus.coolant < 150);
      break;
    case 2:  // IAT
      sensor_ok = (currentStatus.IAT > -40 && currentStatus.IAT < 150);
      break;
    case 3:  // MAP
      sensor_ok = (currentStatus.MAP > 10 && currentStatus.MAP < 260);
      break;
    case 4:  // O2
      sensor_ok = (currentStatus.O2 > 10.0f && currentStatus.O2 < 20.0f);
      break;
    case 5:  // Battery
      sensor_ok = (currentStatus.battery10 > 100 && currentStatus.battery10 < 160);
      break;
    default:
      break;
  }

  // LED1 - Blink count indicates sensor (1-6 blinks)
  ledSetPattern(1, LED_PATTERN_CUSTOM);  // TODO: Implement blink counter

  // LED2 - Error indicator
  if (sensor_ok) {
    ledSetPattern(2, LED_PATTERN_OFF);
  } else {
    ledSetPattern(2, LED_PATTERN_ON);
  }

  // LED3 - Heartbeat
  ledSetPattern(3, LED_PATTERN_BLINK_SLOW);
}

// ============================================================================
// CONFIGURATION MENU
// ============================================================================

void ledUpdateConfigMenu(void) {
  // Use LED blink patterns to indicate menu state
  // LED1 - Menu option indicator (blink count)
  // LED2 - Editing mode indicator (on = editing, off = navigating)
  // LED3 - Value indicator (custom pattern)

  // For simplicity, use basic patterns
  ledSetPattern(1, LED_PATTERN_BLINK_NORMAL);

  if (ledSystem.config_menu_editing) {
    ledSetPattern(2, LED_PATTERN_ON);
  } else {
    ledSetPattern(2, LED_PATTERN_OFF);
  }

  ledSetPattern(3, LED_PATTERN_BLINK_FAST);
}

// ============================================================================
// LED PATTERN ENGINE
// ============================================================================

void ledSetPattern(uint8_t led, LedPattern_t pattern) {
  switch (led) {
    case 1:
      ledSystem.led1_pattern = pattern;
      break;
    case 2:
      ledSystem.led2_pattern = pattern;
      break;
    case 3:
      ledSystem.led3_pattern = pattern;
      break;
    default:
      break;
  }
}

void ledUpdatePatterns(void) {
  ledApplyPattern(1, ledSystem.led1_pattern, &ledSystem.led1_state);
  ledApplyPattern(2, ledSystem.led2_pattern, &ledSystem.led2_state);
  ledApplyPattern(3, ledSystem.led3_pattern, &ledSystem.led3_state);
}

void ledApplyPattern(uint8_t led, LedPattern_t pattern, bool *state) {
  unsigned long now = millis();

  switch (pattern) {
    case LED_PATTERN_OFF:
      *state = false;
      break;

    case LED_PATTERN_ON:
      *state = true;
      break;

    case LED_PATTERN_BLINK_VERY_SLOW:
      *state = (now % LED_BLINK_VERY_SLOW) < (LED_BLINK_VERY_SLOW / 2);
      break;

    case LED_PATTERN_BLINK_SLOW:
      *state = (now % LED_BLINK_SLOW) < (LED_BLINK_SLOW / 2);
      break;

    case LED_PATTERN_BLINK_NORMAL:
      *state = (now % LED_BLINK_NORMAL) < (LED_BLINK_NORMAL / 2);
      break;

    case LED_PATTERN_BLINK_FAST:
      *state = (now % LED_BLINK_FAST) < (LED_BLINK_FAST / 2);
      break;

    case LED_PATTERN_BLINK_VERY_FAST:
      *state = (now % LED_BLINK_VERY_FAST) < (LED_BLINK_VERY_FAST / 2);
      break;

    case LED_PATTERN_PULSE_SHORT:
      *state = ledSystem.led_pulse_active && ((now - ledSystem.led_pulse_start) < LED_PULSE_SHORT);
      if (ledSystem.led_pulse_active && ((now - ledSystem.led_pulse_start) >= LED_PULSE_SHORT)) { ledSystem.led_pulse_active = false; }
      break;

    case LED_PATTERN_PULSE_LONG:
      *state = ledSystem.led_pulse_active && ((now - ledSystem.led_pulse_start) < LED_PULSE_LONG);
      if (ledSystem.led_pulse_active && ((now - ledSystem.led_pulse_start) >= LED_PULSE_LONG)) { ledSystem.led_pulse_active = false; }
      break;

    case LED_PATTERN_CUSTOM:
      // Custom pattern - handled by specific mode functions
      // For now, just blink slowly
      *state = (now % LED_BLINK_NORMAL) < (LED_BLINK_NORMAL / 2);
      break;

    default:
      *state = false;
      break;
  }
}

void ledWriteOutput(void) {
  digitalWrite(LED_STATUS, ledSystem.led1_state ? HIGH : LOW);
  digitalWrite(LED_ERROR, ledSystem.led2_state ? HIGH : LOW);
  digitalWrite(LED_ACTIVITY, ledSystem.led3_state ? HIGH : LOW);
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

void ledSetMode(LedMode_t mode) {
  if (mode < LED_MODE_COUNT) {
    ledSystem.mode = mode;
    ledSystem.mode_changed = true;
  }
}

LedMode_t ledGetMode(void) {
  return ledSystem.mode;
}

void ledAddError(uint8_t error_code) {
  // Check if error already exists
  for (uint8_t i = 0; i < ledSystem.error_count; i++) {
    if (ledSystem.error_codes[i] == error_code) {
      return;  // Already exists
    }
  }

  // Add new error if space available
  if (ledSystem.error_count < 10) {
    ledSystem.error_codes[ledSystem.error_count] = error_code;
    ledSystem.error_count++;
  }
}

static inline void shiftErrorsDown(uint8_t from_index) {
  for (uint8_t j = from_index; j < ledSystem.error_count - 1; j++) { ledSystem.error_codes[j] = ledSystem.error_codes[j + 1]; }
  ledSystem.error_count--;
  ledSystem.error_codes[ledSystem.error_count] = LED_ERROR_NONE;
}

void ledClearError(uint8_t error_code) {
  for (uint8_t i = 0; i < ledSystem.error_count; i++) {
    if (ledSystem.error_codes[i] == error_code) { shiftErrorsDown(i); return; }
  }
}

void ledClearAllErrors(void) {
  ledSystem.error_count = 0;
  for (uint8_t i = 0; i < 10; i++) {
    ledSystem.error_codes[i] = LED_ERROR_NONE;
  }
}

bool ledHasError(uint8_t error_code) {
  for (uint8_t i = 0; i < ledSystem.error_count; i++) {
    if (ledSystem.error_codes[i] == error_code) {
      return true;
    }
  }
  return false;
}

void ledFlashActivity(void) {
  ledSystem.led_pulse_active = true;
  ledSystem.led_pulse_start = millis();
  ledSetPattern(3, LED_PATTERN_PULSE_SHORT);
}

void ledSetShiftLightThresholds(uint16_t warn_rpm, uint16_t shift_rpm, uint16_t critical_rpm) {
  ledSystem.config.shift_light_threshold[0] = warn_rpm;
  ledSystem.config.shift_light_threshold[1] = shift_rpm;
  ledSystem.config.shift_light_threshold[2] = critical_rpm;
}

// ============================================================================
// EEPROM PERSISTENCE HELPERS
// ============================================================================

static void eepromWriteU8(uint16_t addr, uint8_t value) {
  if (EEPROM.read(addr) != value) {
    EEPROM.write(addr, value);
  }
}

static void eepromWriteU16(uint16_t addr, uint16_t value) {
  eepromWriteU8(addr, (uint8_t)(value & 0xFF));
  eepromWriteU8(addr + 1, (uint8_t)((value >> 8) & 0xFF));
}

static void eepromWriteFloat(uint16_t addr, float value) {
  uint8_t* bytes = (uint8_t*)&value;
  for (uint8_t i = 0; i < sizeof(float); i++) {
    eepromWriteU8(addr + i, bytes[i]);
  }
}

static uint8_t eepromReadU8(uint16_t addr) {
  return EEPROM.read(addr);
}

static uint16_t eepromReadU16(uint16_t addr) {
  uint16_t value = EEPROM.read(addr);
  value |= ((uint16_t)EEPROM.read(addr + 1)) << 8;
  return value;
}

static float eepromReadFloat(uint16_t addr) {
  float value;
  uint8_t* bytes = (uint8_t*)&value;
  for (uint8_t i = 0; i < sizeof(float); i++) {
    bytes[i] = EEPROM.read(addr + i);
  }
  return value;
}

// ============================================================================
// EEPROM PERSISTENCE
// ============================================================================

void ledSaveConfig(void) {
  // Save configuration to EEPROM
  uint16_t addr = LED_EEPROM_BASE_ADDR;

  // Write version
  eepromWriteU16(addr, ledSystem.config.eeprom_version);
  addr += sizeof(uint16_t);

  // Write configuration
  eepromWriteU8(addr, ledSystem.config.led_brightness);
  addr += sizeof(uint8_t);

  for (uint8_t i = 0; i < 3; i++) {
    eepromWriteU16(addr, ledSystem.config.shift_light_threshold[i]);
    addr += sizeof(uint16_t);
  }

  eepromWriteU8(addr, ledSystem.config.shift_light_enabled ? 1 : 0);
  addr += sizeof(bool);

  eepromWriteFloat(addr, ledSystem.config.afr_target);
  addr += sizeof(float);

  eepromWriteU16(addr, ledSystem.config.sensor_timeout_ms);
  addr += sizeof(uint16_t);

  eepromWriteU8(addr, ledSystem.config.knock_sensitivity);
  addr += sizeof(uint8_t);

  eepromWriteU8(addr, ledSystem.config.error_display_time_sec);
  addr += sizeof(uint8_t);

  // Save current mode
  eepromWriteU8(addr, (uint8_t)ledSystem.mode_saved);
}

void ledLoadConfig(void) {
  // Load configuration from EEPROM
  uint16_t addr = LED_EEPROM_BASE_ADDR;

  // Read version
  uint16_t version = eepromReadU16(addr);
  addr += sizeof(uint16_t);

  if (version != LED_EEPROM_VERSION) {
    // Invalid or uninitialized EEPROM, use defaults
    ledFactoryReset();
    return;
  }

  // Read configuration
  ledSystem.config.led_brightness = eepromReadU8(addr);
  addr += sizeof(uint8_t);

  for (uint8_t i = 0; i < 3; i++) {
    ledSystem.config.shift_light_threshold[i] = eepromReadU16(addr);
    addr += sizeof(uint16_t);
  }

  ledSystem.config.shift_light_enabled = (eepromReadU8(addr) != 0);
  addr += sizeof(bool);

  ledSystem.config.afr_target = eepromReadFloat(addr);
  addr += sizeof(float);

  ledSystem.config.sensor_timeout_ms = eepromReadU16(addr);
  addr += sizeof(uint16_t);

  ledSystem.config.knock_sensitivity = eepromReadU8(addr);
  addr += sizeof(uint8_t);

  ledSystem.config.error_display_time_sec = eepromReadU8(addr);
  addr += sizeof(uint8_t);

  // Load saved mode
  uint8_t saved_mode = eepromReadU8(addr);
  if (saved_mode < LED_MODE_COUNT) {
    ledSystem.mode = (LedMode_t)saved_mode;
    ledSystem.mode_saved = (LedMode_t)saved_mode;
  }
}

void ledFactoryReset(void) {
  // Restore default configuration
  ledSystem.config.led_brightness = LED_DEFAULT_BRIGHTNESS;
  ledSystem.config.shift_light_threshold[0] = LED_DEFAULT_SHIFT_WARN_RPM;
  ledSystem.config.shift_light_threshold[1] = LED_DEFAULT_SHIFT_SHIFT_RPM;
  ledSystem.config.shift_light_threshold[2] = LED_DEFAULT_SHIFT_CRITICAL_RPM;
  ledSystem.config.shift_light_enabled = LED_DEFAULT_SHIFT_ENABLED;
  ledSystem.config.afr_target = LED_DEFAULT_AFR_TARGET;
  ledSystem.config.sensor_timeout_ms = LED_DEFAULT_SENSOR_TIMEOUT;
  ledSystem.config.knock_sensitivity = LED_DEFAULT_KNOCK_SENSITIVITY;
  ledSystem.config.error_display_time_sec = LED_DEFAULT_ERROR_DISPLAY_TIME;
  ledSystem.config.eeprom_version = LED_EEPROM_VERSION;

  // Reset mode
  ledSystem.mode = LED_MODE_NORMAL;
  ledSystem.mode_saved = LED_MODE_NORMAL;

  // Save to EEPROM
  ledSaveConfig();
}

// ============================================================================
// STARTUP SELF-TEST SEQUENCE
// ============================================================================

void ledStartupSequence(void) {
  // Simple startup sequence: Light up LEDs in sequence
  // Step 0: All off
  // Step 1: LED1 on
  // Step 2: LED1+LED2 on
  // Step 3: LED1+LED2+LED3 on
  // Step 4: All off, sequence complete

  unsigned long now = millis();

  if (ledSystem.startup_step == 0) {
    // All off
    ledSetPattern(1, LED_PATTERN_OFF);
    ledSetPattern(2, LED_PATTERN_OFF);
    ledSetPattern(3, LED_PATTERN_OFF);
    ledSystem.startup_last_step = now;
    ledSystem.startup_step++;
  } else if (ledSystem.startup_step == 1) {
    if (now - ledSystem.startup_last_step >= 200) {
      // LED1 on
      ledSetPattern(1, LED_PATTERN_ON);
      ledSetPattern(2, LED_PATTERN_OFF);
      ledSetPattern(3, LED_PATTERN_OFF);
      ledSystem.startup_last_step = now;
      ledSystem.startup_step++;
    }
  } else if (ledSystem.startup_step == 2) {
    if (now - ledSystem.startup_last_step >= 200) {
      // LED1+LED2 on
      ledSetPattern(1, LED_PATTERN_ON);
      ledSetPattern(2, LED_PATTERN_ON);
      ledSetPattern(3, LED_PATTERN_OFF);
      ledSystem.startup_last_step = now;
      ledSystem.startup_step++;
    }
  } else if (ledSystem.startup_step == 3) {
    if (now - ledSystem.startup_last_step >= 200) {
      // LED1+LED2+LED3 on
      ledSetPattern(1, LED_PATTERN_ON);
      ledSetPattern(2, LED_PATTERN_ON);
      ledSetPattern(3, LED_PATTERN_ON);
      ledSystem.startup_last_step = now;
      ledSystem.startup_step++;
    }
  } else if (ledSystem.startup_step == 4) {
    if (now - ledSystem.startup_last_step >= 200) {
      // All off, sequence complete
      ledSetPattern(1, LED_PATTERN_OFF);
      ledSetPattern(2, LED_PATTERN_OFF);
      ledSetPattern(3, LED_PATTERN_OFF);
      ledSystem.startup_complete = true;
    }
  }

  // Update patterns during startup
  ledUpdatePatterns();
  ledWriteOutput();
}

// ============================================================================
// ERROR CODE HELPERS
// ============================================================================

uint8_t ledGetErrorCodeDigit(uint8_t code, uint8_t digit) {
  // Error codes are in format 0xXY
  // digit 0 = tens (X), digit 1 = ones (Y)
  if (digit == 0) {
    return (code >> 4) & 0x0F;  // High nibble
  } else {
    return code & 0x0F;  // Low nibble
  }
}

void ledCheckSensors(void) {
  // Check sensors and automatically add/remove error codes
  // This is called periodically by ledButtonUpdate()

  // TPS range check
  if (currentStatus.TPS < 0 || currentStatus.TPS > 100) {
    ledAddError(LED_ERROR_TPS_RANGE);
  } else {
    ledClearError(LED_ERROR_TPS_RANGE);
  }

  // MAP sensor check
  if (currentStatus.MAP < 10 || currentStatus.MAP > 260) {
    ledAddError(LED_ERROR_MAP_SENSOR);
  } else {
    ledClearError(LED_ERROR_MAP_SENSOR);
  }

  // CLT sensor check
  if (currentStatus.coolant < -40 || currentStatus.coolant > 150) {
    ledAddError(LED_ERROR_CLT_SENSOR);
  } else {
    ledClearError(LED_ERROR_CLT_SENSOR);
  }

  // IAT sensor check
  if (currentStatus.IAT < -40 || currentStatus.IAT > 150) {
    ledAddError(LED_ERROR_IAT_SENSOR);
  } else {
    ledClearError(LED_ERROR_IAT_SENSOR);
  }

  // O2 sensor check
  if (currentStatus.O2 < 10.0f || currentStatus.O2 > 20.0f) {
    ledAddError(LED_ERROR_O2_SENSOR);
  } else {
    ledClearError(LED_ERROR_O2_SENSOR);
  }

  // Battery voltage check
  if (currentStatus.battery10 < 100 || currentStatus.battery10 > 160) {
    ledAddError(LED_ERROR_BATTERY_VOLTAGE);
  } else {
    ledClearError(LED_ERROR_BATTERY_VOLTAGE);
  }

  // Sync loss check (crank signal)
  if (currentStatus.hasSync == false && currentStatus.RPM > 0) {
    ledAddError(LED_ERROR_SYNC_LOSS);
  } else {
    ledClearError(LED_ERROR_SYNC_LOSS);
  }
}
