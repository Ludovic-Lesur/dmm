/*
 * bpsm.h
 *
 *  Created on: 26 feb. 2023
 *      Author: Ludo
 */

#ifndef __BPSM_H__
#define __BPSM_H__

#include "at_bus.h"
#include "common.h"
#include "bpsm_reg.h"
#include "node.h"
#include "node_common.h"
#include "string.h"
#include "types.h"

/*** BPSM structures ***/

/*!******************************************************************
 * \enum BPSM_line_data_index_t
 * \brief BPSM screen data lines index.
 *******************************************************************/
typedef enum {
	BPSM_LINE_DATA_INDEX_VSRC = COMMON_LINE_DATA_INDEX_LAST,
	BPSM_LINE_DATA_INDEX_VSTR,
	BPSM_LINE_DATA_INDEX_VBKP,
	BPSM_LINE_DATA_INDEX_CHEN,
	BPSM_LINE_DATA_INDEX_CHST,
	BPSM_LINE_DATA_INDEX_BKEN,
	BPSM_LINE_DATA_INDEX_LAST,
} BPSM_line_data_index_t;

/*** BPSM global variables ***/

extern const uint32_t BPSM_REG_WRITE_TIMEOUT_MS[BPSM_REG_ADDR_LAST];

/*** BPSM functions ***/

/*!******************************************************************
 * \fn NODE_status_t BPSM_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status)
 * \brief Write corresponding node register of screen data line.
 * \param[in]  	line_data_write: Pointer to the writing operation parameters.
 * \param[out] 	write_status: Pointer to the writing operation status.
 * \retval		Function execution status.
 *******************************************************************/
NODE_status_t BPSM_write_line_data(NODE_line_data_write_t* line_data_write, NODE_access_status_t* write_status);

/*!******************************************************************
 * \fn NODE_status_t BPSM_read_line_data(NODE_line_data_read_t* line_data_read, NODE_access_status_t* read_status)
 * \brief Read corresponding node register of screen data line.
 * \param[in]  	line_data_read: Pointer to the reading operation parameters.
 * \param[out] 	read_status: Pointer to the reading operation status.
 * \retval		Function execution status.
 *******************************************************************/
NODE_status_t BPSM_read_line_data(NODE_line_data_read_t* line_data_read, NODE_access_status_t* read_status);

/*!******************************************************************
 * \fn NODE_status_t BPSM_build_sigfox_ul_payload(NODE_ul_payload_t* node_ul_payload)
 * \brief Build node Sigfox uplink payload.
 * \param[in]  	none
 * \param[out] 	node_ul_payload: Pointer to the Sigfox uplink payload.
 * \retval		Function execution status.
 *******************************************************************/
NODE_status_t BPSM_build_sigfox_ul_payload(NODE_ul_payload_t* node_ul_payload);

#endif /* __BPSM_H__ */
