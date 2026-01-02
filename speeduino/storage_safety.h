/**
 * @file storage_safety.h
 * @brief Storage safety features - WAL, CRC verification, corruption detection
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 *
 * This module provides three safety mechanisms:
 *
 * 1. **Write-Ahead Log (WAL)** for Config Pages:
 *    - Marks page as "writing" before write operation
 *    - Marks page as "valid" after successful write with CRC
 *    - Detects incomplete writes on boot (power loss during write)
 *
 * 2. **Periodic 3D Table CRC Verification**:
 *    - Stores reference CRCs when tables are loaded from EEPROM
 *    - Periodically recalculates CRCs to detect RAM corruption
 *    - Optionally reloads from EEPROM on mismatch
 *
 * 3. **Corruption Detection and Recovery**:
 *    - Detects corrupted config pages on boot
 *    - Sets status flags for logging/diagnostics
 *    - Optionally loads factory defaults on corruption
 *
 * @note Uses EEPROM addresses 3464-3490 (27 bytes) for WAL metadata
 * @note 3D table CRCs stored in RAM (no EEPROM overhead)
 *
 * @author SCG-ECU Team
 * @date 2025-01-02
 * @version 1.0
 */

#ifndef STORAGE_SAFETY_H
#define STORAGE_SAFETY_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// EEPROM LAYOUT FOR WAL METADATA
// =============================================================================
// Located in empty space: 3457-3674 (217 bytes available)
// EEPROM_WEAR_COUNTER is at 3460-3463 (4 bytes)
// We use 3464-3490 (27 bytes) for WAL

#define EEPROM_WAL_BASE         3464U   ///< Base address for WAL metadata
#define EEPROM_WAL_MAGIC        3464U   ///< 2-byte magic number (0x5741 = "WA")
#define EEPROM_WAL_PAGE_STATUS  3466U   ///< 15 bytes: status for pages 1-15
#define EEPROM_WAL_LAST_VALID   3481U   ///< 1 byte: last successfully written page
#define EEPROM_WAL_WRITE_COUNT  3482U   ///< 4 bytes: total successful writes
#define EEPROM_WAL_FAIL_COUNT   3486U   ///< 4 bytes: total failed/interrupted writes
#define EEPROM_WAL_END          3490U   ///< End of WAL area

// WAL Magic number to detect initialized WAL
#define WAL_MAGIC_NUMBER        0x5741U  ///< "WA" in little-endian

// Page status values
#define WAL_STATUS_UNKNOWN      0xFFU   ///< Page never written (erased EEPROM)
#define WAL_STATUS_WRITING      0x01U   ///< Write in progress (incomplete)
#define WAL_STATUS_VALID        0x00U   ///< Write complete, CRC verified

// =============================================================================
// 3D TABLE CRC VERIFICATION
// =============================================================================

/**
 * @brief Number of 3D tables to verify
 *
 * Tables verified:
 * - fuelTable (16x16)
 * - fuelTable2 (16x16)
 * - ignitionTable (16x16)
 * - ignitionTable2 (16x16)
 * - afrTable (16x16)
 * - boostTable (8x8)
 * - vvtTable (8x8)
 */
#define NUM_3D_TABLES_TO_VERIFY  7U

/**
 * @brief CRC verification result
 */
typedef enum {
    CRC_VERIFY_OK = 0,          ///< All CRCs match
    CRC_VERIFY_MISMATCH = 1,    ///< One or more CRCs don't match
    CRC_VERIFY_NOT_INIT = 2     ///< Reference CRCs not yet captured
} crc_verify_result_t;

// =============================================================================
// PUBLIC API - WAL (Write-Ahead Log)
// =============================================================================

/**
 * @brief Initialize WAL subsystem
 *
 * Called once at startup to:
 * 1. Check if WAL is initialized (magic number present)
 * 2. If not, initialize WAL with default values
 * 3. Check for any pages in "writing" state (incomplete writes)
 *
 * @return Number of pages detected as corrupted (incomplete writes)
 */
uint8_t walInit(void);

/**
 * @brief Mark page as "writing" before starting write operation
 *
 * Must be called BEFORE writeConfig(pageNum) to enable crash detection.
 *
 * @param pageNum Page number (1-15)
 */
void walBeginWrite(uint8_t pageNum);

/**
 * @brief Mark page as "valid" after successful write
 *
 * Must be called AFTER writeConfig(pageNum) completes successfully.
 * Also stores the page CRC for verification.
 *
 * @param pageNum Page number (1-15)
 */
void walCommitWrite(uint8_t pageNum);

/**
 * @brief Check if a page is in valid state
 *
 * @param pageNum Page number (1-15)
 * @return true if page is valid, false if corrupted or never written
 */
bool walIsPageValid(uint8_t pageNum);

/**
 * @brief Get count of corrupted pages detected at boot
 *
 * @return Number of pages in "writing" state at boot (power loss during write)
 */
uint8_t walGetCorruptedPageCount(void);

/**
 * @brief Get total successful write count
 *
 * @return Number of successful page writes since WAL initialized
 */
uint32_t walGetWriteCount(void);

/**
 * @brief Get total failed/interrupted write count
 *
 * @return Number of interrupted writes detected at boot
 */
uint32_t walGetFailCount(void);

// =============================================================================
// PUBLIC API - 3D Table CRC Verification
// =============================================================================

/**
 * @brief Capture reference CRCs for all 3D tables
 *
 * Should be called after loadConfig() to capture the known-good CRCs.
 * These are stored in RAM and used for periodic verification.
 */
void table3dCaptureReferenceCRCs(void);

/**
 * @brief Verify current 3D table CRCs against reference
 *
 * Recalculates CRCs for all 3D tables and compares to reference.
 *
 * @return CRC_VERIFY_OK if all match, CRC_VERIFY_MISMATCH if corruption detected
 */
crc_verify_result_t table3dVerifyCRCs(void);

/**
 * @brief Get index of first corrupted table (if any)
 *
 * @return Table index (0-6) if corruption detected, 0xFF if no corruption
 */
uint8_t table3dGetCorruptedIndex(void);

/**
 * @brief Get name of table by index (for diagnostics)
 *
 * @param index Table index (0-6)
 * @return Pointer to table name string
 */
const char* table3dGetName(uint8_t index);

/**
 * @brief Reload 3D tables from EEPROM
 *
 * Use this to recover from RAM corruption.
 * @note This will overwrite any pending tune changes!
 */
void table3dReloadFromEEPROM(void);

// =============================================================================
// INTEGRATION HELPERS
// =============================================================================

/**
 * @brief Initialize all storage safety subsystems
 *
 * Should be called once at startup, after loadConfig().
 * Initializes:
 * 1. WAL (Write-Ahead Log)
 * 2. Reference CRCs for 3D tables
 */
void storageSafetyInit(void);

/**
 * @brief Perform periodic storage safety checks
 *
 * Should be called from main loop at low frequency (e.g., 1Hz).
 * Performs:
 * 1. 3D table CRC verification (every 10 seconds)
 *
 * @return true if all checks pass, false if corruption detected
 */
bool storageSafetyPeriodicCheck(void);

/**
 * @brief Check if storage integrity is compromised
 *
 * @return true if any corruption has been detected since boot
 */
bool isStorageCorrupted(void);

#endif // STORAGE_SAFETY_H
