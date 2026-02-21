/**
 * Entry point. Call radio_stm32wl_register() before mesh_mini_init when using STM32WL.
 */

#include "../Radio/radio_phy.h"
#include "led.h"

#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"
#endif

extern void radio_stm32wl_register(void);
extern void mesh_mini_init(void);
extern void mesh_mini_loop(void);

/* Weak stub: implement in project (HAL_UART_Transmit etc.). */
__attribute__((weak)) void uart_tx(const uint8_t *data, uint16_t len) {
    (void)data;
    (void)len;
}

#if defined(USE_HAL_DRIVER)
extern void SystemClock_Config_LL(void);
extern void debug_uart_init(void);
extern void debug_uart_puts(const char *s);
#endif

int main(void) {
#if defined(USE_HAL_DRIVER)
    /* 48 MHz via LL before HAL so SysTick = 1 ms (no HAL timeouts during clock switch) */
    SystemClock_Config_LL();
    HAL_Init();
    debug_uart_init();   /* USART1 115200, sends "Meshtastic_mini started" to serial debug */
#endif
    /* Register STM32WL driver (when built with USE_STM32WL_RADIO). */
    radio_stm32wl_register();

    mesh_mini_init();
#if defined(USE_HAL_DRIVER)
    debug_uart_puts("mesh init done, loop\r\n");
#endif

    for (;;)
        mesh_mini_loop();
}
