/*
 * node.c
 *
 *  Created on: 23 jan. 2023
 *      Author: Ludo
 */

#include "node.h"

#include "at_bus.h"
#include "bpsm.h"
#include "bpsm_reg.h"
#include "common.h"
#include "ddrm.h"
#include "ddrm_reg.h"
#include "dinfox.h"
#include "dmm.h"
#include "dmm_reg.h"
#include "error.h"
#include "gpsm.h"
#include "gpsm_reg.h"
#include "lpuart.h"
#include "lvrm.h"
#include "lvrm_reg.h"
#include "mode.h"
#include "node_common.h"
#include "r4s8cr.h"
#include "r4s8cr_reg.h"
#include "rtc.h"
#include "sigfox_types.h"
#include "sm.h"
#include "sm_reg.h"
#include "uhfm.h"
#include "uhfm_reg.h"
#include "xm.h"

/*** NODE local macros ***/

#define NODE_LINE_DATA_INDEX_MAX				32
#define NODE_REGISTER_ADDRESS_MAX				64

#define NODE_SIGFOX_PAYLOAD_SIZE_MAX			12
#define NODE_SIGFOX_PAYLOAD_HEADER_SIZE			2

#define NODE_ACTIONS_DEPTH						10

/*** NODE local structures ***/

/*******************************************************************/
typedef enum {
	NODE_DOWNLINK_OP_CODE_NOP = 0,
	NODE_DOWNLINK_OP_CODE_SINGLE_FULL_WRITE,
	NODE_DOWNLINK_OP_CODE_SINGLE_MASKED_WRITE,
	NODE_DOWNLINK_OP_CODE_TEMPORARY_FULL_WRITE,
	NODE_DOWNLINK_OP_CODE_TEMPORARY_MASKED_WRITE,
	NODE_DOWNLINK_OP_CODE_SUCCESSIVE_FULL_WRITE,
	NODE_DOWNLINK_OP_CODE_SUCCESSIVE_MASKED_WRITE,
	NODE_DOWNLINK_OP_CODE_DUAL_FULL_WRITE,
	NODE_DOWNLINK_OP_CODE_TRIPLE_FULL_WRITE,
	NODE_DOWNLINK_OP_CODE_DUAL_NODE_WRITE,
	NODE_DOWNLINK_OP_CODE_LAST
} NODE_downlink_operation_code_t;

/*******************************************************************/
typedef NODE_status_t (*NODE_write_register_t)(NODE_access_parameters_t* write_params, uint32_t reg_value, uint32_t reg_mask, NODE_access_status_t* write_status);
typedef NODE_status_t (*NODE_read_register_t)(NODE_access_parameters_t* read_params, uint32_t* reg_value, NODE_access_status_t* read_status);
typedef NODE_status_t (*NODE_write_line_data_t)(NODE_line_data_write_t* line_write, NODE_access_status_t* write_status);
typedef NODE_status_t (*NODE_read_line_data_t)(NODE_line_data_read_t* line_read, NODE_access_status_t* read_status);
typedef NODE_status_t (*NODE_build_sigfox_ul_payload_t)(NODE_ul_payload_t* node_ul_payload);

/*******************************************************************/
typedef struct {
	NODE_write_register_t write_register;
	NODE_read_register_t read_register;
	NODE_write_line_data_t write_line_data;
	NODE_read_line_data_t read_line_data;
	NODE_build_sigfox_ul_payload_t build_sigfox_ul_payload;
} NODE_functions_t;

/*******************************************************************/
typedef struct {
	char_t* name;
	NODE_protocol_t protocol;
	uint8_t last_reg_addr;
	uint8_t last_line_data_index;
	uint32_t* register_write_timeout_ms;
	NODE_functions_t functions;
} NODE_descriptor_t;

/*******************************************************************/
typedef union {
	uint8_t frame[SIGFOX_UL_PAYLOAD_MAX_SIZE_BYTES];
	struct {
		unsigned node_addr : 8;
		unsigned board_id : 8;
		uint8_t node_data[NODE_SIGFOX_PAYLOAD_SIZE_MAX - NODE_SIGFOX_PAYLOAD_HEADER_SIZE];
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} NODE_sigfox_ul_payload_t;

/*******************************************************************/
typedef union {
	uint8_t frame[SIGFOX_DL_PAYLOAD_SIZE_BYTES];
	struct {
		unsigned op_code : 8;
		union {
			struct {
				unsigned node_addr : 8;
				unsigned reg_addr : 8;
				unsigned reg_value : 32;
				unsigned duration : 8; // Unused in single.
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) full_write;
			struct {
				unsigned node_addr : 8;
				unsigned reg_addr : 8;
				unsigned reg_mask : 16;
				unsigned reg_value : 16;
				unsigned duration : 8; // Unused in single.
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) masked_write;
			struct {
				unsigned node_addr : 8;
				unsigned reg_addr : 8;
				unsigned reg_value_1 : 16;
				unsigned reg_value_2 : 16;
				unsigned duration : 8;
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) successive_full_write;
			struct {
				unsigned node_addr : 8;
				unsigned reg_addr : 8;
				unsigned reg_mask : 8;
				unsigned reg_value_1 : 8;
				unsigned reg_value_2 : 8;
				unsigned duration : 8;
				unsigned unused : 8;
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) successive_masked_write;
			struct {
				unsigned node_addr : 8;
				unsigned reg_1_addr : 8;
				unsigned reg_1_value : 16;
				unsigned reg_2_addr : 8;
				unsigned reg_2_value : 16;
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) dual_full_write;
			struct {
				unsigned node_addr : 8;
				unsigned reg_1_addr : 8;
				unsigned reg_1_value : 8;
				unsigned reg_2_addr : 8;
				unsigned reg_2_value : 8;
				unsigned reg_3_addr : 8;
				unsigned reg_3_value : 8;
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) triple_full_write;
			struct {
				unsigned node_1_addr : 8;
				unsigned reg_1_addr : 8;
				unsigned reg_1_value : 8;
				unsigned node_2_addr : 8;
				unsigned reg_2_addr : 8;
				unsigned reg_2_value : 8;
				unsigned unused : 8;
			} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed)) dual_node_write;
		};
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} NODE_sigfox_dl_payload_t;

/*******************************************************************/
typedef struct {
	NODE_t* node;
	uint8_t reg_addr;
	uint32_t reg_value;
	uint32_t reg_mask;
	uint32_t timestamp_seconds;
} NODE_action_t;

/*******************************************************************/
typedef struct {
	char_t line_data_name[NODE_LINE_DATA_INDEX_MAX][NODE_STRING_BUFFER_SIZE];
	char_t line_data_value[NODE_LINE_DATA_INDEX_MAX][NODE_STRING_BUFFER_SIZE];
} NODE_data_t;

/*******************************************************************/
typedef struct {
	NODE_data_t data;
	NODE_address_t uhfm_address;
	// Uplink.
	NODE_sigfox_ul_payload_t sigfox_ul_payload;
	uint8_t sigfox_ul_payload_size;
	uint32_t sigfox_ul_next_time_seconds;
	uint8_t sigfox_ul_node_list_index;
	// Downlink.
	NODE_sigfox_dl_payload_t sigfox_dl_payload;
	uint32_t sigfox_dl_next_time_seconds;
	// Write actions list.
	NODE_action_t actions[NODE_ACTIONS_DEPTH];
	uint8_t actions_index;
#ifdef BMS
	NODE_t* bms_node_ptr;
	uint32_t bms_monitoring_next_time_seconds;
#endif
} NODE_context_t;

/*** NODE local global variables ***/

// Note: table is indexed with board ID.
static const NODE_descriptor_t NODES[DINFOX_BOARD_ID_LAST] = {
	{"LVRM", NODE_PROTOCOL_AT_BUS, LVRM_REG_ADDR_LAST, LVRM_LINE_DATA_INDEX_LAST, (uint32_t*) LVRM_REG_WRITE_TIMEOUT_MS,
		{&AT_BUS_write_register, &AT_BUS_read_register, &LVRM_write_line_data, &LVRM_read_line_data, &LVRM_build_sigfox_ul_payload}
	},
	{"BPSM", NODE_PROTOCOL_AT_BUS, BPSM_REG_ADDR_LAST, BPSM_LINE_DATA_INDEX_LAST, (uint32_t*) BPSM_REG_WRITE_TIMEOUT_MS,
		{&AT_BUS_write_register, &AT_BUS_read_register, &BPSM_write_line_data, &BPSM_read_line_data, &BPSM_build_sigfox_ul_payload}
	},
	{"DDRM", NODE_PROTOCOL_AT_BUS, DDRM_REG_ADDR_LAST, DDRM_LINE_DATA_INDEX_LAST, (uint32_t*) DDRM_REG_WRITE_TIMEOUT_MS,
		{&AT_BUS_write_register, &AT_BUS_read_register, &DDRM_write_line_data, &DDRM_read_line_data, &DDRM_build_sigfox_ul_payload}
	},
	{"UHFM", NODE_PROTOCOL_AT_BUS, UHFM_REG_ADDR_LAST, UHFM_LINE_DATA_INDEX_LAST, (uint32_t*) UHFM_REG_WRITE_TIMEOUT_MS,
		{&AT_BUS_write_register, &AT_BUS_read_register, &UHFM_write_line_data, &UHFM_read_line_data, &UHFM_build_sigfox_ul_payload}
	},
	{"GPSM", NODE_PROTOCOL_AT_BUS, GPSM_REG_ADDR_LAST, GPSM_LINE_DATA_INDEX_LAST, (uint32_t*) GPSM_REG_WRITE_TIMEOUT_MS,
		{&AT_BUS_write_register, &AT_BUS_read_register, &GPSM_write_line_data, &GPSM_read_line_data, &GPSM_build_sigfox_ul_payload}
	},
	{"SM", NODE_PROTOCOL_AT_BUS, SM_REG_ADDR_LAST, SM_LINE_DATA_INDEX_LAST, (uint32_t*) SM_REG_WRITE_TIMEOUT_MS,
		{&AT_BUS_write_register, &AT_BUS_read_register, &SM_write_line_data, &SM_read_line_data, &SM_build_sigfox_ul_payload}
	},
	{"DIM", NODE_PROTOCOL_AT_BUS, 0, 0, NULL,
		{NULL, NULL, NULL, NULL}
	},
	{"RRM", NODE_PROTOCOL_AT_BUS, 0, 0, NULL,
		{NULL, NULL, NULL, NULL}
	},
	{"DMM", NODE_PROTOCOL_AT_BUS, DMM_REG_ADDR_LAST, DMM_LINE_DATA_INDEX_LAST, (uint32_t*) DMM_REG_WRITE_TIMEOUT_MS,
		{&DMM_write_register, &DMM_read_register, &DMM_write_line_data, &DMM_read_line_data, &DMM_build_sigfox_ul_payload}
	},
	{"MPMCM", NODE_PROTOCOL_AT_BUS, 0, 0, NULL,
		{NULL, NULL, NULL, NULL}
	},
	{"R4S8CR", NODE_PROTOCOL_R4S8CR, R4S8CR_REG_ADDR_LAST, R4S8CR_LINE_DATA_INDEX_LAST, (uint32_t*) R4S8CR_REG_WRITE_TIMEOUT_MS,
		{&R4S8CR_write_register, &R4S8CR_read_register, &R4S8CR_write_line_data, &R4S8CR_read_line_data, &R4S8CR_build_sigfox_ul_payload}
	},
};
static NODE_context_t node_ctx;

/*** NODE local functions ***/

/*******************************************************************/
#define NODE_check_access_status(void) { \
	if ((node_access_status.all) != 0) { \
		status = (NODE_ERROR_BASE_ACCESS_STATUS + (node_access_status.all)); \
		goto errors; \
	} \
}

/*******************************************************************/
#define _NODE_check_node_and_board_id(void) { \
	if (node == NULL) { \
		status = NODE_ERROR_NULL_PARAMETER; \
		goto errors; \
	} \
	if ((node -> board_id) >= DINFOX_BOARD_ID_LAST) { \
		status = NODE_ERROR_NOT_SUPPORTED; \
		goto errors; \
	} \
}

/*******************************************************************/
#define _NODE_check_function_pointer(function_name) { \
	if ((NODES[node -> board_id].functions.function_name) == NULL) { \
		status = NODE_ERROR_NOT_SUPPORTED; \
		goto errors; \
	} \
}

/*******************************************************************/
static void _NODE_flush_line_data_value(uint8_t line_data_index) {
	// Local variables.
	uint8_t idx = 0;
	// Char loop.
	for (idx=0 ; idx<NODE_STRING_BUFFER_SIZE ; idx++) {
		node_ctx.data.line_data_name[line_data_index][idx] = STRING_CHAR_NULL;
		node_ctx.data.line_data_value[line_data_index][idx] = STRING_CHAR_NULL;
	}
}

/*******************************************************************/
void _NODE_flush_all_data_value(void) {
	// Local variables.
	uint8_t idx = 0;
	// Reset string data.
	for (idx=0 ; idx<NODE_LINE_DATA_INDEX_MAX ; idx++) _NODE_flush_line_data_value(idx);
}

/*******************************************************************/
void _NODE_flush_list(void) {
	// Local variables.
	uint8_t idx = 0;
	// Reset node list.
	for (idx=0 ; idx<NODES_LIST_SIZE_MAX ; idx++) {
		NODES_LIST.list[idx].address = 0xFF;
		NODES_LIST.list[idx].board_id = DINFOX_BOARD_ID_ERROR;
		NODES_LIST.list[idx].startup_data_sent = 0;
		NODES_LIST.list[idx].radio_transmission_count = 0;
	}
	NODES_LIST.count = 0;
}

/*******************************************************************/
NODE_status_t _NODE_write_register(NODE_t* node, uint8_t reg_addr, uint32_t reg_value, uint32_t reg_mask) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_parameters_t write_input;
	NODE_access_status_t node_access_status = {.all = 0};
	// Check node and board ID.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(write_register);
	// Check register address.
	if (NODES[node -> board_id].last_reg_addr == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	if (reg_addr >= (NODES[node -> board_id].last_reg_addr)) {
		status = NODE_ERROR_REGISTER_ADDRESS;
		goto errors;
	}
	// Common write parameters.
	write_input.node_addr = (node -> address);
	write_input.reg_addr = reg_addr;
	// Check node protocol.
	switch (NODES[node -> board_id].protocol) {
	case NODE_PROTOCOL_AT_BUS:
		// Specific write parameters.
		write_input.reply_params.timeout_ms = (reg_addr < COMMON_REG_ADDR_LAST) ? COMMON_REG_WRITE_TIMEOUT_MS[reg_addr] : NODES[node -> board_id].register_write_timeout_ms[reg_addr - COMMON_REG_ADDR_LAST];
		write_input.reply_params.type = NODE_REPLY_TYPE_OK;
		break;
	case NODE_PROTOCOL_R4S8CR:
		// Specific write parameters.
		write_input.reply_params.timeout_ms = NODES[node -> board_id].register_write_timeout_ms[reg_addr];
		write_input.reply_params.type = NODE_REPLY_TYPE_VALUE;
		break;
	default:
		status = NODE_ERROR_PROTOCOL;
		break;
	}
	status = NODES[node -> board_id].functions.write_register(&write_input, reg_value, reg_mask, &node_access_status);
	if (status != NODE_SUCCESS) goto errors;
	NODE_check_access_status();
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_read_register(NODE_t* node, uint8_t reg_addr, uint32_t* reg_value) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_parameters_t read_input;
	NODE_access_status_t node_access_status = {.all = 0};
	// Check node and board ID.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(write_register);
	// Check register address.
	if (NODES[node -> board_id].last_reg_addr == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	if (reg_addr >= (NODES[node -> board_id].last_reg_addr)) {
		status = NODE_ERROR_REGISTER_ADDRESS;
		goto errors;
	}
	// Common write parameters.
	read_input.node_addr = (node -> address);
	read_input.reg_addr = reg_addr;
	// Check node protocol.
	switch (NODES[node -> board_id].protocol) {
	case NODE_PROTOCOL_AT_BUS:
		// Specific write parameters.
		read_input.reply_params.timeout_ms = (reg_addr < COMMON_REG_ADDR_LAST) ? COMMON_REG_WRITE_TIMEOUT_MS[reg_addr] : NODES[node -> board_id].register_write_timeout_ms[reg_addr - COMMON_REG_ADDR_LAST];
		read_input.reply_params.type = NODE_REPLY_TYPE_VALUE;
		break;
	case NODE_PROTOCOL_R4S8CR:
		// Specific write parameters.
		read_input.reply_params.timeout_ms = NODES[node -> board_id].register_write_timeout_ms[reg_addr];
		read_input.reply_params.type = NODE_REPLY_TYPE_VALUE;
		break;
	default:
		status = NODE_ERROR_PROTOCOL;
		break;
	}
	status = NODES[node -> board_id].functions.read_register(&read_input, reg_value, &node_access_status);
	if (status != NODE_SUCCESS) goto errors;
	NODE_check_access_status();
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_radio_send(NODE_t* node, uint8_t bidirectional_flag) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint8_t node_data_size = 0;
	NODE_ul_payload_t node_ul_payload;
	UHFM_sigfox_message_t sigfox_message;
	NODE_access_status_t node_access_status = {.all = 0};
	uint8_t idx = 0;
	// Check board ID.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(build_sigfox_ul_payload);
	// Reset payload.
	for (idx=0 ; idx<NODE_SIGFOX_PAYLOAD_SIZE_MAX ; idx++) node_ctx.sigfox_ul_payload.frame[idx] = 0x00;
	node_ctx.sigfox_ul_payload_size = 0;
	// Build update structure.
	node_ul_payload.node = node;
	node_ul_payload.ul_payload = node_ctx.sigfox_ul_payload.node_data;
	node_ul_payload.size = &node_data_size;
	// Add board ID and node address.
	node_ctx.sigfox_ul_payload.board_id = (node -> board_id);
	node_ctx.sigfox_ul_payload.node_addr = (node -> address);
	node_ctx.sigfox_ul_payload_size = 2;
	// Execute function of the corresponding board ID.
	status = NODES[node -> board_id].functions.build_sigfox_ul_payload(&node_ul_payload);
	if (status != NODE_SUCCESS) goto errors;
	// Update frame size.
	node_ctx.sigfox_ul_payload_size += node_data_size;
	// Check UHFM board availability.
	if (node_ctx.uhfm_address == DINFOX_NODE_ADDRESS_BROADCAST) {
		status = NODE_ERROR_NONE_RADIO_MODULE;
		goto errors;
	}
	// Build Sigfox message structure.
	sigfox_message.ul_payload = (uint8_t*) node_ctx.sigfox_ul_payload.frame;
	sigfox_message.ul_payload_size = node_ctx.sigfox_ul_payload_size;
	sigfox_message.bidirectional_flag = bidirectional_flag;
	// Send message.
	status = UHFM_send_sigfox_message(node_ctx.uhfm_address, &sigfox_message, &node_access_status);
	if (status != NODE_SUCCESS) goto errors;
	NODE_check_access_status();
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_radio_read(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t node_access_status = {.all = 0};
	// Reset operation code.
	node_ctx.sigfox_dl_payload.op_code = NODE_DOWNLINK_OP_CODE_NOP;
	// Read downlink payload.
	status = UHFM_get_dl_payload(node_ctx.uhfm_address, node_ctx.sigfox_dl_payload.frame, &node_access_status);
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_remove_action(uint8_t action_index) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameter.
	if (action_index >= NODE_ACTIONS_DEPTH) {
		status = NODE_ERROR_ACTION_INDEX;
		goto errors;
	}
	node_ctx.actions[action_index].node = NULL;
	node_ctx.actions[action_index].reg_addr = 0x00;
	node_ctx.actions[action_index].reg_value = 0;
	node_ctx.actions[action_index].reg_mask = 0;
	node_ctx.actions[action_index].timestamp_seconds = 0;
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_record_action(NODE_action_t* action) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameter.
	if (action == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Store action.
	node_ctx.actions[node_ctx.actions_index].node = (action -> node);
	node_ctx.actions[node_ctx.actions_index].reg_addr = (action -> reg_addr);
	node_ctx.actions[node_ctx.actions_index].reg_value = (action -> reg_value);
	node_ctx.actions[node_ctx.actions_index].reg_mask = (action -> reg_mask);
	node_ctx.actions[node_ctx.actions_index].timestamp_seconds = (action -> timestamp_seconds);
	// Increment index.
	node_ctx.actions_index = (node_ctx.actions_index + 1) % NODE_ACTIONS_DEPTH;
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_search(NODE_address_t node_addr, NODE_t** node_ptr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint8_t address_match = 0;
	uint8_t idx = 0;
	// Reset pointer.
	(*node_ptr) = NULL;
	// Search board in nodes list.
	for (idx=0 ; idx<NODES_LIST.count ; idx++) {
		// Compare address
		if (NODES_LIST.list[idx].address == node_addr) {
			address_match = 1;
			break;
		}
	}
	// Check flag.
	if (address_match == 0) {
		status = NODE_ERROR_DOWNLINK_NODE_ADDRESS;
		goto errors;
	}
	// Update pointer.
	(*node_ptr) = &NODES_LIST.list[idx];
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_execute_downlink(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_action_t action;
	NODE_t* node_ptr = NULL;
	uint32_t previous_reg_value = 0;
	// Check operation code.
	switch (node_ctx.sigfox_dl_payload.op_code) {
	case NODE_DOWNLINK_OP_CODE_NOP:
		// No operation.
		break;
	case NODE_DOWNLINK_OP_CODE_SINGLE_FULL_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.full_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.full_write.reg_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.full_write.reg_value;
		action.reg_mask = DINFOX_REG_MASK_ALL;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_SINGLE_MASKED_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.masked_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.masked_write.reg_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.masked_write.reg_value;
		action.reg_mask = (uint32_t) node_ctx.sigfox_dl_payload.masked_write.reg_mask;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_TEMPORARY_FULL_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.full_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Read current register value.
		status = _NODE_read_register(node_ptr, node_ctx.sigfox_dl_payload.full_write.reg_addr, &previous_reg_value);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.full_write.reg_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.full_write.reg_value;
		action.reg_mask = DINFOX_REG_MASK_ALL;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Register second action.
		action.reg_value = previous_reg_value;
		action.timestamp_seconds = RTC_get_time_seconds() + DINFOX_get_seconds(node_ctx.sigfox_dl_payload.full_write.duration);
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_TEMPORARY_MASKED_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.masked_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Read current register value.
		status = _NODE_read_register(node_ptr, node_ctx.sigfox_dl_payload.masked_write.reg_addr, &previous_reg_value);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.masked_write.reg_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.masked_write.reg_value;
		action.reg_mask = (uint32_t) node_ctx.sigfox_dl_payload.masked_write.reg_mask;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Register second action.
		action.reg_value = previous_reg_value;
		action.timestamp_seconds = RTC_get_time_seconds() + DINFOX_get_seconds(node_ctx.sigfox_dl_payload.masked_write.duration);
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_SUCCESSIVE_FULL_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.successive_full_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.successive_full_write.reg_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.successive_full_write.reg_value_1;
		action.reg_mask = DINFOX_REG_MASK_ALL;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Register second action.
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.successive_full_write.reg_value_2;
		action.timestamp_seconds = RTC_get_time_seconds() + DINFOX_get_seconds(node_ctx.sigfox_dl_payload.successive_full_write.duration);
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_SUCCESSIVE_MASKED_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.successive_masked_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.successive_masked_write.reg_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.successive_masked_write.reg_value_1;
		action.reg_mask = (uint32_t) node_ctx.sigfox_dl_payload.successive_masked_write.reg_mask;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Register second action.
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.successive_masked_write.reg_value_2;
		action.timestamp_seconds = RTC_get_time_seconds() + DINFOX_get_seconds(node_ctx.sigfox_dl_payload.successive_masked_write.duration);
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_DUAL_FULL_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.dual_full_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.dual_full_write.reg_1_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.dual_full_write.reg_1_value;
		action.reg_mask = DINFOX_REG_MASK_ALL;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Register second action.
		action.reg_addr = node_ctx.sigfox_dl_payload.dual_full_write.reg_2_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.dual_full_write.reg_2_value;
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_TRIPLE_FULL_WRITE:
		// Search node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.triple_full_write.node_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.triple_full_write.reg_1_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.triple_full_write.reg_1_value;
		action.reg_mask = DINFOX_REG_MASK_ALL;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Register second action.
		action.reg_addr = node_ctx.sigfox_dl_payload.triple_full_write.reg_2_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.triple_full_write.reg_2_value;
		_NODE_record_action(&action);
		// Register third action.
		action.reg_addr = node_ctx.sigfox_dl_payload.triple_full_write.reg_3_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.triple_full_write.reg_3_value;
		_NODE_record_action(&action);
		break;
	case NODE_DOWNLINK_OP_CODE_DUAL_NODE_WRITE:
		// Search first node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.dual_node_write.node_1_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register first action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.dual_node_write.reg_1_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.dual_node_write.reg_1_value;
		action.reg_mask = DINFOX_REG_MASK_ALL;
		action.timestamp_seconds = 0;
		_NODE_record_action(&action);
		// Search second node.
		status = _NODE_search(node_ctx.sigfox_dl_payload.dual_node_write.node_2_addr, &node_ptr);
		if (status != NODE_SUCCESS) goto errors;
		// Register second action.
		action.node = node_ptr;
		action.reg_addr = node_ctx.sigfox_dl_payload.dual_node_write.reg_2_addr;
		action.reg_value = (uint32_t) node_ctx.sigfox_dl_payload.dual_node_write.reg_2_value;
		_NODE_record_action(&action);
		break;
	default:
		status = NODE_ERROR_DOWNLINK_OPERATION_CODE;
		break;
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_execute_actions(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	POWER_status_t power_status = POWER_SUCCESS;
	uint8_t idx = 0;
	// Loop on action table.
	for (idx=0 ; idx<NODE_ACTIONS_DEPTH ; idx++) {
		// Check NODE pointer and timestamp.
		if ((node_ctx.actions[idx].node != NULL) && (RTC_get_time_seconds() >= node_ctx.actions[idx].timestamp_seconds)) {
			// Turn bus interface on.
			power_status = POWER_enable(POWER_DOMAIN_RS485, LPTIM_DELAY_MODE_STOP);
			POWER_exit_error(NODE_ERROR_BASE_POWER);
			// Perform write operation.
			status = _NODE_write_register(node_ctx.actions[idx].node, node_ctx.actions[idx].reg_addr, node_ctx.actions[idx].reg_value, node_ctx.actions[idx].reg_mask);
			if (status != NODE_SUCCESS) goto errors;
			// Remove action.
			status = _NODE_remove_action(idx);
			if (status != NODE_SUCCESS) goto errors;
		}
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t _NODE_radio_task(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	POWER_status_t power_status = POWER_SUCCESS;
	NODE_access_parameters_t read_params;
	NODE_access_status_t unused_read_status;
	uint32_t reg_value = 0;
	uint8_t bidirectional_flag = 0;
	uint8_t ul_next_time_update_required = 0;
	uint8_t dl_next_time_update_required = 0;
	// Check uplink period.
	if (RTC_get_time_seconds() >= node_ctx.sigfox_ul_next_time_seconds) {
		// Next time update needed.
		ul_next_time_update_required = 1;
		// Check downlink period.
		if (RTC_get_time_seconds() >= node_ctx.sigfox_dl_next_time_seconds) {
			// Next time update needed and set bidirectional flag.
			dl_next_time_update_required = 1;
			bidirectional_flag = 1;
		}
		// Turn bus interface on.
		power_status = POWER_enable(POWER_DOMAIN_RS485, LPTIM_DELAY_MODE_STOP);
		POWER_exit_error(NODE_ERROR_BASE_POWER);
		// Set radio times to now to compensate node update duration.
		if (ul_next_time_update_required != 0) {
			node_ctx.sigfox_ul_next_time_seconds = RTC_get_time_seconds();
		}
		if (dl_next_time_update_required != 0) {
			node_ctx.sigfox_dl_next_time_seconds = RTC_get_time_seconds();
		}
		// Send data through radio.
		status = _NODE_radio_send(&(NODES_LIST.list[node_ctx.sigfox_ul_node_list_index]), bidirectional_flag);
		// Switch to next node.
		node_ctx.sigfox_ul_node_list_index++;
		if (node_ctx.sigfox_ul_node_list_index >= NODES_LIST.count) {
			// Come back to first node.
			node_ctx.sigfox_ul_node_list_index = 0;
		}
		if (status != NODE_SUCCESS) goto errors;
	}
	// Execute downlink operation if needed.
	if (bidirectional_flag != 0) {
		// Read downlink payload.
		status = _NODE_radio_read();
		if (status != NODE_SUCCESS) goto errors;
		// Decode downlink payload.
		status = _NODE_execute_downlink();
		if (status != NODE_SUCCESS) goto errors;
	}
errors:
	// Update next radio times.
	read_params.node_addr = DINFOX_NODE_ADDRESS_DMM;
	read_params.reg_addr = DMM_REG_ADDR_SYSTEM_CONFIGURATION;
	read_params.reply_params.type = NODE_REPLY_TYPE_OK;
	read_params.reply_params.timeout_ms = AT_BUS_DEFAULT_TIMEOUT_MS;
	DMM_read_register(&read_params, &reg_value, &unused_read_status);
	// This is done here in case the downlink modified one of the periods (in order to take it into account directly for next radio wake-up).
	if (ul_next_time_update_required != 0) {
		node_ctx.sigfox_ul_next_time_seconds += DINFOX_get_seconds(DINFOX_read_field(reg_value, DMM_REG_SYSTEM_CONFIGURATION_MASK_UL_PERIOD));
	}
	if (dl_next_time_update_required != 0) {
		node_ctx.sigfox_dl_next_time_seconds += DINFOX_get_seconds(DINFOX_read_field(reg_value, DMM_REG_SYSTEM_CONFIGURATION_MASK_DL_PERIOD));
	}
	return status;
}

#ifdef BMS
/*******************************************************************/
NODE_status_t _NODE_bms_task(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t node_access_status = {.all = 0};
	POWER_status_t power_status = POWER_SUCCESS;
	uint32_t reg_value = 0;
	uint16_t vbatt_dinfox = 0;
	uint32_t vbatt_mv = 0;
	// Check monitoring period.
	if (RTC_get_time_seconds() >= node_ctx.bms_monitoring_next_time_seconds) {
		// Update next time.
		node_ctx.bms_monitoring_next_time_seconds = (RTC_get_time_seconds() + BMS_MONITORING_PERIOD_SECONDS);
		// Check BMS presence.
		if (node_ctx.bms_node_ptr == NULL) goto errors;
		// Turn bus interface on.
		power_status = POWER_enable(POWER_DOMAIN_RS485, LPTIM_DELAY_MODE_STOP);
		POWER_exit_error(NODE_ERROR_BASE_POWER);
		// Perform measurements.
		status = XM_perform_measurements((node_ctx.bms_node_ptr -> address), &node_access_status);
		if (status != NODE_SUCCESS) goto errors;
		NODE_check_access_status();
		// Read battery voltage.
		status = _NODE_read_register(node_ctx.bms_node_ptr, LVRM_REG_ADDR_ANALOG_DATA_1, &reg_value);
		if (status != NODE_SUCCESS) goto errors;
		// Check error value.
		vbatt_dinfox = (uint16_t) DINFOX_read_field(reg_value, LVRM_REG_ANALOG_DATA_1_MASK_VCOM);
		if (vbatt_dinfox == DINFOX_VOLTAGE_ERROR_VALUE) goto errors;
		// Get battery voltage.
		vbatt_mv = DINFOX_get_mv(vbatt_dinfox);
		// Check battery voltage.
		if (vbatt_mv < BMS_VBATT_LOW_THRESHOLD_MV) {
			// Open relay.
			status = _NODE_write_register(node_ctx.bms_node_ptr, LVRM_REG_ADDR_STATUS_CONTROL_1, 0x00, LVRM_REG_STATUS_CONTROL_1_MASK_RLST);
			if (status != NODE_SUCCESS) goto errors;
		}
		if (vbatt_mv > BMS_VBATT_HIGH_THRESHOLD_MV) {
			// Close relay.
			status = _NODE_write_register(node_ctx.bms_node_ptr, LVRM_REG_ADDR_STATUS_CONTROL_1, 0x01, LVRM_REG_STATUS_CONTROL_1_MASK_RLST);
			if (status != NODE_SUCCESS) goto errors;
		}
	}
errors:
	return status;
}
#endif

/*** NODE functions ***/

/*******************************************************************/
void NODE_init_por(void) {
	// Local variables.
	uint8_t idx = 0;
	// Reset node list.
	_NODE_flush_list();
	// Init context.
	node_ctx.sigfox_ul_node_list_index = 0;
	node_ctx.sigfox_ul_next_time_seconds = 0;
	node_ctx.sigfox_dl_next_time_seconds = 0;
	for (idx=0 ; idx<NODE_ACTIONS_DEPTH ; idx++) _NODE_remove_action(idx);
	node_ctx.actions_index = 0;
#ifdef BMS
	node_ctx.bms_node_ptr = NULL;
	node_ctx.bms_monitoring_next_time_seconds = 0;
#endif
	// Init registers.
	DMM_init_registers();
	R4S8CR_init_registers();
}

/*******************************************************************/
void NODE_init(void) {
	// Init common LPUART interface.
	LPUART1_init(DINFOX_NODE_ADDRESS_DMM);
	// Init interface layers.
	AT_BUS_init();
}

/*******************************************************************/
void NODE_de_init(void) {
	// Release common LPUART interface.
	LPUART1_de_init();
}

/*******************************************************************/
NODE_status_t NODE_scan(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	POWER_status_t power_status = LPUART_SUCCESS;
	uint8_t nodes_count = 0;
	uint8_t idx = 0;
	// Reset list.
	_NODE_flush_list();
	node_ctx.uhfm_address = DINFOX_NODE_ADDRESS_BROADCAST;
	// Add master board to the list.
	NODES_LIST.list[0].board_id = DINFOX_BOARD_ID_DMM;
	NODES_LIST.list[0].address = DINFOX_NODE_ADDRESS_DMM;
	NODES_LIST.count++;
	// Turn bus interface on.
	power_status = POWER_enable(POWER_DOMAIN_RS485, LPTIM_DELAY_MODE_STOP);
	POWER_exit_error(NODE_ERROR_BASE_POWER);
	// Scan LBUS nodes.
	status = AT_BUS_scan(&(NODES_LIST.list[NODES_LIST.count]), (NODES_LIST_SIZE_MAX - NODES_LIST.count), &nodes_count);
	if (status != NODE_SUCCESS) goto errors;
	// Update count.
	NODES_LIST.count += nodes_count;
	// Search UHFM board in nodes list.
	for (idx=0 ; idx<NODES_LIST.count ; idx++) {
		// Check board ID.
		if (NODES_LIST.list[idx].board_id == DINFOX_BOARD_ID_UHFM) {
			node_ctx.uhfm_address = NODES_LIST.list[idx].address;
			break;
		}
	}
#ifdef BMS
	// Search LVRM board dedicated to BMS function.
	node_ctx.bms_node_ptr = NULL;
	for (idx=0 ; idx<NODES_LIST.count ; idx++) {
		// Check board ID.
		if (NODES_LIST.list[idx].address == BMS_NODE_ADDRESS) {
			node_ctx.bms_node_ptr = &(NODES_LIST.list[idx]);
			break;
		}
	}
#endif
	// Scan R4S8CR nodes.
	status = R4S8CR_scan(&(NODES_LIST.list[NODES_LIST.count]), (NODES_LIST_SIZE_MAX - NODES_LIST.count), &nodes_count);
	if (status != NODE_SUCCESS) goto errors;
	// Update count.
	NODES_LIST.count += nodes_count;
errors:
	// Turn bus interface off.
	POWER_disable(POWER_DOMAIN_RS485);
	return status;
}

/*******************************************************************/
void NODE_task(void) {
	// Local variables.
	NODE_status_t node_status = NODE_SUCCESS;
	POWER_status_t power_status = POWER_SUCCESS;
	// Radio task.
	node_status = _NODE_radio_task();
	NODE_stack_error();
	// Execute node actions.
	node_status = _NODE_execute_actions();
	NODE_stack_error();
#ifdef BMS
	// BMS task.
	node_status = _NODE_bms_task();
	NODE_stack_error();
#endif
	// Turn bus interface off.
	power_status = POWER_disable(POWER_DOMAIN_RS485);
	POWER_stack_error();
}

/*******************************************************************/
NODE_status_t NODE_write_line_data(NODE_t* node, uint8_t line_data_index, uint32_t field_value) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_line_data_write_t line_data_write;
	NODE_access_status_t node_access_status = {.all = 0};
	// Check board ID and function.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(write_line_data);
	// Build structure.
	line_data_write.node_addr = (node -> address);
	line_data_write.line_data_index = line_data_index;
	line_data_write.field_value = field_value;
	// Execute function of the corresponding board ID.
	status = NODES[node -> board_id].functions.write_line_data(&line_data_write, &node_access_status);
	if (status != NODE_SUCCESS) goto errors;
	NODE_check_access_status();
errors:
	return status;
}

/*******************************************************************/
NODE_status_t NODE_read_line_data(NODE_t* node, uint8_t line_data_index) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_line_data_read_t line_data_read;
	NODE_access_status_t node_access_status = {.all = 0};
	// Check board ID and function.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(read_line_data);
	// Flush line.
	_NODE_flush_line_data_value(line_data_index);
	// Build structure.
	line_data_read.node_addr = (node -> address);
	line_data_read.line_data_index = line_data_index;
	line_data_read.name_ptr = (char_t*) &(node_ctx.data.line_data_name[line_data_index]);
	line_data_read.value_ptr = (char_t*) &(node_ctx.data.line_data_value[line_data_index]);
	// Execute function of the corresponding board ID.
	status = NODES[node -> board_id].functions.read_line_data(&line_data_read, &node_access_status);
	if (status != NODE_SUCCESS) goto errors;
	NODE_check_access_status();
errors:
	return status;
}

/*******************************************************************/
NODE_status_t NODE_read_line_data_all(NODE_t* node) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t node_access_status = {.all = 0};
	uint8_t idx = 0;
	// Check board ID.
	_NODE_check_node_and_board_id();
	// Check indexes.
	if ((NODES[node -> board_id].last_line_data_index) == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	// Reset buffers.
	_NODE_flush_all_data_value();
	// Check protocol.
	if ((NODES[node -> board_id].protocol) == NODE_PROTOCOL_AT_BUS) {
		// Perform node measurements.
		status = XM_perform_measurements((node -> address), &node_access_status);
		if (status != NODE_SUCCESS) goto errors;
		NODE_check_access_status();
	}
	// String data loop.
	for (idx=0 ; idx<(NODES[node -> board_id].last_line_data_index) ; idx++) {
		status = NODE_read_line_data(node, idx);
		if (status != NODE_SUCCESS) goto errors;
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t NODE_get_name(NODE_t* node, char_t** board_name_ptr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check board ID.
	_NODE_check_node_and_board_id();
	// Get name of the corresponding board ID.
	(*board_name_ptr) = (char_t*) NODES[node -> board_id].name;
errors:
	return status;
}

/*******************************************************************/
NODE_status_t NODE_get_last_line_data_index(NODE_t* node, uint8_t* last_line_data_index) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check board ID.
	_NODE_check_node_and_board_id();
	// Get name of the corresponding board ID.
	(*last_line_data_index) = NODES[node -> board_id].last_line_data_index;
errors:
	return status;
}

/*******************************************************************/
NODE_status_t NODE_get_line_data(NODE_t* node, uint8_t line_data_index, char_t** line_data_name_ptr, char_t** line_data_value_ptr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameters.
	_NODE_check_node_and_board_id();
	// Check index.
	if (NODES[node -> board_id].last_line_data_index == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	if (line_data_index >= (NODES[node -> board_id].last_line_data_index)) { \
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Update pointers.
	(*line_data_name_ptr) = (char_t*) node_ctx.data.line_data_name[line_data_index];
	(*line_data_value_ptr) = (char_t*) node_ctx.data.line_data_value[line_data_index];
errors:
	return status;
}
