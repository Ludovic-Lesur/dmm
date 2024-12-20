/*
 * uhfm.c
 *
 *  Created on: 26 feb. 2023
 *      Author: Ludo
 */

#include "uhfm.h"

#include "at_bus.h"
#include "uhfm_reg.h"
#include "common.h"
#include "dinfox.h"
#include "node.h"
#include "node_common.h"
#include "sigfox_ep_api.h"
#include "sigfox_types.h"
#include "string.h"
#include "xm.h"

/*** UHFM local macros ***/

#define UHFM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE		7

#define UHFM_SIGFOX_SEND_TIMEOUT_MS					90000

/*** UHFM local structures ***/

/*******************************************************************/
typedef enum {
	UHFM_SIGFOX_UL_PAYLOAD_TYPE_MONITORING = 0,
	UHFM_SIGFOX_UL_PAYLOAD_TYPE_LAST
} UHFM_sigfox_ul_payload_type_t;

/*******************************************************************/
typedef union {
	uint8_t frame[UHFM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE];
	struct {
		unsigned vmcu : 16;
		unsigned tmcu : 8;
		unsigned vrf_tx : 16;
		unsigned vrf_rx : 16;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} UHFM_sigfox_ul_payload_monitoring_t;

/*** UHFM global variables ***/

const uint32_t UHFM_REG_WRITE_TIMEOUT_MS[UHFM_REG_ADDR_LAST] = {
	COMMON_REG_WRITE_TIMEOUT_MS_LIST
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	5000,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS
};

/*** UHFM local global variables ***/

static uint32_t UHFM_REGISTERS[UHFM_REG_ADDR_LAST];

static const uint32_t UHFM_REG_ERROR_VALUE[UHFM_REG_ADDR_LAST] = {
	COMMON_REG_ERROR_VALUE_LIST
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	(uint32_t) ((DINFOX_VOLTAGE_ERROR_VALUE << 16) | (DINFOX_VOLTAGE_ERROR_VALUE << 0)),
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000
};

static const XM_node_registers_t UHFM_NODE_REGISTERS = {
	.value = (uint32_t*) UHFM_REGISTERS,
	.error = (uint32_t*) UHFM_REG_ERROR_VALUE,
};

static const NODE_line_data_t UHFM_LINE_DATA[UHFM_LINE_DATA_INDEX_LAST - COMMON_LINE_DATA_INDEX_LAST] = {
	{"EP ID", STRING_NULL, STRING_FORMAT_HEXADECIMAL, 1, UHFM_REG_ADDR_SIGFOX_EP_ID, DINFOX_REG_MASK_ALL,         UHFM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"VRF TX =", " V", STRING_FORMAT_DECIMAL, 0, UHFM_REG_ADDR_ANALOG_DATA_1, UHFM_REG_ANALOG_DATA_1_MASK_VRF_TX, UHFM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"VRF RX =", " V", STRING_FORMAT_DECIMAL, 0, UHFM_REG_ADDR_ANALOG_DATA_1, UHFM_REG_ANALOG_DATA_1_MASK_VRF_RX, UHFM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE}
};

static const uint8_t UHFM_REG_LIST_SIGFOX_UL_PAYLOAD_MONITORING[] = {
	COMMON_REG_ADDR_ANALOG_DATA_0,
	UHFM_REG_ADDR_ANALOG_DATA_1
};

static const UHFM_sigfox_ul_payload_type_t UHFM_SIGFOX_UL_PAYLOAD_PATTERN[] = {
	UHFM_SIGFOX_UL_PAYLOAD_TYPE_MONITORING
};

/*** UHFM functions ***/

/*******************************************************************/
NODE_status_t UHFM_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameters.
	if ((line_data_write == NULL) || (write_status == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Check index.
	if ((line_data_write -> line_data_index) >= UHFM_LINE_DATA_INDEX_LAST) {
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Check common range.
	if ((line_data_write -> line_data_index) < COMMON_LINE_DATA_INDEX_LAST) {
		// Call common function.
		status = COMMON_write_line_data(line_data_write, write_status);
	}
	else {
		// Remove offset.
		(line_data_write -> line_data_index) -= COMMON_LINE_DATA_INDEX_LAST;
		// Call common function.
		status = XM_write_line_data(line_data_write, (NODE_line_data_t*) UHFM_LINE_DATA, (uint32_t*) UHFM_REG_WRITE_TIMEOUT_MS, write_status);
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t UHFM_read_line_data(NODE_line_data_read_t* line_data_read, NODE_access_status_t* read_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	XM_node_registers_t node_reg;
	STRING_status_t string_status = STRING_SUCCESS;
	uint32_t field_value = 0;
	char_t field_str[STRING_DIGIT_FUNCTION_SIZE];
	uint8_t str_data_idx = 0;
	uint8_t reg_addr = 0;
	uint8_t buffer_size = 0;
	uint8_t idx = 0;
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
	if ((line_data_read -> line_data_index) >= UHFM_LINE_DATA_INDEX_LAST) {
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Build node registers structure.
	node_reg.value = (uint32_t*) UHFM_REGISTERS;
	node_reg.error = (uint32_t*) UHFM_REG_ERROR_VALUE;
	// Check common range.
	if ((line_data_read -> line_data_index) < COMMON_LINE_DATA_INDEX_LAST) {
		// Call common function.
		status = COMMON_read_line_data(line_data_read, &node_reg, read_status);
		if (status != NODE_SUCCESS) goto errors;
	}
	else {
		// Compute specific string data index and register address.
		str_data_idx = ((line_data_read -> line_data_index) - COMMON_LINE_DATA_INDEX_LAST);
		reg_addr = UHFM_LINE_DATA[str_data_idx].read_reg_addr;
		// Add data name.
		NODE_append_name_string((char_t*) UHFM_LINE_DATA[str_data_idx].name);
		buffer_size = 0;
		// Reset result to error.
		NODE_flush_string_value();
		NODE_append_value_string((char_t*) NODE_ERROR_STRING);
		// Update register.
		status = XM_read_register((line_data_read -> node_addr), reg_addr, (XM_node_registers_t*) &UHFM_NODE_REGISTERS, read_status);
		if ((status != NODE_SUCCESS) || ((read_status -> flags) != 0)) goto errors;
		// Compute field.
		field_value = DINFOX_read_field(UHFM_REGISTERS[reg_addr], UHFM_LINE_DATA[str_data_idx].read_field_mask);
		// Check index.
		switch (line_data_read -> line_data_index) {
		case UHFM_LINE_DATA_INDEX_SIGFOX_EP_ID:
			NODE_flush_string_value();
			for (idx=0 ; idx<DINFOX_REG_SIZE_BYTES ; idx++) {
				NODE_append_value_int32(((field_value >> (8 * idx)) & 0xFF), STRING_FORMAT_HEXADECIMAL, ((idx == 0) ? 1 : 0));
			}
			break;
		case UHFM_LINE_DATA_INDEX_VRF_TX:
		case UHFM_LINE_DATA_INDEX_VRF_RX:
			// Check error value.
			if (field_value != DINFOX_VOLTAGE_ERROR_VALUE) {
				// Convert to 5 digits string.
				string_status = STRING_value_to_5_digits_string((int32_t) (DINFOX_get_mv((DINFOX_voltage_representation_t) field_value)), (char_t*) field_str);
				STRING_exit_error(NODE_ERROR_BASE_STRING);
				// Add string.
				NODE_flush_string_value();
				NODE_append_value_string(field_str);
				// Add unit.
				NODE_append_value_string((char_t*) UHFM_LINE_DATA[str_data_idx].unit);
			}
			break;
		default:
			NODE_flush_string_value();
			NODE_append_value_int32(field_value, STRING_FORMAT_HEXADECIMAL, 1);
			break;
		}
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t UHFM_build_sigfox_ul_payload(NODE_ul_payload_t* node_ul_payload) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t access_status;
	XM_registers_list_t reg_list;
	UHFM_sigfox_ul_payload_monitoring_t sigfox_ul_payload_monitoring;
	uint8_t idx = 0;
	// Check parameters.
	if (node_ul_payload == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((node_ul_payload -> node) == NULL) || ((node_ul_payload -> ul_payload) == NULL) || ((node_ul_payload -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset payload size.
	(*(node_ul_payload -> size)) = 0;
	// Check event driven payloads.
	status = COMMON_check_event_driven_payloads(node_ul_payload, (XM_node_registers_t*) &UHFM_NODE_REGISTERS);
	if (status != NODE_SUCCESS) goto errors;
	// Directly exits if a common payload was computed.
	if ((*(node_ul_payload -> size)) > 0) goto errors;
	// Else use specific pattern of the node.
	switch (UHFM_SIGFOX_UL_PAYLOAD_PATTERN[node_ul_payload -> node -> radio_transmission_count]) {
	case UHFM_SIGFOX_UL_PAYLOAD_TYPE_MONITORING:
		// Build registers list.
		reg_list.addr_list = (uint8_t*) UHFM_REG_LIST_SIGFOX_UL_PAYLOAD_MONITORING;
		reg_list.size = sizeof(UHFM_REG_LIST_SIGFOX_UL_PAYLOAD_MONITORING);
		// Perform measurements.
		status = XM_perform_measurements((node_ul_payload -> node -> address), &access_status);
		if (status != NODE_SUCCESS) goto errors;
		// Check write status.
		if (access_status.flags == 0) {
			// Read related registers.
			status = XM_read_registers((node_ul_payload -> node -> address), &reg_list, (XM_node_registers_t*) &UHFM_NODE_REGISTERS, &access_status);
			if (status != NODE_SUCCESS) goto errors;
		}
		// Build monitoring payload.
		sigfox_ul_payload_monitoring.vmcu = DINFOX_read_field(UHFM_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0], COMMON_REG_ANALOG_DATA_0_MASK_VMCU);
		sigfox_ul_payload_monitoring.tmcu = DINFOX_read_field(UHFM_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0], COMMON_REG_ANALOG_DATA_0_MASK_TMCU);
		sigfox_ul_payload_monitoring.vrf_tx = DINFOX_read_field(UHFM_REGISTERS[UHFM_REG_ADDR_ANALOG_DATA_1], UHFM_REG_ANALOG_DATA_1_MASK_VRF_TX);
		sigfox_ul_payload_monitoring.vrf_rx = DINFOX_read_field(UHFM_REGISTERS[UHFM_REG_ADDR_ANALOG_DATA_1], UHFM_REG_ANALOG_DATA_1_MASK_VRF_RX);
		// Copy payload.
		for (idx=0 ; idx<UHFM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE ; idx++) {
			(node_ul_payload -> ul_payload)[idx] = sigfox_ul_payload_monitoring.frame[idx];
		}
		(*(node_ul_payload -> size)) = UHFM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE;
		break;
	default:
		status = NODE_ERROR_SIGFOX_UL_PAYLOAD_TYPE;
		goto errors;
	}
	// Increment transmission count.
	(node_ul_payload -> node -> radio_transmission_count) = ((node_ul_payload -> node -> radio_transmission_count) + 1) % (sizeof(UHFM_SIGFOX_UL_PAYLOAD_PATTERN));
errors:
	return status;
}

/*******************************************************************/
NODE_status_t UHFM_send_sigfox_message(NODE_address_t node_addr, UHFM_sigfox_message_t* sigfox_message, NODE_access_status_t* send_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint32_t reg_control_1 = 0;
	uint32_t reg_control_1_mask = 0;
	uint32_t ul_payload_x = 0;
	uint8_t reg_offset = 0;
	uint8_t idx = 0;
	// Configuration register 2.
	DINFOX_write_field(&reg_control_1, &reg_control_1_mask, (uint32_t) SIGFOX_APPLICATION_MESSAGE_TYPE_BYTE_ARRAY, UHFM_REG_CONTROL_1_MASK_MSGT);
	DINFOX_write_field(&reg_control_1, &reg_control_1_mask, (uint32_t) (sigfox_message -> bidirectional_flag), UHFM_REG_CONTROL_1_MASK_BF);
	DINFOX_write_field(&reg_control_1, &reg_control_1_mask, (uint32_t) (sigfox_message -> ul_payload_size), UHFM_REG_CONTROL_1_MASK_UL_PAYLOAD_SIZE);
	// Write register.
	status = XM_write_register(node_addr, UHFM_REG_ADDR_CONTROL_1, reg_control_1, reg_control_1_mask, AT_BUS_DEFAULT_TIMEOUT_MS, send_status);
	if ((status != NODE_SUCCESS) || ((send_status -> flags) != 0)) goto errors;
	// UL payload.
	for (idx=0 ; idx<(sigfox_message -> ul_payload_size) ; idx++) {
		// Build register.
		ul_payload_x |= (((sigfox_message -> ul_payload)[idx]) << (8 * (idx % 4)));
		// Check index.
		if ((((idx + 1) % 4) == 0) || (idx == ((sigfox_message -> ul_payload_size) - 1))) {
			// Write register.
			status = XM_write_register(node_addr, (UHFM_REG_ADDR_SIGFOX_UL_PAYLOAD_0 + reg_offset), ul_payload_x, DINFOX_REG_MASK_ALL, AT_BUS_DEFAULT_TIMEOUT_MS, send_status);
			if ((status != NODE_SUCCESS) || ((send_status -> flags) != 0)) goto errors;
			// Go to next register and reset value.
			reg_offset++;
			ul_payload_x = 0;
		}
	}
	// Send message.
	status = XM_write_register(node_addr, UHFM_REG_ADDR_CONTROL_1, UHFM_REG_CONTROL_1_MASK_STRG, UHFM_REG_CONTROL_1_MASK_STRG, UHFM_SIGFOX_SEND_TIMEOUT_MS, send_status);
errors:
	return status;
}

/*******************************************************************/
NODE_status_t UHFM_get_dl_payload(NODE_address_t node_addr, uint8_t* dl_payload, NODE_access_status_t* read_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	SIGFOX_EP_API_message_status_t message_status;
	uint8_t reg_addr = (UHFM_REG_ADDR_SIGFOX_DL_PAYLOAD_0 - 1);
	uint8_t idx = 0;
	// Check parameters.
	if ((dl_payload == NULL) || (read_status == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Read message status.
	status = XM_read_register(node_addr, UHFM_REG_ADDR_STATUS_1, (XM_node_registers_t*) &UHFM_NODE_REGISTERS, read_status);
	if ((status != NODE_SUCCESS) || ((read_status -> flags) != 0)) goto errors;
	// Compute message status.
	message_status.all = DINFOX_read_field(UHFM_REGISTERS[UHFM_REG_ADDR_STATUS_1], UHFM_REG_STATUS_1_MASK_MESSAGE_STATUS);
	// Check DL flag.
	if (message_status.field.dl_frame == 0) goto errors;
	// Byte loop.
	for (idx=0 ; idx<SIGFOX_DL_PAYLOAD_SIZE_BYTES ; idx++) {
		// Check index.
		if ((idx % 4) == 0) {
			// Go to next register.
			reg_addr++;
			// Read register.
			status = XM_read_register(node_addr, reg_addr, (XM_node_registers_t*) &UHFM_NODE_REGISTERS, read_status);
			if ((status != NODE_SUCCESS) || ((read_status -> flags) != 0)) goto errors;
		}
		// Convert to byte array.
		dl_payload[idx] = (uint8_t) ((UHFM_REGISTERS[reg_addr] >> (8 * (idx % 4))) & 0xFF);
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t UHFM_get_last_bidirectional_mc(NODE_address_t node_addr, uint32_t* last_message_counter, NODE_access_status_t* read_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameters.
	if ((last_message_counter == NULL)  || (read_status == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Read message status.
	status = XM_read_register(node_addr, UHFM_REG_ADDR_STATUS_1, (XM_node_registers_t*) &UHFM_NODE_REGISTERS, read_status);
	if ((status != NODE_SUCCESS) || ((read_status -> flags) != 0)) goto errors;
	// Compute message counter.
	(*last_message_counter) = DINFOX_read_field(UHFM_REGISTERS[UHFM_REG_ADDR_STATUS_1], UHFM_REG_STATUS_1_MASK_BIDIRECTIONAL_MC);
errors:
	return status;
}
