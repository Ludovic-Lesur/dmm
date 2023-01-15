/*
 * main.c
 *
 *  Created on: Jan 7, 2023
 *      Author: Ludo
 */

// Registers
#include "rcc_reg.h"
// Peripherals.
#include "adc.h"
#include "exti.h"
#include "flash.h"
#include "gpio.h"
#include "i2c.h"
#include "iwdg.h"
#include "lptim.h"
#include "lpuart.h"
#include "mapping.h"
#include "nvic.h"
#include "nvm.h"
#include "pwr.h"
#include "rcc.h"
#include "rtc.h"
// Components.
#include "led.h"
#include "rs485.h"
#include "rs485_common.h"
#include "sh1106.h"
// Utils.
#include "types.h"
// Applicative.
#include "error.h"
#include "hmi.h"
#include "mode.h"
#include "version.h"

/*** MAIN local macros ***/

// RTC wake-up timer period.
// Warning: this value must be lower than the watchdog period = 25s.
#define DMM_WAKEUP_PERIOD_SECONDS	10

/*** MAIN local structures ***/

typedef union {
	struct {
		unsigned lse_status : 1;
		unsigned lsi_status : 1;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
	uint8_t all;
} DIM_status_t;

typedef enum {
	DMM_STATE_INIT,
	DMM_STATE_HMI,
	DMM_STATE_OFF,
	DMM_STATE_SLEEP,
	DMM_STATE_LAST,
} DMM_state_t;

typedef struct {
	// Global.
	DMM_state_t state;
	DIM_status_t status;
	// Clocks.
	uint32_t lsi_frequency_hz;
	uint8_t lse_running;
} DMM_context_t;

/*** MAIN local global variables ***/

static DMM_context_t dmm_ctx;

/*** MAIN local functions ***/

/* COMMON INIT FUNCTION FOR MAIN CONTEXT.
 * @param:	None.
 * @return:	None.
 */
void _DMM_init_context(void) {
	// Init context.
	dmm_ctx.state = DMM_STATE_INIT;
	dmm_ctx.lsi_frequency_hz = 0;
	dmm_ctx.lse_running = 0;
	dmm_ctx.status.all = 0;
}

/* COMMON INIT FUNCTION FOR PERIPHERALS AND COMPONENTS.
 * @param:	None.
 * @return:	None.
 */
void _DMM_init_hw(void) {
	// Local variables.
	RCC_status_t rcc_status = RCC_SUCCESS;
	RTC_status_t rtc_status = RTC_SUCCESS;
	ADC_status_t adc1_status = ADC_SUCCESS;
#ifdef AM
	LPUART_status_t lpuart1_status = LPUART_SUCCESS;
	NVM_status_t nvm_status = NVM_SUCCESS;
	RS485_address_t node_address;
#endif
#ifndef DEBUG
	IWDG_status_t iwdg_status = IWDG_SUCCESS;
#endif
	// Init error stack
	ERROR_stack_init();
	// Init memory.
	NVIC_init();
	NVM_init();
	// Init GPIOs.
	GPIO_init();
	EXTI_init();
	// Init clock and power modules.
	RCC_init();
	PWR_init();
	// Reset RTC.
	RTC_reset();
	// Start oscillators.
	rcc_status = RCC_enable_lsi();
	RCC_error_check();
	dmm_ctx.status.lsi_status = (rcc_status == RCC_SUCCESS) ? 1 : 0;
	rcc_status = RCC_enable_lse();
	RCC_error_check();
	dmm_ctx.status.lse_status = (rcc_status == RCC_SUCCESS) ? 1 : 0;
	// Start independent watchdog.
#ifndef DEBUG
	iwdg_status = IWDG_init();
	IWDG_error_check();
#endif
	// High speed oscillator.
	IWDG_reload();
	rcc_status = RCC_switch_to_hsi();
	RCC_error_check();
	// Get LSI effective frequency (must be called after HSI initialization and before RTC inititialization).
	rcc_status = RCC_get_lsi_frequency(&dmm_ctx.lsi_frequency_hz);
	RCC_error_check();
	if (rcc_status != RCC_SUCCESS) dmm_ctx.lsi_frequency_hz = RCC_LSI_FREQUENCY_HZ;
	IWDG_reload();
	// RTC.
	dmm_ctx.lse_running = dmm_ctx.status.lse_status;
	rtc_status = RTC_init(&dmm_ctx.lse_running, dmm_ctx.lsi_frequency_hz);
	RTC_error_check();
	// Update LSE status if RTC failed to start on it.
	if (dmm_ctx.lse_running == 0) {
		dmm_ctx.status.lse_status = 0;
	}
	IWDG_reload();
#ifdef AM
	// Read RS485 address in NVM.
	nvm_status = NVM_read_byte(NVM_ADDRESS_RS485_ADDRESS, &node_address);
	NVM_error_check();
#endif
	// Init peripherals.
	LPTIM1_init(dmm_ctx.lsi_frequency_hz);
	TIM3_init();
	TIM22_init();
	adc1_status = ADC1_init();
	ADC1_error_check();
#ifdef AM
	lpuart1_status = LPUART1_init(node_address);
	LPUART1_error_check();
#else
	LPUART1_init();
#endif
	I2C1_init();
	// Init components.
	LED_init();
	RS485_init();
	// Init applicative layers.
	HMI_init();
}

/*** MAIN functions ***/

/* MAIN FUNCTION.
 * @param:	None.
 * @return:	None.
 */
int main(void) {
	// Init board.
	_DMM_init_context();
	_DMM_init_hw();
	// Local variables.
	RS485_status_t rs485_status = RS485_SUCCESS;
	HMI_status_t hmi_status = HMI_SUCCESS;
	RTC_status_t rtc_status = RTC_SUCCESS;
	// Main loop.
	while (1) {
		// Perform state machine.
		switch (dmm_ctx.state) {
		case DMM_STATE_INIT:
			// Perform first nodes scan.
			rs485_status = RS485_scan_nodes(rs485_common_ctx.nodes_list, RS485_NODES_LIST_SIZE_MAX, &rs485_common_ctx.nodes_count);
			RS485_error_check();
			// Compute next state.
			dmm_ctx.state = DMM_STATE_OFF;
			break;
		case DMM_STATE_HMI:
			// Process HMI.
			hmi_status = HMI_task();
			HMI_error_check();
			// Compute next state.
			dmm_ctx.state = DMM_STATE_OFF;
			break;
		case DMM_STATE_OFF:
			// Start periodic wakeup timer.
			EXTI_clear_all_flags();
			RTC_clear_wakeup_timer_flag();
			rtc_status = RTC_start_wakeup_timer(DMM_WAKEUP_PERIOD_SECONDS);
			RTC_error_check();
			// Enable HMI activation interrupt.
			NVIC_enable_interrupt(NVIC_INTERRUPT_EXTI_0_1);
			// Compute next state.
			dmm_ctx.state = DMM_STATE_SLEEP;
			break;
		case DMM_STATE_SLEEP:
			// Enter sleep mode.
			PWR_enter_stop_mode();
			// Wake-up
			IWDG_reload();
			// Check RTC flag.
			if (RTC_get_wakeup_timer_flag() != 0) {
				// Clear flag.
				RTC_clear_wakeup_timer_flag();
				// TODO check nodes monitoring period.
			}
			// Check HMI activation flag.
			if (EXTI_get_encoder_switch_flag() != 0) {
				// Clear flag.
				EXTI_clear_encoder_switch_flag();
				// Stop periodic wakeup timer.
				rtc_status = RTC_stop_wakeup_timer();
				RTC_error_check();
				// Start HMI.
				dmm_ctx.state = DMM_STATE_HMI;
			}
			break;
		default:
			dmm_ctx.state = DMM_STATE_SLEEP;
			break;
		}
		IWDG_reload();
	}
	return 0;
}
