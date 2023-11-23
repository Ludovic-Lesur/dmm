/*
 * lvrm.c
 *
 *  Created on: 22 jan. 2023
 *      Author: Ludo
 */

#include "lvrm.h"

#include "lvrm_reg.h"
#include "common.h"
#include "dinfox.h"
#include "error.h"
#include "mode.h"
#include "node.h"
#include "node_common.h"
#include "string.h"
#include "xm.h"

/*** LVRM local macros ***/

#define LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE		3
#define LVRM_SIGFOX_PAYLOAD_ELECTRICAL_SIZE		7

/*** LVRM local structures ***/

/*******************************************************************/
typedef enum {
	LVRM_SIGFOX_PAYLOAD_TYPE_MONITORING = 0,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_LAST
} LVRM_sigfox_payload_type_t;

/*******************************************************************/
typedef union {
	uint8_t frame[LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE];
	struct {
		unsigned vmcu : 16;
		unsigned tmcu : 8;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} LVRM_sigfox_payload_monitoring_t;

/*******************************************************************/
typedef union {
	uint8_t frame[LVRM_SIGFOX_PAYLOAD_ELECTRICAL_SIZE];
	struct {
		unsigned vin : 16;
		unsigned vout : 16;
		unsigned iout : 16;
		unsigned unused : 6;
		unsigned rlstst : 2;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} LVRM_sigfox_payload_electrical_t;

/*** LVRM local global variables ***/

static uint32_t LVRM_REGISTERS[LVRM_REG_ADDR_LAST];

static const NODE_line_data_t LVRM_LINE_DATA[LVRM_LINE_DATA_INDEX_LAST - COMMON_LINE_DATA_INDEX_LAST] = {
	{"VCOM =", " V",  STRING_FORMAT_DECIMAL, 0, LVRM_REG_ADDR_ANALOG_DATA_1,   LVRM_REG_ANALOG_DATA_1_MASK_VCOM, LVRM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"VOUT =", " V",  STRING_FORMAT_DECIMAL, 0, LVRM_REG_ADDR_ANALOG_DATA_1,   LVRM_REG_ANALOG_DATA_1_MASK_VOUT, LVRM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"IOUT =", " mA", STRING_FORMAT_DECIMAL, 0, LVRM_REG_ADDR_ANALOG_DATA_2,   LVRM_REG_ANALOG_DATA_2_MASK_IOUT, LVRM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"RELAY =", STRING_NULL, STRING_FORMAT_DECIMAL, 0, LVRM_REG_ADDR_STATUS_1, LVRM_REG_STATUS_1_MASK_RLSTST,    LVRM_REG_ADDR_CONTROL_1, LVRM_REG_CONTROL_1_MASK_RLST}
};

static const uint32_t LVRM_REG_ERROR_VALUE[LVRM_REG_ADDR_LAST] = {
	COMMON_REG_ERROR_VALUE
	0x00000000,
	(DINFOX_BIT_ERROR << 0),
	((DINFOX_VOLTAGE_ERROR_VALUE << 16) | (DINFOX_VOLTAGE_ERROR_VALUE << 0)),
	(DINFOX_VOLTAGE_ERROR_VALUE << 0)
};

static const uint8_t LVRM_REG_LIST_SIGFOX_PAYLOAD_MONITORING[] = {
	COMMON_REG_ADDR_ANALOG_DATA_0
};

static const uint8_t LVRM_REG_LIST_SIGFOX_PAYLOAD_ELECTRICAL[] = {
	LVRM_REG_ADDR_STATUS_1,
	LVRM_REG_ADDR_ANALOG_DATA_1,
	LVRM_REG_ADDR_ANALOG_DATA_2
};

static const LVRM_sigfox_payload_type_t LVRM_SIGFOX_PAYLOAD_PATTERN[] = {
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL,
	LVRM_SIGFOX_PAYLOAD_TYPE_MONITORING,
};

/*** LVRM functions ***/

/*******************************************************************/
NODE_status_t LVRM_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check common range.
	if ((line_data_write -> line_data_index) < COMMON_LINE_DATA_INDEX_LAST) {
		// Call common function.
		status = COMMON_write_line_data(line_data_write, write_status);
	}
	else {
		// Remove offset.
		(line_data_write -> line_data_index) -= COMMON_LINE_DATA_INDEX_LAST;
		// Call common function.
		status = XM_write_line_data(line_data_write, (NODE_line_data_t*) LVRM_LINE_DATA, (uint32_t*) LVRM_REG_WRITE_TIMEOUT_MS, write_status);
	}
	return status;
}

/*******************************************************************/
NODE_status_t LVRM_read_line_data(NODE_line_data_read_t* line_data_read, NODE_access_status_t* read_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	XM_node_registers_t node_reg;
	STRING_status_t string_status = STRING_SUCCESS;
	uint32_t field_value = 0;
	char_t field_str[STRING_DIGIT_FUNCTION_SIZE];
	uint8_t str_data_idx = 0;
	uint8_t reg_addr = 0;
	uint8_t buffer_size = 0;
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
	if ((line_data_read -> line_data_index) >= LVRM_LINE_DATA_INDEX_LAST) {
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Build node registers structure.
	node_reg.value = (uint32_t*) LVRM_REGISTERS;
	node_reg.error = (uint32_t*) LVRM_REG_ERROR_VALUE;
	// Check common range.
	if ((line_data_read -> line_data_index) < COMMON_LINE_DATA_INDEX_LAST) {
		// Call common function.
		status = COMMON_read_line_data(line_data_read, &node_reg, read_status);
		if (status != NODE_SUCCESS) goto errors;
	}
	else {
		// Compute specific string data index and register address.
		str_data_idx = ((line_data_read -> line_data_index) - COMMON_LINE_DATA_INDEX_LAST);
		reg_addr = LVRM_LINE_DATA[str_data_idx].read_reg_addr;
		// Add data name.
		NODE_append_name_string((char_t*) LVRM_LINE_DATA[str_data_idx].name);
		buffer_size = 0;
		// Reset result to error.
		NODE_flush_string_value();
		NODE_append_value_string((char_t*) NODE_ERROR_STRING);
		// Update register.
		status = XM_read_register((line_data_read -> node_addr), reg_addr, LVRM_REG_ERROR_VALUE[reg_addr], &(LVRM_REGISTERS[reg_addr]), read_status);
		if ((status != NODE_SUCCESS) || ((read_status -> all) != 0)) goto errors;
		// Compute field.
		field_value = DINFOX_read_field(LVRM_REGISTERS[reg_addr], LVRM_LINE_DATA[str_data_idx].read_field_mask);
		// Check index.
		switch (line_data_read -> line_data_index) {
		case LVRM_LINE_DATA_INDEX_RLST:
			// Specific print for boolean data.
			NODE_flush_string_value();
			switch (field_value) {
			case DINFOX_BIT_0:
				NODE_append_value_string("OFF");
				break;
			case DINFOX_BIT_1:
				NODE_append_value_string("ON");
				break;
			case DINFOX_BIT_FORCED_HARDWARE:
				NODE_append_value_string("HW");
				break;
			default:
				NODE_append_value_string("ERROR");
				break;
			}
			break;
		case LVRM_LINE_DATA_INDEX_VCOM:
		case LVRM_LINE_DATA_INDEX_VOUT:
			// Check error value.
			if (field_value != DINFOX_VOLTAGE_ERROR_VALUE) {
				// Convert to 5 digits string.
				string_status = STRING_value_to_5_digits_string(DINFOX_get_mv(field_value), (char_t*) field_str);
				STRING_exit_error(NODE_ERROR_BASE_STRING);
				// Add string.
				NODE_flush_string_value();
				NODE_append_value_string(field_str);
				// Add unit.
				NODE_append_value_string((char_t*) LVRM_LINE_DATA[str_data_idx].unit);
			}
			break;
		case LVRM_LINE_DATA_INDEX_IOUT:
			// Check error value.
			if (field_value != DINFOX_CURRENT_ERROR_VALUE) {
				// Convert to 5 digits string.
				string_status = STRING_value_to_5_digits_string(DINFOX_get_ua(field_value), (char_t*) field_str);
				STRING_exit_error(NODE_ERROR_BASE_STRING);
				// Add string.
				NODE_flush_string_value();
				NODE_append_value_string(field_str);
				// Add unit.
				NODE_append_value_string((char_t*) LVRM_LINE_DATA[str_data_idx].unit);
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
NODE_status_t LVRM_build_sigfox_ul_payload(NODE_ul_payload_t* node_ul_payload) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t write_status;
	XM_node_registers_t node_reg;
	XM_registers_list_t reg_list;
	LVRM_sigfox_payload_monitoring_t sigfox_payload_monitoring;
	LVRM_sigfox_payload_electrical_t sigfox_payload_electrical;
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
	// Build node registers structure.
	node_reg.value = (uint32_t*) LVRM_REGISTERS;
	node_reg.error = (uint32_t*) LVRM_REG_ERROR_VALUE;
	// Reset payload size.
	(*(node_ul_payload -> size)) = 0;
	// Check event driven payloads.
	status = COMMON_check_event_driven_payloads(node_ul_payload, &node_reg);
	if (status != NODE_SUCCESS) goto errors;
	// Directly exits if a common payload was computed.
	if ((*(node_ul_payload -> size)) > 0) goto errors;
	// Else use specific pattern of the node.
	switch (LVRM_SIGFOX_PAYLOAD_PATTERN[node_ul_payload -> node -> radio_transmission_count]) {
	case LVRM_SIGFOX_PAYLOAD_TYPE_MONITORING:
		// Build registers list.
		reg_list.addr_list = (uint8_t*) LVRM_REG_LIST_SIGFOX_PAYLOAD_MONITORING;
		reg_list.size = sizeof(LVRM_REG_LIST_SIGFOX_PAYLOAD_MONITORING);
		// Reset registers.
		status = XM_reset_registers(&reg_list, &node_reg);
		if (status != NODE_SUCCESS) goto errors;
		// Perform measurements.
		status = XM_perform_measurements((node_ul_payload -> node -> address), &write_status);
		if (status != NODE_SUCCESS) goto errors;
		// Check write status.
		if (write_status.all == 0) {
			// Read related registers.
			status = XM_read_registers((node_ul_payload -> node -> address), &reg_list, &node_reg);
			if (status != NODE_SUCCESS) goto errors;
		}
		// Build monitoring payload.
		sigfox_payload_monitoring.vmcu = DINFOX_read_field(LVRM_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0], COMMON_REG_ANALOG_DATA_0_MASK_VMCU);
		sigfox_payload_monitoring.tmcu = DINFOX_read_field(LVRM_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0], COMMON_REG_ANALOG_DATA_0_MASK_TMCU);
		// Copy payload.
		for (idx=0 ; idx<LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE ; idx++) {
			(node_ul_payload -> ul_payload)[idx] = sigfox_payload_monitoring.frame[idx];
		}
		(*(node_ul_payload -> size)) = LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE;
		break;
	case LVRM_SIGFOX_PAYLOAD_TYPE_ELECTRICAL:
		// Build registers list.
		reg_list.addr_list = (uint8_t*) LVRM_REG_LIST_SIGFOX_PAYLOAD_ELECTRICAL;
		reg_list.size = sizeof(LVRM_REG_LIST_SIGFOX_PAYLOAD_ELECTRICAL);
		// Reset registers.
		status = XM_reset_registers(&reg_list, &node_reg);
		if (status != NODE_SUCCESS) goto errors;
		// Perform measurements.
		status = XM_perform_measurements((node_ul_payload -> node -> address), &write_status);
		if (status != NODE_SUCCESS) goto errors;
		// Check write status.
		if (write_status.all == 0) {
			// Read related registers.
			status = XM_read_registers((node_ul_payload -> node -> address), &reg_list, &node_reg);
			if (status != NODE_SUCCESS) goto errors;
		}
		// Build data payload.
		sigfox_payload_electrical.vin = DINFOX_read_field(LVRM_REGISTERS[LVRM_REG_ADDR_ANALOG_DATA_1], LVRM_REG_ANALOG_DATA_1_MASK_VCOM);
		sigfox_payload_electrical.vout = DINFOX_read_field(LVRM_REGISTERS[LVRM_REG_ADDR_ANALOG_DATA_1], LVRM_REG_ANALOG_DATA_1_MASK_VOUT);
		sigfox_payload_electrical.iout = DINFOX_read_field(LVRM_REGISTERS[LVRM_REG_ADDR_ANALOG_DATA_2], LVRM_REG_ANALOG_DATA_2_MASK_IOUT);
		sigfox_payload_electrical.unused = 0;
		sigfox_payload_electrical.rlstst = DINFOX_read_field(LVRM_REGISTERS[LVRM_REG_ADDR_STATUS_1], LVRM_REG_STATUS_1_MASK_RLSTST);
		// Copy payload.
		for (idx=0 ; idx<LVRM_SIGFOX_PAYLOAD_ELECTRICAL_SIZE ; idx++) {
			(node_ul_payload -> ul_payload)[idx] = sigfox_payload_electrical.frame[idx];
		}
		(*(node_ul_payload -> size)) = LVRM_SIGFOX_PAYLOAD_ELECTRICAL_SIZE;
		break;
	default:
		status = NODE_ERROR_SIGFOX_PAYLOAD_TYPE;
		goto errors;
	}
	// Increment transmission count.
	(node_ul_payload -> node -> radio_transmission_count) = ((node_ul_payload -> node -> radio_transmission_count) + 1) % (sizeof(LVRM_SIGFOX_PAYLOAD_PATTERN));
errors:
	return status;
}

#ifdef DMM_BMS_ENABLE
/*******************************************************************/
NODE_status_t LVRM_bms_process(NODE_address_t lvrm_node_addr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t node_access_status = {.all = 0};
	DINFOX_voltage_representation_t vbatt_dinfox = 0;
	uint32_t vbatt_mv = 0;
	uint32_t reg_value = 0;
	// Perform measurements.
	status = XM_perform_measurements(lvrm_node_addr, &node_access_status);
	if (status != NODE_SUCCESS) goto errors;
	NODE_check_access_status(lvrm_node_addr);
	// Read battery voltage.
	status = XM_read_register(lvrm_node_addr, LVRM_REG_ADDR_ANALOG_DATA_1, LVRM_REG_ERROR_VALUE[LVRM_REG_ADDR_ANALOG_DATA_1], &reg_value, &node_access_status);
	if ((status != NODE_SUCCESS) || (node_access_status.all != 0)) goto errors;
	// Check error value.
	vbatt_dinfox = (uint16_t) DINFOX_read_field(reg_value, LVRM_REG_ANALOG_DATA_1_MASK_VCOM);
	if (vbatt_dinfox == DINFOX_VOLTAGE_ERROR_VALUE) goto errors;
	// Get battery voltage.
	vbatt_mv = DINFOX_get_mv(vbatt_dinfox);
	// Check battery voltage.
	if (vbatt_mv < DMM_BMS_VBATT_LOW_THRESHOLD_MV) {
		// Open relay.
		status = XM_write_register(lvrm_node_addr, LVRM_REG_ADDR_CONTROL_1, 0b0, LVRM_REG_CONTROL_1_MASK_RLST, AT_BUS_DEFAULT_TIMEOUT_MS, &node_access_status);
		if ((status != NODE_SUCCESS) || (node_access_status.all != 0)) goto errors;
	}
	if (vbatt_mv > DMM_BMS_VBATT_HIGH_THRESHOLD_MV) {
		// Close relay.
		status = XM_write_register(lvrm_node_addr, LVRM_REG_ADDR_CONTROL_1, 0b1, LVRM_REG_CONTROL_1_MASK_RLST, AT_BUS_DEFAULT_TIMEOUT_MS, &node_access_status);
		if ((status != NODE_SUCCESS) || (node_access_status.all != 0)) goto errors;
	}
errors:
	return status;
}
#endif
