/*
 * lpuart.h
 *
 *  Created on: 25 oct. 2022
 *      Author: Ludo
 */

#ifndef __LPUART_H__
#define __LPUART_H__

#include "node_common.h"
#include "types.h"

/*** LPUART structures ***/

/*!******************************************************************
 * \enum LPUART_status_t
 * \brief LPUART driver error codes.
 *******************************************************************/
typedef enum {
	LPUART_SUCCESS = 0,
	LPUART_ERROR_NULL_PARAMETER,
	LPUART_ERROR_BAUD_RATE,
	LPUART_ERROR_RX_MODE,
	LPUART_ERROR_TX_TIMEOUT,
	LPUART_ERROR_TC_TIMEOUT,
	LPUART_ERROR_BASE_LAST = 0x0100
} LPUART_status_t;

/*!******************************************************************
 * \enum LPUART_rx_mode_t
 * \brief LPUART RX modes list.
 *******************************************************************/
typedef enum {
	LPUART_RX_MODE_ADDRESSED = 0,
	LPUART_RX_MODE_DIRECT,
	LPUART_RX_MODE_LAST
} LPUART_rx_mode_t;

/*!******************************************************************
 * \fn LPUART_rx_irq_cb_t
 * \brief LPUART RX interrupt callback.
 *******************************************************************/
typedef void (*LPUART_rx_irq_cb_t)(uint8_t data);

/*!******************************************************************
 * \fn LPUART_configuration_t
 * \brief LPUART configuration parameters.
 *******************************************************************/
typedef struct {
	uint32_t baud_rate;
	LPUART_rx_mode_t rx_mode;
	LPUART_rx_irq_cb_t rx_callback;
} LPUART_configuration_t;

/*** LPUART functions ***/

/*!******************************************************************
 * \fn void LPUART1_init(NODE_address_t self_address)
 * \brief Init LPUART1 peripheral.
 * \param[in]  	self_address: RS485 address of the node.
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void LPUART1_init(NODE_address_t self_address);

/*!******************************************************************
 * \fn void LPUART1_de_init(void)
 * \brief Release LPUART1 peripheral.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void LPUART1_de_init(void);

/*!******************************************************************
 * \fn LPUART_status_t LPUART1_configure(LPUART_configuration_t* config)
 * \brief Configure LPUART1 peripheral.
 * \param[in]  	config: Pointer to the LPUART configuration parameters.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
LPUART_status_t LPUART1_configure(LPUART_configuration_t* config);

/*!******************************************************************
 * \fn void LPUART1_enable_rx(void)
 * \brief Enable LPUART1 RX operation.
 * \param[in]   none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void LPUART1_enable_rx(void);

/*!******************************************************************
 * \fn void LPUART1_disable_rx(void)
 * \brief Disable LPUART1 RX operation.
 * \param[in]   none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void LPUART1_disable_rx(void);

/*!******************************************************************
 * \fn LPUART_status_t LPUART1_write(uint8_t* data, uint32_t data_size_bytes)
 * \brief Send data over LPUART1.
 * \param[in]	data: Byte array to send.
 * \param[in]	data_size_bytes: Number of bytes to send.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
LPUART_status_t LPUART1_write(uint8_t* data, uint32_t data_size_bytes);

/*******************************************************************/
#define LPUART1_check_status(error_base) { if (lpuart1_status != LPUART_SUCCESS) { status = error_base + lpuart1_status; goto errors; } }

/*******************************************************************/
#define LPUART1_stack_error(void) { ERROR_stack_error(lpuart1_status, LPUART_SUCCESS, ERROR_BASE_LPUART1); }

/*******************************************************************/
#define LPUART1_print_error(void) { ERROR_print_error(lpuart1_status, LPUART_SUCCESS, ERROR_BASE_LPUART1); }

#endif /* __LPUART_H__ */
