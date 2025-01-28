/*
 * uhfm.c
 *
 *  Created on: 26 feb. 2023
 *      Author: Ludo
 */

#include "hmi_uhfm.h"

#include "common_registers.h"
#include "hmi_common.h"
#include "uhfm_registers.h"
#include "una.h"

/*** UHFM global variables ***/

const HMI_NODE_line_t HMI_UHFM_LINE[HMI_UHFM_LINE_INDEX_LAST] = {
    HMI_COMMON_LINE
    { "EP ID", HMI_NODE_DATA_TYPE_EP_ID, UHFM_REGISTER_ADDRESS_EP_ID, UNA_REGISTER_MASK_ALL, UHFM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE },
    { "VRF TX =", HMI_NODE_DATA_TYPE_VOLTAGE, UHFM_REGISTER_ADDRESS_ANALOG_DATA_1, UHFM_REGISTER_ANALOG_DATA_1_MASK_VRF_TX, UHFM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE },
    { "VRF RX =", HMI_NODE_DATA_TYPE_VOLTAGE, UHFM_REGISTER_ADDRESS_ANALOG_DATA_1, UHFM_REGISTER_ANALOG_DATA_1_MASK_VRF_RX, UHFM_REGISTER_ADDRESS_CONTROL_1, UNA_REGISTER_MASK_NONE }
};
