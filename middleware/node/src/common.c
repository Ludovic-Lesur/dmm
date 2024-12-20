/*
 * common.c
 *
 *  Created on: 26 jan 2023
 *      Author: Ludo
 */

#include "common.h"

#include "common_reg.h"
#include "dinfox.h"
#include "node.h"
#include "node_common.h"
#include "rcc_reg.h"
#include "string.h"
#include "types.h"
#include "version.h"
#include "xm.h"

/*** COMMON local macros ***/

#define COMMON_SIGFOX_UL_PAYLOAD_ACTION_LOG_SIZE	8
#define COMMON_SIGFOX_UL_PAYLOAD_STARTUP_SIZE		8
#define COMMON_SIGFOX_UL_PAYLOAD_ERROR_STACK_SIZE	10

/*** COMMON local structures ***/

/*******************************************************************/
typedef union {
	uint8_t frame[COMMON_SIGFOX_UL_PAYLOAD_ACTION_LOG_SIZE];
	struct {
		unsigned marker : 4;
		unsigned downlink_hash : 12;
		unsigned reg_addr : 8;
		unsigned reg_value : 32;
		unsigned node_access_status : 8;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} COMMON_sigfox_ul_payload_action_log_t;

/*******************************************************************/
typedef union {
	uint8_t frame[COMMON_SIGFOX_UL_PAYLOAD_STARTUP_SIZE];
	struct {
		unsigned reset_reason : 8;
		unsigned major_version : 8;
		unsigned minor_version : 8;
		unsigned commit_index : 8;
		unsigned commit_id : 28;
		unsigned dirty_flag : 4;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} COMMON_sigfox_ul_payload_startup_t;

/*** COMMON global variables ***/

const uint32_t COMMON_REG_WRITE_TIMEOUT_MS[COMMON_REG_ADDR_LAST] = {
	COMMON_REG_WRITE_TIMEOUT_MS_LIST
};

/*** COMMON local global variables ***/

static const NODE_line_data_t COMMON_LINE_DATA[COMMON_LINE_DATA_INDEX_LAST] = {
	{"HW =", STRING_NULL, STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_HW_VERSION, DINFOX_REG_MASK_ALL, COMMON_REG_ADDR_CONTROL_0, DINFOX_REG_MASK_NONE},
	{"SW =", STRING_NULL, STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_SW_VERSION_0, DINFOX_REG_MASK_ALL, COMMON_REG_ADDR_CONTROL_0, DINFOX_REG_MASK_NONE},
	{"RESET =", STRING_NULL, STRING_FORMAT_HEXADECIMAL, 1, COMMON_REG_ADDR_STATUS_0, COMMON_REG_STATUS_0_MASK_RESET_FLAGS, COMMON_REG_ADDR_CONTROL_0, COMMON_REG_CONTROL_0_MASK_RTRG},
	{"VMCU =", " V", STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_ANALOG_DATA_0, COMMON_REG_ANALOG_DATA_0_MASK_VMCU, COMMON_REG_ADDR_CONTROL_0, DINFOX_REG_MASK_NONE},
	{"TMCU =", " |C", STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_ANALOG_DATA_0, COMMON_REG_ANALOG_DATA_0_MASK_TMCU, COMMON_REG_ADDR_CONTROL_0, DINFOX_REG_MASK_NONE}
};

static const uint8_t COMMON_REG_LIST_SIGFOX_UL_PAYLOAD_STARTUP[] = {
	COMMON_REG_ADDR_SW_VERSION_0,
	COMMON_REG_ADDR_SW_VERSION_1,
	COMMON_REG_ADDR_STATUS_0
};

/*** COMMON local functions ***/

/*******************************************************************/
static NODE_status_t _COMMON_build_sigfox_ul_payload_startup(NODE_ul_payload_t* node_ul_payload, XM_node_registers_t* node_reg) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t access_status;
	XM_registers_list_t reg_list;
	COMMON_sigfox_ul_payload_startup_t sigfox_ul_payload_startup;
	uint8_t idx = 0;
	// Check parameters.
	if ((node_ul_payload == NULL) || (node_reg == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((node_ul_payload -> ul_payload) == NULL) || ((node_ul_payload -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset payload size.
	(*(node_ul_payload -> size)) = 0;
	// Build registers list.
	reg_list.addr_list = (uint8_t*) COMMON_REG_LIST_SIGFOX_UL_PAYLOAD_STARTUP;
	reg_list.size = sizeof(COMMON_REG_LIST_SIGFOX_UL_PAYLOAD_STARTUP);
	// Read related registers.
	status = XM_read_registers((node_ul_payload -> node -> address), &reg_list, node_reg, &access_status);
	if (status != NODE_SUCCESS) goto errors;
	// Build data payload.
	sigfox_ul_payload_startup.reset_reason = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_STATUS_0], COMMON_REG_STATUS_0_MASK_RESET_FLAGS);
	sigfox_ul_payload_startup.major_version = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_MAJOR);
	sigfox_ul_payload_startup.minor_version = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_MINOR);
	sigfox_ul_payload_startup.commit_index = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_COMMIT_INDEX);
	sigfox_ul_payload_startup.commit_id = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_1], COMMON_REG_SW_VERSION_1_MASK_COMMIT_ID);
	sigfox_ul_payload_startup.dirty_flag = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_DTYF);
	// Copy payload.
	for (idx=0 ; idx<COMMON_SIGFOX_UL_PAYLOAD_STARTUP_SIZE ; idx++) {
		(node_ul_payload -> ul_payload)[idx] = sigfox_ul_payload_startup.frame[idx];
	}
	(*(node_ul_payload -> size)) = COMMON_SIGFOX_UL_PAYLOAD_STARTUP_SIZE;
errors:
	return status;
}

/*******************************************************************/
static NODE_status_t _COMMON_build_sigfox_ul_payload_error_stack(NODE_ul_payload_t* node_ul_payload, XM_node_registers_t* node_reg) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t access_status;
	uint32_t reg_value = 0;
	uint8_t idx = 0;
	// Check parameters.
	if ((node_ul_payload == NULL) || (node_reg == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((node_ul_payload -> ul_payload) == NULL) || ((node_ul_payload -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset payload size.
	(*(node_ul_payload -> size)) = 0;
	// Read error stack register.
	for (idx=0 ; idx<(COMMON_SIGFOX_UL_PAYLOAD_ERROR_STACK_SIZE / 2) ; idx++) {
		// Read error stack register.
		status = XM_read_register((node_ul_payload -> node -> address), COMMON_REG_ADDR_ERROR_STACK, node_reg, &access_status);
		if ((status != NODE_SUCCESS) || (access_status.flags != 0)) goto errors;
		// Update local value.
		reg_value = (node_reg -> value)[COMMON_REG_ADDR_ERROR_STACK];
		// If the first error is zero, the stack is empty, no frame has to be sent.
		if ((idx == 0) && ((reg_value & COMMON_REG_ERROR_STACK_MASK_ERROR) == 0)) {
			goto errors;
		}
		(node_ul_payload -> ul_payload)[(2 * idx) + 0] = (uint8_t) ((reg_value >> 8) & 0x000000FF);
		(node_ul_payload -> ul_payload)[(2 * idx) + 1] = (uint8_t) ((reg_value >> 0) & 0x000000FF);
	}
	(*(node_ul_payload -> size)) = COMMON_SIGFOX_UL_PAYLOAD_ERROR_STACK_SIZE;
errors:
	return status;
}

/*** COMMON functions ***/

/*******************************************************************/
NODE_status_t COMMON_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Call common function.
	status = XM_write_line_data(line_data_write, (NODE_line_data_t*) COMMON_LINE_DATA, (uint32_t*) COMMON_REG_WRITE_TIMEOUT_MS, write_status);
	return status;
}

/*******************************************************************/
NODE_status_t COMMON_read_line_data(NODE_line_data_read_t* line_data_read, XM_node_registers_t* node_reg, NODE_access_status_t* read_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	STRING_status_t string_status = STRING_SUCCESS;
	uint8_t buffer_size = 0;
	uint8_t str_data_idx = 0;
	uint8_t reg_addr = 0;
	uint32_t reg_value = 0;
	uint32_t field_value = 0;
	char_t field_str[STRING_DIGIT_FUNCTION_SIZE];
	int8_t tmcu = 0;
	// Check parameters.
	if (line_data_read == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((line_data_read -> name_ptr) == NULL) || ((line_data_read -> value_ptr) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Check index.
	if ((line_data_read -> line_data_index) >= COMMON_LINE_DATA_INDEX_LAST) {
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Get string data index and register address.
	str_data_idx = (line_data_read -> line_data_index);
	reg_addr = COMMON_LINE_DATA[(line_data_read -> line_data_index)].read_reg_addr;
	// Add data name.
	NODE_append_name_string(COMMON_LINE_DATA[line_data_read -> line_data_index].name);
	// Reset result to error.
	NODE_flush_string_value();
	NODE_append_value_string((char_t*) NODE_ERROR_STRING);
	// Read register.
	status = XM_read_register((line_data_read -> node_addr), reg_addr, node_reg, read_status);
	if ((status != NODE_SUCCESS) || ((read_status -> flags) != 0)) goto errors;
	// Update local value.
	reg_value = (node_reg -> value)[reg_addr];
	// Check index.
	switch (str_data_idx) {
	case COMMON_LINE_DATA_INDEX_HW_VERSION:
		// Print HW version.
		field_value = DINFOX_read_field(reg_value, COMMON_REG_HW_VERSION_MASK_MAJOR);
		NODE_flush_string_value();
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		NODE_append_value_string(".");
		field_value = DINFOX_read_field(reg_value, COMMON_REG_HW_VERSION_MASK_MINOR);
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		break;
	case COMMON_LINE_DATA_INDEX_SW_VERSION:
		// Print SW version.
		field_value = DINFOX_read_field(reg_value, COMMON_REG_SW_VERSION_0_MASK_MAJOR);
		NODE_flush_string_value();
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		NODE_append_value_string(".");
		field_value = DINFOX_read_field(reg_value, COMMON_REG_SW_VERSION_0_MASK_MINOR);
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		NODE_append_value_string(".");
		field_value = DINFOX_read_field(reg_value, COMMON_REG_SW_VERSION_0_MASK_COMMIT_INDEX);
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		if (DINFOX_read_field(reg_value, COMMON_REG_SW_VERSION_0_MASK_DTYF) != 0) {
			NODE_append_value_string(".d");
		}
		break;
	case COMMON_LINE_DATA_INDEX_RESET_REASON:
		// Print reset reason.
		field_value = DINFOX_read_field(reg_value, COMMON_LINE_DATA[str_data_idx].read_field_mask);
		NODE_flush_string_value();
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		break;
	case COMMON_LINE_DATA_INDEX_VMCU_MV:
		// Get MCU voltage.
		field_value = DINFOX_read_field(reg_value, COMMON_LINE_DATA[str_data_idx].read_field_mask);
		// Check error value.
		if (field_value != DINFOX_VOLTAGE_ERROR_VALUE) {
			// Convert to 5 digits string.
			string_status = STRING_value_to_5_digits_string((int32_t) (DINFOX_get_mv((DINFOX_voltage_representation_t) field_value)), (char_t*) field_str);
			STRING_exit_error(NODE_ERROR_BASE_STRING);
			// Add string.
			NODE_flush_string_value();
			NODE_append_value_string(field_str)
			NODE_append_value_string(COMMON_LINE_DATA[line_data_read -> line_data_index].unit);
		}
		break;
	case COMMON_LINE_DATA_INDEX_TMCU_DEGREES:
		// Get MCU temperature.
		field_value = DINFOX_read_field(reg_value, COMMON_LINE_DATA[str_data_idx].read_field_mask);
		// Check error value.
		if (field_value != DINFOX_TEMPERATURE_ERROR_VALUE) {
			// Convert to degrees.
			tmcu = (int32_t) DINFOX_get_degrees((DINFOX_temperature_representation_t) field_value);
			// Add string.
			NODE_flush_string_value();
			NODE_append_value_int32(tmcu, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
			NODE_append_value_string(COMMON_LINE_DATA[line_data_read -> line_data_index].unit);
		}
		break;
	default:
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t COMMON_check_event_driven_payloads(NODE_ul_payload_t* node_ul_payload, XM_node_registers_t* node_reg) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint32_t reg_status_0 = 0;
	NODE_access_status_t access_status;
	// Check parameters.
	if ((node_ul_payload == NULL) || (node_reg == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((node_ul_payload -> ul_payload) == NULL) || ((node_ul_payload -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset payload size.
	(*(node_ul_payload -> size)) = 0;
	// Read status register.
	status = XM_read_register((node_ul_payload -> node -> address), COMMON_REG_ADDR_STATUS_0, node_reg, &access_status);
	if ((status != NODE_SUCCESS) || (access_status.flags != 0)) goto errors;
	// Update local value.
	reg_status_0 = (node_reg -> value)[COMMON_REG_ADDR_STATUS_0];
	// Read boot flag.
	if (DINFOX_read_field(reg_status_0, COMMON_REG_STATUS_0_MASK_BF) != 0) {
		// Compute startup payload.
		status = _COMMON_build_sigfox_ul_payload_startup(node_ul_payload, node_reg);
		if (status != NODE_SUCCESS) goto errors;
		// Clear boot flag.
		status = XM_write_register((node_ul_payload -> node -> address), COMMON_REG_ADDR_CONTROL_0, COMMON_REG_CONTROL_0_MASK_BFC, COMMON_REG_CONTROL_0_MASK_BFC, AT_BUS_DEFAULT_TIMEOUT_MS, &access_status);
		if (status != NODE_SUCCESS) goto errors;
	}
	else {
		if (DINFOX_read_field(reg_status_0, COMMON_REG_STATUS_0_MASK_ESF) != 0) {
			// Compute error stack payload.
			status = _COMMON_build_sigfox_ul_payload_error_stack(node_ul_payload, node_reg);
			if (status != NODE_SUCCESS) goto errors;
		}
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t COMMON_build_sigfox_action_log_ul_payload(NODE_ul_payload_t* node_ul_payload, NODE_action_t* node_action) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	COMMON_sigfox_ul_payload_action_log_t sigfox_ul_payload_action_log;
	uint8_t idx = 0;
	// Check parameters.
	if ((node_ul_payload == NULL) || (node_action == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((node_ul_payload -> node) == NULL) || ((node_ul_payload -> ul_payload) == NULL) || ((node_ul_payload -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if ((node_action -> node) == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Build frame.
	sigfox_ul_payload_action_log.marker = 0b1111;
	sigfox_ul_payload_action_log.downlink_hash = ((node_action -> downlink_hash) & 0x00000FFF);
	sigfox_ul_payload_action_log.reg_addr = (node_action -> reg_addr);
	sigfox_ul_payload_action_log.reg_value = (node_action -> reg_value);
	sigfox_ul_payload_action_log.node_access_status = ((node_action -> access_status).all);
	// Copy payload.
	for (idx=0 ; idx<COMMON_SIGFOX_UL_PAYLOAD_ACTION_LOG_SIZE ; idx++) {
		(node_ul_payload -> ul_payload)[idx] = sigfox_ul_payload_action_log.frame[idx];
	}
	(*(node_ul_payload -> size)) = COMMON_SIGFOX_UL_PAYLOAD_ACTION_LOG_SIZE;
errors:
	return status;
}
