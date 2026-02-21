/**
 * Minimal interrupt handlers for STM32WL (ARM + HAL build).
 * SysTick: only HAL tick. LED blink is done in main loop (led_tick) to avoid
 * double-toggle with ISR.
 */
#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"

void SysTick_Handler(void) {
    HAL_IncTick();
}
#endif
