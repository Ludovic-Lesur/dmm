/*
 * common.h
 *
 *  Created on: 12 nov 2022
 *      Author: Ludo
 */

#ifndef __HMI_COMMON_H__
#define __HMI_COMMON_H__

#include "hmi_node.h"
#include "string.h"
#include "types.h"
#include "una.h"

/*** HMI COMMON macros ***/

#define HMI_COMMON_LINE_INDEX(board) \
    HMI_##board##_LINE_INDEX_HW_VERSION = 0, \
    HMI_##board##_LINE_INDEX_SW_VERSION, \
    HMI_##board##_LINE_INDEX_RESET_REASON, \
    HMI_##board##_LINE_INDEX_VMCU_MV, \
    HMI_##board##_LINE_INDEX_TMCU_DEGREES, \

#define HMI_COMMON_LINE \
    { "HW =", HMI_NODE_DATA_TYPE_HARDWARE_VERSION, COMMON_REGISTER_ADDRESS_HW_VERSION, UNA_REGISTER_MASK_ALL, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE }, \
    { "SW =", HMI_NODE_DATA_TYPE_SOFTWARE_VERSION, COMMON_REGISTER_ADDRESS_SW_VERSION_0, UNA_REGISTER_MASK_ALL, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE },  \
    { "RESET =", HMI_NODE_DATA_TYPE_RAW_HEXADECIMAL, COMMON_REGISTER_ADDRESS_STATUS_0, COMMON_REGISTER_STATUS_0_MASK_RESET_FLAGS, COMMON_REGISTER_ADDRESS_CONTROL_0, COMMON_REGISTER_CONTROL_0_MASK_RTRG }, \
    { "VMCU =", HMI_NODE_DATA_TYPE_VOLTAGE, COMMON_REGISTER_ADDRESS_ANALOG_DATA_0, COMMON_REGISTER_ANALOG_DATA_0_MASK_VMCU, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE }, \
    { "TMCU =", HMI_NODE_DATA_TYPE_TEMPERATURE, COMMON_REGISTER_ADDRESS_ANALOG_DATA_0, COMMON_REGISTER_ANALOG_DATA_0_MASK_TMCU, COMMON_REGISTER_ADDRESS_CONTROL_0, UNA_REGISTER_MASK_NONE }, \

#endif /* __HMI_COMMON_H__ */
