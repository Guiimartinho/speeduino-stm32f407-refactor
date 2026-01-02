/**
 * @file storage_safety.cpp
 * @brief Storage safety implementation - WAL, CRC verification, corruption detection
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 */

#include "storage_safety.h"
#include "globals.h"
#include "storage.h"
#include "page_crc.h"
#include "table3d.h"
#include EEPROM_LIB_H

// =============================================================================
// PRIVATE STATE
// =============================================================================

static uint8_t corruptedPageCount = 0;       ///< Pages detected as corrupted at boot
static uint32_t referenceCRCs[NUM_3D_TABLES_TO_VERIFY] = {0};  ///< Reference CRCs for 3D tables
static bool referenceCRCsValid = false;      ///< True after capture
static bool storageCorrupted = false;        ///< True if any corruption detected

// Table names for diagnostics
static const char* const tableNames[NUM_3D_TABLES_TO_VERIFY] = {
    "fuelTable",
    "fuelTable2",
    "ignitionTable",
    "ignitionTable2",
    "afrTable",
    "boostTable",
    "vvtTable"
};

// =============================================================================
// CRC32 CALCULATION FOR 3D TABLES
// =============================================================================

/**
 * @brief Simple CRC32 for a memory block
 */
static uint32_t calculateCRC32(const uint8_t* data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1U) { crc = (crc >> 1) ^ 0xEDB88320U; }
            else { crc >>= 1; }
        }
    }
    return ~crc;
}

/**
 * @brief Calculate CRC for a 16x16 3D table
 */
static uint32_t calculate3DTableCRC16x16(void* pTable)
{
    // table3d16RpmLoad is 256 values + 16 X axis + 16 Y axis = 288 bytes
    return calculateCRC32((const uint8_t*)pTable, 288);
}

/**
 * @brief Calculate CRC for an 8x8 3D table
 */
static uint32_t calculate3DTableCRC8x8(void* pTable)
{
    // table3d8RpmLoad is 64 values + 8 X axis + 8 Y axis = 80 bytes
    return calculateCRC32((const uint8_t*)pTable, 80);
}

/**
 * @brief Get CRC for table by index
 */
static uint32_t getTableCRC(uint8_t index)
{
    switch (index)
    {
        case 0: return calculate3DTableCRC16x16(&fuelTable);
        case 1: return calculate3DTableCRC16x16(&fuelTable2);
        case 2: return calculate3DTableCRC16x16(&ignitionTable);
        case 3: return calculate3DTableCRC16x16(&ignitionTable2);
        case 4: return calculate3DTableCRC16x16(&afrTable);
        case 5: return calculate3DTableCRC8x8(&boostTable);
        case 6: return calculate3DTableCRC8x8(&vvtTable);
        default: return 0;
    }
}

// =============================================================================
// WAL (Write-Ahead Log) IMPLEMENTATION
// =============================================================================

uint8_t walInit(void)
{
    corruptedPageCount = 0;

    // Check for WAL magic number
    uint16_t magic = 0;
    EEPROM.get(EEPROM_WAL_MAGIC, magic);

    if (magic != WAL_MAGIC_NUMBER)
    {
        // WAL not initialized - initialize now
        magic = WAL_MAGIC_NUMBER;
        EEPROM.put(EEPROM_WAL_MAGIC, magic);

        // Set all pages as unknown (0xFF = erased state)
        for (uint8_t i = 0; i < 15; i++)
        {
            EEPROM.write(EEPROM_WAL_PAGE_STATUS + i, WAL_STATUS_UNKNOWN);
        }

        // Initialize counters
        uint32_t zero = 0;
        EEPROM.put(EEPROM_WAL_WRITE_COUNT, zero);
        EEPROM.put(EEPROM_WAL_FAIL_COUNT, zero);
        EEPROM.write(EEPROM_WAL_LAST_VALID, 0);

        return 0;
    }

    // WAL exists - check for incomplete writes
    for (uint8_t i = 0; i < 15; i++)
    {
        uint8_t status = EEPROM.read(EEPROM_WAL_PAGE_STATUS + i);
        if (status == WAL_STATUS_WRITING)
        {
            // Page was being written when power failed
            corruptedPageCount++;
            storageCorrupted = true;

            // Increment fail counter
            uint32_t failCount = 0;
            EEPROM.get(EEPROM_WAL_FAIL_COUNT, failCount);
            if (failCount < 0xFFFFFFFE) { failCount++; }
            EEPROM.put(EEPROM_WAL_FAIL_COUNT, failCount);
        }
    }

    return corruptedPageCount;
}

void walBeginWrite(uint8_t pageNum)
{
    if (pageNum < 1 || pageNum > 15) { return; }

    // Mark page as "writing"
    EEPROM.write(EEPROM_WAL_PAGE_STATUS + (pageNum - 1), WAL_STATUS_WRITING);
}

void walCommitWrite(uint8_t pageNum)
{
    if (pageNum < 1 || pageNum > 15) { return; }

    // Mark page as "valid"
    EEPROM.write(EEPROM_WAL_PAGE_STATUS + (pageNum - 1), WAL_STATUS_VALID);
    EEPROM.write(EEPROM_WAL_LAST_VALID, pageNum);

    // Increment write counter
    uint32_t writeCount = 0;
    EEPROM.get(EEPROM_WAL_WRITE_COUNT, writeCount);
    if (writeCount < 0xFFFFFFFE) { writeCount++; }
    EEPROM.put(EEPROM_WAL_WRITE_COUNT, writeCount);
}

bool walIsPageValid(uint8_t pageNum)
{
    if (pageNum < 1 || pageNum > 15) { return false; }

    uint8_t status = EEPROM.read(EEPROM_WAL_PAGE_STATUS + (pageNum - 1));
    return (status == WAL_STATUS_VALID);
}

uint8_t walGetCorruptedPageCount(void)
{
    return corruptedPageCount;
}

uint32_t walGetWriteCount(void)
{
    uint32_t count = 0;
    EEPROM.get(EEPROM_WAL_WRITE_COUNT, count);
    if (count == 0xFFFFFFFF) { count = 0; }
    return count;
}

uint32_t walGetFailCount(void)
{
    uint32_t count = 0;
    EEPROM.get(EEPROM_WAL_FAIL_COUNT, count);
    if (count == 0xFFFFFFFF) { count = 0; }
    return count;
}

// =============================================================================
// 3D TABLE CRC VERIFICATION
// =============================================================================

void table3dCaptureReferenceCRCs(void)
{
    for (uint8_t i = 0; i < NUM_3D_TABLES_TO_VERIFY; i++)
    {
        referenceCRCs[i] = getTableCRC(i);
    }
    referenceCRCsValid = true;
}

crc_verify_result_t table3dVerifyCRCs(void)
{
    if (!referenceCRCsValid) { return CRC_VERIFY_NOT_INIT; }

    for (uint8_t i = 0; i < NUM_3D_TABLES_TO_VERIFY; i++)
    {
        uint32_t currentCRC = getTableCRC(i);
        if (currentCRC != referenceCRCs[i])
        {
            storageCorrupted = true;
            return CRC_VERIFY_MISMATCH;
        }
    }

    return CRC_VERIFY_OK;
}

uint8_t table3dGetCorruptedIndex(void)
{
    if (!referenceCRCsValid) { return 0xFF; }

    for (uint8_t i = 0; i < NUM_3D_TABLES_TO_VERIFY; i++)
    {
        uint32_t currentCRC = getTableCRC(i);
        if (currentCRC != referenceCRCs[i]) { return i; }
    }

    return 0xFF;
}

const char* table3dGetName(uint8_t index)
{
    if (index >= NUM_3D_TABLES_TO_VERIFY) { return "unknown"; }
    return tableNames[index];
}

void table3dReloadFromEEPROM(void)
{
    // Reload tables from EEPROM
    // This uses the same loading logic as loadConfig() but only for tables
    loadTable(&fuelTable, decltype(fuelTable)::type_key, EEPROM_CONFIG1_MAP);
    loadTable(&ignitionTable, decltype(ignitionTable)::type_key, EEPROM_CONFIG3_MAP);
    loadTable(&afrTable, decltype(afrTable)::type_key, EEPROM_CONFIG5_MAP);
    loadTable(&boostTable, decltype(boostTable)::type_key, EEPROM_CONFIG7_MAP1);
    loadTable(&vvtTable, decltype(vvtTable)::type_key, EEPROM_CONFIG7_MAP2);
    loadTable(&fuelTable2, decltype(fuelTable2)::type_key, EEPROM_CONFIG11_MAP);
    loadTable(&ignitionTable2, decltype(ignitionTable2)::type_key, EEPROM_CONFIG14_MAP);

    // Recapture reference CRCs
    table3dCaptureReferenceCRCs();
}

// =============================================================================
// INITIALIZATION AND PERIODIC CHECKS
// =============================================================================

void storageSafetyInit(void)
{
    // Initialize WAL (Write-Ahead Log)
    walInit();

    // Capture reference CRCs for 3D tables
    // Note: This should be called after loadConfig() has loaded tables
    table3dCaptureReferenceCRCs();
}

// Counter for periodic verification (called at 1Hz)
static uint8_t periodicCheckCounter = 0;
static constexpr uint8_t CRC_CHECK_INTERVAL_SEC = 10U;  // Check every 10 seconds

bool storageSafetyPeriodicCheck(void)
{
    periodicCheckCounter++;

    // Check 3D table CRCs every 10 seconds
    if (periodicCheckCounter >= CRC_CHECK_INTERVAL_SEC)
    {
        periodicCheckCounter = 0;

        crc_verify_result_t result = table3dVerifyCRCs();
        if (result == CRC_VERIFY_MISMATCH)
        {
            // Corruption detected - reload from EEPROM
            table3dReloadFromEEPROM();
            return false;
        }
    }

    return !storageCorrupted;
}

bool isStorageCorrupted(void)
{
    return storageCorrupted;
}

// loadTable is declared in storage.h
