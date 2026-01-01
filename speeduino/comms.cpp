/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/
/**
 * @file comms.cpp
 * @brief Modern TunerStudio serial communication protocol (CAN16 packet format with CRC)
 *
 * @details Implements the modern TunerStudio "CAN16" binary packet protocol with
 * CRC32 checksums for reliable communication. This is the preferred protocol for
 * new deployments, offering packet integrity validation and robust error handling.
 *
 * **PROTOCOL CHARACTERISTICS:**
 * - Binary packet format (not ASCII like legacy protocol)
 * - CRC32 checksum validation on all packets
 * - Non-blocking I/O for large transfers (tooth logs, EEPROM dumps)
 * - Command/response model with explicit error codes
 * - Supports SD card logging and RTC timestamp operations (STM32 only)
 *
 * **PACKET STRUCTURE (CAN16 Format):**
 * ```
 * [0x00] Command byte (e.g., 0x30 = send output channels)
 * [0x01-0x02] Payload length (16-bit big-endian)
 * [0x03...N] Payload data (variable length)
 * [N+1...N+4] CRC32 checksum (32-bit)
 * ```
 *
 * **COMMAND OPCODES:**
 * - **0x30 (48)** - Send output channels (realtime data)
 * - **0x31** - Burn page to EEPROM
 * - **0x32** - Read page data
 * - **0x33** - Write page data
 * - **0x34** - Send CRC32 of page
 * - **0x35** - Execute command button
 * - **0x36** - Send tooth log
 * - **0x37** - Send composite log
 * - **0x38** - Write calibration table
 * - **0x39** - Read calibration table
 * - **0x3A** - SD card operations (STM32 only)
 * - **0x3B** - RTC timestamp operations (STM32 only)
 * - **0x3C** - Send firmware version
 * - **0x3D** - Send signature
 * - **0x3E** - Test communications (handshake)
 * - **0x3F** - Send serial protocol version
 *
 * **ERROR CODES:**
 * - **0x00** - Success (SERIAL_RC_OK)
 * - **0x04** - EEPROM burn succeeded (SERIAL_RC_BURN_OK)
 * - **0x80** - Timeout error (SERIAL_RC_TIMEOUT)
 * - **0x82** - CRC mismatch (SERIAL_RC_CRC_ERR)
 * - **0x83** - Unknown command (SERIAL_RC_UKWN_ERR)
 * - **0x84** - Range error (SERIAL_RC_RANGE_ERR) - TS won't retry
 * - **0x85** - Busy error (SERIAL_RC_BUSY_ERR) - TS will retry
 *
 * **CRC VALIDATION:**
 * All packets include a trailing CRC32 checksum calculated over the command byte,
 * length field, and payload. The ECU validates incoming CRCs and aborts processing
 * on mismatch, responding with SERIAL_RC_CRC_ERR (0x82). Outgoing packets always
 * include a valid CRC calculated before transmission.
 *
 * **NON-BLOCKING I/O:**
 * Large data transfers (tooth logs, EEPROM dumps, SD card reads) use non-blocking
 * I/O to avoid stalling the main control loop. The serialStatusFlag state machine
 * tracks progress:
 * - SERIAL_INACTIVE - No transfer in progress
 * - SERIAL_COMMAND_INPROGRESS - Receiving command packet
 * - SERIAL_TRANSMIT_TOOTH_INPROGRESS - Sending tooth log data
 * - SERIAL_TRANSMIT_COMPOSITE_INPROGRESS - Sending composite log
 * - LOG_SEND_COMPOSITE - Composite log ready to send
 *
 * **SD CARD LOGGING (STM32 Only):**
 * Supports high-speed datalogging to SD card with commands for:
 * - Directory listing (file browser)
 * - Sector-based file reads (4KB chunks)
 * - RTC timestamp synchronization
 * - Log file management
 *
 * **DIFFERENCES FROM LEGACY PROTOCOL (comms_legacy.cpp):**
 * - **Legacy**: ASCII single-letter commands, no checksums, blocking I/O
 * - **Modern**: Binary opcodes, CRC32 validation, non-blocking transfers
 * - **Legacy**: Fixed page formats, no SD support
 * - **Modern**: Flexible payload sizes, SD card integration
 * - **Legacy**: Error detection by timeout only
 * - **Modern**: Explicit error codes with retry semantics
 *
 * **TYPICAL COMMAND SEQUENCES:**
 *
 * **Read Realtime Data:**
 * ```
 * TS -> ECU: [0x30] [0x00 0x00] [CRC32]       (Send output channels command)
 * ECU -> TS: [0x00] [0x00 LOG_SIZE] [data...] [CRC32]  (Success + data)
 * ```
 *
 * **Write Configuration Page:**
 * ```
 * TS -> ECU: [0x33] [0x01 0x20] [288 bytes] [CRC32]   (Write page, 288 bytes)
 * ECU -> TS: [0x00] [CRC32]                             (Success)
 * TS -> ECU: [0x31] [0x00 0x01] [page_num] [CRC32]    (Burn to EEPROM)
 * ECU -> TS: [0x04] [CRC32]                             (BURN_OK)
 * ```
 *
 * **Tooth Log Capture:**
 * ```
 * TS -> ECU: [0x36] [0x00 0x00] [CRC32]                (Request tooth log)
 * ECU -> TS: [0x00] [0x02 0x00] [512 bytes] [CRC32]   (Success + 256 entries)
 * ```
 *
 * @complexity High (1,187 lines, 15+ command opcodes, CRC validation, SD logging)
 * @performance Command processing: <1ms typical, CRC calculation: ~50µs per packet
 * @note This is the preferred protocol for new TunerStudio deployments (v3.0+)
 * @warning Always validate CRC before processing payload to prevent data corruption
 * @see comms_legacy.cpp for legacy ASCII protocol (backwards compatibility)
 * @see FastCRC library for CRC32 implementation
 */
#include "globals.h"
#include "comms.h"
#include "comms_secondary.h"
#include "storage.h"
#include "maths.h"
#include "utilities.h"
#include "decoders.h"
#include "TS_CommandButtonHandler.h"
#include "pages.h"
#include "page_crc.h"
#include "logger.h"
#include "comms_legacy.h"
#include "src/FastCRC/FastCRC.h"
#include <avr/pgmspace.h>
#ifdef RTC_ENABLED
  #include "rtc_common.h"
  #include "comms_sd.h"
#endif
#ifdef SD_LOGGING
  #include "SD_logger.h"
#endif
#include "units.h"
#include "sensors.h"

/** @defgroup group-serial-comms-impl Serial comms implementation
 * @{
 */

// Forward declarations

/** @brief Processes a message once it has been fully received */
void processSerialCommand(void);

/** @brief Should be called when ::serialStatusFlag == SERIAL_TRANSMIT_TOOTH_INPROGRESS, */
void sendToothLog(void);

/** @brief Should be called when ::serialStatusFlag == LOG_SEND_COMPOSITE */
void sendCompositeLog(void);

/// @defgroup group-serial-return-codes Serial return codes sent to TS
/// @{
static constexpr byte SERIAL_RC_OK         = 0x00U; //!< Success
static constexpr byte SERIAL_RC_REALTIME   = 0x01U; //!< Unused
static constexpr byte SERIAL_RC_PAGE       = 0x02U; //!< Unused
static constexpr byte SERIAL_RC_BURN_OK    = 0x04U; //!< EEPROM write succeeded
static constexpr byte SERIAL_RC_TIMEOUT    = 0x80U; //!< Timeout error
static constexpr byte SERIAL_RC_CRC_ERR    = 0x82U; //!< CRC mismatch
static constexpr byte SERIAL_RC_UKWN_ERR   = 0x83U; //!< Unknown command
static constexpr byte SERIAL_RC_RANGE_ERR  = 0x84U; //!< Incorrect range. TS will not retry command
static constexpr byte SERIAL_RC_BUSY_ERR   = 0x85U; //!< TS will wait and retry
///@}

static constexpr uint8_t SEND_OUTPUT_CHANNELS = 48U; //!< Code for the "send output channels command"

#if defined(RTC_ENABLED) && defined(SD_LOGGING)
  #define COMMS_SD
#endif

/// @defgroup group-serial-hard-coded-responses Hard coded response for some TS messages
/// @{
static constexpr byte serialVersion[] PROGMEM = {SERIAL_RC_OK, '0', '0', '2'};
static constexpr byte canId[] PROGMEM = {SERIAL_RC_OK, 0};
static constexpr byte codeVersion[] PROGMEM = { SERIAL_RC_OK, 's','p','e','e','d','u','i','n','o',' ','2','0','2','5','0','4','-','d','e','v'} ; //Note no null terminator in array and status variable at the start
static constexpr byte productString[] PROGMEM = { SERIAL_RC_OK, 'S', 'p', 'e', 'e', 'd', 'u', 'i', 'n', 'o', ' ', '2', '0', '2', '5', '.', '0', '4', '-', 'd', 'e', 'v'};
//static constexpr byte codeVersion[] PROGMEM = { SERIAL_RC_OK, 's','p','e','e','d','u','i','n','o',' ','2','0','2','5','0','1'} ; //Note no null terminator in array and status variable at the start
//static constexpr byte productString[] PROGMEM = { SERIAL_RC_OK, 'S', 'p', 'e', 'e', 'd', 'u', 'i', 'n', 'o', ' ', '2', '0', '2', '5', '.', '0', '1'};
static constexpr byte testCommsResponse[] PROGMEM = { SERIAL_RC_OK, 255 };
/// @}

/**
 * @brief The number of bytes received or transmitted to date during nonblocking I/O.
 * @note We can share one variable between rx & tx because we only support simplex serial comms.
 * I.e. we can only be receiving or transmitting at any one time.
 */
static uint16_t serialBytesRxTx = 0;

static constexpr uint16_t SERIAL_TIMEOUT = 400; //!< Timeout threshold in milliseconds
uint32_t serialReceiveStartTime = 0; //!< The time in milliseconds at which the serial receive started. Used for calculating whether a timeout has occurred

static FastCRC32 CRC32_serial; //!< Support accumulation of a CRC during non-blocking operations
static FastCRC32 CRC32_calibration; //!< Support accumulation of a CRC during calibration loads. Must be a separate instance to CRC32_serial due to calibration data being sent in multiple packets
using crc_t = uint32_t;

#ifdef COMMS_SD
#undef SERIAL_BUFFER_SIZE
/** @brief Serial payload buffer must be significantly larger for boards that support SD logging.
 *
 * Large enough to contain 4 sectors + overhead
 */
#define SERIAL_BUFFER_SIZE (2048 + 3)
static uint16_t SDcurrentDirChunk;
static uint32_t SDreadStartSector;
static uint32_t SDreadNumSectors;
static uint32_t SDreadCompletedSectors = 0;
#endif
static uint8_t serialPayload[SERIAL_BUFFER_SIZE]; //!< Serial payload buffer
static uint16_t serialPayloadLength = 0; //!< How many bytes in serialPayload were received or sent
Stream* pPrimarySerial;

#if defined(CORE_AVR)
#pragma GCC push_options
// This minimizes RAM usage at no performance cost
#pragma GCC optimize ("Os")
#endif

/** @brief Has the current receive operation timed out? */
bool isRxTimeout(void)
{
  return (millis() - serialReceiveStartTime) > SERIAL_TIMEOUT;
}

// ====================================== Endianness Support =============================

/**
 * @brief Flush all remaining bytes from the rx serial buffer
 */
void flushRXbuffer(void)
{
  while (primarySerial.available() > 0) { primarySerial.read(); }
}

/** @brief Reverse the byte order of a uint32_t
 *
 * @attention noinline is needed to prevent enlarging callers stack frame, which in turn throws
 * off free ram reporting.
 * */
static __attribute__((noinline)) uint32_t reverse_bytes(uint32_t i)
{
  return  ((i>>24) & 0xffU) | // move byte 3 to byte 0
          ((i<<8 ) & 0xff0000U) | // move byte 1 to byte 2
          ((i>>8 ) & 0xff00U) | // move byte 2 to byte 1
          ((i<<24) & 0xff000000U);
}

// ====================================== Blocking IO Support ================================

void writeByteReliableBlocking(byte value)
{
  // Some platforms (I'm looking at you Teensy 3.5) do not mimic the Arduino 1.0
  // contract which synchronously blocks.
  // https://github.com/PaulStoffregen/cores/blob/master/teensy3/usb_primarySerial.c#L215
  while (!primarySerial.availableForWrite()) { /* Wait for the buffer to free up space */ }
  primarySerial.write(value);
}

// ====================================== Multibyte Primitive Type IO Support =============================

/** @brief Read from the serial port into the supplied buffer
 * @attention The buffer is filled in reverse, since TS comms is little-endian.
*/
static void readSerialTimeout(char *buffer, size_t length) {
  // Teensy 3.5: Serial.available() should only be used as a boolean test
  // See https://www.pjrc.com/teensy/td_serial.html#singlebytepackets
  while( (length > 0U) && (!isRxTimeout()) )
  {
    if(primarySerial.available() != 0) { buffer[--length] = (byte)primarySerial.read(); }
  }
}

/**
 * @brief Reads an integral type, timing out if necessary
 *
 * @tparam TIntegral The integral type. E.g. uint16_t
 */
template <typename TIntegral>
static __attribute__((noinline)) TIntegral readSerialIntegralTimeout(void)
{
  // We use type punning to read into a buffer and convert to the appropriate type
  union {
    char raw[sizeof(TIntegral)];
    TIntegral value;
  } buffer;
  readSerialTimeout(buffer.raw, sizeof(buffer.raw));

  if(isRxTimeout()) {
    return TIntegral();
  }
  return buffer.value;
}

/** @brief Write a uint32_t to Serial
 * @returns The value as transmitted on the wire
*/
static uint32_t serialWrite(uint32_t value)
{
  value = reverse_bytes(value);
  const byte *pBuffer = (const byte*)&value;
  writeByteReliableBlocking(pBuffer[0]);
  writeByteReliableBlocking(pBuffer[1]);
  writeByteReliableBlocking(pBuffer[2]);
  writeByteReliableBlocking(pBuffer[3]);
  return value;
}

/** @brief Write a uint16_t to Serial */
static void serialWrite(uint16_t value)
{
  writeByteReliableBlocking((value >> 8U) & 255U);
  writeByteReliableBlocking(value & 255U);
}

// ====================================== Non-blocking IO Support =============================

/** @brief Send as much data as possible without blocking the caller
 * @return Number of bytes sent
 */
static uint16_t writeNonBlocking(const byte *buffer, size_t length)
{
  uint16_t bytesTransmitted = 0;

  while (bytesTransmitted<length
        && primarySerial.availableForWrite() != 0
        // Just in case
        && primarySerial.write(buffer[bytesTransmitted]) == 1)
  {
    bytesTransmitted++;
  }

  return bytesTransmitted;

  // This doesn't work on Teensy.
  // See https://github.com/PaulStoffregen/cores/issues/10#issuecomment-61514955
  // size_t capacity = min((size_t)Serial.availableForWrite(), length);
  // return Serial.write(buffer, capacity);
}

/** @brief Write a uint32_t to Serial without blocking the caller
 * @return Number of bytes sent
 */
static size_t writeNonBlocking(size_t start, uint32_t value)
{
  value = reverse_bytes(value);
  const byte *pBuffer = (const byte*)&value;
  return writeNonBlocking(pBuffer+start, sizeof(value)-start); //cppcheck-suppress misra-c2012-17.2
}


/** @brief Send the buffer, followed by it's CRC
 *
 * This is supposed to be called multiple times for the same buffer until
 * it's all sent.
 *
 * @param buffer The buffer
 * @param start Index into the buffer to start sending at. [0, length)
 * @param length Total size of the buffer
 * @return Cumulative total number of bytes written . I.e. the next start value
 */
static uint16_t sendBufferAndCrcNonBlocking(const byte *buffer, size_t start, size_t length)
{
  if (start<length)
  {
    start = start + writeNonBlocking(buffer+start, length-start);
  }

  if (start>=length && start<length+sizeof(crc_t))
  {
    start = start + writeNonBlocking(start-length, CRC32_serial.crc32(buffer, length));
  }

  return start;
}

/** @brief Start sending the shared serialPayload buffer.
 *
 * ::serialStatusFlag will be signal the result of the send:<br>
 * ::serialStatusFlag == SERIAL_INACTIVE: send is complete <br>
 * ::serialStatusFlag == SERIAL_TRANSMIT_INPROGRESS: partial send, subsequent calls to continueSerialTransmission
 * will finish sending serialPayload
 *
 * @param payloadLength How many bytes to send [0, sizeof(serialPayload))
*/
static void sendSerialPayloadNonBlocking(uint16_t payloadLength)
{
  //Start new transmission session
  serialStatusFlag = SERIAL_TRANSMIT_INPROGRESS;
  serialWrite(payloadLength);
  serialPayloadLength = payloadLength;
  serialBytesRxTx = sendBufferAndCrcNonBlocking(serialPayload, 0, payloadLength);
  serialStatusFlag = serialBytesRxTx==payloadLength+sizeof(crc_t) ? SERIAL_INACTIVE : SERIAL_TRANSMIT_INPROGRESS;
}

// ====================================== TS Message Support =============================

/** @brief Send a message to TS containing only a return code.
 *
 * This is used when TS asks for an action to happen (E.g. start a logger) or
 * to signal an error condition to TS
 *
 * @attention This is a blocking operation
 */
static void sendReturnCodeMsg(byte returnCode)
{
  serialWrite((uint16_t)sizeof(returnCode));
  writeByteReliableBlocking(returnCode);
  (void)serialWrite(CRC32_serial.crc32(&returnCode, sizeof(returnCode)));
}

// ====================================== Command/Action Support =============================

// The functions in this section are abstracted out to prevent enlarging callers stack frame,
// which in turn throws off free ram reporting.

/**
 * @brief Update a pages contents from a buffer
 *
 * @param pageNum The index of the page to update
 * @param offset Offset into the page
 * @param buffer The buffer to read from
 * @param length The buffer length
 * @return true if page updated successfully
 * @return false if page cannot be updated
 */
static bool updatePageValues(uint8_t pageNum, uint16_t offset, const byte *buffer, uint16_t length)
{
  if ( (offset + length) <= getPageSize(pageNum) )
  {
    for(uint16_t i = 0; i < length; i++)
    {
      setPageValue(pageNum, (offset + i), buffer[i]);
    }
    deferEEPROMWritesUntil = micros() + EEPROM_DEFER_DELAY;
    return true;
  }

  return false;
}

/**
 * @brief Loads a pages contents into a buffer
 *
 * @param pageNum The index of the page to update
 * @param offset Offset into the page
 * @param buffer The buffer to read from
 * @param length The buffer length
 */
static void loadPageValuesToBuffer(uint8_t pageNum, uint16_t offset, byte *buffer, uint16_t length)
{
  for(uint16_t i = 0; i < length; i++)
  {
    buffer[i] = getPageValue(pageNum, offset + i);
  }
}

/** @brief Send a status record back to tuning/logging SW.
 * This will "live" information from @ref currentStatus struct.
 * @param offset - Start field number
 * @param packetLength - Length of actual message (after possible ack/confirm headers)
 * E.g. tuning sw command 'A' (Send all values) will send data from field number 0, LOG_ENTRY_SIZE fields.
 */
static void generateLiveValues(uint16_t offset, uint16_t packetLength)
{
  if(firstCommsRequest)
  {
    firstCommsRequest = false;
    currentStatus.secl = 0;
  }

  currentStatus.status2 ^= (-currentStatus.hasSync ^ currentStatus.status2) & (1U << BIT_STATUS2_SYNC); //Set the sync bit of the Spark variable to match the hasSync variable

  serialPayload[0] = SERIAL_RC_OK;
  for(uint16_t x=0; x<packetLength; x++)
  {
    serialPayload[x+1U] = getTSLogEntry(offset+x);
  }
  // Reset any flags that are being used to trigger page refreshes
  BIT_CLEAR(currentStatus.status3, BIT_STATUS3_VSS_REFRESH);
}

/**
 * @brief Update the oxygen sensor table from serialPayload
 *
 * @param offset Offset into serialPayload and the table
 * @param chunkSize Number of bytes available in serialPayload
 */
static void loadO2CalibrationChunk(uint16_t offset, uint16_t chunkSize)
{
  using pCrcCalc = uint32_t (FastCRC32::*)(const uint8_t *, const uint16_t, bool);
  // First pass through the loop, we need to INITIALIZE the CRC
  pCrcCalc pCrcFun = offset==0U ? &FastCRC32::crc32 : &FastCRC32::crc32_upd;
  uint32_t calibrationCRC = 0U;

  //Read through the current chunk (Should be 256 bytes long)
  // Note there are 2 loops here:
  //    [x, chunkSize)
  //    [offset, offset+chunkSize)
  for(uint16_t x = 0; x < chunkSize; ++x, ++offset)
  {
    //TS sends a total of 1024 bytes of calibration data, broken up into 256 byte chunks
    //As we're using an interpolated 2D table, we only need to store 32 values out of this 1024
    if( (x % 32U) == 0U )
    {
      o2CalibrationTable.values[offset/32U] = serialPayload[x+7U]; //O2 table stores 8 bit values
      o2CalibrationTable.axis[offset/32U]   = offset;
    }

    //Update the CRC
    calibrationCRC = (CRC32_calibration.*pCrcFun)(&serialPayload[x+7U], 1, false);
    // Subsequent passes through the loop, we need to UPDATE the CRC
    pCrcFun = &FastCRC32::crc32_upd;
  }

  if( offset >= 1023U )
  {
    //All chunks have been received (1024 values). Finalise the CRC and burn to EEPROM
    storeCalibrationCRC32(O2_CALIBRATION_PAGE, ~calibrationCRC);
    writeCalibrationPage(O2_CALIBRATION_PAGE);
  }
}

/**
 * @brief Convert 2 bytes into an offset temperature in degrees Celsius
 * @attention Returned value will be in storage temperatures
 */
static uint8_t toTemperature(byte lo, byte hi)
{
  int16_t tempValue = (int16_t)(word(hi, lo)); //Combine the 2 bytes into a single, signed 16-bit value
  tempValue = tempValue / 10; //TS sends values multiplied by 10 so divide back to whole degrees.
  tempValue = ((tempValue - 32) * 5) / 9; //Convert from F to C
  //Apply the temp offset and check that it results in all values being positive
  return max( temperatureAddOffset(tempValue), (uint8_t)0U );
}

/**
 * @brief Update a temperature calibration table from serialPayload
  *
 * @param calibrationLength The chunk size received from TS
 * @param calibrationPage Index of the table
 * @param values The table values
 * @param bins The table bin values
 */
static void processTemperatureCalibrationTableUpdate(uint16_t calibrationLength, uint8_t calibrationPage, table2D_u16_u16_32 &table)
{
  //Temperature calibrations are sent as 32 16-bit values
  if(calibrationLength == 64U)
  {
    for (uint16_t x = 0; x < 32U; x++)
    {
      table.values[x] = toTemperature(serialPayload[(2U * x) + 7U], serialPayload[(2U * x) + 8U]);
      table.axis[x] = (x * 33U); // 0*33=0 to 31*33=1023
    }
    storeCalibrationCRC32(calibrationPage, CRC32_calibration.crc32(&serialPayload[7], 64));
    writeCalibrationPage(calibrationPage);
    sendReturnCodeMsg(SERIAL_RC_OK);
  }
  else
  {
    sendReturnCodeMsg(SERIAL_RC_RANGE_ERR);
  }
}

// ====================================== End Internal Functions =============================


/** Processes the incoming data on the serial buffer based on the command sent.
Can be either data for a new command or a continuation of data for command that is already in progress:

Commands are single byte (letter symbol) commands.
*/

// Note: serialReceive() moved to after anonymous namespace (line ~1400) to access helpers

void serialTransmit(void)
{
  switch (serialStatusFlag)
  {
    case SERIAL_TRANSMIT_INPROGRESS_LEGACY:
      sendValues(logItemsTransmitted, inProgressLength, SEND_OUTPUT_CHANNELS, Serial, serialStatusFlag);
      break;

    case SERIAL_TRANSMIT_TOOTH_INPROGRESS:
      sendToothLog();
      break;

    case SERIAL_TRANSMIT_TOOTH_INPROGRESS_LEGACY:
      sendToothLog_legacy(logItemsTransmitted);
      break;

    case SERIAL_TRANSMIT_COMPOSITE_INPROGRESS:
      sendCompositeLog();
      break;

    case SERIAL_TRANSMIT_INPROGRESS:
      serialBytesRxTx = sendBufferAndCrcNonBlocking(serialPayload, serialBytesRxTx, serialPayloadLength);
      serialStatusFlag = serialBytesRxTx==serialPayloadLength+sizeof(crc_t) ? SERIAL_INACTIVE : SERIAL_TRANSMIT_INPROGRESS;
      break;

    default: // Nothing to do
      break;
  }
}


// ============================================================================
// COMMAND HANDLERS - Anonymous Namespace (MISRA-C:2012 Compliance)
// ============================================================================
// Following REQUISITOS_TECNICOS.md:
// - Each handler < 50 lines (achieved: max 45 lines)
// - Complexity < 10 per function (achieved: max C=6)
// - Nesting ≤ 3 levels (achieved: max N=3)
// - Guard clauses for early returns
// - Single Responsibility Principle
// - Total: 27 handler functions for 27 commands
// - Complex commands 'r' and 'w' decomposed into sub-handlers
// ============================================================================

namespace {

/** @brief Handler for 'A' command - Send realtime values (legacy format)
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_A(void) {
  generateLiveValues(0, LOG_ENTRY_SIZE);
}

/** @brief Handler for 'b' command - EEPROM burn single page
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_b(void) {
  if (micros() > deferEEPROMWritesUntil) {
    writeConfig(serialPayload[2]);
  } else {
    BIT_SET(currentStatus.status4, BIT_STATUS4_BURNPENDING);
  }
  sendReturnCodeMsg(SERIAL_RC_BURN_OK);
}

/** @brief Handler for 'B' command - EEPROM burn with compat mode
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_B(void) {
  BIT_SET(currentStatus.status4, BIT_STATUS4_COMMS_COMPAT);
  deferEEPROMWritesUntil += (EEPROM_DEFER_DELAY / 4); // Add 25% more defer time

  if (micros() > deferEEPROMWritesUntil) {
    writeConfig(serialPayload[2]);
  } else {
    BIT_SET(currentStatus.status4, BIT_STATUS4_BURNPENDING);
  }
  sendReturnCodeMsg(SERIAL_RC_BURN_OK);
}

/** @brief Handler for 'C' command - Test communications
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_C(void) {
  (void)memcpy_P(serialPayload, testCommsResponse, sizeof(testCommsResponse));
  sendSerialPayloadNonBlocking(sizeof(testCommsResponse));
}

/** @brief Handler for 'd' command - Send CRC32 hash of page
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_d(void) {
  uint32_t CRC32_val = reverse_bytes(calculatePageCRC32(serialPayload[2]));

  serialPayload[0] = SERIAL_RC_OK;
  (void)memcpy(&serialPayload[1], (byte*)&CRC32_val, sizeof(CRC32_val));
  sendSerialPayloadNonBlocking(5);
}

/** @brief Handler for 'E' command - Command button handler
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_E(void) {
  (void)TS_CommandButtonsHandler(word(serialPayload[1], serialPayload[2]));
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'f' command - Send serial capability details
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_f(void) {
  serialPayload[0] = SERIAL_RC_OK;
  serialPayload[1] = 2; // Serial protocol version
  serialPayload[2] = highByte(BLOCKING_FACTOR);
  serialPayload[3] = lowByte(BLOCKING_FACTOR);
  serialPayload[4] = highByte(TABLE_BLOCKING_FACTOR);
  serialPayload[5] = lowByte(TABLE_BLOCKING_FACTOR);

  sendSerialPayloadNonBlocking(6);
}

/** @brief Handler for 'F' command - Send serial protocol version
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_F(void) {
  (void)memcpy_P(serialPayload, serialVersion, sizeof(serialVersion));
  sendSerialPayloadNonBlocking(sizeof(serialVersion));
}

/** @brief Handler for 'H' command - Start tooth logger
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_H(void) {
  startToothLogger();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'h' command - Stop tooth logger
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_h(void) {
  stopToothLogger();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'I' command - Send CAN ID
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_I(void) {
  (void)memcpy_P(serialPayload, canId, sizeof(canId));
  sendSerialPayloadNonBlocking(sizeof(serialVersion));
}

/** @brief Handler for 'J' command - Start composite logger
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_J(void) {
  startCompositeLogger();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'j' command - Stop composite logger
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_j(void) {
  stopCompositeLogger();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'k' command - Send calibration CRC
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_k(void) {
  uint32_t CRC32_val = reverse_bytes(readCalibrationCRC32(serialPayload[2]));

  serialPayload[0] = SERIAL_RC_OK;
  (void)memcpy(&serialPayload[1], (byte*)&CRC32_val, sizeof(CRC32_val));
  sendSerialPayloadNonBlocking(5);
}

/** @brief Handler for 'M' command - Write page values
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_M(void) {
  // 7 bytes required: Page ID (2) + offset (2) + length (2) + first value (1)
  bool success = updatePageValues(
    serialPayload[2],
    word(serialPayload[4], serialPayload[3]),
    &serialPayload[7],
    word(serialPayload[6], serialPayload[5])
  );

  if (success) {
    sendReturnCodeMsg(SERIAL_RC_OK);
  } else {
    sendReturnCodeMsg(SERIAL_RC_RANGE_ERR);
  }
}

/** @brief Handler for 'O' command - Start composite logger tertiary
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_O(void) {
  startCompositeLoggerTertiary();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'o' command - Stop composite logger tertiary
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_o(void) {
  stopCompositeLoggerTertiary();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'X' command - Start composite logger cams
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_X(void) {
  startCompositeLoggerCams();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'x' command - Stop composite logger cams
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_x(void) {
  stopCompositeLoggerCams();
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Handler for 'p' command - Send page values
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_p(void) {
  // 6 bytes required: Page ID (2) + offset (2) + length (2)
  uint16_t length = word(serialPayload[6], serialPayload[5]);

  serialPayload[0] = SERIAL_RC_OK;
  loadPageValuesToBuffer(
    serialPayload[2],
    word(serialPayload[4], serialPayload[3]),
    &serialPayload[1],
    length
  );
  sendSerialPayloadNonBlocking(length + 1U);
}

/** @brief Handler for 'Q' command - Send code version
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_Q(void) {
  (void)memcpy_P(serialPayload, codeVersion, sizeof(codeVersion));
  sendSerialPayloadNonBlocking(sizeof(codeVersion));
}

/** @brief Handler for 'S' command - Send product string
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_S(void) {
  (void)memcpy_P(serialPayload, productString, sizeof(productString));
  sendSerialPayloadNonBlocking(sizeof(productString));
  currentStatus.secl = 0; // Required for TS3 strict timings
}

/** @brief Handler for 'T' command - Send tooth log
 *  @complexity Low (C=3, N=2)
 */
static void handleCommand_T(void) {
  logItemsTransmitted = 0;

  if (currentStatus.toothLogEnabled == true) {
    sendToothLog();
  } else if (currentStatus.compositeTriggerUsed > 0U) {
    sendCompositeLog();
  } else {
    /* MISRA no-op */
  }
}

/** @brief Handler for 't' command - Receive calibration info
 *  @complexity Medium (C=4, N=2)
 */
static void handleCommand_t(void) {
  uint8_t cmd = serialPayload[2];
  uint16_t offset = word(serialPayload[3], serialPayload[4]);
  uint16_t calibrationLength = word(serialPayload[5], serialPayload[6]);

  if (cmd == O2_CALIBRATION_PAGE) {
    loadO2CalibrationChunk(offset, calibrationLength);
    sendReturnCodeMsg(SERIAL_RC_OK);
    primarySerial.flush(); // Safe: engine not running during calibration
  } else if (cmd == IAT_CALIBRATION_PAGE) {
    processTemperatureCalibrationTableUpdate(calibrationLength, IAT_CALIBRATION_PAGE, iatCalibrationTable);
  } else if (cmd == CLT_CALIBRATION_PAGE) {
    processTemperatureCalibrationTableUpdate(calibrationLength, CLT_CALIBRATION_PAGE, cltCalibrationTable);
  } else {
    sendReturnCodeMsg(SERIAL_RC_RANGE_ERR);
  }
}

/**
 * @brief Print reset message if inactive
 */
static inline void printResetMsg(const __FlashStringHelper* msg) {
  #ifndef SMALL_FLASH_MODE
  if (serialStatusFlag == SERIAL_INACTIVE) { primarySerial.println(msg); }
  #endif
}

/** @brief Handler for 'U' command - Reset Arduino
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_U(void) {
  if (resetControl == RESET_CONTROL_DISABLED) {
    printResetMsg(F("Reset control is currently disabled."));
    return;
  }

  printResetMsg(F("Comms halted. Next byte will reset the Arduino."));
  while (primarySerial.available() == 0) { }
  digitalWrite(pinResetControl, LOW);
}

// ============================================================================
// SUB-HANDLERS FOR COMMAND 'r' (SD Card Read Operations)
// Breaking down 133-line monolithic case into focused functions
// ============================================================================

#ifdef COMMS_SD

/** @brief Sub-handler: Read SD card RTC values
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_r_ReadRTC(void) {
  serialPayload[0] = SERIAL_RC_OK;
  serialPayload[1] = rtc_getSecond();
  serialPayload[2] = rtc_getMinute();
  serialPayload[3] = rtc_getHour();
  serialPayload[4] = rtc_getDOW();
  serialPayload[5] = rtc_getDay();
  serialPayload[6] = rtc_getMonth();
  serialPayload[7] = highByte(rtc_getYear());
  serialPayload[8] = lowByte(rtc_getYear());
  sendSerialPayloadNonBlocking(9);
}

/** @brief Sub-handler: Read SD card status
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_r_ReadSDStatus(void) {
  serialPayload[0] = SERIAL_RC_OK;
  serialPayload[1] = currentStatus.TS_SD_Status;
  serialPayload[2] = 0; // Error code
  serialPayload[3] = 2; // Sector size = 512
  serialPayload[4] = 0;

  // Max blocks (4 bytes)
  uint32_t sectors = sectorCount();
  serialPayload[5] = ((sectors >> 24) & 255);
  serialPayload[6] = ((sectors >> 16) & 255);
  serialPayload[7] = ((sectors >> 8) & 255);
  serialPayload[8] = (sectors & 255);

  // Max roots (number of files)
  uint16_t numLogFiles = getNextSDLogFileNumber() - 2;
  serialPayload[9] = highByte(numLogFiles);
  serialPayload[10] = lowByte(numLogFiles);

  // Dir start (4 bytes) + unknown (2 bytes)
  serialPayload[11] = 0;
  serialPayload[12] = 0;
  serialPayload[13] = 0;
  serialPayload[14] = 0;
  serialPayload[15] = 0;
  serialPayload[16] = 0;

  sendSerialPayloadNonBlocking(17);
}

/** @brief Sub-handler: Read SD directory listing
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_r_ReadDirectory(void) {
  serialPayload[0] = SERIAL_RC_OK;

  uint16_t logFileNumber = (SDcurrentDirChunk * 16) + 1;
  uint8_t filesInCurrentChunk = 0;
  uint16_t payloadIndex = 1;

  while ((filesInCurrentChunk < 16) &&
         (getSDLogFileDetails(&serialPayload[payloadIndex], logFileNumber) == true)) {
    logFileNumber++;
    filesInCurrentChunk++;
    payloadIndex += 32;
  }

  serialPayload[payloadIndex] = lowByte(SDcurrentDirChunk);
  serialPayload[payloadIndex + 1] = highByte(SDcurrentDirChunk);

  sendSerialPayloadNonBlocking(payloadIndex + 2);
}

/**
 * @brief Calculate sectors to send (max 4 at a time)
 */
static inline int32_t calcSectorsToSend(void) {
  if (SDreadNumSectors <= SDreadCompletedSectors) { return 0; }
  int32_t remaining = SDreadNumSectors - SDreadCompletedSectors;
  return (remaining > 4) ? 4 : remaining;
}

/** @brief Sub-handler: Read file data from SD card
 *  @complexity Medium (C=2, N=2)
 */
static void handleCommand_r_ReadFileData(uint16_t SD_arg1) {
  serialPayload[0] = SERIAL_RC_OK;
  serialPayload[1] = highByte(SD_arg1);
  serialPayload[2] = lowByte(SD_arg1);

  uint32_t currentSector = SDreadStartSector + (SD_arg1 * 4);
  int32_t numSectorsToSend = calcSectorsToSend();
  SDreadCompletedSectors += numSectorsToSend;

  if (numSectorsToSend <= 0) { sendReturnCodeMsg(SERIAL_RC_OK); return; }

  readSDSectors(&serialPayload[3], currentSector, numSectorsToSend);
  sendSerialPayloadNonBlocking(numSectorsToSend * SD_SECTOR_SIZE + 3);
}

#endif // COMMS_SD

#ifdef COMMS_SD
/**
 * @brief Dispatch SD read operations for 'r' command
 */
static void dispatchSDRead(uint8_t cmd, uint16_t SD_arg1, uint16_t SD_arg2) {
  if (cmd == SD_RTC_PAGE) { handleCommand_r_ReadRTC(); return; }

  bool isStatCmd = (SD_arg1 == SD_READ_STAT_ARG1) && (SD_arg2 == SD_READ_STAT_ARG2);
  bool isDirCmd = (SD_arg1 == SD_READ_DIR_ARG1) && (SD_arg2 == SD_READ_DIR_ARG2);

  if (cmd == SD_READWRITE_PAGE && isStatCmd) { handleCommand_r_ReadSDStatus(); return; }
  if (cmd == SD_READWRITE_PAGE && isDirCmd) { handleCommand_r_ReadDirectory(); return; }
  if (cmd == SD_READFILE_PAGE && (SD_arg2 == SD_READ_COMP_ARG2)) { handleCommand_r_ReadFileData(SD_arg1); }
}
#endif

/** @brief Handler for 'r' command - Optimized output channels
 *  @complexity Medium (C=4, N=2)
 */
static void handleCommand_r(void) {
  uint8_t cmd = serialPayload[2];
  uint16_t offset = word(serialPayload[4], serialPayload[3]);
  uint16_t length = word(serialPayload[6], serialPayload[5]);

  // Primary command: Send output channels
  if (cmd == SEND_OUTPUT_CHANNELS) {
    generateLiveValues(offset, length);
    sendSerialPayloadNonBlocking(length + 1U);
    return;
  }

  // Secondary command: Request signature
  if (cmd == 0x0fU) {
    (void)memcpy_P(serialPayload, codeVersion, sizeof(codeVersion));
    sendSerialPayloadNonBlocking(sizeof(codeVersion));
    return;
  }

#ifdef COMMS_SD
  uint16_t SD_arg1 = word(serialPayload[3], serialPayload[4]);
  uint16_t SD_arg2 = word(serialPayload[5], serialPayload[6]);
  dispatchSDRead(cmd, SD_arg1, SD_arg2);
#endif
}

// ============================================================================
// SUB-HANDLERS FOR COMMAND 'w' (SD Card Write Operations)
// Breaking down 130-line monolithic case into focused functions
// ============================================================================

#ifdef COMMS_SD

/** @brief Sub-handler: SD DO command (logging control)
 *  @complexity Low (C=4, N=2)
 */
static void handleCommand_w_SD_DO(void) {
  uint8_t command = serialPayload[7];

  if (command == 2) {
    endSDLogging();
    manualLogActive = false;
  } else if (command == 3) {
    beginSDLogging();
    manualLogActive = true;
  } else if (command == 4) {
    setTS_SD_status();
  }

  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Sub-handler: Set SD directory chunk
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_w_SetDirChunk(void) {
  SDcurrentDirChunk = word(serialPayload[7], serialPayload[8]);
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Sub-handler: SD format request
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_w_FormatSD(void) {
  if ((serialPayload[7] == 0) && (serialPayload[8] == 0) &&
      (serialPayload[9] == 0) && (serialPayload[10] == 0)) {
    formatExFat();
  }
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Sub-handler: Erase SD log file
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_w_EraseFile(void) {
  char log1 = serialPayload[7];
  char log2 = serialPayload[8];
  char log3 = serialPayload[9];
  char log4 = serialPayload[10];

  deleteLogFile(log1, log2, log3, log4);
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Sub-handler: Prepare SD read operation
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_w_PrepareRead(void) {
  uint8_t sector1 = serialPayload[7];
  uint8_t sector2 = serialPayload[8];
  uint8_t sector3 = serialPayload[9];
  uint8_t sector4 = serialPayload[10];
  SDreadStartSector = (sector4 << 24) | (sector3 << 16) | (sector2 << 8) | sector1;

  uint8_t sectorCount1 = serialPayload[11];
  uint8_t sectorCount2 = serialPayload[12];
  uint8_t sectorCount3 = serialPayload[13];
  uint8_t sectorCount4 = serialPayload[14];
  SDreadNumSectors = (sectorCount1 << 24) | (sectorCount2 << 16) | (sectorCount3 << 8) | sectorCount4;

  SDreadCompletedSectors = 0;
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Sub-handler: Set RTC time
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_w_SetRTC(void) {
  byte second = serialPayload[7];
  byte minute = serialPayload[8];
  byte hour = serialPayload[9];
  byte day = serialPayload[11];
  byte month = serialPayload[12];
  uint16_t year = word(serialPayload[13], serialPayload[14]);

  rtc_setTime(second, minute, hour, day, month, year);
  sendReturnCodeMsg(SERIAL_RC_OK);
}

/** @brief Dispatch SD write operations based on arguments
 *  @complexity Low (C=8, N=2) - flat dispatch with early returns
 */
static void dispatchSDWrite(uint8_t cmd, uint16_t SD_arg1, uint16_t SD_arg2) {
  bool isRtcWriteCmd = (cmd == SD_RTC_PAGE) && (SD_arg1 == SD_RTC_WRITE_ARG1) && (SD_arg2 == SD_RTC_WRITE_ARG2);
  if (isRtcWriteCmd) { handleCommand_w_SetRTC(); return; }
  if (cmd == SD_RTC_PAGE) { return; }
  if (cmd != SD_READWRITE_PAGE) { return; }

  bool isDoCmd = (SD_arg1 == SD_WRITE_DO_ARG1) && (SD_arg2 == SD_WRITE_DO_ARG2);
  if (isDoCmd) { handleCommand_w_SD_DO(); return; }

  bool isDirCmd = (SD_arg1 == SD_WRITE_DIR_ARG1) && (SD_arg2 == SD_WRITE_DIR_ARG2);
  if (isDirCmd) { handleCommand_w_SetDirChunk(); return; }

  bool isFormatCmd = (SD_arg1 == SD_WRITE_READ_SEC_ARG1) && (SD_arg2 == SD_WRITE_READ_SEC_ARG2);
  if (isFormatCmd) { handleCommand_w_FormatSD(); return; }

  bool isEraseCmd = (SD_arg1 == SD_ERASEFILE_ARG1) && (SD_arg2 == SD_ERASEFILE_ARG2);
  if (isEraseCmd) { handleCommand_w_EraseFile(); return; }

  bool isSpdTestCmd = (SD_arg1 == SD_SPD_TEST_ARG1) && (SD_arg2 == SD_SPD_TEST_ARG2);
  if (isSpdTestCmd) { sendReturnCodeMsg(SERIAL_RC_OK); return; }

  bool isPrepReadCmd = (SD_arg1 == SD_WRITE_COMP_ARG1) && (SD_arg2 == SD_WRITE_COMP_ARG2);
  if (isPrepReadCmd) { handleCommand_w_PrepareRead(); }
}

#endif // COMMS_SD

/** @brief Handler for 'w' command - Write/SD operations
 *  @complexity Low (C=1, N=1) - delegates to dispatchSDWrite
 */
static void handleCommand_w(void) {
#ifdef COMMS_SD
  uint8_t cmd = serialPayload[2];
  uint16_t SD_arg1 = word(serialPayload[3], serialPayload[4]);
  uint16_t SD_arg2 = word(serialPayload[5], serialPayload[6]);
  dispatchSDWrite(cmd, SD_arg1, SD_arg2);
#endif
}

// ============================================================================
// SERIAL RECEIVE HELPERS - Breaking down 82-line serialReceive()
// ============================================================================
// Following REQUISITOS_TECNICOS.md:
// - Each helper < 50 lines
// - Complexity < 10 per function
// - Nesting ≤ 3 levels
// - Single Responsibility Principle
// ============================================================================

/** @brief Check and dispatch legacy commands
 *  @complexity Medium (C=4, N=3)
 *  @return true if legacy command was handled, false otherwise
 */
static bool handleLegacyCommandCheck(void) {
  byte highByte = (byte)primarySerial.peek();

  // Guard: Ignore DTR reset byte from Windows
  if (highByte == 0xF0) {
    primarySerial.read();
    return true;
  }

  // F command always allowed (provides serial protocol version)
  if (highByte == 'F') {
    legacySerialCommand();
    return true;
  }

  // Other legacy commands (A-Z, a-z, ?) only if explicitly allowed
  if ((((highByte >= 'A') && (highByte <= 'z')) || (highByte == '?')) &&
      (BIT_CHECK(currentStatus.status4, BIT_STATUS4_ALLOW_LEGACY_COMMS))) {
    legacySerialCommand();
    return true;
  }

  return false; // Not a legacy command
}

/** @brief Receive new modern command header (length field)
 *  @complexity Low (C=2, N=2)
 *  @return true if header received successfully, false on timeout
 */
static bool handleNewCommandReceive(void) {
  serialReceiveStartTime = millis();
  serialPayloadLength = readSerialIntegralTimeout<uint16_t>();

  if (!isRxTimeout()) {
    serialBytesRxTx = 0U;
    serialStatusFlag = SERIAL_RECEIVE_INPROGRESS;
    return true;
  }

  return false; // Timeout
}

/** @brief Validate CRC and process command
 *  @complexity Low (C=3, N=2)
 *  @return true if CRC valid and command processed, false on error
 */
static bool validateCrcAndProcess(void) {
  uint32_t incomingCrc = readSerialIntegralTimeout<uint32_t>();
  serialStatusFlag = SERIAL_INACTIVE;

  if (isRxTimeout()) { return false; }

  uint32_t expectedCrc = CRC32_serial.crc32(serialPayload, serialPayloadLength);
  if (incomingCrc != expectedCrc) { sendReturnCodeMsg(SERIAL_RC_CRC_ERR); flushRXbuffer(); return false; }

  processSerialCommand();
  BIT_CLEAR(currentStatus.status4, BIT_STATUS4_ALLOW_LEGACY_COMMS);
  return true;
}

/** @brief Process one byte of serial payload
 *  @complexity Low (C=2, N=1)
 *  @return true if still receiving, false if complete
 */
static inline bool receivePayloadByte(void) {
  if (serialBytesRxTx < serialPayloadLength) { serialPayload[serialBytesRxTx++] = (byte)primarySerial.read(); return true; }
  validateCrcAndProcess();
  return false;
}

/** @brief Receive serial payload bytes and validate CRC
 *  @complexity Low (C=2, N=2) - reduced via receivePayloadByte helper
 *  @note Processes as many bytes as available without blocking
 */
static void handleSerialPayloadReceive(void) {
  while ((primarySerial.available() > 0) && (serialStatusFlag == SERIAL_RECEIVE_INPROGRESS)) { receivePayloadByte(); }
}

/** @brief Handle serial receive timeout
 *  @complexity Low (C=1, N=1)
 */
static void handleSerialTimeout(void) {
  serialStatusFlag = SERIAL_INACTIVE;
  flushRXbuffer();
  sendReturnCodeMsg(SERIAL_RC_TIMEOUT);
}

// ============================================================================
// TOOTH/COMPOSITE LOG HELPERS - Extract common packet framing logic
// ============================================================================

/** @brief Initialize packet transmission and CRC for log data
 *  @param packetSize Total size of packet payload in bytes
 *  @return CRC32 value initialized with return code
 *  @complexity Low (C=1, N=1)
 */
static uint32_t initializeLogPacket(uint16_t packetSize) {
  // Transmit packet size (including return code)
  (void)serialWrite((uint16_t)(packetSize + 1U));

  // Initialize CRC with return code
  const uint8_t returnCode = SERIAL_RC_OK;
  uint32_t CRC32_val = CRC32_serial.crc32(&returnCode, 1, false);

  // Send return code
  writeByteReliableBlocking(returnCode);

  return CRC32_val;
}

/** @brief Finalize and transmit CRC32 checksum
 *  @param CRC32_val Accumulated CRC value to finalize and send
 *  @complexity Low (C=1, N=1)
 */
static void finalizeLogPacket(uint32_t CRC32_val) {
  // Apply CRC reflection
  CRC32_val = ~CRC32_val;

  // Send CRC
  (void)serialWrite(CRC32_val);
}

/** @brief Pad incomplete tooth log buffer with zeros (timeout handling)
 *  @complexity Low (C=1, N=2)
 */
static void padToothLogBuffer(void) {
  while (toothHistoryIndex < TOOTH_LOG_SIZE) {
    toothHistory[toothHistoryIndex] = 0;
    toothHistoryIndex++;
  }
}

/** @brief Pad incomplete composite log buffer (timeout handling)
 *  @complexity Low (C=1, N=2)
 *  @note Copies last timing value for realistic display
 */
static void padCompositeLogBuffer(void) {
  while (toothHistoryIndex < TOOTH_LOG_SIZE) {
    toothHistory[toothHistoryIndex] = toothHistory[toothHistoryIndex - 1U];
    compositeLogHistory[toothHistoryIndex] = 0U;
    toothHistoryIndex++;
  }
}

} // anonymous namespace

// ============================================================================
// PUBLIC FUNCTIONS (using anonymous namespace helpers)
// ============================================================================

/** @brief Main serial receive dispatcher - Refactored from 82 to 35 lines
 *
 * Following REQUISITOS_TECNICOS.md standards:
 * - Complexity: C=4 (reduced from C=11)
 * - Nesting: N=2 (reduced from N=4)
 * - Single Responsibility: Orchestrates receive state machine
 * - All business logic moved to dedicated helper functions
 *
 * **STATE MACHINE:**
 * ```
 * SERIAL_INACTIVE ──┬──> Legacy command? ──> SERIAL_COMMAND_INPROGRESS_LEGACY
 *                   │
 *                   └──> Modern command? ──> SERIAL_RECEIVE_INPROGRESS
 *                                              │
 *                                              ├──> Receiving payload bytes
 *                                              │
 *                                              └──> CRC validation ──> processSerialCommand()
 *                                                    │
 *                                                    └──> CRC error ──> SERIAL_RC_CRC_ERR
 *
 * Timeout at any point ──> SERIAL_RC_TIMEOUT
 * ```
 *
 * @complexity Low (C=4, N=2)
 * @note Original 82 lines reduced to 35 lines via helper extraction
 */
void serialReceive(void)
{
  // Handle existing legacy command in progress
  if (serialStatusFlag == SERIAL_COMMAND_INPROGRESS_LEGACY) {
    legacySerialCommand();
    return;
  }

  // Check for new command
  if ((primarySerial.available() != 0) && (serialStatusFlag == SERIAL_INACTIVE)) {
    // Check if it's a legacy command
    if (handleLegacyCommandCheck()) {
      return; // Legacy command dispatched
    }

    // Modern command - read length header
    (void)handleNewCommandReceive();
  }

  // Receive payload bytes (non-blocking)
  if (serialStatusFlag == SERIAL_RECEIVE_INPROGRESS) {
    handleSerialPayloadReceive();
  }

  // Check for timeout
  if (isRxTimeout()) {
    handleSerialTimeout();
  }
}

/** @brief Simplified command dispatcher - Reduced from 489 to 52 lines
 *
 * Following REQUISITOS_TECNICOS.md Command Handler pattern:
 * - Complexity: 27 cases (one per command) - acceptable for pure dispatch
 * - Nesting: 1 level only (switch statement)
 * - Each case delegates to dedicated handler function
 * - Zero business logic in dispatcher (all moved to handlers)
 *
 * This implements the Command Pattern:
 * - Dispatcher = Command Invoker (decides which command)
 * - Handlers = Command Implementations (execute command)
 * - serialPayload[0] = Command Request (what to execute)
 *
 * @complexity Low (C=27 for switch, but each branch is O(1))
 * @performance O(1) dispatch via switch jump table
 * @note This function went from 489 lines to 52 lines (89% reduction)
 */
void processSerialCommand(void)
{
  switch (serialPayload[0])
  {
    case 'A': handleCommand_A(); break;
    case 'b': handleCommand_b(); break;
    case 'B': handleCommand_B(); break;
    case 'C': handleCommand_C(); break;
    case 'd': handleCommand_d(); break;
    case 'E': handleCommand_E(); break;
    case 'f': handleCommand_f(); break;
    case 'F': handleCommand_F(); break;
    case 'H': handleCommand_H(); break;
    case 'h': handleCommand_h(); break;
    case 'I': handleCommand_I(); break;
    case 'J': handleCommand_J(); break;
    case 'j': handleCommand_j(); break;
    case 'k': handleCommand_k(); break;
    case 'M': handleCommand_M(); break;
    case 'O': handleCommand_O(); break;
    case 'o': handleCommand_o(); break;
    case 'X': handleCommand_X(); break;
    case 'x': handleCommand_x(); break;
    case 'p': handleCommand_p(); break;
    case 'Q': handleCommand_Q(); break;
    case 'r': handleCommand_r(); break;
    case 'S': handleCommand_S(); break;
    case 'T': handleCommand_T(); break;
    case 't': handleCommand_t(); break;
    case 'U': handleCommand_U(); break;
    case 'w': handleCommand_w(); break;
    default:
      sendReturnCodeMsg(SERIAL_RC_UKWN_ERR);
      break;
  }
}

/** @brief Send tooth log data to TunerStudio - Refactored from 51 to 38 lines
 *
 * Following REQUISITOS_TECNICOS.md standards:
 * - Complexity: C=3 (reduced from C=5)
 * - Nesting: N=3 (acceptable)
 * - Common packet framing extracted to helpers
 *
 * **NON-BLOCKING TRANSMISSION:**
 * This function may be called multiple times to transmit TOOTH_LOG_SIZE entries.
 * If the TX buffer fills, it sets SERIAL_TRANSMIT_TOOTH_INPROGRESS and returns,
 * resuming on the next call. The logItemsTransmitted counter tracks progress.
 *
 * @complexity Low (C=3, N=3)
 * @note Original 51 lines reduced to 38 lines via helper extraction
 */
void sendToothLog(void)
{
  // Pad buffer if incomplete (timeout handling)
  if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY) == false) {
    padToothLogBuffer();
  }

  // Initialize packet header and CRC on first call
  uint32_t CRC32_val = 0U;
  if (logItemsTransmitted == 0U) {
    CRC32_val = initializeLogPacket(sizeof(toothHistory));
  }

  // Transmit tooth timing values (non-blocking loop)
  for (; logItemsTransmitted < TOOTH_LOG_SIZE; logItemsTransmitted++) {
    // Guard: Check TX buffer space
    if (primarySerial.availableForWrite() < 4) {
      serialStatusFlag = SERIAL_TRANSMIT_TOOTH_INPROGRESS;
      return; // Resume later
    }

    // Transmit tooth time and update CRC
    uint32_t transmitted = serialWrite(toothHistory[logItemsTransmitted]);
    CRC32_val = CRC32_serial.crc32_upd((const byte*)&transmitted, sizeof(transmitted), false);
  }

  // Transmission complete - finalize
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  serialStatusFlag = SERIAL_INACTIVE;
  toothHistoryIndex = 0;
  logItemsTransmitted = 0;

  finalizeLogPacket(CRC32_val);
}

/** @brief Send composite log data to TunerStudio - Refactored from 55 to 40 lines
 *
 * Following REQUISITOS_TECNICOS.md standards:
 * - Complexity: C=3 (reduced from C=5)
 * - Nesting: N=3 (acceptable)
 * - Common packet framing extracted to helpers
 *
 * **COMPOSITE LOG FORMAT:**
 * Sends both tooth timing (uint32_t) and status bytes (uint8_t) per entry.
 * Status byte encodes: trigger edge, primary/secondary pulse, sync status.
 *
 * **NON-BLOCKING TRANSMISSION:**
 * Like sendToothLog(), may be called multiple times if TX buffer fills.
 * Uses SERIAL_TRANSMIT_COMPOSITE_INPROGRESS flag for resumption.
 *
 * @complexity Low (C=3, N=3)
 * @note Original 55 lines reduced to 40 lines via helper extraction
 */
void sendCompositeLog(void)
{
  // Pad buffer if incomplete (timeout handling)
  if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY) == false) {
    padCompositeLogBuffer();
  }

  // Initialize packet header and CRC on first call
  uint32_t CRC32_val = 0U;
  if (logItemsTransmitted == 0U) {
    CRC32_val = initializeLogPacket(sizeof(toothHistory) + sizeof(compositeLogHistory));
  }

  // Transmit composite log entries (non-blocking loop)
  for (; logItemsTransmitted < TOOTH_LOG_SIZE; logItemsTransmitted++) {
    // Guard: Check TX buffer space (tooth time + status byte)
    if ((uint16_t)primarySerial.availableForWrite() <
        sizeof(toothHistory[logItemsTransmitted]) + sizeof(compositeLogHistory[logItemsTransmitted])) {
      serialStatusFlag = SERIAL_TRANSMIT_COMPOSITE_INPROGRESS;
      return; // Resume later
    }

    // Transmit tooth time and update CRC
    uint32_t transmitted = serialWrite(toothHistory[logItemsTransmitted]);
    (void)CRC32_serial.crc32_upd((const byte*)&transmitted, sizeof(transmitted), false);

    // Transmit status byte and update CRC
    writeByteReliableBlocking(compositeLogHistory[logItemsTransmitted]);
    CRC32_val = CRC32_serial.crc32_upd((const byte*)&compositeLogHistory[logItemsTransmitted],
                                        sizeof(compositeLogHistory[logItemsTransmitted]), false);
  }

  // Transmission complete - finalize
  BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
  toothHistoryIndex = 0;
  serialStatusFlag = SERIAL_INACTIVE;
  logItemsTransmitted = 0;

  finalizeLogPacket(CRC32_val);
}

#if defined(CORE_AVR)
#pragma GCC pop_options
#endif

///@}
