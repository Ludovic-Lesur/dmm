/*
 * nvm.h
 *
 *  Created on: 19 jun. 2018
 *      Author: Ludo
 */

#ifndef __NVM_H__
#define __NVM_H__

#include "flash_reg.h"
#include "types.h"

/*** NVM structures ***/

/*!******************************************************************
 * \enum NVM_status_t
 * \brief NVM driver error codes.
 *******************************************************************/
typedef enum {
	// Driver errors.
	NVM_SUCCESS = 0,
	NVM_ERROR_NULL_PARAMETER,
	NVM_ERROR_ADDRESS,
	NVM_ERROR_UNLOCK,
	NVM_ERROR_LOCK,
	NVM_ERROR_READ,
	NVM_ERROR_WRITE,
	// Last base value.
	NVM_ERROR_BASE_LAST = 0x0100
} NVM_status_t;

/*!******************************************************************
 * \enum NVM_address_t
 * \brief NVM address mapping.
 *******************************************************************/
typedef enum {
	NVM_ADDRESS_SELF_ADDRESS = 0,
	NVM_ADDRESS_REGISTERS = 0x40,
	NVM_ADDRESS_LAST = (EEPROM_SIZE_BYTES - 1)
} NVM_address_t;

/*** NVM functions ***/

/*!******************************************************************
 * \fn NVM_status_t NVM_read_byte(NVM_address_t address, uint8_t* data)
 * \brief Read byte in NVM.
 * \param[in]  	address: Address to read.
 * \param[out] 	data: Pointer to byte that will contain the read value.
 * \retval		Function execution status.
 *******************************************************************/
NVM_status_t NVM_read_byte(NVM_address_t address, uint8_t* data);

/*!******************************************************************
 * \fn NVM_status_t NVM_write_byte(NVM_address_t address, uint8_t data)
 * \brief Write byte in NVM.
 * \param[in]  	address: Address to write.
 * \param[out] 	data: Byte to write.
 * \retval		Function execution status.
 *******************************************************************/
NVM_status_t NVM_write_byte(NVM_address_t address, uint8_t data);

/*******************************************************************/
#define NVM_exit_error(error_base) { if (nvm_status != NVM_SUCCESS) { status = (error_base + nvm_status); goto errors; } }

/*******************************************************************/
#define NVM_stack_error(void) { if (nvm_status != NVM_SUCCESS) { ERROR_stack_add(ERROR_BASE_NVM + nvm_status); } }

/*******************************************************************/
#define NVM_stack_exit_error(error_code) { if (nvm_status != NVM_SUCCESS) { ERROR_stack_add(ERROR_BASE_NVM + nvm_status); status = error_code; goto errors; } }

#endif /* __NVM_H__ */
