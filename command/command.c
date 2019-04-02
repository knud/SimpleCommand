/*!
 * @file command.c
 * @author Steven Knudsen
 * @date 2019-03-22
 * @brief Simple BLE command processing
 *
 * This file is part of the Simple BLE Commander example.
 *
 * Copyright (C) 2019 by Steven Knudsen
 *
 * This software may be modified and distributed under the terms of the
 * MIT license. See the LICENSE file for details.
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "app_timer.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "mem_manager.h"
#include "nrf_sdm.h"

#include "ble_cmd.h"

#include "command.h"
#include "commandInternal.h"

// declare and initialize a reader command instance
command_t m_command;

#define SIMPLE_COMMAND_DEBUG 1

#define BLE_MTU 20

void
bleEventInitiate(char *message)
{
  // Determine how much data and send notification
  uint16_t msgLength = strlen(message);
  uint16_t len;

  char dataAvailable[64];
  sprintf(dataAvailable, "dataAvailable:%04d", msgLength);
  uint16_t dataAvailableLength = strlen(dataAvailable);
  uint32_t sendError = ble_cmd_data_send(dataAvailable, &dataAvailableLength);
  if (sendError != 0)
    NRF_LOG_INFO("sendError =%d",sendError);

  uint16_t bytesToSend = msgLength;
  char * msgPtr = message;
  while (bytesToSend > 0) {
    if (bytesToSend >= BLE_MTU) {
      len = BLE_MTU;
      sendError = ble_cmd_data_send(msgPtr, &len);
      if (sendError != 0)
        NRF_LOG_INFO("sendError =%d",sendError);
      bytesToSend -= BLE_MTU;
      msgPtr += BLE_MTU;
    } else {
      len = bytesToSend;
      sendError = ble_cmd_data_send(msgPtr,  &len);
      if (sendError != 0)
        NRF_LOG_INFO("sendError =%d",sendError);
      bytesToSend = 0;
    }
    nrf_delay_ms(10);
  }
}

void
commandInit()
{
  m_command.initialized = true;
  m_command.commandState = READY_FOR_COMMAND;
  m_command.rawCommandReceived = false;
  m_command.commandValid = false;
}

void
receiveRawCommand(uint8_t const *raw, uint16_t rawLength)
{
  // TODO just drop the command? Maybe, eventually notify the app the command failed
  if (m_command.commandState != READY_FOR_COMMAND)
    return;

  // TODO the state used to be useful when there were chunked commands, but perhaps no longer...
  m_command.commandState = DECODING_COMMAND;
  m_command.commandValid = false,


  m_command.command.commandID = raw[0];

  if (!isValidCommandID())
  {
    m_command.commandState = READY_FOR_COMMAND;
    m_command.rawCommandReceived = false;
    NRF_LOG_INFO("Invalid command ID");
    return;
  }
  // Command length is base-16 and passed to us as an ASCII-encoded 3-digit int
  //
  // Bounds check
  if (!(isASCIIHexDigit(raw[1]) &&
      isASCIIHexDigit(raw[2]) &&
      isASCIIHexDigit(raw[3])))
  {
    m_command.commandState = READY_FOR_COMMAND;
    m_command.rawCommandReceived = false;
    return;
  }

  char hex[4];
  hex[0] = raw[1];
  hex[1] = raw[2];
  hex[2] = raw[3];
  hex[3] = '\0';
  uint16_t len = (uint16_t)strtol(hex, NULL, 16);
  m_command.command.argLength = len;
  strncpy((char *) (m_command.command.argData), (char *)(raw+4), len);

  // TODO could check the length against that expected for a given command ID

#if SIMPLE_COMMAND_DEBUG
  char argString[4096];
  strncpy(argString, ((char *)raw)+4, len);
  argString[len] = '\0';
  NRF_LOG_INFO("Received command:");
  NRF_LOG_INFO("  command ID  = 0x%02x",m_command.command.commandID);
  NRF_LOG_INFO("  arg length  = %d",m_command.command.argLength);
  NRF_LOG_INFO("  argData = %s",argString);
#endif

  m_command.commandValid = true;
  m_command.commandState = READY_FOR_COMMAND;
  m_command.rawCommandReceived = true;
}

bool
validCommandReceived()
{
  return m_command.rawCommandReceived == true &&
      m_command.commandValid == true;
}

void
executeCommand()
{
  m_command.rawCommandReceived = false;

  switch(m_command.command.commandID)
  {
    case NO_COMMAND:
      noCommand();
    break;
    case FAST_BLINK:
      fastBlink();
    break;
    case SLOW_BLINK:
      slowBlink();
    break;
    case ALT_BLINK:
      altBlink();
    break;
    case OFF:
      off();
    break;
    case ABORT:
      abortCommand();
    break;
    default:
    break;
  }

  NRF_LOG_INFO("readerCommandExecute done");
  m_command.commandState = READY_FOR_COMMAND;
  m_command.commandValid = false;
}

command_id_t
currentCommand()
{
  return m_command.command.commandID;
}

void
setCurrentCommand(command_id_t commandID)
{
  m_command.command.commandID = commandID;
}

int
noCommand()
{
  // Check the argLength
  if (m_command.command.argLength != 0)
    return COMMAND_FAILURE;

  // This could be anything, like data from a sensor...
  bleEventInitiate("No Command received");

  return COMMAND_SUCCESS;
}


int
fastBlink()
{
  // Check the argLength
  if (m_command.command.argLength != 0)
    return COMMAND_FAILURE;

  // This could be anything, like data from a sensor...
  bleEventInitiate("LED blinking quickly");

  return COMMAND_SUCCESS;
}

int
slowBlink()
{
  // Check the argLength
  if (m_command.command.argLength != 0)
    return COMMAND_FAILURE;

  // This could be anything, like data from a sensor...
  bleEventInitiate("LED blinking slowly");

  return COMMAND_SUCCESS;
}

int
altBlink()
{
  // Check the argLength
  if (m_command.command.argLength != 0)
    return COMMAND_FAILURE;

  // This could be anything, like data from a sensor...
  bleEventInitiate("Alternating LEDs");

  return COMMAND_SUCCESS;
}

int
off()
{
  // Check the argLength
  if (m_command.command.argLength != 0)
    return COMMAND_FAILURE;

  // This could be anything, like data from a sensor...
  bleEventInitiate("LEDs are off");

  return COMMAND_SUCCESS;
}


int
abortCommand()
{
  // Check the argLength
  if (m_command.command.argLength != 0)
    return COMMAND_FAILURE;

  // This could be anything, like data from a sensor...
  bleEventInitiate("Aborting (just pretending...)");

  // TODO for now
  return COMMAND_SUCCESS;
}

bool isASCIIHexDigit(char c)
{
  bool okay;
  okay = c >= 0x30 && c <= 0x39;  // '0' to '9'
  okay |= c >= 0x41 && c <= 0x46; // 'A' to 'F'
  okay |= c >= 0x61 && c <= 0x66; // 'A' to 'F'
  return okay;
}

bool
isValidCommandID()
{
  bool valid =
      m_command.command.commandID == NO_COMMAND ||
      m_command.command.commandID == FAST_BLINK ||
      m_command.command.commandID == SLOW_BLINK ||
      m_command.command.commandID == ALT_BLINK ||
      m_command.command.commandID == OFF ||
      m_command.command.commandID == ABORT;
  return valid;
}
