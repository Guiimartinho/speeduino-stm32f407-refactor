/**
 * @file LPS22HB.cpp
 * @brief LPS22HB/LPS22HBTR I2C Barometric Pressure Sensor Driver Implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 *
 * This driver provides I2C communication with the LPS22HB MEMS barometric
 * pressure sensor from STMicroelectronics. Used for altitude compensation
 * (atmospheric pressure measurement) in the Speeduino ECU.
 *
 * Hardware Connection (SCG-ECU 2.0):
 * - I2C2: PB10 = SCL, PB11 = SDA
 * - I2C Address: 0x5C (SA0=GND) or 0x5D (SA0=VCC)
 *
 * @note This sensor measures ATMOSPHERIC pressure (260-1260 hPa), not suitable
 *       for manifold pressure measurement. Use external MAP sensor for that.
 *
 * @author SCG-ECU Team
 * @date 2026-01-02
 * @version 1.0
 */

#include "LPS22HB.h"
#include "../globals.h"

// Only compile for STM32 targets with I2C support
#if defined(CORE_STM32) || defined(STM32F4xx) || defined(STM32F407xx)

#include <Wire.h>

// =============================================================================
// I2C2 INSTANCE FOR LPS22HB (PB10=SCL, PB11=SDA)
// =============================================================================

/**
 * @brief I2C2 instance for LPS22HB barometric sensor
 * @note STM32duino requires explicit TwoWire instance for non-default I2C bus
 * @note PB10 = I2C2_SCL, PB11 = I2C2_SDA on STM32F407VGT6
 */
#if defined(STM32F4xx) || defined(STM32F407xx)
  // Define I2C2 pins for SCG-ECU 2.0
  #ifndef PB10
    #define PB10 0x1A  // Port B, pin 10
  #endif
  #ifndef PB11
    #define PB11 0x1B  // Port B, pin 11
  #endif

  // Create I2C2 instance with explicit pins (SDA, SCL)
  static TwoWire WireI2C2(PB11, PB10);  // SDA=PB11, SCL=PB10
  #define LPS22HB_WIRE WireI2C2
#else
  // Non-STM32F4 platforms use default Wire
  #define LPS22HB_WIRE Wire
#endif

// =============================================================================
// PRIVATE VARIABLES
// =============================================================================

namespace {

/**
 * @brief Initialization status flag
 */
static volatile bool lps22hb_initialized = false;

/**
 * @brief Last successful read flag
 */
static volatile bool lps22hb_lastReadOk = false;

/**
 * @brief I2C clock speed (400 kHz fast mode)
 */
static constexpr uint32_t LPS22HB_I2C_CLOCK = 400000UL;

/**
 * @brief Timeout for I2C operations (milliseconds)
 */
static constexpr uint32_t LPS22HB_TIMEOUT_MS = 10UL;

} // anonymous namespace

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

/**
 * @brief Write a single byte to a register
 *
 * @param reg Register address
 * @param value Value to write
 * @return true if write successful
 */
static bool lps22hb_writeRegister(uint8_t reg, uint8_t value)
{
  LPS22HB_WIRE.beginTransmission(LPS22HB_I2C_ADDR);
  LPS22HB_WIRE.write(reg);
  LPS22HB_WIRE.write(value);
  return (LPS22HB_WIRE.endTransmission() == 0);
}

/**
 * @brief Read a single byte from a register
 *
 * @param reg Register address
 * @param value Pointer to store read value
 * @return true if read successful
 */
static bool lps22hb_readRegister(uint8_t reg, uint8_t* value)
{
  LPS22HB_WIRE.beginTransmission(LPS22HB_I2C_ADDR);
  LPS22HB_WIRE.write(reg);
  if (LPS22HB_WIRE.endTransmission() != 0) {
    return false;
  }

  if (LPS22HB_WIRE.requestFrom((uint8_t)LPS22HB_I2C_ADDR, (uint8_t)1) != 1) {
    return false;
  }

  *value = LPS22HB_WIRE.read();
  return true;
}

/**
 * @brief Read multiple bytes starting from a register (auto-increment)
 *
 * @param reg Starting register address
 * @param buffer Pointer to buffer for data
 * @param length Number of bytes to read
 * @return true if read successful
 */
static bool lps22hb_readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length)
{
  // Set auto-increment bit for multi-byte read
  LPS22HB_WIRE.beginTransmission(LPS22HB_I2C_ADDR);
  LPS22HB_WIRE.write(reg | 0x80);  // Bit 7 = auto-increment
  if (LPS22HB_WIRE.endTransmission() != 0) {
    return false;
  }

  if (LPS22HB_WIRE.requestFrom((uint8_t)LPS22HB_I2C_ADDR, length) != length) {
    return false;
  }

  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = LPS22HB_WIRE.read();
  }

  return true;
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

/**
 * @brief Initialize the LPS22HB sensor
 */
bool lps22hb_init(void)
{
  // Prevent re-initialization
  if (lps22hb_initialized) {
    return true;
  }

  // Initialize I2C bus
  LPS22HB_WIRE.begin();
  LPS22HB_WIRE.setClock(LPS22HB_I2C_CLOCK);

  // Small delay for sensor power-up
  delay(10);

  // Check WHO_AM_I register
  if (!lps22hb_isConnected()) {
    lps22hb_initialized = false;
    return false;
  }

  // Software reset
  lps22hb_softReset();
  delay(10);

  // Configure sensor:
  // - ODR = 25 Hz (recommended for automotive)
  // - BDU enabled (block data update - ensures coherent readings)
  // - Low-pass filter: ODR/9 bandwidth
  uint8_t ctrl1 = LPS22HB_ODR_25_HZ | LPS22HB_BDU_ENABLE | LPS22HB_LPF_ODR_9;
  if (!lps22hb_writeRegister(LPS22HB_REG_CTRL_REG1, ctrl1)) {
    lps22hb_initialized = false;
    return false;
  }

  // Enable auto-increment for burst reads
  if (!lps22hb_writeRegister(LPS22HB_REG_CTRL_REG2, LPS22HB_IF_ADD_INC)) {
    lps22hb_initialized = false;
    return false;
  }

  lps22hb_initialized = true;
  lps22hb_lastReadOk = true;

  return true;
}

/**
 * @brief Check if LPS22HB sensor is connected and responding
 */
bool lps22hb_isConnected(void)
{
  uint8_t whoami = 0;
  if (!lps22hb_readRegister(LPS22HB_REG_WHO_AM_I, &whoami)) {
    return false;
  }
  return (whoami == LPS22HB_WHO_AM_I_VALUE);
}

/**
 * @brief Read pressure as raw 24-bit value
 */
int32_t lps22hb_readPressureRaw(void)
{
  uint8_t data[3];

  if (!lps22hb_readRegisters(LPS22HB_REG_PRESS_OUT_XL, data, 3)) {
    lps22hb_lastReadOk = false;
    return 0;
  }

  // Combine 3 bytes into 24-bit signed value
  // Data format: XL (LSB), L, H (MSB)
  int32_t rawPressure = (int32_t)data[2] << 16 |
                        (int32_t)data[1] << 8 |
                        (int32_t)data[0];

  // Sign extension for negative values (though pressure should always be positive)
  if (rawPressure & 0x800000) {
    rawPressure |= 0xFF000000;
  }

  lps22hb_lastReadOk = true;
  return rawPressure;
}

/**
 * @brief Read pressure in hPa (hectopascals)
 */
float lps22hb_readPressureHPa(void)
{
  int32_t raw = lps22hb_readPressureRaw();
  if (!lps22hb_lastReadOk) {
    return 0.0f;
  }

  // Convert to hPa: raw / 4096.0
  return (float)raw / LPS22HB_PRESSURE_SCALE;
}

/**
 * @brief Read pressure in kPa (kilopascals)
 */
float lps22hb_readPressureKPa(void)
{
  // hPa / 10 = kPa
  return lps22hb_readPressureHPa() / 10.0f;
}

/**
 * @brief Read temperature as raw 16-bit value
 */
int16_t lps22hb_readTemperatureRaw(void)
{
  uint8_t data[2];

  if (!lps22hb_readRegisters(LPS22HB_REG_TEMP_OUT_L, data, 2)) {
    lps22hb_lastReadOk = false;
    return 0;
  }

  // Combine 2 bytes into 16-bit signed value
  int16_t rawTemp = (int16_t)data[1] << 8 | (int16_t)data[0];

  lps22hb_lastReadOk = true;
  return rawTemp;
}

/**
 * @brief Read temperature in degrees Celsius
 */
float lps22hb_readTemperature(void)
{
  int16_t raw = lps22hb_readTemperatureRaw();
  if (!lps22hb_lastReadOk) {
    return 0.0f;
  }

  // Convert to °C: raw / 100.0
  return (float)raw / LPS22HB_TEMP_SCALE;
}

/**
 * @brief Trigger one-shot measurement
 */
void lps22hb_triggerOneShot(void)
{
  uint8_t ctrl2;
  if (lps22hb_readRegister(LPS22HB_REG_CTRL_REG2, &ctrl2)) {
    ctrl2 |= LPS22HB_ONE_SHOT;
    lps22hb_writeRegister(LPS22HB_REG_CTRL_REG2, ctrl2);
  }
}

/**
 * @brief Check if new pressure data is available
 */
bool lps22hb_isPressureReady(void)
{
  uint8_t status;
  if (!lps22hb_readRegister(LPS22HB_REG_STATUS, &status)) {
    return false;
  }
  return (status & LPS22HB_STATUS_P_DA) != 0;
}

/**
 * @brief Check if new temperature data is available
 */
bool lps22hb_isTemperatureReady(void)
{
  uint8_t status;
  if (!lps22hb_readRegister(LPS22HB_REG_STATUS, &status)) {
    return false;
  }
  return (status & LPS22HB_STATUS_T_DA) != 0;
}

/**
 * @brief Set output data rate
 */
void lps22hb_setODR(lps22hb_odr_t odr)
{
  uint8_t ctrl1;
  if (lps22hb_readRegister(LPS22HB_REG_CTRL_REG1, &ctrl1)) {
    // Clear ODR bits (bits 6:4) and set new value
    ctrl1 &= 0x0F;
    ctrl1 |= odr;
    lps22hb_writeRegister(LPS22HB_REG_CTRL_REG1, ctrl1);
  }
}

/**
 * @brief Perform software reset
 */
void lps22hb_softReset(void)
{
  lps22hb_writeRegister(LPS22HB_REG_CTRL_REG2, LPS22HB_SWRESET);
}

/**
 * @brief Get barometric pressure for Speeduino (in kPa * 2)
 *
 * This is the main function called by Speeduino's sensors module.
 * Returns pressure in the same format as analog BARO sensors.
 *
 * Speeduino uses kPa with 0.5 kPa resolution (value * 2).
 * Example: 101.5 kPa -> returns 203
 *
 * @return Pressure in kPa * 2, or 0 if error
 */
uint8_t lps22hb_getBaroForSpeeduino(void)
{
  if (!lps22hb_initialized) {
    return 0;
  }

  float pressureKPa = lps22hb_readPressureKPa();

  if (!lps22hb_lastReadOk || pressureKPa < 26.0f || pressureKPa > 126.0f) {
    // Invalid reading - outside sensor range
    return 0;
  }

  // Convert to Speeduino format (kPa, uint8_t, 0-255 range)
  // Speeduino baro is typically 0-255 representing ~50-155 kPa
  // For standard atmospheric range (85-108 kPa), we return kPa directly
  // as that fits nicely in uint8_t

  // Clamp to valid atmospheric range
  if (pressureKPa < 50.0f) { pressureKPa = 50.0f; }
  if (pressureKPa > 115.0f) { pressureKPa = 115.0f; }

  return (uint8_t)(pressureKPa + 0.5f);  // Round to nearest kPa
}

/**
 * @brief Check if LPS22HB driver is initialized and working
 */
bool lps22hb_isInitialized(void)
{
  return lps22hb_initialized && lps22hb_lastReadOk;
}

#else // Non-STM32 platforms - stub implementations

bool lps22hb_init(void) { return false; }
bool lps22hb_isConnected(void) { return false; }
float lps22hb_readPressureHPa(void) { return 0.0f; }
float lps22hb_readPressureKPa(void) { return 0.0f; }
int32_t lps22hb_readPressureRaw(void) { return 0; }
float lps22hb_readTemperature(void) { return 0.0f; }
int16_t lps22hb_readTemperatureRaw(void) { return 0; }
void lps22hb_triggerOneShot(void) { }
bool lps22hb_isPressureReady(void) { return false; }
bool lps22hb_isTemperatureReady(void) { return false; }
void lps22hb_setODR(lps22hb_odr_t odr) { (void)odr; }
void lps22hb_softReset(void) { }
uint8_t lps22hb_getBaroForSpeeduino(void) { return 0; }
bool lps22hb_isInitialized(void) { return false; }

#endif // CORE_STM32
