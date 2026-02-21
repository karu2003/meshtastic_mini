/**
 * Minimal interrupt handlers for STM32WL (ARM + HAL build).
 * SysTick: only HAL tick. LED blink is done in main loop (led_tick).
 * USART1: push RX byte to main serial ring buffer.
 */
#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"
#include "serial_io.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}

void USART1_IRQHandler(void) {
    if (USART1->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)(USART1->RDR & 0xFF);
        serial_push_byte(b);
    }
}
#endif
