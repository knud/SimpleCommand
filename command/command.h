/*!
 * @file command.h
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

#ifndef _SIMPLE_COMMAND_H
#define _SIMPLE_COMMAND_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/*!
 * @brief Reader Commands
 * @ingroup simple
 *
 * @details A reader command is defined as a byte array comprising three
 * fields as described by @p command_id_t below.
 *
 * @note Below, 'B' stands for byte 'C' stands for a char.
 *
 * Reader Command:
 *   +--ID--+-Arg Len--+-Arg Data------------------------------------------+
 *   | 0xnn | [0,FFF]  | depends on command                                |
 *   +------+----------+---------------------------------------------------+
 *   | 1 B  | 3 C      | <= 4095 C                                         |
 *   +------+----------+---------------------------------------------------+
 * @field ID The command identifier; see external
 * @field Arg Len - the length fo the Arg Data field as a hex int
 *                  represented by 3 ASCII-encoded hex digits.
 * @field Arg Data This is command-dependent ASCII-encoded data.
 */

/*!
 * @brief The Reader Command IDs
 */
typedef enum
{
  NO_COMMAND               = 0xFE, // No Command
  FAST_BLINK               = 0x01, // Command 1
  SLOW_BLINK               = 0x02, // Command 2
  ALT_BLINK                = 0x03, // Command 2
  OFF                      = 0x04, // Command 2
  ABORT                    = 0xFF  // Abort current command
} command_id_t;

#define NO_COMMAND_STRING               "no_command"
#define FAST_BLINK_STRING               "fast_blink"
#define SLOW_BLINK_STRING               "slow_blink"
#define ALT_BLINK_STRING                "alt_blink"
#define OFF_STRING                      "off"
#define ABORT_STRING                    "abort"

typedef enum
{
  COMMAND_SUCCESS = 1,
  COMMAND_FAILURE = 0
} command_status_t;


/*!
 * @brief Initialize command handling.
 * @ingroup simple
 *
 * @details This function must be called at initialization.
 */
void commandInit();

/*!
 * @brief Receive and begin processing a raw command
 * @ingroup simple
 *
 * @details This is called when a new command is received. The raw command
 * is processed and if valid, processed.
 *
 * @param raw       - pointer to received raw command char array
 * @param rawLength - total number of chars received and in @p raw
 */
void receiveRawCommand(uint8_t const *raw, uint16_t rawLength);

/*!
 * @brief Check if most recently received command is valid
 *
 * @return true if valid, false otherwise
 */
bool validCommandReceived();

/*!
 * @brief Execute the most recently received command.
 */
void executeCommand();

/*!
 * @brief Initiate a BLE event (notify) to respond to a command with a message.
 *
 * @details This could be data from a sensor or just an acknowledgement.
 * Whatever makes sense in your applicaiton...
 *
 * @param message - the command response message
 */
void bleEventInitiate(char *message);

/*!
 * @brief Return the current command ID
 *
 * @return the current command ID
 */
command_id_t currentCommand();

/*!
 * @brief Set the current command ID
 *
 * @param commandID The command ID
 */
void setCurrentCommand(command_id_t commandID);

#endif // _SIMPLE_COMMAND_H
