/*
 * common.c
 *
 *  Created on: 26 jan 2023
 *      Author: Ludo
 */

#include "common.h"

#include "at_bus.h"
#include "common_reg.h"
#include "dinfox.h"
#include "node.h"
#include "rcc_reg.h"
#include "string.h"
#include "types.h"
#include "version.h"
#include "xm.h"

/*** COMMON local macros ***/

#define COMMON_SIGFOX_PAYLOAD_STARTUP_SIZE		8
#define COMMON_SIGFOX_PAYLOAD_ERROR_STACK_SIZE	10

/*** COMMON local structures ***/

typedef union {
	uint8_t frame[COMMON_SIGFOX_PAYLOAD_STARTUP_SIZE];
	struct {
		unsigned reset_reason : 8;
		unsigned major_version : 8;
		unsigned minor_version : 8;
		unsigned commit_index : 8;
		unsigned commit_id : 28;
		unsigned dirty_flag : 4;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} COMMON_sigfox_payload_startup_t;

/*** COMMON local global variables ***/

static const NODE_line_data_t COMMON_LINE_DATA[COMMON_LINE_DATA_INDEX_LAST] = {
	{"HW =", STRING_NULL, STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_HW_VERSION, DINFOX_REG_MASK_ALL},
	{"SW =", STRING_NULL, STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_SW_VERSION_0, DINFOX_REG_MASK_ALL},
	{"RESET =", STRING_NULL, STRING_FORMAT_HEXADECIMAL, 1, COMMON_REG_ADDR_RESET_FLAGS, COMMON_REG_RESET_FLAGS_MASK_ALL},
	{"VMCU =", " V", STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_ANALOG_DATA_0, COMMON_REG_ANALOG_DATA_0_MASK_VMCU},
	{"TMCU =", " |C", STRING_FORMAT_DECIMAL, 0, COMMON_REG_ADDR_ANALOG_DATA_0, COMMON_REG_ANALOG_DATA_0_MASK_TMCU}
};

static const uint8_t COMMON_REG_LIST_SIGFOX_PAYLOAD_STARTUP[] = {
	COMMON_REG_ADDR_SW_VERSION_0,
	COMMON_REG_ADDR_SW_VERSION_1,
	COMMON_REG_ADDR_RESET_FLAGS
};

/* WRITE COMMON DATA.
 * @param line_data_write:	Pointer to the data write structure.
 * @return status:			Function execution status.
 */
NODE_status_t COMMON_write_line_data(NODE_line_data_write_t* line_data_write) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_parameters_t write_params;
	NODE_access_status_t write_status;
	uint8_t reg_addr = 0;
	uint32_t reg_value = 0;
	uint32_t reg_mask = 0;
	uint32_t timeout_ms = 0;
	// Compute parameters.
	reg_addr = COMMON_LINE_DATA[(line_data_write -> line_data_index)].reg_addr;
	reg_mask = COMMON_LINE_DATA[(line_data_write -> line_data_index)].field_mask;
	reg_value |= (line_data_write -> field_value) << DINFOX_get_field_offset(reg_mask);
	timeout_ms = COMMON_REG_WRITE_TIMEOUT_MS[reg_addr - COMMON_REG_ADDR_LAST];
	// Write parameters.
	write_params.node_addr = (line_data_write -> node_addr);
	write_params.reg_addr = reg_addr;
	write_params.reply_params.type = NODE_REPLY_TYPE_OK;
	write_params.reply_params.timeout_ms = timeout_ms;
	// Write register.
	status = AT_BUS_write_register(&write_params, reg_value, reg_mask, &write_status);
	return status;
}

/* UPDATE COMMON MEASUREMENTS OF DINFOX NODES.
 * @param line_data_read:	Pointer to the data update structure.
 * @param node_reg:			Pointer to the node registers.
 * @return status:			Function execution status.
 */
NODE_status_t COMMON_read_line_data(NODE_line_data_read_t* line_data_read, XM_node_registers_t* node_reg) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	STRING_status_t string_status = STRING_SUCCESS;
	NODE_access_status_t read_status;
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
	reg_addr = COMMON_LINE_DATA[(line_data_read -> line_data_index)].reg_addr;
	// Add data name.
	NODE_append_name_string(COMMON_LINE_DATA[line_data_read -> line_data_index].name);
	// Reset result to error.
	NODE_flush_string_value();
	NODE_append_value_string((char_t*) NODE_ERROR_STRING);
	// Read register.
	status = XM_read_register((line_data_read -> node_addr), reg_addr, (node_reg -> error)[reg_addr], &reg_value, &read_status);
	if ((status != NODE_SUCCESS) || (read_status.all != 0)) goto errors;
	// Update register.
	NODE_flush_string_value();
	(node_reg -> value)[reg_addr] = reg_value;
	// Check index.
	switch (str_data_idx) {
	case COMMON_LINE_DATA_INDEX_HW_VERSION:
		// Print HW version.
		field_value = DINFOX_read_field(reg_value, COMMON_REG_HW_VERSION_MASK_MAJOR);
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		NODE_append_value_string(".");
		field_value = DINFOX_read_field(reg_value, COMMON_REG_HW_VERSION_MASK_MINOR);
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		break;
	case COMMON_LINE_DATA_INDEX_SW_VERSION:
		// Print SW version.
		field_value = DINFOX_read_field(reg_value, COMMON_REG_SW_VERSION_0_MASK_MAJOR);
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
		field_value = DINFOX_read_field(reg_value, COMMON_LINE_DATA[str_data_idx].field_mask);
		NODE_append_value_int32(field_value, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		break;
	case COMMON_LINE_DATA_INDEX_VMCU_MV:
		// Get MCU voltage.
		field_value = DINFOX_read_field(reg_value, COMMON_LINE_DATA[str_data_idx].field_mask);
		// Convert to 5 digits string.
		string_status = STRING_value_to_5_digits_string(DINFOX_get_mv(field_value), (char_t*) field_str);
		STRING_status_check(NODE_ERROR_BASE_STRING);
		// Add string.
		NODE_append_value_string(field_str);
		break;
	case COMMON_LINE_DATA_INDEX_TMCU_DEGREES:
		// Print MCU temperature.
		field_value = DINFOX_read_field(reg_value, COMMON_LINE_DATA[str_data_idx].field_mask);
		tmcu = (int32_t) DINFOX_get_degrees(field_value);
		NODE_append_value_int32(tmcu, COMMON_LINE_DATA[str_data_idx].print_format, COMMON_LINE_DATA[str_data_idx].print_prefix);
		break;
	default:
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Add unit if no error.
	NODE_append_value_string(COMMON_LINE_DATA[line_data_read -> line_data_index].unit);
errors:
	return status;
}

/* BUILD COMMON STARTUP UL PAYLOAD.
 * @param ul_payload_update:	Pointer to the UL payload update structure.
 * @param node_reg:				Pointer to the node registers.
 * @return status:				Function execution status.
 */
NODE_status_t COMMON_build_sigfox_payload_startup(NODE_ul_payload_update_t* ul_payload_update, XM_node_registers_t* node_reg) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	XM_registers_list_t reg_list;
	COMMON_sigfox_payload_startup_t sigfox_payload_startup;
	uint8_t idx = 0;
	// Check parameters.
	if (ul_payload_update == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((ul_payload_update -> ul_payload) == NULL) || ((ul_payload_update -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset payload size.
	(*(ul_payload_update -> size)) = 0;
	// Build registers list.
	reg_list.addr_list = (uint8_t*) COMMON_REG_LIST_SIGFOX_PAYLOAD_STARTUP;
	reg_list.size = sizeof(COMMON_REG_LIST_SIGFOX_PAYLOAD_STARTUP);
	// Reset related registers.
	status = XM_reset_registers(&reg_list, node_reg);
	if (status != NODE_SUCCESS) goto errors;
	// Read related registers.
	status = XM_read_registers((ul_payload_update -> node -> address), &reg_list, node_reg);
	if (status != NODE_SUCCESS) goto errors;
	// Build data payload.
	sigfox_payload_startup.reset_reason = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_RESET_FLAGS], COMMON_REG_RESET_FLAGS_MASK_ALL);
	sigfox_payload_startup.major_version = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_MAJOR);
	sigfox_payload_startup.minor_version = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_MINOR);
	sigfox_payload_startup.commit_index = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_COMMIT_INDEX);
	sigfox_payload_startup.commit_id = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_1], COMMON_REG_SW_VERSION_1_MASK_COMMIT_ID);
	sigfox_payload_startup.dirty_flag = DINFOX_read_field((node_reg -> value)[COMMON_REG_ADDR_SW_VERSION_0], COMMON_REG_SW_VERSION_0_MASK_DTYF);
	// Copy payload.
	for (idx=0 ; idx<COMMON_SIGFOX_PAYLOAD_STARTUP_SIZE ; idx++) {
		(ul_payload_update -> ul_payload)[idx] = sigfox_payload_startup.frame[idx];
	}
	(*(ul_payload_update -> size)) = COMMON_SIGFOX_PAYLOAD_STARTUP_SIZE;
errors:
	return status;
}

/* BUILD COMMON ERROR STACK UL PAYLOAD.
 * @param ul_payload_update:	Pointer to the UL payload update structure.
 * @param node_reg:				Pointer to the node registers.
 * @return status:				Function execution status.
 */
NODE_status_t COMMON_build_sigfox_payload_error_stack(NODE_ul_payload_update_t* ul_payload_update, XM_node_registers_t* node_reg) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t read_status;
	uint32_t reg_value = 0;
	uint8_t idx = 0;
	// Check parameters.
	if (ul_payload_update == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((ul_payload_update -> ul_payload) == NULL) || ((ul_payload_update -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset payload size.
	(*(ul_payload_update -> size)) = 0;
	// Read error stack register.
	for (idx=0 ; idx<(COMMON_SIGFOX_PAYLOAD_ERROR_STACK_SIZE / 2) ; idx++) {
		// Read error stack register.
		status = XM_read_register((ul_payload_update -> node -> address), COMMON_REG_ADDR_ERROR_STACK, 0x00000000, &reg_value, &read_status);
		if ((status != NODE_SUCCESS) || ((read_status.all) != 0)) goto errors;
		// If the first error is zero, the stack is empty, no frame has to be sent.
		if ((idx == 0) && ((reg_value & COMMON_REG_ERROR_STACK_MASK_ERROR) == 0)) {
			goto errors;
		}
		(ul_payload_update -> ul_payload)[(2 * idx) + 0] = (uint8_t) ((reg_value >> 8) & 0x000000FF);
		(ul_payload_update -> ul_payload)[(2 * idx) + 1] = (uint8_t) ((reg_value >> 0) & 0x000000FF);
	}
	(*(ul_payload_update -> size)) = COMMON_SIGFOX_PAYLOAD_ERROR_STACK_SIZE;
errors:
	return status;
}
