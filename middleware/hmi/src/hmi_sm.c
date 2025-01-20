/*
 * sm.c
 *
 *  Created on: 04 mar. 2023
 *      Author: Ludo
 */

#include "hmi_sm.h"

#include "common_registers.h"
#include "hmi_common.h"
#include "sm_registers.h"
#include "una.h"

/*** SM global variables ***/

const HMI_NODE_line_t HMI_SM_LINE[HMI_SM_LINE_INDEX_LAST] = {
    HMI_COMMON_LINE
	{ "AIN0 =", HMI_NODE_DATA_TYPE_VOLTAGE, SM_REGISTER_ADDRESS_ANALOG_DATA_1, SM_REGISTER_ANALOG_DATA_1_MASK_VAIN0, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "AIN1 =", HMI_NODE_DATA_TYPE_VOLTAGE, SM_REGISTER_ADDRESS_ANALOG_DATA_1, SM_REGISTER_ANALOG_DATA_1_MASK_VAIN1, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "AIN2 =", HMI_NODE_DATA_TYPE_VOLTAGE, SM_REGISTER_ADDRESS_ANALOG_DATA_2, SM_REGISTER_ANALOG_DATA_2_MASK_VAIN2, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "AIN3 =", HMI_NODE_DATA_TYPE_VOLTAGE, SM_REGISTER_ADDRESS_ANALOG_DATA_2, SM_REGISTER_ANALOG_DATA_2_MASK_VAIN3, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "DIO0 =", HMI_NODE_DATA_TYPE_BIT, SM_REGISTER_ADDRESS_DIGITAL_DATA, SM_REGISTER_DIGITAL_DATA_MASK_DIO0, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "DIO1 =", HMI_NODE_DATA_TYPE_BIT, SM_REGISTER_ADDRESS_DIGITAL_DATA, SM_REGISTER_DIGITAL_DATA_MASK_DIO1, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "DIO2 =", HMI_NODE_DATA_TYPE_BIT, SM_REGISTER_ADDRESS_DIGITAL_DATA, SM_REGISTER_DIGITAL_DATA_MASK_DIO2, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "DIO3 =", HMI_NODE_DATA_TYPE_BIT, SM_REGISTER_ADDRESS_DIGITAL_DATA, SM_REGISTER_DIGITAL_DATA_MASK_DIO3, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "TAMB =", HMI_NODE_DATA_TYPE_TEMPERATURE, SM_REGISTER_ADDRESS_ANALOG_DATA_3, SM_REGISTER_ANALOG_DATA_3_MASK_TAMB, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },
	{ "HAMB =", HMI_NODE_DATA_TYPE_HUMIDITY, SM_REGISTER_ADDRESS_ANALOG_DATA_3, SM_REGISTER_ANALOG_DATA_3_MASK_HAMB, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE }
};