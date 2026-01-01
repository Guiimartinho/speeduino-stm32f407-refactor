/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
can_comms was originally contributed by Darren Siepka
*/

/*
secondserial_command is called when a command is received from the secondary serial port
It parses the command and calls the relevant function.

can_command is called when a command is received by the onboard/attached canbus module
It parses the command and calls the relevant function.

sendcancommand is called when a command is to be sent either to serial3
,to the external Can interface, or to the onboard/attached can interface
*/
#include "globals.h"
#include "comms.h"
#include "comms_secondary.h"
#include "comms_CAN.h"
#include "maths.h"
#include "utilities.h"
#include "comms_legacy.h"
#include "logger.h"
#include "page_crc.h"
#include BOARD_H

uint8_t currentSecondaryCommand;
SECONDARY_SERIAL_T* pSecondarySerial;

#if defined(CORE_AVR)
#pragma GCC push_options
// This minimizes RAM usage at no performance cost
#pragma GCC optimize ("Os")
#endif

/**
 * @brief Get high byte for 2-byte CAN input value
 */
static inline uint8_t getCANInputHighByte(const uint8_t* data, uint8_t channelIdx)
{
  bool isTwoByte = BIT_CHECK(configPage9.caninput_source_num_bytes, channelIdx) > 0U;
  if (!isTwoByte) { return 0U; }

  uint8_t startByte = configPage9.caninput_source_start_byte[channelIdx] & 7U;
  if (startByte >= 7U) { return 0U; }  // Can't have 2-byte starting at byte 7

  return data[startByte + 1U];
}

/**
 * @brief Process CAN interface 'G' reply command data
 */
static inline void processCANReplyData(uint8_t destChannel)
{
  uint8_t Gdata[8];
  for (uint8_t i = 0; i < 8; i++) { Gdata[i] = secondarySerial.read(); }

  uint8_t startByte = configPage9.caninput_source_start_byte[destChannel] & 7U;
  uint8_t Glow = Gdata[startByte];
  uint8_t Ghigh = getCANInputHighByte(Gdata, destChannel);

  currentStatus.canin[destChannel] = (Ghigh << 8) | Glow;
}

/**
 * @brief Handle CAN reply 'G' command
 */
static inline void handleCANReplyCommand(void)
{
  serialSecondaryStatusFlag = SERIAL_COMMAND_INPROGRESS_LEGACY;
  if (secondarySerial.available() < 9) { return; }

  serialSecondaryStatusFlag = SERIAL_INACTIVE;
  uint8_t cmdSuccessful = secondarySerial.read();
  uint8_t destChannel = secondarySerial.read();
  if (cmdSuccessful != 0U) { processCANReplyData(destChannel); }
}

void secondserial_Command(void)
{
  //If the selected protocol is Tuner Studio then everything is routed via the primary serial functions but with the output diverted to the secondary serial interface
  if(configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_TUNERSTUDIO)
  {
    pPrimarySerial = pSecondarySerial; //Divert the output of all primary serial functions to the secondary serial interface
    serialReceive();
    if(serialStatusFlag == SERIAL_INACTIVE) { pPrimarySerial = &Serial; } //Reset serial to primary to ensure other requests can be handled.
    return;
  }


  if ( serialSecondaryStatusFlag == SERIAL_INACTIVE )  { currentSecondaryCommand = secondarySerial.read(); }

  switch (currentSecondaryCommand)
  {
    case 'A':
      // sends a fixed 75 bytes of data. Used by Real Dash (Among others)
      if(configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_GENERIC_FIXED) { sendValues(0, CAN_PACKET_SIZE, 0x31, secondarySerial, serialSecondaryStatusFlag, &getLegacySecondarySerialLogEntry); } // Send values using the legacy fixed byte order
      else { sendValues(0, CAN_PACKET_SIZE, 0x31, secondarySerial, serialSecondaryStatusFlag); } //send values to serial3 using the order in the ini file
      break;

    case 'b': // New EEPROM burn command to only burn a single page at a time
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
      break;

    case 'B': // AS above but for the serial compatibility mode.
      BIT_SET(currentStatus.status4, BIT_STATUS4_COMMS_COMPAT); //Force the compat mode
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
      break;

    case 'd': // Send a CRC32 hash of a given page
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
      break;

    case 'G': // Reply command from CAN interface
      handleCANReplyCommand();
      break;

    case 'k':   //placeholder for new can interface (toucan etc) commands

        break;

    case 'M':
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
      break;

    case 'n': // sends the bytes of realtime values from the NEW CAN list
      //sendValues(0, NEW_CAN_PACKET_SIZE, 0x32, secondarySerial, serialSecondaryStatusFlag); //send values to serial3
      if(configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_GENERIC_FIXED) { sendValues(0, NEW_CAN_PACKET_SIZE, 0x32, secondarySerial, serialSecondaryStatusFlag, &getLegacySecondarySerialLogEntry); } // Send values using the legacy fixed byte order
      else { sendValues(0, NEW_CAN_PACKET_SIZE, 0x32, secondarySerial, serialSecondaryStatusFlag); } //send values to serial3 using the order in the ini file
      break;

    case 'p':
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
      break;

    case 'Q': // send code version
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
       break;

    case 'r': //New format for the optimised OutputChannels over CAN
      legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag);
      break;

    case 's': // send the "a" stream code version
      secondarySerial.print(F("Speeduino csx02019.8"));
      break;

    case 'S': // send code version
      if(configPage9.secondarySerialProtocol == SECONDARY_SERIAL_PROTO_MSDROID) { legacySerialHandler('Q', secondarySerial, serialSecondaryStatusFlag); } //Note 'Q', this is a workaround for msDroid
      else { legacySerialHandler(currentSecondaryCommand, secondarySerial, serialSecondaryStatusFlag); }

      break;

    case 'Z': //dev use
       break;

    default:
       break;
  }
}

// this routine sends a request(either "0" for a "G" , "1" for a "L" , "2" for a "R" to the Can interface or "3" sends the request via the actual local canbus
void sendCancommand(uint8_t cmdtype, uint16_t canaddress, uint8_t candata1, uint8_t candata2, uint16_t sourcecanAddress)
{
  switch (cmdtype)
  {
    case 0:
      secondarySerial.print("G");
      secondarySerial.write(canaddress);  //tscanid of speeduino device
      secondarySerial.write(candata1);    // table id
      secondarySerial.write(candata2);    //table memory offset
      break;

    case 1:                      //send request to listen for a can message
      secondarySerial.print("L");
      secondarySerial.write(canaddress);  //11 bit canaddress of device to listen for
      break;

    case 2:                                          // requests via serial3
      secondarySerial.print("R");                         //send "R" to request data from the sourcecanAddress whose value is sent next
      secondarySerial.write(candata1);                    //the currentStatus.current_caninchannel
      secondarySerial.write(lowByte(sourcecanAddress) );       //send lsb first
      secondarySerial.write(highByte(sourcecanAddress) );
      break;

    case 3:
      //send to truecan send routine
      //canaddress == speeduino canid, candata1 == canin channel dest, paramgroup == can address  to request from
      //This section is to be moved to the correct can output routine later
      #if defined(NATIVE_CAN_AVAILABLE)
      outMsg.id = (canaddress);
      outMsg.len = 8;
      outMsg.buf[0] = 0x0B ;  //11;
      outMsg.buf[1] = 0x15;
      outMsg.buf[2] = candata1;
      outMsg.buf[3] = 0x24;
      outMsg.buf[4] = 0x7F;
      outMsg.buf[5] = 0x70;
      outMsg.buf[6] = 0x9E;
      outMsg.buf[7] = 0x4D;
      CAN_write();
      #endif
      break;

    default:
      break;
  }
}

#if defined(CORE_AVR)
#pragma GCC pop_options
#endif
