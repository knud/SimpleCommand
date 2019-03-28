/*!
 * @file commandInternal.h
 * @author Steven Knudsen
 * @date 2019-03-22
 * @brief Simple BLE command processing, not public
 *
 * This file is part of the Simple BLE Commander example.
 *
 * Copyright (C) 2019 by Steven Knudsen
 *
 * This software may be modified and distributed under the terms of the
 * MIT license. See the LICENSE file for details.
 */

#ifndef _COMMAND_INTERNAL_H
#define _COMMAND_INTERNAL_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "command.h"

#define COMMAND_ID_FIELD_LENGTH              1
#define COMMAND_ARG_LENGTH_FIELD_LENGTH      3
#define COMMAND_ARG_DATA_FIELD_MAX_LENGTH 4095

// TODO make the max arg length less?
typedef struct
{
  command_id_t commandID; // The command ID
  uint16_t     argLength; // The number of arg bytes [0,4095]
  uint8_t      argData[COMMAND_ARG_DATA_FIELD_MAX_LENGTH];
} command_packet_t;

typedef enum
{
  READY_FOR_COMMAND  = 0x00,
  DECODING_COMMAND   = 0x01,
  PROCESSING_COMMAND = 0x02,
  ACCEPT_ARG_DATA    = 0x03,
  INVALID            = 0xFF
} command_state_t;

/*!
 * @brief A struct to encapsulate the Reader Command.
 * @ingroup simple
 *
 * @field initialized        - true if the command has been initiallized, false otherwise
 * @field rawCommandReceived - true if a new command has been received, false otherwise
 * @field commandValid       - true if the command is valid
 * @field command            - the interpreted raw command
 * @field commandState       - the command processing state
 */
typedef struct
{
  bool initialized;
  bool rawCommandReceived;
  bool commandValid;
  command_packet_t command;
  command_state_t commandState;
} command_t;


/*!
 * @brief Provide more argument data for current command.
 * @ingroup simple
 *
 * @details Argument data received is added to that already received in the
 * original command or previous More Argument Data commands. When the total
 * specified in the original command has been received, the original command is
 * executed.
 *
 * @param command (format below)
 *   +--ID--+-Arg Len-+-Arg Data-------------------------------------------+
 *   | 0x00 | 000     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 *   | 1 B  | 3 C     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 * @return SUCCESS if successful, FAILURE otherwise.
 */
int noCommand();

/*!
 * @brief Blink and LED quickly
 * @ingroup simple
 *
 * @details
 *
 * @param command (format below)
 *   +--ID--+-Arg Len-+-Arg Data-------------------------------------------+
 *   | 0x01 | 003     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 *   | 1 B  | 3 C     | 59 C                                               |
 *   +------+---------+----------------------------------------------------+
 * @return SUCCESS if successful, FAILURE otherwise.
 */
int fastBlink();

/*!
 * @brief Blink and LED slowly
 * @ingroup simple
 *
 * @details
 *
 * @param command (format below)
 *   +--ID--+-Arg Len-+-Arg Data-------------------------------------------+
 *   | 0x02 | 003     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 *   | 1 B  | 3 C     | 59 C                                               |
 *   +------+---------+----------------------------------------------------+
 * @return SUCCESS if successful, FAILURE otherwise.
 */
int slowBlink();

/*!
 * @brief Alternately blink LEDs
 * @ingroup simple
 *
 * @details
 *
 * @param command (format below)
 *   +--ID--+-Arg Len-+-Arg Data-------------------------------------------+
 *   | 0x03 | 003     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 *   | 1 B  | 3 C     | 59 C                                               |
 *   +------+---------+----------------------------------------------------+
 * @return SUCCESS if successful, FAILURE otherwise.
 */
int altBlink();

/*!
 * @brief Turn off LEDs
 * @ingroup simple
 *
 * @details
 *
 * @param command (format below)
 *   +--ID--+-Arg Len-+-Arg Data-------------------------------------------+
 *   | 0x04 | 003     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 *   | 1 B  | 3 C     | 59 C                                               |
 *   +------+---------+----------------------------------------------------+
 * @return SUCCESS if successful, FAILURE otherwise.
 */
int off();


/*!
 * @brief
 * @ingroup simple
 *
 * @details
 *
 * @param command (format below)
 *   +--ID--+-Arg Len-+-Arg Data-------------------------------------------+
 *   | 0xFF | 000     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 *   | 1 B  | 3 C     | don't care                                         |
 *   +------+---------+----------------------------------------------------+
 * @return SUCCESS if successful, FAILURE otherwise.
 */
int abortCommand();

// Internal support

bool isASCIIHexDigit(char c);

bool isValidCommandID();

#endif // _COMMAND_INTERNAL_H
