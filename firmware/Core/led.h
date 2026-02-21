/**
 * Status LED (blinking). On ARM build uses HAL GPIO; on host build no-op.
 * Board: PB5=LED, USART1=PB6/PB7, PA4=RF_CTRL1, PA5=RF_CTRL2, PB13=Boot Button.
 */

#ifndef FIRMWARE_CORE_LED_H
#define FIRMWARE_CORE_LED_H

void led_init(void);
void led_tick(void);
/* Called from SysTick_Handler to blink LED (no dependency on main loop). */
void led_toggle_from_isr(void);

#endif
