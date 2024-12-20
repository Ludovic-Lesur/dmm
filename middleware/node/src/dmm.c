/*
 * dmm.c
 *
 *  Created on: 04 mar. 2023
 *      Author: Ludo
 */

#include "dmm.h"

#include "dmm_reg.h"
#include "common.h"
#include "dinfox.h"
#include "error.h"
#include "gpio.h"
#include "mapping.h"
#include "mode.h"
#include "node.h"
#include "node_common.h"
#include "power.h"
#include "pwr.h"
#include "rcc_reg.h"
#include "string.h"
#include "version.h"
#include "xm.h"

/*** DMM local macros ***/

#define DMM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE	7

#define DMM_SIGFOX_UL_PERIOD_SECONDS_MIN		60
#define DMM_SIGFOX_DL_PERIOD_SECONDS_MIN		600
#define DMM_NODES_SCAN_PERIOD_SECONDS_MIN		3600

/*** DMM local structures ***/

/*******************************************************************/
typedef enum {
	DMM_SIGFOX_UL_PAYLOAD_TYPE_MONITORING = 0,
	DMM_SIGFOX_UL_PAYLOAD_TYPE_LAST
} DMM_sigfox_ul_payload_type_t;

/*******************************************************************/
typedef union {
	uint8_t frame[DMM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE];
	struct {
		unsigned vrs : 16;
		unsigned vhmi : 16;
		unsigned vusb : 16;
		unsigned nodes_count : 8;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} DMM_sigfox_ul_payload_monitoring_t;

/*** DMM global variables ***/

const uint32_t DMM_REG_WRITE_TIMEOUT_MS[DMM_REG_ADDR_LAST] = {
	COMMON_REG_WRITE_TIMEOUT_MS_LIST
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS,
	AT_BUS_DEFAULT_TIMEOUT_MS
};

/*** DMM local global variables ***/

static uint32_t DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_LAST];

static uint32_t DMM_REGISTERS[DMM_REG_ADDR_LAST];

static const uint32_t DMM_REG_ERROR_VALUE[DMM_REG_ADDR_LAST] = {
	COMMON_REG_ERROR_VALUE_LIST
	((DINFOX_TIME_ERROR_VALUE << 16) | (DINFOX_TIME_ERROR_VALUE << 8) | (DINFOX_TIME_ERROR_VALUE << 0)),
	0x00000000,
	0x00000000,
	(uint32_t) ((DINFOX_VOLTAGE_ERROR_VALUE << 16) | (DINFOX_VOLTAGE_ERROR_VALUE << 0)),
	(DINFOX_VOLTAGE_ERROR_VALUE << 0)
};

static const XM_node_registers_t DMM_NODE_REGISTERS = {
	.value = (uint32_t*) DMM_REGISTERS,
	.error = (uint32_t*) DMM_REG_ERROR_VALUE,
};

static const NODE_line_data_t DMM_LINE_DATA[DMM_LINE_DATA_INDEX_LAST - COMMON_LINE_DATA_INDEX_LAST] = {
	{"VRS =", " V", STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_ANALOG_DATA_1, DMM_REG_ANALOG_DATA_1_MASK_VRS,   DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"VHMI =", " V", STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_ANALOG_DATA_1, DMM_REG_ANALOG_DATA_1_MASK_VHMI, DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"VUSB =", " V", STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_ANALOG_DATA_2, DMM_REG_ANALOG_DATA_2_MASK_VUSB, DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"NODES_CNT =", STRING_NULL, STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_STATUS_1, DMM_REG_STATUS_1_MASK_NODES_COUNT, DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"SC_PRD =", " s", STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_CONFIGURATION_0, DMM_REG_CONFIGURATION_0_MASK_NODES_SCAN_PERIOD, DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"UL_PRD =", " s", STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_CONFIGURATION_0, DMM_REG_CONFIGURATION_0_MASK_SIGFOX_UL_PERIOD,  DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE},
	{"DL_PRD =", " s", STRING_FORMAT_DECIMAL, 0, DMM_REG_ADDR_CONFIGURATION_0, DMM_REG_CONFIGURATION_0_MASK_SIGFOX_DL_PERIOD,  DMM_REG_ADDR_CONTROL_1, DINFOX_REG_MASK_NONE}
};

static const uint8_t DMM_REG_LIST_SIGFOX_UL_PAYLOAD_MONITORING[] = {
	DMM_REG_ADDR_STATUS_1,
	DMM_REG_ADDR_ANALOG_DATA_1,
	DMM_REG_ADDR_ANALOG_DATA_2
};

static const DINFOX_register_access_t DMM_REG_ACCESS[DMM_REG_ADDR_LAST] = {
	COMMON_REG_ACCESS
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_WRITE,
	DINFOX_REG_ACCESS_READ_WRITE,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY
};

static const DMM_sigfox_ul_payload_type_t DMM_SIGFOX_UL_PAYLOAD_PATTERN[] = {
	DMM_SIGFOX_UL_PAYLOAD_TYPE_MONITORING
};

/*** DMM local functions ***/

/*******************************************************************/
static void _DMM_load_dynamic_configuration(void) {
	// Local variables.
	uint8_t reg_addr = 0;
	uint32_t reg_value = 0;
	// Load configuration registers from NVM.
	for (reg_addr=DMM_REG_ADDR_CONFIGURATION_0 ; reg_addr<DMM_REG_ADDR_STATUS_1 ; reg_addr++) {
		// Read NVM.
		reg_value = DINFOX_read_nvm_register(reg_addr);
		// Write register.
		DMM_INTERNAL_REGISTERS[reg_addr] = reg_value;
	}
}

/*******************************************************************/
static void _DMM_reset_analog_data(void) {
	// Local variables.
	uint32_t unused_mask = 0;
	// Reset to error value.
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0]), &unused_mask, DINFOX_VOLTAGE_ERROR_VALUE, COMMON_REG_ANALOG_DATA_0_MASK_VMCU);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0]), &unused_mask, DINFOX_TEMPERATURE_ERROR_VALUE, COMMON_REG_ANALOG_DATA_0_MASK_TMCU);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_1]), &unused_mask, DINFOX_VOLTAGE_ERROR_VALUE, DMM_REG_ANALOG_DATA_1_MASK_VRS);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_1]), &unused_mask, DINFOX_VOLTAGE_ERROR_VALUE, DMM_REG_ANALOG_DATA_1_MASK_VHMI);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_2]), &unused_mask, DINFOX_VOLTAGE_ERROR_VALUE, DMM_REG_ANALOG_DATA_2_MASK_VUSB);
}

/*******************************************************************/
static NODE_status_t _DMM_mtrg_callback(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	ADC_status_t adc1_status = ADC_SUCCESS;
	POWER_status_t power_status = POWER_SUCCESS;
	uint32_t adc_data = 0;
	uint32_t unused_mask = 0;
	int8_t tmcu_degrees = 0;
	uint8_t hmi_on = 0;
	// Reset results.
	_DMM_reset_analog_data();
	// Check HMI power status.
	hmi_on = GPIO_read(&GPIO_HMI_POWER_ENABLE);
	// Turn HMI on.
	if (hmi_on == 0) {
		power_status = POWER_enable(POWER_DOMAIN_HMI, LPTIM_DELAY_MODE_STOP);
		POWER_exit_error(NODE_ERROR_BASE_POWER);
	}
	// Init ADC.
	power_status = POWER_enable(POWER_DOMAIN_ANALOG, LPTIM_DELAY_MODE_ACTIVE);
	POWER_exit_error(NODE_ERROR_BASE_POWER);
	// Perform analog measurements.
	adc1_status = ADC1_perform_measurements();
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	// Release ADC.
	power_status = POWER_disable(POWER_DOMAIN_ANALOG);
	POWER_exit_error(NODE_ERROR_BASE_POWER);
	// Disable HMI power supply.
	if (hmi_on == 0) {
		power_status = POWER_disable(POWER_DOMAIN_HMI);
		POWER_exit_error(NODE_ERROR_BASE_POWER);
	}
	// VMCU.
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VMCU_MV, &adc_data);
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0]), &unused_mask, DINFOX_convert_mv(adc_data), COMMON_REG_ANALOG_DATA_0_MASK_VMCU);
	// TMCU.
	adc1_status = ADC1_get_tmcu(&tmcu_degrees);
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_ANALOG_DATA_0]), &unused_mask, DINFOX_convert_degrees(tmcu_degrees), COMMON_REG_ANALOG_DATA_0_MASK_TMCU);
	// VRS.
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VRS_MV, &adc_data);
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_1]), &unused_mask, DINFOX_convert_mv(adc_data), DMM_REG_ANALOG_DATA_1_MASK_VRS);
	// VHMI.
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VHMI_MV, &adc_data);
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_1]), &unused_mask, DINFOX_convert_mv(adc_data), DMM_REG_ANALOG_DATA_1_MASK_VHMI);
	// VUSB.
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VUSB_MV, &adc_data);
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_2]), &unused_mask, DINFOX_convert_mv(adc_data), DMM_REG_ANALOG_DATA_2_MASK_VUSB);
errors:
	// Turn power block off.
	if (hmi_on == 0) {
		POWER_disable(POWER_DOMAIN_HMI);
	}
	POWER_disable(POWER_DOMAIN_ANALOG);
	return status;
}

/*******************************************************************/
static NODE_status_t _DMM_update_register(uint8_t reg_addr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint32_t unused_mask = 0;
	// Check address.
	switch (reg_addr) {
	case COMMON_REG_ADDR_ERROR_STACK:
		// Unstack error.
		DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_ERROR_STACK]), &unused_mask, (uint32_t) (ERROR_stack_read()), COMMON_REG_ERROR_STACK_MASK_ERROR);
		break;
	case COMMON_REG_ADDR_STATUS_0:
		// Check error stack.
		DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_STATUS_0]), &unused_mask, ((ERROR_stack_is_empty() == 0) ? 0b1 : 0b0), COMMON_REG_STATUS_0_MASK_ESF);
		break;
	case DMM_REG_ADDR_STATUS_1:
		// Update nodes count.
		DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_STATUS_1]), &unused_mask, (uint32_t) (NODES_LIST.count), DMM_REG_STATUS_1_MASK_NODES_COUNT);
		break;
	default:
		// Nothing to do on other registers.
		break;
	}
	return status;
}

/*******************************************************************/
static NODE_status_t _DMM_check_register(uint8_t reg_addr, uint32_t reg_mask) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint32_t reg_value = 0;
	uint32_t unused_mask = 0;
	uint32_t duration = 0;
	// Read register.
	reg_value = DMM_INTERNAL_REGISTERS[reg_addr];
	// Check address.
	switch (reg_addr) {
	case DMM_REG_ADDR_CONFIGURATION_0:
		// Check nodes scan period.
		if ((reg_mask & DMM_REG_CONFIGURATION_0_MASK_NODES_SCAN_PERIOD) != 0) {
			// Check range.
			duration = DINFOX_get_seconds((DINFOX_time_representation_t) DINFOX_read_field(reg_value, DMM_REG_CONFIGURATION_0_MASK_NODES_SCAN_PERIOD));
			if (duration < DMM_NODES_SCAN_PERIOD_SECONDS_MIN) {
				// Force minimum value and notify error.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONFIGURATION_0]), &unused_mask, DINFOX_convert_seconds(DMM_NODES_SCAN_PERIOD_SECONDS_MIN), DMM_REG_CONFIGURATION_0_MASK_NODES_SCAN_PERIOD);
				status = NODE_ERROR_REGISTER_FIELD_RANGE;
			}
		}
		// Check Sigfox uplink period.
		if ((reg_mask & DMM_REG_CONFIGURATION_0_MASK_SIGFOX_UL_PERIOD) != 0) {
			// Check range.
			duration = DINFOX_get_seconds((DINFOX_time_representation_t) DINFOX_read_field(reg_value, DMM_REG_CONFIGURATION_0_MASK_SIGFOX_UL_PERIOD));
			if (duration < DMM_SIGFOX_UL_PERIOD_SECONDS_MIN) {
				// Force minimum value and notify error.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONFIGURATION_0]), &unused_mask, DINFOX_convert_seconds(DMM_SIGFOX_UL_PERIOD_SECONDS_MIN), DMM_REG_CONFIGURATION_0_MASK_SIGFOX_UL_PERIOD);
				status = NODE_ERROR_REGISTER_FIELD_RANGE;
			}
		}
		// Check Sigfox downlink period.
		if ((reg_mask & DMM_REG_CONFIGURATION_0_MASK_SIGFOX_DL_PERIOD) != 0) {
			// Check range.
			duration = DINFOX_get_seconds((DINFOX_time_representation_t) DINFOX_read_field(reg_value, DMM_REG_CONFIGURATION_0_MASK_SIGFOX_DL_PERIOD));
			if (duration < DMM_SIGFOX_DL_PERIOD_SECONDS_MIN) {
				// Force minimum value and notify error.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONFIGURATION_0]), &unused_mask, DINFOX_convert_seconds(DMM_SIGFOX_DL_PERIOD_SECONDS_MIN), DMM_REG_CONFIGURATION_0_MASK_SIGFOX_DL_PERIOD);
				status = NODE_ERROR_REGISTER_FIELD_RANGE;
			}
		}
		// Store new value in NVM.
		if (reg_mask != 0) {
			DINFOX_write_nvm_register(reg_addr, reg_value);
		}
		break;
	case COMMON_REG_ADDR_CONTROL_0:
		// RTRG.
		if ((reg_mask & COMMON_REG_CONTROL_0_MASK_RTRG) != 0) {
			// Read bit.
			if (DINFOX_read_field(reg_value, COMMON_REG_CONTROL_0_MASK_RTRG) != 0) {
				// Reset MCU.
				PWR_software_reset();
			}
		}
		// MTRG.
		if ((reg_mask & COMMON_REG_CONTROL_0_MASK_MTRG) != 0) {
			// Read bit.
			if (DINFOX_read_field(reg_value, COMMON_REG_CONTROL_0_MASK_MTRG) != 0) {
				// Clear request.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_CONTROL_0]), &unused_mask, 0b0, COMMON_REG_CONTROL_0_MASK_MTRG);
				// Perform measurements.
				status = _DMM_mtrg_callback();
				if (status != NODE_SUCCESS) goto errors;
			}
		}
		// BFC.
		if ((reg_mask & COMMON_REG_CONTROL_0_MASK_BFC) != 0) {
			// Read bit.
			if (DINFOX_read_field(reg_value, COMMON_REG_CONTROL_0_MASK_BFC) != 0) {
				// Clear request.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_CONTROL_0]), &unused_mask, 0b0, COMMON_REG_CONTROL_0_MASK_BFC);
				// Clear boot flag.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_STATUS_0]), &unused_mask, 0b0, COMMON_REG_STATUS_0_MASK_BF);
			}
		}
		break;
	case DMM_REG_ADDR_CONTROL_1:
		// STRG.
		if ((reg_mask & DMM_REG_CONTROL_1_MASK_STRG) != 0) {
			// Read bit.
			if (DINFOX_read_field(reg_value, DMM_REG_CONTROL_1_MASK_STRG) != 0) {
				// Clear request.
				DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONTROL_1]), &unused_mask, 0b0, DMM_REG_CONTROL_1_MASK_STRG);
				// Perform node scanning.
				status = NODE_scan();
				if (status != NODE_SUCCESS) goto errors;
			}
		}
		break;
	default:
		// Nothing to do for other registers.
		break;
	}
errors:
	return status;
}

/*** DMM functions ***/

/*******************************************************************/
void DMM_init_registers(void) {
	// Local variables.
	uint8_t idx = 0;
	uint32_t unused_mask = 0;
#ifdef NVM_FACTORY_RESET
	// Radio monitoring and node scanning periods.
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONFIGURATION_0]), &unused_mask, DINFOX_convert_seconds(DMM_NODES_SCAN_PERIOD_SECONDS), DMM_REG_CONFIGURATION_0_MASK_NODES_SCAN_PERIOD);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONFIGURATION_0]), &unused_mask, DINFOX_convert_seconds(DMM_SIGFOX_UL_PERIOD_SECONDS),  DMM_REG_CONFIGURATION_0_MASK_SIGFOX_UL_PERIOD);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[DMM_REG_ADDR_CONFIGURATION_0]), &unused_mask, DINFOX_convert_seconds(DMM_SIGFOX_DL_PERIOD_SECONDS),  DMM_REG_CONFIGURATION_0_MASK_SIGFOX_DL_PERIOD);
	_DMM_check_register(DMM_REG_ADDR_CONFIGURATION_0, DINFOX_REG_MASK_ALL);
#endif
	// Reset all registers.
	for (idx=0 ; idx<DMM_REG_ADDR_LAST ; idx++) {
		DMM_INTERNAL_REGISTERS[idx] = 0;
	}
	// Node ID register.
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_NODE_ID]), &unused_mask, DINFOX_NODE_ADDRESS_DMM, COMMON_REG_NODE_ID_MASK_NODE_ADDR);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_NODE_ID]), &unused_mask, DINFOX_BOARD_ID_DMM, COMMON_REG_NODE_ID_MASK_BOARD_ID);
	// HW version register.
#ifdef HW1_0
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_HW_VERSION]), &unused_mask, 1, COMMON_REG_HW_VERSION_MASK_MAJOR);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_HW_VERSION]), &unused_mask, 0, COMMON_REG_HW_VERSION_MASK_MINOR);
#endif
	// SW version register 0.
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_SW_VERSION_0]), &unused_mask, GIT_MAJOR_VERSION, COMMON_REG_SW_VERSION_0_MASK_MAJOR);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_SW_VERSION_0]), &unused_mask, GIT_MINOR_VERSION, COMMON_REG_SW_VERSION_0_MASK_MINOR);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_SW_VERSION_0]), &unused_mask, GIT_COMMIT_INDEX, COMMON_REG_SW_VERSION_0_MASK_COMMIT_INDEX);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_SW_VERSION_0]), &unused_mask, GIT_DIRTY_FLAG, COMMON_REG_SW_VERSION_0_MASK_DTYF);
	// SW version register 1.
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_SW_VERSION_1]), &unused_mask, GIT_COMMIT_ID, COMMON_REG_SW_VERSION_1_MASK_COMMIT_ID);
	// Reset flags registers.
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_STATUS_0]), &unused_mask, ((uint32_t) (((RCC -> CSR) >> 24) & 0xFF)), COMMON_REG_STATUS_0_MASK_RESET_FLAGS);
	DINFOX_write_field(&(DMM_INTERNAL_REGISTERS[COMMON_REG_ADDR_STATUS_0]), &unused_mask, 0b1, COMMON_REG_STATUS_0_MASK_BF);
	// Load default values.
	_DMM_load_dynamic_configuration();
	_DMM_reset_analog_data();
}

/*******************************************************************/
NODE_status_t DMM_write_register(NODE_access_parameters_t* write_params, uint32_t reg_value, uint32_t reg_mask, NODE_access_status_t* write_status, uint8_t access_error_stack) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_status_t node_status = NODE_SUCCESS;
	uint32_t temp = 0;
	// Check parameters.
	if ((write_params == NULL) || (write_status == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset access status.
	(write_status -> all) = 0;
	(write_status -> type) = NODE_ACCESS_TYPE_WRITE;
	// Check address.
	if ((write_params -> reg_addr) >= DMM_REG_ADDR_LAST) {
		// Act as a slave.
		(*write_status).error_received = 1;
		goto errors;
	}
	if (DMM_REG_ACCESS[(write_params -> reg_addr)] == DINFOX_REG_ACCESS_READ_ONLY) {
		// Act as a slave.
		(*write_status).error_received = 1;
		goto errors;
	}
	if ((write_params -> node_addr) != DINFOX_NODE_ADDRESS_DMM) {
		// Act as a slave.
		(*write_status).reply_timeout = 1;
		goto errors;
	}
	// Read register.
	temp = DMM_INTERNAL_REGISTERS[(write_params -> reg_addr)];
	// Compute new value.
	temp &= ~reg_mask;
	temp |= (reg_value & reg_mask);
	// Write register.
	DMM_INTERNAL_REGISTERS[(write_params -> reg_addr)] = temp;
	// Check control bits.
	node_status = _DMM_check_register((write_params -> reg_addr), reg_mask);
	if (node_status != NODE_SUCCESS) {
		// Act as a slave.
		NODE_stack_error();
		(*write_status).error_received = 1;
		goto errors;
	}
errors:
	// Store eventual access status error.
	if (((write_status -> flags) != 0) && (access_error_stack != 0)) {
		ERROR_stack_add(ERROR_BASE_NODE + NODE_ERROR_BASE_ACCESS_STATUS_CODE + (write_status -> all));
		ERROR_stack_add(ERROR_BASE_NODE + NODE_ERROR_BASE_ACCESS_STATUS_ADDRESS + (write_params -> node_addr));
	}
	return status;
}

/*******************************************************************/
NODE_status_t DMM_read_register(NODE_access_parameters_t* read_params, uint32_t* reg_value, NODE_access_status_t* read_status, uint8_t access_error_stack) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_status_t node_status = NODE_SUCCESS;
	// Check parameters.
	if ((read_params == NULL) || (read_status == NULL) || (reg_value == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Reset access status.
	(read_status -> all) = 0;
	(read_status -> type) = NODE_ACCESS_TYPE_READ;
	// Check address.
	if ((read_params -> reg_addr) >= DMM_REG_ADDR_LAST) {
		// Act as a slave.
		(*read_status).error_received = 1;
		goto errors;
	}
	if ((read_params -> node_addr) != DINFOX_NODE_ADDRESS_DMM) {
		// Act as a slave.
		(*read_status).reply_timeout = 1;
		goto errors;
	}
	// Reset value.
	(*reg_value) = DMM_REG_ERROR_VALUE[(read_params -> reg_addr)];
	// Update register.
	node_status = _DMM_update_register(read_params -> reg_addr);
	if (node_status != NODE_SUCCESS) {
		// Act as a slave.
		NODE_stack_error();
		(*read_status).error_received = 1;
		goto errors;
	}
	// Read register.
	(*reg_value) = DMM_INTERNAL_REGISTERS[(read_params -> reg_addr)];
errors:
	// Store eventual access status error.
	if (((read_status -> flags) != 0) && (access_error_stack != 0)) {
		ERROR_stack_add(ERROR_BASE_NODE + NODE_ERROR_BASE_ACCESS_STATUS_CODE + (read_status -> all));
		ERROR_stack_add(ERROR_BASE_NODE + NODE_ERROR_BASE_ACCESS_STATUS_ADDRESS + (read_params -> node_addr));
	}
	return status;
}

/*******************************************************************/
NODE_status_t DMM_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameters.
	if ((line_data_write == NULL) || (write_status == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Check index.
	if ((line_data_write -> line_data_index) >= DMM_LINE_DATA_INDEX_LAST) {
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
		status = XM_write_line_data(line_data_write, (NODE_line_data_t*) DMM_LINE_DATA, (uint32_t*) DMM_REG_WRITE_TIMEOUT_MS, write_status);
	}
errors:
	return status;
}

/*******************************************************************/
NODE_status_t DMM_read_line_data(NODE_line_data_read_t* line_data_read, NODE_access_status_t* read_status) {
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
	if ((line_data_read -> line_data_index) >= DMM_LINE_DATA_INDEX_LAST) {
		status = NODE_ERROR_LINE_DATA_INDEX;
		goto errors;
	}
	// Build node registers structure.
	node_reg.value = (uint32_t*) DMM_REGISTERS;
	node_reg.error = (uint32_t*) DMM_REG_ERROR_VALUE;
	// Check common range.
	if ((line_data_read -> line_data_index) < COMMON_LINE_DATA_INDEX_LAST) {
		// Call common function.
		status = COMMON_read_line_data(line_data_read, &node_reg, read_status);
		if (status != NODE_SUCCESS) goto errors;
	}
	else {
		// Compute specific string data index and register address.
		str_data_idx = ((line_data_read -> line_data_index) - COMMON_LINE_DATA_INDEX_LAST);
		reg_addr = DMM_LINE_DATA[str_data_idx].read_reg_addr;
		// Add data name.
		NODE_append_name_string((char_t*) DMM_LINE_DATA[str_data_idx].name);
		buffer_size = 0;
		// Reset result to error.
		NODE_flush_string_value();
		NODE_append_value_string((char_t*) NODE_ERROR_STRING);
		// Update register.
		status = XM_read_register((line_data_read -> node_addr), reg_addr, (XM_node_registers_t*) &DMM_NODE_REGISTERS, read_status);
		if ((status != NODE_SUCCESS) || ((read_status -> flags) != 0)) goto errors;
		// Compute field.
		field_value = DINFOX_read_field(DMM_REGISTERS[reg_addr], DMM_LINE_DATA[str_data_idx].read_field_mask);
		// Check index.
		switch (line_data_read -> line_data_index) {
		case DMM_LINE_DATA_INDEX_VRS:
		case DMM_LINE_DATA_INDEX_VHMI:
		case DMM_LINE_DATA_INDEX_VUSB:
			// Check error value.
			if (field_value != DINFOX_VOLTAGE_ERROR_VALUE) {
				// Convert to 5 digits string.
				string_status = STRING_value_to_5_digits_string((int32_t) (DINFOX_get_mv((DINFOX_voltage_representation_t) field_value)), (char_t*) field_str);
				STRING_exit_error(NODE_ERROR_BASE_STRING);
				// Add string.
				NODE_flush_string_value();
				NODE_append_value_string(field_str);
				// Add unit.
				NODE_append_value_string((char_t*) DMM_LINE_DATA[str_data_idx].unit);
			}
			break;
		case DMM_LINE_DATA_INDEX_NODES_COUNT:
			// Raw value.
			NODE_flush_string_value();
			NODE_append_value_int32(field_value,  DMM_LINE_DATA[str_data_idx].print_format,  DMM_LINE_DATA[str_data_idx].print_prefix);
			// Add unit.
			NODE_append_value_string((char_t*) DMM_LINE_DATA[str_data_idx].unit);
			break;
		case DMM_LINE_DATA_INDEX_NODES_SCAN_PERIOD:
		case DMM_LINE_DATA_INDEX_SIGFOX_UL_PERIOD:
		case DMM_LINE_DATA_INDEX_SIGFOX_DL_PERIOD:
			// Convert to seconds.
			NODE_flush_string_value();
			NODE_append_value_int32((int32_t) (DINFOX_get_seconds((DINFOX_time_representation_t) field_value)),  DMM_LINE_DATA[str_data_idx].print_format,  DMM_LINE_DATA[str_data_idx].print_prefix);
			// Add unit.
			NODE_append_value_string((char_t*) DMM_LINE_DATA[str_data_idx].unit);
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
NODE_status_t DMM_build_sigfox_ul_payload(NODE_ul_payload_t* node_ul_payload) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_access_status_t access_status;
	XM_registers_list_t reg_list;
	DMM_sigfox_ul_payload_monitoring_t sigfox_ul_payload_monitoring;
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
	status = COMMON_check_event_driven_payloads(node_ul_payload, (XM_node_registers_t*) &DMM_NODE_REGISTERS);
	if (status != NODE_SUCCESS) goto errors;
	// Directly exits if a common payload was computed.
	if ((*(node_ul_payload -> size)) > 0) goto errors;
	// Else use specific pattern of the node.
	switch (DMM_SIGFOX_UL_PAYLOAD_PATTERN[node_ul_payload -> node -> radio_transmission_count]) {
	case DMM_SIGFOX_UL_PAYLOAD_TYPE_MONITORING:
		// Build registers list.
		reg_list.addr_list = (uint8_t*) DMM_REG_LIST_SIGFOX_UL_PAYLOAD_MONITORING;
		reg_list.size = sizeof(DMM_REG_LIST_SIGFOX_UL_PAYLOAD_MONITORING);
		// Perform measurements.
		status = XM_perform_measurements((node_ul_payload -> node -> address), &access_status);
		if (status != NODE_SUCCESS) goto errors;
		// Check write status.
		if (access_status.flags == 0) {
			// Read related registers.
			status = XM_read_registers((node_ul_payload -> node -> address), &reg_list, (XM_node_registers_t*) &DMM_NODE_REGISTERS, &access_status);
			if (status != NODE_SUCCESS) goto errors;
		}
		// Build monitoring payload.
		sigfox_ul_payload_monitoring.vrs = DINFOX_read_field(DMM_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_1], DMM_REG_ANALOG_DATA_1_MASK_VRS);
		sigfox_ul_payload_monitoring.vhmi = DINFOX_read_field(DMM_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_1], DMM_REG_ANALOG_DATA_1_MASK_VHMI);
		sigfox_ul_payload_monitoring.vusb = DINFOX_read_field(DMM_REGISTERS[DMM_REG_ADDR_ANALOG_DATA_2], DMM_REG_ANALOG_DATA_2_MASK_VUSB);
		sigfox_ul_payload_monitoring.nodes_count = DINFOX_read_field(DMM_REGISTERS[DMM_REG_ADDR_STATUS_1], DMM_REG_STATUS_1_MASK_NODES_COUNT);
		// Copy payload.
		for (idx=0 ; idx<DMM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE ; idx++) {
			(node_ul_payload -> ul_payload)[idx] = sigfox_ul_payload_monitoring.frame[idx];
		}
		(*(node_ul_payload -> size)) = DMM_SIGFOX_UL_PAYLOAD_MONITORING_SIZE;
		break;
	default:
		status = NODE_ERROR_SIGFOX_UL_PAYLOAD_TYPE;
		goto errors;
	}
	// Increment transmission count.
	(node_ul_payload -> node -> radio_transmission_count) = ((node_ul_payload -> node -> radio_transmission_count) + 1) % (sizeof(DMM_SIGFOX_UL_PAYLOAD_PATTERN));
errors:
	return status;
}
