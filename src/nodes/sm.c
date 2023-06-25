/*
 * sm.c
 *
 *  Created on: 4 mar. 2023
 *      Author: Ludo
 */

#include "sm.h"

#include "at_bus.h"
#include "sm_reg.h"
#include "common.h"
#include "dinfox.h"
#include "node.h"
#include "string.h"
#include "xm.h"

/*** SM local macros ***/

#define SM_SIGFOX_PAYLOAD_SENSOR_1_SIZE		9
#define SM_SIGFOX_PAYLOAD_SENSOR_2_SIZE		5

#define BPSM_SIGFOX_PAYLOAD_LOOP_MAX		10

/*** SM local structures ***/

typedef enum {
	SM_SIGFOX_PAYLOAD_TYPE_STARTUP = 0,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2,
	SM_SIGFOX_PAYLOAD_TYPE_ERROR_STACK,
	SM_SIGFOX_PAYLOAD_TYPE_LAST
} SM_sigfox_payload_type_t;

typedef union {
	uint8_t frame[SM_SIGFOX_PAYLOAD_SENSOR_1_SIZE];
	struct {
		unsigned ain0 : 16;
		unsigned ain1 : 16;
		unsigned ain2 : 16;
		unsigned ain3 : 16;
		unsigned unused : 4;
		unsigned dio3 : 1;
		unsigned dio2 : 1;
		unsigned dio1 : 1;
		unsigned dio0 : 1;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} SM_sigfox_payload_sensor_1_t;

typedef union {
	uint8_t frame[SM_SIGFOX_PAYLOAD_SENSOR_2_SIZE];
	struct {
		unsigned vmcu : 16;
		unsigned tmcu : 8;
		unsigned tamb : 8;
		unsigned hamb : 8;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} SM_sigfox_payload_sensor_2_t;

/*** SM local global variables ***/

static uint32_t SM_REGISTERS[SM_REG_ADDR_LAST];

static const NODE_line_data_t SM_LINE_DATA[SM_LINE_DATA_INDEX_LAST - COMMON_LINE_DATA_INDEX_LAST] = {
	{"AIN0 =", " V", STRING_FORMAT_DECIMAL, 0, SM_REG_ADDR_ANALOG_DATA_1, SM_REG_ANALOG_DATA_1_MASK_VAIN0},
	{"AIN1 =", " V", STRING_FORMAT_DECIMAL, 0, SM_REG_ADDR_ANALOG_DATA_1, SM_REG_ANALOG_DATA_1_MASK_VAIN1},
	{"AIN2 =", " V", STRING_FORMAT_DECIMAL, 0, SM_REG_ADDR_ANALOG_DATA_2, SM_REG_ANALOG_DATA_2_MASK_VAIN2},
	{"AIN3 =", " V", STRING_FORMAT_DECIMAL, 0, SM_REG_ADDR_ANALOG_DATA_2, SM_REG_ANALOG_DATA_2_MASK_VAIN3},
	{"DIO0 =", STRING_NULL, STRING_FORMAT_BOOLEAN, 0, SM_REG_ADDR_DIGITAL_DATA, SM_REG_DIGITAL_DATA_MASK_DIO0},
	{"DIO1 =", STRING_NULL, STRING_FORMAT_BOOLEAN, 0, SM_REG_ADDR_DIGITAL_DATA, SM_REG_DIGITAL_DATA_MASK_DIO1},
	{"DIO2 =", STRING_NULL, STRING_FORMAT_BOOLEAN, 0, SM_REG_ADDR_DIGITAL_DATA, SM_REG_DIGITAL_DATA_MASK_DIO2},
	{"DIO3 =", STRING_NULL, STRING_FORMAT_BOOLEAN, 0, SM_REG_ADDR_DIGITAL_DATA, SM_REG_DIGITAL_DATA_MASK_DIO3},
	{"TAMB =", " |C", STRING_FORMAT_DECIMAL, 0, SM_REG_ADDR_ANALOG_DATA_3, SM_REG_ANALOG_DATA_3_MASK_TAMB},
	{"HAMB =", " %", STRING_FORMAT_DECIMAL, 0, SM_REG_ADDR_ANALOG_DATA_3, SM_REG_ANALOG_DATA_3_MASK_HAMB}
};

static const uint32_t SM_REG_ERROR_VALUE[SM_REG_ADDR_LAST] = {
	COMMON_REG_ERROR_VALUE
	((DINFOX_VOLTAGE_ERROR_VALUE << 16) | (DINFOX_VOLTAGE_ERROR_VALUE << 0)),
	((DINFOX_VOLTAGE_ERROR_VALUE << 16) | (DINFOX_VOLTAGE_ERROR_VALUE << 0)),
	((DINFOX_HUMIDITY_ERROR_VALUE << 8) | (DINFOX_TEMPERATURE_ERROR_VALUE << 0)),
	0x00000000
};

static const uint8_t SM_REG_LIST_SIGFOX_PAYLOAD_SENSOR_1[] = {
	SM_REG_ADDR_ANALOG_DATA_1,
	SM_REG_ADDR_ANALOG_DATA_2,
	SM_REG_ADDR_DIGITAL_DATA
};

static const uint8_t SM_REG_LIST_SIGFOX_PAYLOAD_SENSOR_2[] = {
	COMMON_REG_ADDR_ANALOG_DATA_0,
	SM_REG_ADDR_ANALOG_DATA_3
};

static const SM_sigfox_payload_type_t SM_SIGFOX_PAYLOAD_PATTERN[] = {
	SM_SIGFOX_PAYLOAD_TYPE_STARTUP,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1,
	SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2,
	SM_SIGFOX_PAYLOAD_TYPE_ERROR_STACK
};

/*** SM functions ***/

/* WRITE SM DATA.
 * @param line_data_write:	Pointer to the data write structure.
 * @param read_status:		Pointer to the writing operation status.
 * @return status:			Function execution status.
 */
NODE_status_t SM_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Call common function with local data.
	status = XM_write_line_data(line_data_write, (NODE_line_data_t*) SM_LINE_DATA, (uint32_t*) SM_REG_WRITE_TIMEOUT_MS, write_status);
	return status;
}

/* READ SM DATA.
 * @param line_data_read:	Pointer to the data read structure.
 * @param read_status:		Pointer to the reading operation status.
 * @return status:			Function execution status.
 */
NODE_status_t SM_read_line_data(NODE_line_data_read_t* line_data_read, NODE_access_status_t* read_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	XM_node_registers_t node_reg;
	STRING_status_t string_status = STRING_SUCCESS;
	uint32_t field_value = 0;
	char_t field_str[STRING_DIGIT_FUNCTION_SIZE];
	uint8_t str_data_idx = 0;
	uint8_t reg_addr = 0;
	uint8_t buffer_size = 0;
	int32_t tamb = 0;
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
	if ((line_data_read -> line_data_index) >= SM_LINE_DATA_INDEX_LAST) {
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Build node registers structure.
	node_reg.value = (uint32_t*) SM_REGISTERS;
	node_reg.error = (uint32_t*) SM_REG_ERROR_VALUE;
	// Check common range.
	if ((line_data_read -> line_data_index) < COMMON_LINE_DATA_INDEX_LAST) {
		// Call common function.
		status = COMMON_read_line_data(line_data_read, &node_reg);
		if (status != NODE_SUCCESS) goto errors;
	}
	else {
		// Compute specific string data index and register address.
		str_data_idx = ((line_data_read -> line_data_index) - COMMON_LINE_DATA_INDEX_LAST);
		reg_addr = SM_LINE_DATA[str_data_idx].reg_addr;
		// Add data name.
		NODE_append_name_string((char_t*) SM_LINE_DATA[str_data_idx].name);
		buffer_size = 0;
		// Reset result to error.
		NODE_flush_string_value();
		NODE_append_value_string((char_t*) NODE_ERROR_STRING);
		// Update register.
		status = XM_read_register((line_data_read -> node_addr), reg_addr, SM_REG_ERROR_VALUE[reg_addr], &(SM_REGISTERS[reg_addr]), read_status);
		if ((status != NODE_SUCCESS) || ((read_status -> all) != 0)) goto errors;
		// Compute field.
		field_value = DINFOX_read_field(SM_REGISTERS[reg_addr], SM_LINE_DATA[str_data_idx].field_mask);
		// Check index.
		switch (line_data_read -> line_data_index) {
		case SM_LINE_DATA_INDEX_DIO0:
		case SM_LINE_DATA_INDEX_DIO1:
		case SM_LINE_DATA_INDEX_DIO2:
		case SM_LINE_DATA_INDEX_DIO3:
			// Specific print for boolean data.
			NODE_flush_string_value();
			NODE_append_value_int32(field_value, SM_LINE_DATA[str_data_idx].print_format, SM_LINE_DATA[str_data_idx].print_prefix);
			break;
		case SM_LINE_DATA_INDEX_HAMB:
			// Specific print for humidity.
			NODE_flush_string_value();
			NODE_append_value_int32(field_value, SM_LINE_DATA[str_data_idx].print_format, SM_LINE_DATA[str_data_idx].print_prefix);
			// Add unit.
			NODE_append_value_string((char_t*) SM_LINE_DATA[str_data_idx].unit);
			break;
		case SM_LINE_DATA_INDEX_AIN0:
		case SM_LINE_DATA_INDEX_AIN1:
		case SM_LINE_DATA_INDEX_AIN2:
		case SM_LINE_DATA_INDEX_AIN3:
			// Check error value.
			if (field_value != DINFOX_VOLTAGE_ERROR_VALUE) {
				// Convert to 5 digits string.
				string_status = STRING_value_to_5_digits_string(DINFOX_get_mv(field_value), (char_t*) field_str);
				STRING_status_check(NODE_ERROR_BASE_STRING);
				// Add string.
				NODE_flush_string_value();
				NODE_append_value_string(field_str);
				// Add unit.
				NODE_append_value_string((char_t*) SM_LINE_DATA[str_data_idx].unit);
			}
			break;
		case SM_LINE_DATA_INDEX_TAMB:
			// Check error value.
			if (field_value != DINFOX_TEMPERATURE_ERROR_VALUE) {
				// Convert temperature.
				tamb = (int32_t) DINFOX_get_degrees(field_value);
				// Add string.
				NODE_flush_string_value();
				NODE_append_value_int32(tamb, SM_LINE_DATA[str_data_idx].print_format, SM_LINE_DATA[str_data_idx].print_prefix);
				// Add unit.
				NODE_append_value_string((char_t*) SM_LINE_DATA[str_data_idx].unit);
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

/* UPDATE SM NODE SIGFOX UPLINK PAYLOAD.
 * @param ul_payload_update:	Pointer to the UL payload update structure.
 * @return status:				Function execution status.
 */
NODE_status_t SM_build_sigfox_ul_payload(NODE_ul_payload_update_t* ul_payload_update) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t write_status;
	XM_node_registers_t node_reg;
	XM_registers_list_t reg_list;
	SM_sigfox_payload_sensor_1_t sigfox_payload_sensor_1;
	SM_sigfox_payload_sensor_2_t sigfox_payload_sensor_2;
	uint8_t idx = 0;
	uint32_t loop_count = 0;
	// Check parameters.
	if (ul_payload_update == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (((ul_payload_update -> ul_payload) == NULL) || ((ul_payload_update -> size) == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Build node registers structure.
	node_reg.value = (uint32_t*) SM_REGISTERS;
	node_reg.error = (uint32_t*) SM_REG_ERROR_VALUE;
	// Reset payload size.
	(*(ul_payload_update -> size)) = 0;
	// Main loop.
	do {
		// Check payload type.
		switch (SM_SIGFOX_PAYLOAD_PATTERN[ul_payload_update -> node -> radio_transmission_count]) {
		case SM_SIGFOX_PAYLOAD_TYPE_STARTUP:
			// Check flag.
			if ((ul_payload_update -> node -> startup_data_sent) == 0) {
				// Use common format.
				status = COMMON_build_sigfox_payload_startup(ul_payload_update, &node_reg);
				if (status != NODE_SUCCESS) goto errors;
				// Update flag.
				(ul_payload_update -> node -> startup_data_sent) = 1;
			}
			break;
		case SM_SIGFOX_PAYLOAD_TYPE_ERROR_STACK:
			// Use common format.
			status = COMMON_build_sigfox_payload_error_stack(ul_payload_update, &node_reg);
			if (status != NODE_SUCCESS) goto errors;
			break;
		case SM_SIGFOX_PAYLOAD_TYPE_SENSOR_1:
			// Build registers list.
			reg_list.addr_list = (uint8_t*) SM_REG_LIST_SIGFOX_PAYLOAD_SENSOR_1;
			reg_list.size = sizeof(SM_REG_LIST_SIGFOX_PAYLOAD_SENSOR_1);
			// Reset registers.
			status = XM_reset_registers(&reg_list, &node_reg);
			if (status != NODE_SUCCESS) goto errors;
			// Perform measurements.
			status = XM_perform_measurements((ul_payload_update -> node -> address), &write_status);
			if (status != NODE_SUCCESS) goto errors;
			// Check write status.
			if (write_status.all == 0) {
				// Read related registers.
				status = XM_read_registers((ul_payload_update -> node -> address), &reg_list, &node_reg);
				if (status != NODE_SUCCESS) goto errors;
			}
			// Build data payload.
			sigfox_payload_sensor_1.ain0 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_ANALOG_DATA_1], SM_REG_ANALOG_DATA_1_MASK_VAIN0);
			sigfox_payload_sensor_1.ain1 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_ANALOG_DATA_1], SM_REG_ANALOG_DATA_1_MASK_VAIN1);
			sigfox_payload_sensor_1.ain2 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_ANALOG_DATA_2], SM_REG_ANALOG_DATA_2_MASK_VAIN2);
			sigfox_payload_sensor_1.ain3 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_ANALOG_DATA_2], SM_REG_ANALOG_DATA_2_MASK_VAIN3);
			sigfox_payload_sensor_1.unused = 0;
			sigfox_payload_sensor_1.dio0 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_DIGITAL_DATA], SM_REG_DIGITAL_DATA_MASK_DIO0);
			sigfox_payload_sensor_1.dio1 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_DIGITAL_DATA], SM_REG_DIGITAL_DATA_MASK_DIO1);
			sigfox_payload_sensor_1.dio2 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_DIGITAL_DATA], SM_REG_DIGITAL_DATA_MASK_DIO2);
			sigfox_payload_sensor_1.dio3 = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_DIGITAL_DATA], SM_REG_DIGITAL_DATA_MASK_DIO3);
			// Copy payload.
			for (idx=0 ; idx<SM_SIGFOX_PAYLOAD_SENSOR_1_SIZE ; idx++) {
				(ul_payload_update -> ul_payload)[idx] = sigfox_payload_sensor_1.frame[idx];
			}
			(*(ul_payload_update -> size)) = SM_SIGFOX_PAYLOAD_SENSOR_1_SIZE;
			break;
		case SM_SIGFOX_PAYLOAD_TYPE_SENSOR_2:
			// Build registers list.
			reg_list.addr_list = (uint8_t*) SM_REG_LIST_SIGFOX_PAYLOAD_SENSOR_2;
			reg_list.size = sizeof(SM_REG_LIST_SIGFOX_PAYLOAD_SENSOR_2);
			// Reset registers.
			status = XM_reset_registers(&reg_list, &node_reg);
			if (status != NODE_SUCCESS) goto errors;
			// Perform measurements.
			status = XM_perform_measurements((ul_payload_update -> node -> address), &write_status);
			if (status != NODE_SUCCESS) goto errors;
			// Check write status.
			if (write_status.all == 0) {
				// Read related registers.
				status = XM_read_registers((ul_payload_update -> node -> address), &reg_list, &node_reg);
				if (status != NODE_SUCCESS) goto errors;
			}
			// Build monitoring payload.
			sigfox_payload_sensor_2.vmcu = DINFOX_read_field(SM_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0], COMMON_REG_ANALOG_DATA_0_MASK_VMCU);
			sigfox_payload_sensor_2.tmcu = DINFOX_read_field(SM_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0], COMMON_REG_ANALOG_DATA_0_MASK_TMCU);
			sigfox_payload_sensor_2.tamb = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_ANALOG_DATA_3], SM_REG_ANALOG_DATA_3_MASK_TAMB);
			sigfox_payload_sensor_2.hamb = DINFOX_read_field(SM_REGISTERS[SM_REG_ADDR_ANALOG_DATA_3], SM_REG_ANALOG_DATA_3_MASK_HAMB);
			// Copy payload.
			for (idx=0 ; idx<SM_SIGFOX_PAYLOAD_SENSOR_2_SIZE ; idx++) {
				(ul_payload_update -> ul_payload)[idx] = sigfox_payload_sensor_2.frame[idx];
			}
			(*(ul_payload_update -> size)) = SM_SIGFOX_PAYLOAD_SENSOR_2_SIZE;
			break;
		default:
			status = NODE_ERROR_SIGFOX_PAYLOAD_TYPE;
			goto errors;
		}
		// Increment transmission count.
		(ul_payload_update -> node -> radio_transmission_count) = ((ul_payload_update -> node -> radio_transmission_count) + 1) % (sizeof(SM_SIGFOX_PAYLOAD_PATTERN));
		// Exit in case of loop error.
		loop_count++;
		if (loop_count > BPSM_SIGFOX_PAYLOAD_LOOP_MAX) {
			status = NODE_ERROR_SIGFOX_PAYLOAD_LOOP;
			goto errors;
		}
	}
	while ((*(ul_payload_update -> size)) == 0);
errors:
	return status;
}
