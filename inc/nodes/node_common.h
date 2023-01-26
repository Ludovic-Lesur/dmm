/*
 * node_common.h
 *
 *  Created on: 23 jan. 2023
 *      Author: Ludo
 */

#ifndef __NODE_COMMON_H__
#define __NODE_COMMON_H__

#include "rs485.h"
#include "string.h"

/*** NODE common structures ***/

typedef enum {
	NODE_SUCCESS = 0,
	NODE_ERROR_NOT_SUPPORTED,
	NODE_ERROR_NULL_PARAMETER,
	NODE_ERROR_RS485_ADDRESS,
	NODE_ERROR_DATA_INDEX,
	NODE_ERROR_BOARD_ID,
	NODE_ERROR_BASE_LPUART = 0x0100,
	NODE_ERROR_BASE_STRING = (NODE_ERROR_BASE_LPUART + LPUART_ERROR_BASE_LAST),
	NODE_ERROR_BASE_RS485 = (NODE_ERROR_BASE_STRING + STRING_ERROR_BASE_LAST),
	NODE_ERROR_BASE_LAST = (NODE_ERROR_BASE_RS485 + RS485_ERROR_BASE_LAST)
} NODE_status_t;

#endif /* __NODE_COMMON_H__ */
