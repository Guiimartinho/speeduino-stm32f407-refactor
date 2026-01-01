/**
 * @file communication_handler.cpp
 * @brief Communication handling implementation
 *
 * SCG-ECU 2.0 - STM32F407VGT6
 */

#include "communication_handler.h"
#include "comms.h"
#include "comms_legacy.h"
#include "comms_secondary.h"
#include "comms_CAN.h"
#include "modularization_globals.h"
#include "led_button_system.h"

void handleSerialComms(void)
{
  // Check if there's an ongoing serial transmission
  if(serialTransmitInProgress())
  {
    serialTransmit();
  }

  // Check for any new or in-progress requests from serial
  if((Serial.available() > 0) || serialRecieveInProgress())
  {
    serialReceive();
#if defined(BOARD_SCG_ECU_20)
    // Flash activity LED on serial communication
    ledFlashActivity();
#endif
  }
}

void handleSecondarySerial(void)
{
  // Check for secondary comms requiring action
  if(configPage9.enable_secondarySerial == 1) // Secondary serial interface enabled
  {
    if(secondarySerial.available() > 0) {
      secondserial_Command();
    }
  }
}

void handleCANComms(void)
{
  #if defined(NATIVE_CAN_AVAILABLE)
    if(configPage9.enable_intcan != 1) { return; } // Use internal CAN module
    // Check local CAN module
    while(CAN_read())
    {
      can_Command();
      readAuxCanBus();
      if(configPage2.canWBO > 0) { receiveCANwbo(); }
    }
  #endif
}
