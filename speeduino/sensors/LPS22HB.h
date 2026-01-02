/**
 * @file LPS22HB.h
 * @brief LPS22HB/LPS22HBTR I2C Barometric Pressure Sensor Driver
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 *
 * The LPS22HB is a MEMS barometric pressure sensor from STMicroelectronics.
 * Used for altitude compensation (atmospheric pressure measurement).
 *
 * Features:
 * - Range: 260-1260 hPa (26-126 kPa)
 * - Resolution: 24-bit pressure, 16-bit temperature
 * - Accuracy: ±0.1 hPa (±10 Pa)
 * - Interface: I2C (up to 400kHz) or SPI
 * - Ultra-low power: 3µA @ 1Hz ODR
 *
 * Hardware Connection (SCG-ECU 2.0):
 * - I2C2: PB10 = SCL, PB11 = SDA
 * - I2C Address: 0x5C (SA0=GND) or 0x5D (SA0=VCC)
 *
 * @note This sensor measures ATMOSPHERIC pressure, not manifold pressure!
 *       Use external MAP sensor (0-5V analog) for manifold pressure.
 *
 * @author SCG-ECU Team
 * @date 2026-01-02
 * @version 1.0
 */

#ifndef LPS22HB_H
#define LPS22HB_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// I2C CONFIGURATION
// =============================================================================

/**
 * @brief LPS22HB I2C address options
 * @note SA0 pin determines address: GND=0x5C, VCC=0x5D
 */
#define LPS22HB_I2C_ADDR_LOW    0x5C  ///< SA0 = GND (default on most boards)
#define LPS22HB_I2C_ADDR_HIGH   0x5D  ///< SA0 = VCC

/**
 * @brief Default I2C address for SCG-ECU 2.0
 * @note Change if your hardware uses different SA0 connection
 */
#define LPS22HB_I2C_ADDR        LPS22HB_I2C_ADDR_LOW

// =============================================================================
// REGISTER MAP
// =============================================================================

#define LPS22HB_REG_INTERRUPT_CFG   0x0B  ///< Interrupt configuration
#define LPS22HB_REG_THS_P_L         0x0C  ///< Pressure threshold (low byte)
#define LPS22HB_REG_THS_P_H         0x0D  ///< Pressure threshold (high byte)
#define LPS22HB_REG_WHO_AM_I        0x0F  ///< Device ID (should return 0xB1)
#define LPS22HB_REG_CTRL_REG1       0x10  ///< Control register 1 (ODR, BDU)
#define LPS22HB_REG_CTRL_REG2       0x11  ///< Control register 2 (boot, reset)
#define LPS22HB_REG_CTRL_REG3       0x12  ///< Control register 3 (interrupts)
#define LPS22HB_REG_FIFO_CTRL       0x14  ///< FIFO control
#define LPS22HB_REG_REF_P_XL        0x15  ///< Reference pressure (XL byte)
#define LPS22HB_REG_REF_P_L         0x16  ///< Reference pressure (L byte)
#define LPS22HB_REG_REF_P_H         0x17  ///< Reference pressure (H byte)
#define LPS22HB_REG_RPDS_L          0x18  ///< Pressure offset (low byte)
#define LPS22HB_REG_RPDS_H          0x19  ///< Pressure offset (high byte)
#define LPS22HB_REG_RES_CONF        0x1A  ///< Resolution configuration
#define LPS22HB_REG_INT_SOURCE      0x25  ///< Interrupt source
#define LPS22HB_REG_FIFO_STATUS     0x26  ///< FIFO status
#define LPS22HB_REG_STATUS          0x27  ///< Status register
#define LPS22HB_REG_PRESS_OUT_XL    0x28  ///< Pressure output (XL byte, LSB)
#define LPS22HB_REG_PRESS_OUT_L     0x29  ///< Pressure output (L byte)
#define LPS22HB_REG_PRESS_OUT_H     0x2A  ///< Pressure output (H byte, MSB)
#define LPS22HB_REG_TEMP_OUT_L      0x2B  ///< Temperature output (low byte)
#define LPS22HB_REG_TEMP_OUT_H      0x2C  ///< Temperature output (high byte)
#define LPS22HB_REG_LPFP_RES        0x33  ///< Low-pass filter reset

// =============================================================================
// DEVICE IDENTIFICATION
// =============================================================================

#define LPS22HB_WHO_AM_I_VALUE      0xB1  ///< Expected WHO_AM_I response

// =============================================================================
// CTRL_REG1 CONFIGURATION
// =============================================================================

/**
 * @brief Output Data Rate (ODR) settings
 */
typedef enum {
    LPS22HB_ODR_POWER_DOWN  = 0x00,  ///< Power-down / one-shot mode
    LPS22HB_ODR_1_HZ        = 0x10,  ///< 1 Hz
    LPS22HB_ODR_10_HZ       = 0x20,  ///< 10 Hz
    LPS22HB_ODR_25_HZ       = 0x30,  ///< 25 Hz (recommended for automotive)
    LPS22HB_ODR_50_HZ       = 0x40,  ///< 50 Hz
    LPS22HB_ODR_75_HZ       = 0x50   ///< 75 Hz
} lps22hb_odr_t;

/**
 * @brief Low-pass filter configuration
 */
typedef enum {
    LPS22HB_LPF_DISABLED    = 0x00,  ///< Filter disabled (ODR/2 bandwidth)
    LPS22HB_LPF_ODR_9       = 0x08,  ///< ODR/9 bandwidth
    LPS22HB_LPF_ODR_20      = 0x0C   ///< ODR/20 bandwidth
} lps22hb_lpf_t;

#define LPS22HB_BDU_ENABLE          0x02  ///< Block Data Update (recommended)

// =============================================================================
// CTRL_REG2 CONFIGURATION
// =============================================================================

#define LPS22HB_BOOT                0x80  ///< Reboot memory content
#define LPS22HB_FIFO_EN             0x40  ///< FIFO enable
#define LPS22HB_STOP_ON_FTH         0x20  ///< Stop on FIFO threshold
#define LPS22HB_IF_ADD_INC          0x10  ///< Auto-increment address (for burst read)
#define LPS22HB_I2C_DIS             0x08  ///< Disable I2C (SPI only)
#define LPS22HB_SWRESET             0x04  ///< Software reset
#define LPS22HB_ONE_SHOT            0x01  ///< One-shot measurement trigger

// =============================================================================
// STATUS REGISTER FLAGS
// =============================================================================

#define LPS22HB_STATUS_T_OR         0x20  ///< Temperature overrun
#define LPS22HB_STATUS_P_OR         0x10  ///< Pressure overrun
#define LPS22HB_STATUS_T_DA         0x02  ///< Temperature data available
#define LPS22HB_STATUS_P_DA         0x01  ///< Pressure data available

// =============================================================================
// CONVERSION CONSTANTS
// =============================================================================

/**
 * @brief Pressure conversion factor
 * @note Raw value is in 1/4096 hPa, divide by 4096 to get hPa
 *       1 hPa = 100 Pa = 0.1 kPa
 */
#define LPS22HB_PRESSURE_SCALE      4096.0f

/**
 * @brief Temperature conversion factor
 * @note Raw value is in 1/100 °C
 */
#define LPS22HB_TEMP_SCALE          100.0f

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the LPS22HB sensor
 *
 * Performs:
 * 1. I2C bus initialization
 * 2. Device identification (WHO_AM_I check)
 * 3. Configuration (ODR, BDU, filter settings)
 *
 * @return true if initialization successful, false if sensor not found
 */
bool lps22hb_init(void);

/**
 * @brief Check if LPS22HB sensor is connected and responding
 *
 * @return true if WHO_AM_I returns expected value
 */
bool lps22hb_isConnected(void);

/**
 * @brief Read pressure in hPa (hectopascals)
 *
 * @return Pressure in hPa (e.g., 1013.25 for sea level)
 * @note Returns 0.0 if read fails
 */
float lps22hb_readPressureHPa(void);

/**
 * @brief Read pressure in kPa (kilopascals)
 *
 * @return Pressure in kPa (e.g., 101.325 for sea level)
 * @note Returns 0.0 if read fails
 */
float lps22hb_readPressureKPa(void);

/**
 * @brief Read pressure as raw 24-bit value
 *
 * @return Raw pressure value (divide by 4096 for hPa)
 */
int32_t lps22hb_readPressureRaw(void);

/**
 * @brief Read temperature in degrees Celsius
 *
 * @return Temperature in °C
 * @note Returns 0.0 if read fails
 */
float lps22hb_readTemperature(void);

/**
 * @brief Read temperature as raw 16-bit value
 *
 * @return Raw temperature value (divide by 100 for °C)
 */
int16_t lps22hb_readTemperatureRaw(void);

/**
 * @brief Trigger one-shot measurement (for power-down mode)
 *
 * In power-down mode, call this to trigger a single measurement.
 * Wait for data ready, then read pressure/temperature.
 */
void lps22hb_triggerOneShot(void);

/**
 * @brief Check if new pressure data is available
 *
 * @return true if pressure data ready
 */
bool lps22hb_isPressureReady(void);

/**
 * @brief Check if new temperature data is available
 *
 * @return true if temperature data ready
 */
bool lps22hb_isTemperatureReady(void);

/**
 * @brief Set output data rate
 *
 * @param odr Output data rate (see lps22hb_odr_t)
 */
void lps22hb_setODR(lps22hb_odr_t odr);

/**
 * @brief Perform software reset
 *
 * Resets all registers to default values.
 */
void lps22hb_softReset(void);

/**
 * @brief Get barometric pressure for Speeduino (in kPa * 2)
 *
 * This is the main function called by Speeduino's sensors module.
 * Returns pressure in the same format as analog BARO sensors.
 *
 * @return Pressure in kPa * 2 (e.g., 203 for 101.5 kPa)
 *         Returns 0 if sensor not initialized or read error
 */
uint8_t lps22hb_getBaroForSpeeduino(void);

/**
 * @brief Check if LPS22HB driver is initialized and working
 *
 * @return true if sensor is initialized and last read was successful
 */
bool lps22hb_isInitialized(void);

#endif // LPS22HB_H
