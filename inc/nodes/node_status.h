/*
 * node_status.h
 *
 *  Created on: 5 feb 2023
 *      Author: Ludo
 */

#ifndef __NODE_STATUS_H__
#define __NODE_STATUS_H__

#include "lptim.h"
#include "lpuart.h"
#include "string.h"

/*** NODE status definition ***/

typedef enum {
	NODE_SUCCESS = 0,
	NODE_ERROR_NOT_SUPPORTED,
	NODE_ERROR_NULL_PARAMETER,
	NODE_ERROR_PROTOCOL,
	NODE_ERROR_NODE_ADDRESS,
	NODE_ERROR_REGISTER_ADDRESS,
	NODE_ERROR_REGISTER_FORMAT,
	NODE_ERROR_STRING_DATA_INDEX,
	NODE_ERROR_READ_TYPE,
	NODE_ERROR_ACCESS,
	NODE_ERROR_SIGFOX_PAYLOAD_TYPE,
	NODE_ERROR_SIGFOX_PAYLOAD_EMPTY,
	NODE_ERROR_SIGFOX_LOOP,
	NODE_ERROR_SIGFOX_SEND,
	NODE_ERROR_NONE_RADIO_MODULE,
	NODE_ERROR_BASE_LPUART = 0x0100,
	NODE_ERROR_BASE_LPTIM = (NODE_ERROR_BASE_LPUART + LPUART_ERROR_BASE_LAST),
	NODE_ERROR_BASE_STRING = (NODE_ERROR_BASE_LPTIM + LPTIM_ERROR_BASE_LAST),
	NODE_ERROR_BASE_LAST = (NODE_ERROR_BASE_STRING + STRING_ERROR_BASE_LAST)
} NODE_status_t;

#endif /* __NODE_STATUS_H__ */
