/*
 * dinfox.h
 *
 *  Created on: 12 nov 2022
 *      Author: Ludo
 */

#ifndef __DINFOX_H__
#define __DINFOX_H__

#include "node_common.h"
#include "node_status.h"
#include "string.h"
#include "types.h"

/*** DINFOX boards identifier ***/

typedef enum {
	DINFOX_BOARD_ID_LVRM = 0,
	DINFOX_BOARD_ID_BPSM,
	DINFOX_BOARD_ID_DDRM,
	DINFOX_BOARD_ID_UHFM,
	DINFOX_BOARD_ID_GPSM,
	DINFOX_BOARD_ID_SM,
	DINFOX_BOARD_ID_DIM,
	DINFOX_BOARD_ID_RRM,
	DINFOX_BOARD_ID_DMM,
	DINFOX_BOARD_ID_MPMCM,
	DINFOX_BOARD_ID_R4S8CR,
	DINFOX_BOARD_ID_LAST,
	DINFOX_BOARD_ID_ERROR
} DINFOX_board_id_t;

/*** DINFOX boards RS485 address ranges ***/

#define DINFOX_RS485_ADDRESS_RANGE_LVRM		16
#define DINFOX_RS485_ADDRESS_RANGE_BPSM		4
#define DINFOX_RS485_ADDRESS_RANGE_DDRM		16
#define DINFOX_RS485_ADDRESS_RANGE_UHFM		4
#define DINFOX_RS485_ADDRESS_RANGE_GPSM		4
#define DINFOX_RS485_ADDRESS_RANGE_SM		4
#define DINFOX_RS485_ADDRESS_RANGE_RRM		4
#define DINFOX_RS485_ADDRESS_RANGE_MPMCM	4
#define DINFOX_RS485_ADDRESS_RANGE_R4S8CR	15

typedef enum {
	DINFOX_RS485_ADDRESS_DMM = 0x00,
	DINFOX_RS485_ADDRESS_DIM = 0x01,
	DINFOX_RS485_ADDRESS_BPSM_START = 0x08,
	DINFOX_RS485_ADDRESS_UHFM_START = (DINFOX_RS485_ADDRESS_BPSM_START + DINFOX_RS485_ADDRESS_RANGE_BPSM),
	DINFOX_RS485_ADDRESS_GPSM_START = (DINFOX_RS485_ADDRESS_UHFM_START + DINFOX_RS485_ADDRESS_RANGE_UHFM),
	DINFOX_RS485_ADDRESS_SM_START = (DINFOX_RS485_ADDRESS_GPSM_START + DINFOX_RS485_ADDRESS_RANGE_GPSM),
	DINFOX_RS485_ADDRESS_RRM_START = (DINFOX_RS485_ADDRESS_SM_START + DINFOX_RS485_ADDRESS_RANGE_SM),
	DINFOX_RS485_ADDRESS_MPMCM_START = (DINFOX_RS485_ADDRESS_RRM_START + DINFOX_RS485_ADDRESS_RANGE_RRM),
	// Note: 0x20-0x2F address range is forbidden due to R4S8CR read command.
	DINFOX_RS485_ADDRESS_LVRM_START = 0x30,
	DINFOX_RS485_ADDRESS_DDRM_START = (DINFOX_RS485_ADDRESS_LVRM_START + DINFOX_RS485_ADDRESS_RANGE_LVRM),
	DINFOX_RS485_ADDRESS_LBUS_LAST = 0x6F,
	DINFOX_RS485_ADDRESS_R4S8CR_START = (DINFOX_RS485_ADDRESS_LBUS_LAST + 1),
	DINFOX_RS485_ADDRESS_BROADCAST = (DINFOX_RS485_ADDRESS_R4S8CR_START + DINFOX_RS485_ADDRESS_RANGE_R4S8CR)
} DINFOX_rs485_address_range_t;

/*** DINFOX common registers set ***/

typedef enum {
	DINFOX_REGISTER_RS485_ADDRESS = 0,
	DINFOX_REGISTER_BOARD_ID,
	DINFOX_REGISTER_HW_VERSION_MAJOR,
	DINFOX_REGISTER_HW_VERSION_MINOR,
	DINFOX_REGISTER_SW_VERSION_MAJOR,
	DINFOX_REGISTER_SW_VERSION_MINOR,
	DINFOX_REGISTER_SW_VERSION_COMMIT_INDEX,
	DINFOX_REGISTER_SW_VERSION_COMMIT_ID,
	DINFOX_REGISTER_SW_VERSION_DIRTY_FLAG,
	DINFOX_REGISTER_RESET,
	DINFOX_REGISTER_ERROR_STACK,
	DINFOX_REGISTER_TMCU_DEGREES,
	DINFOX_REGISTER_VMCU_MV,
	DINFOX_REGISTER_LAST
} DINFOX_register_address_t;

/*** DINFOX common data index ***/

typedef enum {
	DINFOX_STRING_DATA_INDEX_HW_VERSION = 0,
	DINFOX_STRING_DATA_INDEX_SW_VERSION,
	DINFOX_STRING_DATA_INDEX_RESET_FLAG,
	DINFOX_STRING_DATA_INDEX_TMCU_DEGREES,
	DINFOX_STRING_DATA_INDEX_VMCU_MV,
	DINFOX_STRING_DATA_INDEX_LAST
} DINFOX_string_data_index_t;

static const STRING_format_t DINFOX_REGISTERS_FORMAT[DINFOX_REGISTER_LAST] = {
	STRING_FORMAT_HEXADECIMAL,
	STRING_FORMAT_HEXADECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_HEXADECIMAL,
	STRING_FORMAT_BOOLEAN,
	STRING_FORMAT_HEXADECIMAL,
	STRING_FORMAT_HEXADECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL
};

/*** DINFOX functions ***/

NODE_status_t DINFOX_update_data(NODE_address_t rs485_address, uint8_t string_data_index, NODE_single_data_ptr_t* single_data_ptr);

#endif /* __DINFOX_H__ */
