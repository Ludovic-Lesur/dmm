/*
 * bpsm.c
 *
 *  Created on: 26 feb. 2023
 *      Author: Ludo
 */

#include "hmi_bpsm.h"

#include "bpsm_registers.h"
#include "common_registers.h"
#include "hmi_common.h"
#include "una.h"

/*** BPSM global variables ***/

const HMI_NODE_line_t HMI_BPSM_LINE[HMI_BPSM_LINE_INDEX_LAST] = {
    HMI_COMMON_LINE
	{ "VSRC =", HMI_NODE_DATA_TYPE_VOLTAGE, BPSM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE, BPSM_REGISTER_ADDRESS_ANALOG_DATA_1, BPSM_REGISTER_ANALOG_DATA_1_MASK_VSRC },
	{ "VSTR =", HMI_NODE_DATA_TYPE_VOLTAGE, BPSM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE, BPSM_REGISTER_ADDRESS_ANALOG_DATA_1, BPSM_REGISTER_ANALOG_DATA_1_MASK_VSTR },
	{ "VBKP =", HMI_NODE_DATA_TYPE_VOLTAGE, BPSM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE, BPSM_REGISTER_ADDRESS_ANALOG_DATA_2, BPSM_REGISTER_ANALOG_DATA_2_MASK_VBKP },
	{ "CHRG_EN =", HMI_NODE_DATA_TYPE_BIT, BPSM_REGISTER_ADDRESS_STATUS_1, BPSM_REGISTER_STATUS_1_MASK_CHENST, BPSM_REGISTER_ADDRESS_CONTROL_1, BPSM_REGISTER_CONTROL_1_MASK_CHEN },
	{ "CHRG_ST =", HMI_NODE_DATA_TYPE_BIT, BPSM_REGISTER_ADDRESS_STATUS_1, BPSM_REGISTER_STATUS_1_MASK_CHRGST, BPSM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE },
	{ "BKP_EN =", HMI_NODE_DATA_TYPE_BIT, BPSM_REGISTER_ADDRESS_STATUS_1, BPSM_REGISTER_STATUS_1_MASK_BKENST, BPSM_REGISTER_ADDRESS_CONTROL_1, BPSM_REGISTER_CONTROL_1_MASK_BKEN }
};
