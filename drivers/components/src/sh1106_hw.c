/*
 * sh1106_hw.c
 *
 *  Created on: 27 aug. 2024
 *      Author: Ludo
 */

#include "sh1106_hw.h"

#ifndef SH1106_DRIVER_DISABLE_FLAGS_FILE
#include "sh1106_driver_flags.h"
#endif
#include "error.h"
#include "gpio_mapping.h"
#include "i2c.h"
#include "i2c_address.h"
#include "sh1106.h"
#include "types.h"

#ifndef SH1106_DRIVER_DISABLE

#define SH1106_HW_I2C_INSTANCE  I2C_INSTANCE_I2C1

/*** SH1106 HW functions ***/

/*******************************************************************/
SH1106_status_t SH1106_HW_init(void) {
    // Local variables.
    SH1106_status_t status = SH1106_SUCCESS;
    I2C_status_t i2c_status = I2C_SUCCESS;
    // Init I2C.
    i2c_status = I2C_init(SH1106_HW_I2C_INSTANCE, &GPIO_HMI_I2C);
    I2C_exit_error(SH1106_ERROR_BASE_I2C);
errors:
    return status;
}

/*******************************************************************/
SH1106_status_t SH1106_HW_de_init(void) {
    // Local variables.
    SH1106_status_t status = SH1106_SUCCESS;
    I2C_status_t i2c_status = I2C_SUCCESS;
    // Release I2C.
    i2c_status = I2C_de_init(SH1106_HW_I2C_INSTANCE, &GPIO_HMI_I2C);
    I2C_exit_error(SH1106_ERROR_BASE_I2C);
errors:
    return status;
}

/*******************************************************************/
SH1106_status_t SH1106_HW_i2c_write(uint8_t i2c_address, uint8_t* data, uint8_t data_size_bytes, uint8_t stop_flag) {
    // Local variables.
    SH1106_status_t status = SH1106_SUCCESS;
    I2C_status_t i2c_status = I2C_SUCCESS;
    // I2C transfer.
    i2c_status = I2C_write(SH1106_HW_I2C_INSTANCE, i2c_address, data, data_size_bytes, stop_flag);
    I2C_exit_error(SH1106_ERROR_BASE_I2C);
errors:
    return status;
}

#endif /* SH1106_DRIVER_DISABLE */
