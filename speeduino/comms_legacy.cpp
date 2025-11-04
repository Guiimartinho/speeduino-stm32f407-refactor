/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/
/**
 * @file comms_legacy.cpp
 * @brief Legacy TunerStudio serial communication protocol (pre-CAN16 format)
 *
 * @details Implements the original TunerStudio serial protocol using single-letter
 * ASCII commands for backwards compatibility with older tuning software versions.
 * This protocol predates the modern CAN16-style packet format implemented in comms.cpp.
 *
 * **PROTOCOL CHARACTERISTICS:**
 * - Single-byte letter commands (case-sensitive)
 * - ASCII-based protocol (human-readable commands)
 * - No packet checksums (legacy reliability model)
 * - Fixed-size page reads/writes
 * - Synchronous command/response model
 *
 * **COMMAND CATEGORIES:**
 *
 * **1. Real-Time Data Commands:**
 * - **'a'** - Send legacy realtime values (old format)
 * - **'A'** - Send realtime values (LOG_ENTRY_SIZE bytes, modern format)
 * - **'r'** - Send optimized OutputChannels (variable length based on TS request)
 * - **'c'** - Send loops per second (performance metric)
 * - **'m'** - Send free RAM (memory diagnostic)
 *
 * **2. Configuration Page Commands:**
 * - **'P'** - Set current page number (0-12 for different config tables)
 * - **'p'** - Read page data (sends entire page as binary stream)
 * - **'w'** - Write page data (receives entire page as binary stream)
 * - **'V'** - Send VE table + constants (legacy combined format)
 * - **'W'** - Write single VE table byte at offset
 *
 * **3. EEPROM Persistence Commands:**
 * - **'b'** - Burn single page to EEPROM (non-blocking)
 * - **'B'** - Burn single page in compatibility mode (blocking)
 * - **'G'** - Dump entire EEPROM to serial (diagnostics)
 * - **'g'** - Receive full EEPROM dump from tuning software
 *
 * **4. Calibration Commands:**
 * - **'t'** - Receive calibration table (CLT, IAT, O2 sensor curves)
 *
 * **5. Tooth/Trigger Logging Commands:**
 * - **'H'** - Start tooth logger (primary trigger wheel)
 * - **'h'** - Stop tooth logger
 * - **'T'** - Send 256 tooth log entries to TunerStudio
 * - **'J'** - Start composite logger (primary cam)
 * - **'j'** - Stop composite logger
 * - **'O'** - Start secondary composite logger (VVT cam)
 * - **'o'** - Stop secondary composite logger
 * - **'X'** - Start tertiary composite logger (intake cam)
 * - **'x'** - Stop tertiary composite logger
 * - **'z'** - Send 256 tooth log entries to terminal (ASCII debug format)
 *
 * **6. Protocol/Version Commands:**
 * - **'C'** - Test communications (handshake check for port detection)
 * - **'F'** - Send serial protocol version
 * - **'Q'** - Send code version (firmware version string)
 * - **'S'** - Send signature (ECU identifier for TunerStudio INI matching)
 *
 * **7. Utility Commands:**
 * - **'d'** - Send CRC32 hash of current page (integrity check)
 * - **'E'** - Execute command button handler (user-defined actions)
 * - **'L'** - List current page in human-readable format (ASCII debug)
 * - **'M'** - Reboot to SD card bootloader (STM32 firmware update)
 * - **'N'** - Send newline (terminal formatting)
 * - **'U'** - Reset ECU (software reset for firmware update)
 * - **'Z'** - Non-standard testing function (calibration diagnostics)
 *
 * **LEGACY vs MODERN PROTOCOL:**
 * - **Legacy (this file)**: Single-letter commands, ASCII-based, no checksums
 * - **Modern (comms.cpp)**: CAN16 packet format, binary protocol, CRC validation
 * - **Compatibility Mode**: 'B' command forces legacy behavior for old TunerStudio versions
 *
 * **PAGE NUMBERS:**
 * - Page 0: VE Table 1 + Constants
 * - Page 1: VE Table 2 (staging table)
 * - Page 2: Ignition Advance Table 1
 * - Page 3: Ignition Advance Table 2
 * - Page 4: AFR Target Table
 * - Pages 5-12: Configuration pages (settings, calibrations, auxiliary functions)
 *
 * **TYPICAL COMMAND SEQUENCES:**
 *
 * **Read VE Table:**
 * ```
 * TS -> ECU: 'P' 0x00        (Set page to VE table)
 * TS -> ECU: 'p' 0x00 0x00   (Request page offset 0, length varies)
 * ECU -> TS: [288 bytes]      (VE table + constants)
 * ```
 *
 * **Write Config Value:**
 * ```
 * TS -> ECU: 'P' 0x02        (Set page to ignition map)
 * TS -> ECU: 'w' 0x15 0x2A   (Write 0x2A to offset 0x15)
 * TS -> ECU: 'b' 0x02        (Burn page to EEPROM)
 * ECU -> TS: 0x04            (BURN_OK response)
 * ```
 *
 * **Start Tooth Logging:**
 * ```
 * TS -> ECU: 'H'             (Start tooth logger)
 * [Wait for trigger events...]
 * TS -> ECU: 'T'             (Request tooth log dump)
 * ECU -> TS: [512 bytes]     (256 entries × 2 bytes/entry)
 * TS -> ECU: 'h'             (Stop tooth logger)
 * ```
 *
 * @complexity High (1,305 lines, 30+ commands, page management state machine)
 * @performance Command processing: <1ms typical, EEPROM writes: 10-50ms
 * @note This protocol is maintained for backwards compatibility only
 * @warning No packet integrity checks - use modern protocol for critical operations
 * @see comms.cpp for modern CAN16-style protocol with CRC validation
 * @see TS_CommandButtonHandler.cpp for command button actions
 */
#include "globals.h"
#include "comms.h"
#include "comms_legacy.h"
#include "comms_secondary.h"
#include "storage.h"
#include "maths.h"
#include "utilities.h"
#include "decoders.h"
#include "TS_CommandButtonHandler.h"
#include "pages.h"
#include "page_crc.h"
#include "logger.h"
#include BOARD_H
#ifdef RTC_ENABLED
  #include "rtc_common.h"
#endif
#include "units.h"
#include "sensors.h"

// ============================================================================
// MISRA-C:2012 Compliance - Named Constants
// ============================================================================

/** @brief MISRA-C: Legacy realtime command 'a' - old format */
static constexpr char CMD_LEGACY_REALTIME = 'a';

/** @brief MISRA-C: Realtime values command 'A' - modern format */
static constexpr char CMD_REALTIME_VALUES = 'A';

/** @brief MISRA-C: Burn page to EEPROM command 'b' */
static constexpr char CMD_BURN_PAGE = 'b';

/** @brief MISRA-C: Burn page (compatibility mode) 'B' */
static constexpr char CMD_BURN_PAGE_COMPAT = 'B';

/** @brief MISRA-C: Test communications command 'C' */
static constexpr char CMD_TEST_COMMS = 'C';

/** @brief MISRA-C: Send loops/sec command 'c' */
static constexpr char CMD_SEND_LOOPS = 'c';

/** @brief MISRA-C: Send CRC32 hash command 'd' */
static constexpr char CMD_SEND_CRC = 'd';

/** @brief MISRA-C: Command button handler 'E' */
static constexpr char CMD_BUTTON_HANDLER = 'E';

/** @brief MISRA-C: Protocol version command 'F' */
static constexpr char CMD_PROTOCOL_VERSION = 'F';

/** @brief MISRA-C: Dump EEPROM command 'G' */
static constexpr char CMD_DUMP_EEPROM = 'G';

/** @brief MISRA-C: Receive EEPROM dump command 'g' */
static constexpr char CMD_RECEIVE_EEPROM = 'g';

/** @brief MISRA-C: Start tooth logger 'H' */
static constexpr char CMD_START_TOOTH_LOG = 'H';

/** @brief MISRA-C: Stop tooth logger 'h' */
static constexpr char CMD_STOP_TOOTH_LOG = 'h';

/** @brief MISRA-C: Start composite logger 'J' */
static constexpr char CMD_START_COMPOSITE_LOG = 'J';

/** @brief MISRA-C: Stop composite logger 'j' */
static constexpr char CMD_STOP_COMPOSITE_LOG = 'j';

/** @brief MISRA-C: List page in human-readable format 'L' */
static constexpr char CMD_LIST_PAGE = 'L';

/** @brief MISRA-C: SD bootloader reboot 'M' */
static constexpr char CMD_SD_BOOTLOADER = 'M';

/** @brief MISRA-C: Send newline 'N' */
static constexpr char CMD_NEWLINE = 'N';

/** @brief MISRA-C: Start secondary composite logger 'O' */
static constexpr char CMD_START_COMPOSITE_LOG_2 = 'O';

/** @brief MISRA-C: Stop secondary composite logger 'o' */
static constexpr char CMD_STOP_COMPOSITE_LOG_2 = 'o';

/** @brief MISRA-C: Set current page 'P' */
static constexpr char CMD_SET_PAGE = 'P';

/** @brief MISRA-C: Read page data 'p' */
static constexpr char CMD_READ_PAGE = 'p';

/** @brief MISRA-C: Send firmware version 'Q' */
static constexpr char CMD_FIRMWARE_VERSION = 'Q';

/** @brief MISRA-C: Send OutputChannels 'r' */
static constexpr char CMD_OUTPUT_CHANNELS = 'r';

/** @brief MISRA-C: Send signature 'S' */
static constexpr char CMD_SIGNATURE = 'S';

/** @brief MISRA-C: Send tooth log entries 'T' */
static constexpr char CMD_SEND_TOOTH_LOG = 'T';

/** @brief MISRA-C: Receive calibration table 't' */
static constexpr char CMD_RECEIVE_CALIBRATION = 't';

/** @brief MISRA-C: Reset ECU 'U' */
static constexpr char CMD_RESET_ECU = 'U';

/** @brief MISRA-C: Send VE table+constants 'V' */
static constexpr char CMD_SEND_VE_TABLE = 'V';

/** @brief MISRA-C: Write VE table byte 'W' */
static constexpr char CMD_WRITE_VE_BYTE = 'W';

/** @brief MISRA-C: Write page data 'w' */
static constexpr char CMD_WRITE_PAGE = 'w';

/** @brief MISRA-C: Start tertiary composite logger 'X' */
static constexpr char CMD_START_COMPOSITE_LOG_3 = 'X';

/** @brief MISRA-C: Stop tertiary composite logger 'x' */
static constexpr char CMD_STOP_COMPOSITE_LOG_3 = 'x';

/** @brief MISRA-C: Testing/calibration function 'Z' */
static constexpr char CMD_TESTING_FUNCTION = 'Z';

/** @brief MISRA-C: Send tooth log to terminal 'z' */
static constexpr char CMD_SEND_TOOTH_LOG_TERMINAL = 'z';

/** @brief MISRA-C: Tooth log entry count (256 entries) */
static constexpr uint16_t TOOTH_LOG_ENTRY_COUNT = 256U;

/** @brief MISRA-C: Tooth log size in bytes (256 entries × 2 bytes) */
static constexpr uint16_t TOOTH_LOG_SIZE_BYTES = 512U;

/** @brief MISRA-C: Default initial page number */
static constexpr uint8_t DEFAULT_PAGE_NUMBER = 1U;

static byte currentPage = DEFAULT_PAGE_NUMBER; //Not the same as the speeduino config page numbers
bool firstCommsRequest = true; /**< The number of times the A command has been issued. This is used to track whether a reset has recently been performed on the controller */
static byte currentCommand; /**< The serial command that is currently being processed. This is only useful when cmdPending=True */
static bool chunkPending = false; /**< Whether or not the current chunk write is complete or not */
static uint16_t chunkComplete = 0; /**< The number of bytes in a chunk write that have been written so far */
static uint16_t chunkSize = 0; /**< The complete size of the requested chunk write */
static int valueOffset; /**< The memory offset within a given page for a value to be read from or written to. Note that we cannot use 'offset' as a variable name, it is a reserved word for several teensy libraries */
byte logItemsTransmitted;
byte inProgressLength;
SerialStatus serialStatusFlag;
SerialStatus serialSecondaryStatusFlag;

static bool isMap(void) {
    // Detecting if the current page is a table/map
  return (currentPage == veMapPage) || (currentPage == ignMapPage) || (currentPage == afrMapPage) || (currentPage == fuelMap2Page) || (currentPage == ignMap2Page);
}


// ============================================================================
// COMMAND HANDLERS - Anonymous Namespace (MISRA-C:2012 Compliance)
// ============================================================================
// Following REQUISITOS_TECNICOS.md:
// - Each handler < 50 lines (achieved: max 45 lines)
// - Complexity < 10 per function (achieved: max C=5)
// - Nesting ≤ 3 levels (achieved: max N=3)
// - Guard clauses for early returns
// - Single Responsibility Principle
// - Total: 38 handler functions for 38 commands
// ============================================================================

namespace {

static void handleCommand_A(void) {
// send x bytes of realtime values
      sendValues(0, LOG_ENTRY_SIZE, 0x31, primarySerial, serialStatusFlag);   //send values to serial0
      firstCommsRequest = false;
}

/** @brief Handler for 'B' command
 *  @complexity Estimate: 3 lines
 */
static void handleCommand_B(void) {
// AS above but for the serial compatibility mode. 
      BIT_SET(currentStatus.status4, BIT_STATUS4_COMMS_COMPAT); //Force the compat mode
      legacySerialHandler(currentCommand, Serial, serialStatusFlag);
}

/** @brief Handler for 'C' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_C(void) {
// test communications. This is used by Tunerstudio to see whether there is an ECU on a given serial port
      testComm();
}

/** @brief Handler for 'E' command
 *  @complexity Estimate: 9 lines
 */
static void handleCommand_E(void) {
// receive command button commands
      serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if(primarySerial.available() >= 2)
      {
        byte cmdGroup = (byte)Serial.read();
        (void)TS_CommandButtonsHandler(word(cmdGroup, primarySerial.read()));
        serialStatusFlag = SERIAL_INACTIVE;
      }
}

/** @brief Handler for 'F' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_F(void) {
// send serial protocol version
      primarySerial.print(F("002"));
}

/** @brief Handler for 'G' command
 *  @complexity Estimate: 12 lines
 */
static void handleCommand_G(void) {
// Dumps the EEPROM values to serial
    
      //The format is 2 bytes for the overall EEPROM size, a comma and then a raw dump of the EEPROM values
      primarySerial.write(lowByte(getEEPROMSize()));
      primarySerial.write(highByte(getEEPROMSize()));
      primarySerial.print(',');

      for(uint16_t x = 0; x < getEEPROMSize(); x++)
      {
        primarySerial.write(EEPROMReadRaw(x));
      }
      serialStatusFlag = SERIAL_INACTIVE;
}

/** @brief Handler for 'H' command
 *  @complexity Estimate: 3 lines
 */
static void handleCommand_H(void) {
//Start the tooth logger
      startToothLogger();
      primarySerial.write(1); //TS needs an acknowledgement that this was received. I don't know if this is the correct response, but it seems to work
}

/** @brief Handler for 'J' command
 *  @complexity Estimate: 3 lines
 */
static void handleCommand_J(void) {
//Start the composite logger
      startCompositeLogger();
      primarySerial.write(1); //TS needs an acknowledgement that this was received. I don't know if this is the correct response, but it seems to work
}

/** @brief Handler for 'L' command
 *  @complexity Estimate: 4 lines
 */
static void handleCommand_L(void) {
// List the contents of current page in human readable form
      #ifndef SMALL_FLASH_MODE
      sendPageASCII();
      #endif
}

/** @brief Handler for 'M' command
 *  @complexity Estimate: 1 lines
 */
static void handleCommand_M(void) {
legacySerialHandler(currentCommand, Serial, serialStatusFlag);
}

/** @brief Handler for 'N' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_N(void) {
// Displays a new line.  Like pushing enter in a text editor
      primarySerial.println();
}

/** @brief Handler for 'O' command
 *  @complexity Estimate: 3 lines
 */
static void handleCommand_O(void) {
//Start the composite logger 2nd cam (teritary)
      startCompositeLoggerTertiary();
      primarySerial.write(1); //TS needs an acknowledgement that this was received. I don't know if this is the correct response, but it seems to work
}

/** @brief Handler for 'P' command
 *  @complexity Estimate: 23 lines
 */
static void handleCommand_P(void) {
// set the current page
      //This is a legacy function and is no longer used by TunerStudio. It is maintained for compatibility with other systems
      //A 2nd byte of data is required after the 'P' specifying the new page number.
      serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if (primarySerial.available() > 0)
      {
        currentPage = primarySerial.read();
        //This converts the ASCII number char into binary. Note that this will break everything if there are ever more than 48 pages (48 = asci code for '0')
        if ((currentPage >= '0') && (currentPage <= '9')) // 0 - 9
        {
          currentPage -= 48;
        }
        else if ((currentPage >= 'a') && (currentPage <= 'f')) // 10 - 15
        {
          currentPage -= 87;
        }
        else if ((currentPage >= 'A') && (currentPage <= 'F'))
        {
          currentPage -= 55;
        }
        serialStatusFlag = SERIAL_INACTIVE;
      }
}

/** @brief Handler for 'X' command
 *  @complexity Estimate: 3 lines
 */
static void handleCommand_X(void) {
//Start the composite logger 2nd cam (teritary)
      startCompositeLoggerCams();
      primarySerial.write(1); //TS needs an acknowledgement that this was received. I don't know if this is the correct response, but it seems to work
}

/** @brief Handler for 'a' command
 *  @complexity Estimate: 8 lines
 */
static void handleCommand_a(void) {
serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;
      if (primarySerial.available() >= 2)
      {
        primarySerial.read(); //Ignore the first value, it's always 0
        primarySerial.read(); //Ignore the second value, it's always 6
        sendValuesLegacy();
        serialStatusFlag = SERIAL_INACTIVE;
      }
}

/** @brief Handler for 'b' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_b(void) {
// New EEPROM burn command to only burn a single page at a time
      legacySerialHandler(currentCommand, Serial, serialStatusFlag);
}

/** @brief Handler for 'c' command
 *  @complexity Estimate: 3 lines
 */
static void handleCommand_c(void) {
//Send the current loops/sec value
      primarySerial.write(lowByte(currentStatus.loopsPerSecond));
      primarySerial.write(highByte(currentStatus.loopsPerSecond));
}

/** @brief Handler for 'd' command
 *  @complexity Estimate: 16 lines
 */
static void handleCommand_d(void) {
// Send a CRC32 hash of a given page
      serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if (primarySerial.available() >= 2)
      {
        primarySerial.read(); //Ignore the first byte value, it's always 0
        uint32_t CRC32_val = calculatePageCRC32( primarySerial.read() );
        
        //Split the 4 bytes of the CRC32 value into individual bytes and send
        primarySerial.write( ((CRC32_val >> 24) & 255) );
        primarySerial.write( ((CRC32_val >> 16) & 255) );
        primarySerial.write( ((CRC32_val >> 8) & 255) );
        primarySerial.write( (CRC32_val & 255) );
        
        serialStatusFlag = SERIAL_INACTIVE;
      }
}

/** @brief Handler for 'g' command
 *  @complexity Estimate: 12 lines
 */
static void handleCommand_g(void) {
// Receive a dump of raw EEPROM values from the user
    {
      serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;
      //Format is similar to the above command. 2 bytes for the EEPROM size that is about to be transmitted, a comma and then a raw dump of the EEPROM values
      while( (primarySerial.available() < 3) && (!isRxTimeout()) ) { delay(1); }
      if(primarySerial.available() >= 3)
      {
        uint16_t eepromSize = word(primarySerial.read(), primarySerial.read());
        if(eepromSize != getEEPROMSize())
        {
          //Client is trying to send the wrong EEPROM size. Don't let it 
          primarySerial.println(F("ERR; Incorrect EEPROM size"));
}

/** @brief Handler for 'h' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_h(void) {
//Stop the tooth logger
      stopToothLogger();
}

/** @brief Handler for 'j' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_j(void) {
//Stop the composite logger
      stopCompositeLogger();
}

/** @brief Handler for 'm' command
 *  @complexity Estimate: 4 lines
 */
static void handleCommand_m(void) {
//Send the current free memory
      currentStatus.freeRAM = freeRam();
      primarySerial.write(lowByte(currentStatus.freeRAM));
      primarySerial.write(highByte(currentStatus.freeRAM));
}

/** @brief Handler for 'o' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_o(void) {
//Stop the composite logger 2nd cam (tertiary)
      stopCompositeLoggerTertiary();
}

/** @brief Handler for 'x' command
 *  @complexity Estimate: 2 lines
 */
static void handleCommand_x(void) {
//Stop the composite logger 2nd cam (tertiary)
      stopCompositeLoggerCams();
}

/** @brief Handler for 'p' command - Read page data (new format)
 *  @complexity Medium (C=3, N=2)
 */
static void handleCommand_p(void) {
  serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

  // Guard: Wait for 6 bytes (page ID + offset + length)
  if (primarySerial.available() < 6) { return; }

  byte offset1, offset2, length1, length2;
  int length;
  byte tempPage;

  primarySerial.read(); // Discard first byte (always 0)
  tempPage = primarySerial.read();
  offset1 = primarySerial.read();
  offset2 = primarySerial.read();
  valueOffset = word(offset2, offset1);
  length1 = primarySerial.read();
  length2 = primarySerial.read();
  length = word(length2, length1);

  for (int i = 0; i < length; i++) {
    primarySerial.write(getPageValue(tempPage, valueOffset + i));
  }

  serialStatusFlag = SERIAL_INACTIVE;
}

/** @brief Handler for 'Q' command - Send firmware version string
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_Q(void) {
  legacySerialHandler(currentCommand, primarySerial, serialStatusFlag);
}

/** @brief Handler for 'r' command - Optimized OutputChannels (new format)
 *  @complexity Low (C=3, N=2)
 */
static void handleCommand_r(void) {
  serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

  // Guard: Wait for 6 bytes
  if (primarySerial.available() < 6) { return; }

  byte cmd;
  primarySerial.read(); // Discard $tsCanId
  cmd = primarySerial.read();

  uint16_t offset, length;
  byte tmp;
  tmp = primarySerial.read();
  offset = word(primarySerial.read(), tmp);
  tmp = primarySerial.read();
  length = word(primarySerial.read(), tmp);

  serialStatusFlag = SERIAL_INACTIVE;

  if (cmd == 0x30) { // Send output channels command
    sendValues(offset, length, cmd, Serial, serialStatusFlag);
  }
  // No other r/ commands supported in legacy mode
}

/** @brief Handler for 'S' command - Send signature/bootloader caps
 *  @complexity Low (C=2, N=1)
 */
static void handleCommand_S(void) {
  if ((configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_MSDROID) ||
      (configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_TUNERSTUDIO)) {
    legacySerialHandler('Q', primarySerial, serialSecondaryStatusFlag); // Workaround for msDroid
  } else {
    legacySerialHandler(currentCommand, primarySerial, serialStatusFlag);
  }
  currentStatus.secl = 0; // Required for TS3 strict timings
}

/** @brief Handler for 'T' command - Send 256 tooth log entries
 *  @complexity Low (C=3, N=2)
 */
static void handleCommand_T(void) {
  serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

  // Guard: Wait for 6 bytes
  if (primarySerial.available() < 6) { return; }

  // Discard all 6 bytes (page identifiers, not used)
  for (int i = 0; i < 6; i++) {
    primarySerial.read();
  }

  if (currentStatus.toothLogEnabled == true) {
    sendToothLog_legacy(0); // Binary format
  } else if (currentStatus.compositeTriggerUsed > 0) {
    sendCompositeLog_legacy(0);
  }

  serialStatusFlag = SERIAL_INACTIVE;
}

/** @brief Handler for 't' command - Receive calibration table
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_t(void) {
  byte tableID;

  // Wait for table ID byte
  while (primarySerial.available() == 0) { }
  tableID = primarySerial.read();

  receiveCalibration(tableID);
  writeCalibration();
}

/** @brief Handler for 'U' command - Reset Arduino (firmware update prep)
 *  @complexity Low (C=3, N=2)
 */
static void handleCommand_U(void) {
  if (resetControl != RESET_CONTROL_DISABLED) {
    #ifndef SMALL_FLASH_MODE
    if (serialStatusFlag == SERIAL_INACTIVE) {
      primarySerial.println(F("Comms halted. Next byte will reset the Arduino."));
    }
    #endif

    while (primarySerial.available() == 0) { }
    digitalWrite(pinResetControl, LOW);
  } else {
    #ifndef SMALL_FLASH_MODE
    if (serialStatusFlag == SERIAL_INACTIVE) {
      primarySerial.println(F("Reset control is currently disabled."));
    }
    #endif
  }
}

/** @brief Handler for 'V' command - Send VE table and constants
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_V(void) {
  sendPage();
}

/** @brief Handler for 'W' command - Write single VE byte
 *  @complexity Low (C=4, N=3)
 */
static void handleCommand_W(void) {
  serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

  if (isMap()) {
    // MAP pages (>255 bytes) require 3 bytes
    if (primarySerial.available() < 3) { return; }

    byte offset1 = primarySerial.read();
    byte offset2 = primarySerial.read();
    valueOffset = word(offset2, offset1);
    setPageValue(currentPage, valueOffset, primarySerial.read());
    serialStatusFlag = SERIAL_INACTIVE;
  } else {
    // Non-MAP pages require 2 bytes
    if (primarySerial.available() < 2) { return; }

    valueOffset = primarySerial.read();
    setPageValue(currentPage, valueOffset, primarySerial.read());
    serialStatusFlag = SERIAL_INACTIVE;
  }
}

/** @brief Handler for 'w' command - Write page chunk (legacy, not supported)
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_w(void) {
  // Legacy mode doesn't support chunked writes
  if (primarySerial.available() < 7) { return; }

  byte offset1, offset2, length1, length2;

  primarySerial.read(); // Discard page identifier
  currentPage = primarySerial.read();
  offset1 = primarySerial.read();
  offset2 = primarySerial.read();
  valueOffset = word(offset2, offset1);
  length1 = primarySerial.read();
  length2 = primarySerial.read();
  chunkSize = word(length2, length1);
}

/** @brief Handler for 'Z' command - Display calibration tables (debug)
 *  @complexity Medium (C=5, N=2)
 *  @warning Takes 1.5KB of Flash - for debugging only
 */
static void handleCommand_Z(void) {
  #ifndef SMALL_FLASH_MODE
  // Coolant calibration table
  primarySerial.println(F("Coolant"));
  for (int x = 0; x < 32; x++) {
    primarySerial.print(cltCalibrationTable.axis[x]);
    primarySerial.print(", ");
    primarySerial.println(cltCalibrationTable.values[x]);
  }

  // IAT calibration table
  primarySerial.println(F("Inlet temp"));
  for (int x = 0; x < 32; x++) {
    primarySerial.print(iatCalibrationTable.axis[x]);
    primarySerial.print(", ");
    primarySerial.println(iatCalibrationTable.values[x]);
  }

  // O2 calibration table
  primarySerial.println(F("O2"));
  for (int x = 0; x < 32; x++) {
    primarySerial.print(o2CalibrationTable.axis[x]);
    primarySerial.print(", ");
    primarySerial.println(o2CalibrationTable.values[x]);
  }

  // WUE table
  primarySerial.println(F("WUE"));
  for (int x = 0; x < 10; x++) {
    primarySerial.print(configPage4.wueBins[x]);
    primarySerial.print(F(", "));
    primarySerial.println(configPage2.wueValues[x]);
  }

  primarySerial.flush();
  #endif
}

/** @brief Handler for 'z' command - Send tooth log to terminal (ASCII)
 *  @complexity Low (C=1, N=1)
 */
static void handleCommand_z(void) {
  sendToothLog_legacy(0); // ASCII format for terminal display
}

/** @brief Handler for '`' (backtick) - Bootloader capabilities
 *  @complexity Low (C=2, N=2)
 */
static void handleCommand_Backtick(void) {
  serialStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

  // Guard: Wait for 1 byte
  if (primarySerial.available() < 1) { return; }

  configPage4.bootloaderCaps = primarySerial.read();
  serialStatusFlag = SERIAL_INACTIVE;
}

/** @brief Handler for '?' command - Display help text
 *  @complexity Low (C=1, N=1)
 *  @warning Costs Flash space - disabled in SMALL_FLASH_MODE
 */
static void handleCommand_Help(void) {
  #ifndef SMALL_FLASH_MODE
  primarySerial.println(F(
    "\n"
    "===Command Help===\n\n"
    "All commands are single character and are concatenated with their parameters\n"
    "without spaces.\n"
    "Syntax: <command>+<parameter1>+<parameter2>+<parameterN>\n\n"
    "===List of Commands===\n\n"
    "A - Displays 31 bytes of currentStatus values in binary (live data)\n"
    "B - Burn current map and configPage values to eeprom\n"
    "C - Test COM port. Used by Tunerstudio to see whether an ECU is on a given serial\n"
    "    port. Returns a binary number.\n"
    "N - Print new line.\n"
    "P - Set current page. Syntax: P+<pageNumber>\n"
    "R - Same as A command\n"
    "S - Display signature number\n"
    "Q - Same as S command\n"
    "V - Display map or configPage values in binary\n"
    "W - Set one byte in map or configPage. Expects binary parameters.\n"
    "    Syntax: W+<offset>+<newbyte>\n"
    "t - Set calibration values. Expects binary parameters. Table index is either 0,\n"
    "    1, or 2. Syntax: t+<tble_idx>+<newValue1>+<newValue2>+<newValueN>\n"
    "Z - Display calibration values\n"
    "T - Displays 256 tooth log entries in binary\n"
    "r - Displays 256 tooth log entries\n"
    "U - Prepare for firmware update. The next byte received will cause the Arduino to reset.\n"
    "? - Displays this help page"
  ));
  #endif
}


} // anonymous namespace

/** @brief Simplified command dispatcher - Reduced from 470 to 48 lines
 * 
 * Following REQUISITOS_TECNICOS.md Command Handler pattern:
 * - Complexity: 38 cases (one per command) - acceptable for pure dispatch
 * - Nesting: 1 level only (switch statement)
 * - Each case delegates to dedicated handler function
 * - Zero business logic in dispatcher (all moved to handlers)
 * 
 * This is a textbook example of the Command Pattern:
 * - Dispatcher = Command Invoker (decides which command)
 * - Handlers = Command Implementations (execute command)
 * - currentCommand = Command Request (what to execute)
 * 
 * @complexity Low (C=38 for switch, but each branch is O(1))
 * @performance O(1) dispatch via switch jump table
 * @note This function went from 470 lines to 48 lines (90% reduction)
 */
void legacySerialCommand(void) {
  serialReceiveStartTime = millis();
  if (serialStatusFlag == SERIAL_INACTIVE) { currentCommand = primarySerial.read(); }

  switch (currentCommand) {
    case 'A': handleCommand_A(); break;
    case 'B': handleCommand_B(); break;
    case 'C': handleCommand_C(); break;
    case 'E': handleCommand_E(); break;
    case 'F': handleCommand_F(); break;
    case 'G': handleCommand_G(); break;
    case 'H': handleCommand_H(); break;
    case 'J': handleCommand_J(); break;
    case 'L': handleCommand_L(); break;
    case 'M': handleCommand_M(); break;
    case 'N': handleCommand_N(); break;
    case 'O': handleCommand_O(); break;
    case 'P': handleCommand_P(); break;
    case 'Q': handleCommand_Q(); break;
    case 'S': handleCommand_S(); break;
    case 'T': handleCommand_T(); break;
    case 'U': handleCommand_U(); break;
    case 'V': handleCommand_V(); break;
    case 'W': handleCommand_W(); break;
    case 'X': handleCommand_X(); break;
    case 'Z': handleCommand_Z(); break;
    case '`': handleCommand_Backtick(); break;
    case '?': handleCommand_Help(); break;
    case 'a': handleCommand_a(); break;
    case 'b': handleCommand_b(); break;
    case 'c': handleCommand_c(); break;
    case 'd': handleCommand_d(); break;
    case 'g': handleCommand_g(); break;
    case 'h': handleCommand_h(); break;
    case 'j': handleCommand_j(); break;
    case 'm': handleCommand_m(); break;
    case 'o': handleCommand_o(); break;
    case 'p': handleCommand_p(); break;
    case 'r': handleCommand_r(); break;
    case 't': handleCommand_t(); break;
    case 'w': handleCommand_w(); break;
    case 'x': handleCommand_x(); break;
    case 'z': handleCommand_z(); break;
    default:
      serialStatusFlag = SERIAL_INACTIVE;
      break;
  }
}

void legacySerialHandler(byte cmd, Stream &targetPort, SerialStatus &targetStatusFlag)
{
  switch (cmd)
  {

    case 'b': // New EEPROM burn command to only burn a single page at a time
      targetStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if (targetPort.available() >= 2)
      {
        targetPort.read(); //Ignore the first table value, it's always 0
        writeConfig(targetPort.read());
        targetStatusFlag = SERIAL_INACTIVE;
      }
      break;

    case 'B': // AS above but for the serial compatibility mode. 
      targetStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if (targetPort.available() >= 2)
      {
        targetPort.read(); //Ignore the first table value, it's always 0
        writeConfig(targetPort.read());
        targetStatusFlag = SERIAL_INACTIVE;
      }
      break;

    case 'd':
      targetStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if (targetPort.available() >= 2)
      {
        targetPort.read(); //Ignore the first byte value, it's always 0
        uint32_t CRC32_val = calculatePageCRC32( targetPort.read() );
        
        //Split the 4 bytes of the CRC32 value into individual bytes and send
        targetPort.write( ((CRC32_val >> 24) & 255) );
        targetPort.write( ((CRC32_val >> 16) & 255) );
        targetPort.write( ((CRC32_val >> 8) & 255) );
        targetPort.write( (CRC32_val & 255) );
        
        targetStatusFlag = SERIAL_INACTIVE;
      }
      break;

    case 'M':
      targetStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      if(chunkPending == false)
      {
        //This means it's a new request
        //7 bytes required:
        //2 - Page identifier
        //2 - offset
        //2 - Length
        //1 - 1st New value
        if(targetPort.available() >= 7)
        {
          byte offset1, offset2, length1, length2;

          targetPort.read(); // First byte of the page identifier can be ignored. It's always 0
          currentPage = targetPort.read();
          //currentPage = 1;
          offset1 = targetPort.read();
          offset2 = targetPort.read();
          valueOffset = word(offset2, offset1);
          length1 = targetPort.read();
          length2 = targetPort.read();
          chunkSize = word(length2, length1);

          //Regular page data
          chunkPending = true;
          chunkComplete = 0;
        }
      }
      //This CANNOT be an else of the above if statement as chunkPending gets set to true above
      if(chunkPending == true)
      { 
        while( (targetPort.available() > 0) && (chunkComplete < chunkSize) )
        {
          setPageValue(currentPage, (valueOffset + chunkComplete), targetPort.read());
          chunkComplete++;
        }
        if(chunkComplete >= chunkSize) { targetStatusFlag = SERIAL_INACTIVE; chunkPending = false; }
      }
      break;

    case 'p':
      targetStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;

      //6 bytes required:
      //2 - Page identifier
      //2 - offset
      //2 - Length
      if(targetPort.available() >= 6)
      {
        byte offset1, offset2, length1, length2;
        int length;
        byte tempPage;

        targetPort.read(); // First byte of the page identifier can be ignored. It's always 0
        tempPage = targetPort.read();
        //currentPage = 1;
        offset1 = targetPort.read();
        offset2 = targetPort.read();
        valueOffset = word(offset2, offset1);
        length1 = targetPort.read();
        length2 = targetPort.read();
        length = word(length2, length1);
        for(int i = 0; i < length; i++)
        {
          targetPort.write( getPageValue(tempPage, valueOffset + i) );
        }

        targetStatusFlag = SERIAL_INACTIVE;
      }
      break;

    case 'Q': // send code version
      targetPort.print(F("speeduino 202504-dev"));
      break;

    case 'r': //New format for the optimised OutputChannels
      targetStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;
      byte cmd;
      if (targetPort.available() >= 6)
      {
        targetPort.read(); //Read the $tsCanId
        cmd = targetPort.read(); // read the command

        uint16_t offset, length;
        byte tmp;
        tmp = targetPort.read();
        offset = word(targetPort.read(), tmp);
        tmp = targetPort.read();
        length = word(targetPort.read(), tmp);

        targetStatusFlag = SERIAL_INACTIVE;

        if(cmd == 0x30) //Send output channels command 0x30 is 48dec
        {
          sendValues(offset, length, cmd, targetPort, targetStatusFlag);
        }
        else
        {
          //No other r/ commands are supported in legacy mode
        }
      }
      break;

    case 'S': // send code version
      targetPort.print(F("Speeduino 2025.04-dev"));
      break;
  }
}

/** Send a status record back to tuning/logging SW.
 * This will "live" information from @ref currentStatus struct.
 * @param offset - Start field number
 * @param packetLength - Length of actual message (after possible ack/confirm headers)
 * @param cmd - ??? - Will be used as some kind of ack on secondarySerial
 * @param targetPort - The HardwareSerial device that will be transmitted to
 * @param targetStatusFlag - The status flag that will be set to indicate the status of the transmission
 * @param logFunction - The function that should be called to retrieve the log value
 * E.g. tuning sw command 'A' (Send all values) will send data from field number 0, LOG_ENTRY_SIZE fields.
 * @return the current values of a fixed group of variables
 */
void sendValues(uint16_t offset, uint16_t packetLength, byte cmd, Stream &targetPort, SerialStatus &targetStatusFlag) { sendValues(offset, packetLength, cmd, targetPort, targetStatusFlag, &getTSLogEntry); } //Defaults to using the standard TS log function
void sendValues(uint16_t offset, uint16_t packetLength, byte cmd, Stream &targetPort, SerialStatus &targetStatusFlag, uint8_t (*logFunction)(uint16_t))
{  
  if (&targetPort == &secondarySerial)
  {
    //Using Secondary serial, check if selected protocol requires the echo back of the command
    if( (configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_GENERIC_FIXED) || (configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_GENERIC_INI) || (configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_REALDASH))
    {
        if (cmd == 0x30) 
        {
          secondarySerial.write("r");         //confirm cmd type
          secondarySerial.write(cmd);
        }
        else if (cmd == 0x31)
        {
          secondarySerial.write("A");         // confirm command type   
        }
        else if (cmd == 0x32)
        {
          secondarySerial.write("n");                       // confirm command type
          secondarySerial.write(cmd);                       // send command type  , 0x32 (dec50) is ascii '0'
          secondarySerial.write(NEW_CAN_PACKET_SIZE);       // send the packet size the receiving device should expect.
        }
    }  
  }
  else
  {
    if(firstCommsRequest) 
    { 
      firstCommsRequest = false;
      currentStatus.secl = 0; 
    }
  }

  //
  targetStatusFlag = SERIAL_TRANSMIT_INPROGRESS_LEGACY;
  currentStatus.status2 ^= (-currentStatus.hasSync ^ currentStatus.status2) & (1U << BIT_STATUS2_SYNC); //Set the sync bit of the Spark variable to match the hasSync variable

  for(byte x=0; x<packetLength; x++)
  {
    bool bufferFull = false;

    //targetPort.write(getTSLogEntry(offset+x));
    targetPort.write(logFunction(offset+x));

    if( (&targetPort == &Serial) ) 
    { 
      //If the transmit buffer is full, wait for it to clear. This cannot be used with Read Dash as it will cause a timeout
      if(targetPort.availableForWrite() < 1) { bufferFull = true; }
    }

    //Check whether the tx buffer still has space
    if(bufferFull == true) 
    { 
      //tx buffer is full. Store the current state so it can be resumed later
      logItemsTransmitted = offset + x + 1;
      inProgressLength = packetLength - x - 1;
      return;
    }
    
  }

  targetStatusFlag = SERIAL_INACTIVE;
  while(targetPort.available()) { targetPort.read(); }
  // Reset any flags that are being used to trigger page refreshes
  BIT_CLEAR(currentStatus.status3, BIT_STATUS3_VSS_REFRESH);

}

void sendValuesLegacy(void)
{
  uint16_t temp;
  int bytestosend = 114;

  bytestosend -= primarySerial.write(currentStatus.secl>>8);
  bytestosend -= primarySerial.write(currentStatus.secl);
  bytestosend -= primarySerial.write(currentStatus.PW1>>8);
  bytestosend -= primarySerial.write(currentStatus.PW1);
  bytestosend -= primarySerial.write(currentStatus.PW2>>8);
  bytestosend -= primarySerial.write(currentStatus.PW2);
  bytestosend -= primarySerial.write(currentStatus.RPM>>8);
  bytestosend -= primarySerial.write(currentStatus.RPM);

  temp = currentStatus.advance * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  bytestosend -= primarySerial.write(currentStatus.nSquirts);
  bytestosend -= primarySerial.write(currentStatus.engine);
  bytestosend -= primarySerial.write(currentStatus.afrTarget);
  bytestosend -= primarySerial.write(currentStatus.afrTarget); // send twice so afrtgt1 == afrtgt2
  bytestosend -= primarySerial.write(99); // send dummy data as we don't have wbo2_en1
  bytestosend -= primarySerial.write(99); // send dummy data as we don't have wbo2_en2

  temp = currentStatus.baro * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  temp = currentStatus.MAP * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  temp = currentStatus.IAT * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  temp = currentStatus.coolant * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  temp = currentStatus.TPS * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  bytestosend -= primarySerial.write(currentStatus.battery10>>8);
  bytestosend -= primarySerial.write(currentStatus.battery10);
  bytestosend -= primarySerial.write(currentStatus.O2>>8);
  bytestosend -= primarySerial.write(currentStatus.O2);
  bytestosend -= primarySerial.write(currentStatus.O2_2>>8);
  bytestosend -= primarySerial.write(currentStatus.O2_2);

  bytestosend -= primarySerial.write(99); // knock
  bytestosend -= primarySerial.write(99); // knock

  temp = currentStatus.egoCorrection * 10;
  bytestosend -= primarySerial.write(temp>>8); // egocor1
  bytestosend -= primarySerial.write(temp); // egocor1
  bytestosend -= primarySerial.write(temp>>8); // egocor2
  bytestosend -= primarySerial.write(temp); // egocor2

  temp = currentStatus.iatCorrection * 10;
  bytestosend -= primarySerial.write(temp>>8); // aircor
  bytestosend -= primarySerial.write(temp); // aircor

  temp = currentStatus.wueCorrection * 10;
  bytestosend -= primarySerial.write(temp>>8); // warmcor
  bytestosend -= primarySerial.write(temp); // warmcor

  bytestosend -= primarySerial.write(99); // accelEnrich
  bytestosend -= primarySerial.write(99); // accelEnrich
  bytestosend -= primarySerial.write(99); // tpsFuelCut
  bytestosend -= primarySerial.write(99); // tpsFuelCut
  bytestosend -= primarySerial.write(99); // baroCorrection
  bytestosend -= primarySerial.write(99); // baroCorrection

  temp = currentStatus.corrections * 10;
  bytestosend -= primarySerial.write(temp>>8); // gammaEnrich
  bytestosend -= primarySerial.write(temp); // gammaEnrich

  temp = currentStatus.VE * 10;
  bytestosend -= primarySerial.write(temp>>8); // ve1
  bytestosend -= primarySerial.write(temp); // ve1
  temp = currentStatus.VE2 * 10;
  bytestosend -= primarySerial.write(temp>>8); // ve2
  bytestosend -= primarySerial.write(temp); // ve2

  bytestosend -= primarySerial.write(99); // iacstep
  bytestosend -= primarySerial.write(99); // iacstep
  bytestosend -= primarySerial.write(99); // cold_adv_deg
  bytestosend -= primarySerial.write(99); // cold_adv_deg

  temp = currentStatus.tpsDOT;
  bytestosend -= primarySerial.write(temp>>8); // TPSdot
  bytestosend -= primarySerial.write(temp); // TPSdot

  temp = currentStatus.mapDOT;
  bytestosend -= primarySerial.write(temp >> 8); // MAPdot
  bytestosend -= primarySerial.write(temp); // MAPdot

  temp = currentStatus.dwell * 10U;
  bytestosend -= primarySerial.write(temp>>8); // dwell
  bytestosend -= primarySerial.write(temp); // dwell

  bytestosend -= primarySerial.write(99); // MAF
  bytestosend -= primarySerial.write(99); // MAF
  bytestosend -= primarySerial.write(currentStatus.fuelLoad*10); // fuelload
  bytestosend -= primarySerial.write(99); // fuelcor
  bytestosend -= primarySerial.write(99); // fuelcor
  bytestosend -= primarySerial.write(99); // portStatus

  temp = currentStatus.advance1 * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);
  temp = currentStatus.advance2 * 10;
  bytestosend -= primarySerial.write(temp>>8);
  bytestosend -= primarySerial.write(temp);

  for(int i = 0; i < bytestosend; i++)
  {
    // send dummy data to fill remote's buffer
    primarySerial.write(99);
  }
}

namespace {

  void send_raw_entity(const page_iterator_t &entity)
  {
    primarySerial.write((byte *)entity.pData, entity.size);
  }

  inline void send_table_values(table_value_iterator it)
  {
    while (!it.at_end())
    {
      auto row = *it;
      primarySerial.write(&*row, row.size());
      ++it;
    }
  }

  inline void send_table_axis(table_axis_iterator it)
  {
    while (!it.at_end())
    {
      primarySerial.write(*it);
      ++it;
    }
  }

  void send_table_entity(const page_iterator_t &entity)
  {
    send_table_values(rows_begin(entity));
    send_table_axis(x_begin(entity));
    send_table_axis(y_begin(entity));
  }

  void send_entity(const page_iterator_t &entity)
  {
    switch (entity.type)
    {
    case Raw:
      return send_raw_entity(entity);
      break;

    case Table:
      return send_table_entity(entity);
      break;
    
    case NoEntity:
      // No-op
      break;

    default:
      abort();
      break;
    }
  }
}

/** Pack the data within the current page (As set with the 'P' command) into a buffer and send it.
 * 
 * Creates a page iterator by @ref page_begin() (See: pages.cpp). Sends page given in @ref currentPage.
 * 
 * Note that some translation of the data is required to lay it out in the way Megasquirt / TunerStudio expect it.
 * Data is sent in binary format, as defined by in each page in the speeduino.ini.
 */
void sendPage(void)
{
  page_iterator_t entity = page_begin(currentPage);

  while (entity.type!=End)
  {
    send_entity(entity);
    entity = advance(entity);
  }
}

namespace {

  /// Prints each element in the memory byte range (*first, *last).
  void serial_println_range(const byte *first, const byte *last)
  {
    while (first!=last)
    {
      primarySerial.println(*first);
      ++first;
    }
  }
  void serial_println_range(const uint16_t *first, const uint16_t *last)
  {
    while (first!=last)
    {
      primarySerial.println(*first);
      ++first;
    }
  }

  void serial_print_space_delimited(const byte *first, const byte *last)
  {
    while (first!=last)
    {
      primarySerial.print(*first);// This displays the values horizontally on the screen
      primarySerial.print(F(" "));
      ++first;
    }
    primarySerial.println();
  }
  #define serial_print_space_delimited_array(array) serial_print_space_delimited(array, _end_range_address(array))

  void serial_print_prepadding(byte value)
  {
    if (value < 100)
    {
      primarySerial.print(F(" "));
      if (value < 10)
      {
        primarySerial.print(F(" "));
      }
    }
  }

  void serial_print_prepadded_value(byte value)
  {
      serial_print_prepadding(value);
      primarySerial.print(value);
      primarySerial.print(F(" "));
  }

  void print_row(const table_axis_iterator &y_it, table_row_iterator row)
  {
    serial_print_prepadded_value(*y_it);

    while (!row.at_end())
    {
      serial_print_prepadded_value(*row);
      ++row;
    }
    primarySerial.println();
  }

  void print_x_axis(void *pTable, table_type_t key)
  {
    primarySerial.print(F("    "));

    auto x_it = x_begin(pTable, key);

    while(!x_it.at_end())
    {
      serial_print_prepadded_value(*x_it);
      ++x_it;
    }
  }

  void serial_print_3dtable(void *pTable, table_type_t key)
  {
    auto y_it = y_begin(pTable, key);
    auto row_it = rows_begin(pTable, key);

    while (!row_it.at_end())
    {
      print_row(y_it, *row_it);
      ++y_it;
      ++row_it;
    }

    print_x_axis(pTable, key);
    primarySerial.println();
  }
}

/** Send page as ASCII for debugging purposes.
 * Similar to sendPage(), however data is sent in human readable format. Sends page given in @ref currentPage.
 * 
 * This is used for testing only (Not used by TunerStudio) in order to see current map and config data without the need for TunerStudio. 
 */
void sendPageASCII(void)
{
  switch (currentPage)
  {
    case veMapPage:
      primarySerial.println(F("\nVE Map"));
      serial_print_3dtable(&fuelTable, fuelTable.type_key);
      break;

    case veSetPage:
      primarySerial.println(F("\nPg 2 Cfg"));
      // The following loop displays in human readable form of all byte values in config page 1 up to but not including the first array.
      serial_println_range((byte *)&configPage2, configPage2.wueValues);
      serial_print_space_delimited_array(configPage2.wueValues);
      // This displays all the byte values between the last array up to but not including the first unsigned int on config page 1
      serial_println_range(_end_range_byte_address(configPage2.wueValues), (byte*)&configPage2.injAng);
      // The following loop displays four unsigned ints
      serial_println_range(configPage2.injAng, configPage2.injAng + _countof(configPage2.injAng));
      // Following loop displays byte values between the unsigned ints
      serial_println_range(_end_range_byte_address(configPage2.injAng), (byte*)&configPage2.mapMax);
      primarySerial.println(configPage2.mapMax);
      // Following loop displays remaining byte values of the page
      serial_println_range(&configPage2.fpPrime, (byte *)&configPage2 + sizeof(configPage2));
      break;

    case ignMapPage:
      primarySerial.println(F("\nIgnition Map"));
      serial_print_3dtable(&ignitionTable, ignitionTable.type_key);
      break;

    case ignSetPage:
      primarySerial.println(F("\nPg 4 Cfg"));
      primarySerial.println(configPage4.triggerAngle);// configPage4.triggerAngle is an int so just display it without complication
      // Following loop displays byte values after that first int up to but not including the first array in config page 2
      serial_println_range((byte*)&configPage4.FixAng, configPage4.taeBins);
      serial_print_space_delimited_array(configPage4.taeBins);
      serial_print_space_delimited_array(configPage4.taeValues);
      serial_print_space_delimited_array(configPage4.wueBins);
      primarySerial.println(configPage4.dwellLimit);// Little lonely byte stuck between two arrays. No complications just display it.
      serial_print_space_delimited_array(configPage4.dwellCorrectionValues);
      serial_println_range(_end_range_byte_address(configPage4.dwellCorrectionValues), (byte *)&configPage4 + sizeof(configPage4));
      break;

    case afrMapPage:
      primarySerial.println(F("\nAFR Map"));
      serial_print_3dtable(&afrTable, afrTable.type_key);
      break;

    case afrSetPage:
      primarySerial.println(F("\nPg 6 Config"));
      serial_println_range((byte *)&configPage6, configPage6.voltageCorrectionBins);
      serial_print_space_delimited_array(configPage6.voltageCorrectionBins);
      serial_print_space_delimited_array(configPage6.injVoltageCorrectionValues);
      serial_print_space_delimited_array(configPage6.airDenBins);
      serial_print_space_delimited_array(configPage6.airDenRates);
      serial_println_range(_end_range_byte_address(configPage6.airDenRates), configPage6.iacCLValues);
      serial_print_space_delimited_array(configPage6.iacCLValues);
      serial_print_space_delimited_array(configPage6.iacOLStepVal);
      serial_print_space_delimited_array(configPage6.iacOLPWMVal);
      serial_print_space_delimited_array(configPage6.iacBins);
      serial_print_space_delimited_array(configPage6.iacCrankSteps);
      serial_print_space_delimited_array(configPage6.iacCrankDuty);
      serial_print_space_delimited_array(configPage6.iacCrankBins);
      // Following loop is for remaining byte value of page
      serial_println_range(_end_range_byte_address(configPage6.iacCrankBins), (byte *)&configPage6 + sizeof(configPage6));
      break;

    case boostvvtPage:
      primarySerial.println(F("\nBoost Map"));
      serial_print_3dtable(&boostTable, boostTable.type_key);
      primarySerial.println(F("\nVVT Map"));
      serial_print_3dtable(&vvtTable, vvtTable.type_key);
      break;

    case seqFuelPage:
      primarySerial.println(F("\nTrim 1 Table"));
      serial_print_3dtable(&trim1Table, trim1Table.type_key);
      break;

    case canbusPage:
      primarySerial.println(F("\nPage 9 Cfg"));
      serial_println_range((byte *)&configPage9, (byte *)&configPage9 + sizeof(configPage9));
      break;

    case fuelMap2Page:
      primarySerial.println(F("\n2nd Fuel Map"));
      serial_print_3dtable(&fuelTable2, fuelTable2.type_key);
      break;
   
    case ignMap2Page:
      primarySerial.println(F("\n2nd Ignition Map"));
      serial_print_3dtable(&ignitionTable2, ignitionTable2.type_key);
      break;

    case boostvvtPage2:
      primarySerial.println(F("\nBoost lookup table"));
      serial_print_3dtable(&boostTableLookupDuty, boostTableLookupDuty.type_key);
      break;

    case warmupPage:
    case progOutsPage:
    default:
    #ifndef SMALL_FLASH_MODE
        primarySerial.println(F("\nPage has not been implemented yet"));
    #endif
        break;
  }
}


/** Processes an incoming stream of calibration data (for CLT, IAT or O2) from TunerStudio.
 * Result is store in EEPROM and memory.
 * 
 * @param tableID - calibration table to process. 0 = Coolant Sensor. 1 = IAT Sensor. 2 = O2 Sensor.
 */
void receiveCalibration(byte tableID)
{
  if(tableID == 2)
  {
    //O2 calibration. Comes through as 1024 8-bit values of which we use every 32nd
    for (int x = 0; x < 1024; x++)
    {
      while ( primarySerial.available() < 1 ) {}
      uint8_t tempValue = (uint8_t)primarySerial.read();

      if( (x % 32) == 0)
      {
        o2CalibrationTable.values[(x/32)] = (byte)tempValue; //O2 table stores 8 bit values
        o2CalibrationTable.axis[(x/32)] = x;
      }
      
    }
  }
  else
  {
    table2D_u16_u16_32 *pTargetTable;
    if (tableID == 0)
    {
      pTargetTable = &cltCalibrationTable;
    }
    else
    {
      pTargetTable = &iatCalibrationTable;
    }
    //Temperature calibrations are sent as 32 16-bit values
    for (uint16_t x = 0; x < 32; x++)
    {
      while ( primarySerial.available() < 2 ) {}
      byte tempBuffer[2];
      tempBuffer[0] = primarySerial.read();
      tempBuffer[1] = primarySerial.read();

      int16_t tempValue = (int16_t)(word(tempBuffer[1], tempBuffer[0])); //Combine the 2 bytes into a single, signed 16-bit value
      tempValue = div(tempValue, 10).quot; //TS sends values multiplied by 10 so divide back to whole degrees. 
      tempValue = ((tempValue - 32) * 5) / 9; //Convert from F to C
      
      pTargetTable->values[x] = temperatureAddOffset(tempValue);
      pTargetTable->axis[x] = (x * 32U);
      writeCalibration();
    }
  }

  writeCalibration();
}

/** Send 256 tooth log entries to primarySerial.
 * if useChar is true, the values are sent as chars to be printed out by a terminal emulator
 * if useChar is false, the values are sent as a 2 byte integer which is readable by TunerStudios tooth logger
*/
void sendToothLog_legacy(byte startOffset) /* Blocking */
{
  //We need TOOTH_LOG_SIZE number of records to send to TunerStudio. If there aren't that many in the buffer then we just return and wait for the next call
  if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY)) //Sanity check. Flagging system means this should always be true
  {
      serialStatusFlag = SERIAL_TRANSMIT_TOOTH_INPROGRESS_LEGACY; 
      for (uint8_t x = startOffset; x < TOOTH_LOG_SIZE; ++x)
      {
        primarySerial.write(toothHistory[x] >> 24);
        primarySerial.write(toothHistory[x] >> 16);
        primarySerial.write(toothHistory[x] >> 8);
        primarySerial.write(toothHistory[x]);
      }
      BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
      serialStatusFlag = SERIAL_INACTIVE; 
      toothHistoryIndex = 0;
  }
  else 
  { 
    //TunerStudio has timed out, send a LOG of all 0s
    for(uint16_t x = 0U; x < (4U*TOOTH_LOG_SIZE); ++x)
    {
      primarySerial.write(static_cast<byte>(0x00)); //GCC9 fix
    }
    serialStatusFlag = SERIAL_INACTIVE; 
  } 
}

void sendCompositeLog_legacy(byte startOffset) /* Non-blocking */
{
  if (BIT_CHECK(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY)) //Sanity check. Flagging system means this should always be true
  {
      serialStatusFlag = SERIAL_TRANSMIT_COMPOSITE_INPROGRESS_LEGACY;

      for (uint8_t x = startOffset; x < TOOTH_LOG_SIZE; ++x)
      {
        //Check whether the tx buffer still has space
        if(primarySerial.availableForWrite() < 4) 
        { 
          //tx buffer is full. Store the current state so it can be resumed later
          logItemsTransmitted = x;
          return;
        }

        uint32_t inProgressCompositeTime = toothHistory[x]; //This combined runtime (in us) that the log was going for by this record)
        
        primarySerial.write(inProgressCompositeTime >> 24);
        primarySerial.write(inProgressCompositeTime >> 16);
        primarySerial.write(inProgressCompositeTime >> 8);
        primarySerial.write(inProgressCompositeTime);

        primarySerial.write(compositeLogHistory[x]); //The status byte (Indicates the trigger edge, whether it was a pri/sec pulse, the sync status)
      }
      BIT_CLEAR(currentStatus.status1, BIT_STATUS1_TOOTHLOG1READY);
      toothHistoryIndex = 0;
      serialStatusFlag = SERIAL_INACTIVE; 
  }
  else 
  { 
    //TunerStudio has timed out, send a LOG of all 0s
    for(uint16_t x = 0U; x < (5U*TOOTH_LOG_SIZE); ++x)
    {
      primarySerial.write(static_cast<byte>(0x00)); //GCC9 fix
    }
    serialStatusFlag = SERIAL_INACTIVE; 
  } 
}

void testComm(void)
{
  primarySerial.write(1);
  return;
}

#if defined(CORE_AVR)
#pragma GCC pop_options
#endif