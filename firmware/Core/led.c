/**
 * Status LED: init GPIO and blink in led_tick().
 * Uses HAL when USE_HAL_DRIVER is defined (ARM build); otherwise no-op for host build.
 */

#include "led.h"

#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"

/* Board: PB5=LED (active low), USART1=PB6/PB7, PA4=RF_CTRL1, PA5=RF_CTRL2 */
#define LED_GPIO_PORT       GPIOB
#define LED_GPIO_PIN        GPIO_PIN_5
/* Пол секунды светим, пол секунды не светим */
#define LED_HALF_PERIOD_MS  500u

static uint32_t led_last_toggle_ms;

void led_init(void) {
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Pin   = LED_GPIO_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &g);
    /* LED active low: 1 = off */
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
    led_last_toggle_ms = 0;
}

void led_tick(void) {
    uint32_t now = HAL_GetTick();
    if (now - led_last_toggle_ms >= LED_HALF_PERIOD_MS) {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
        led_last_toggle_ms = now;
    }
}

void led_toggle_from_isr(void) {
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
}

#else

void led_init(void) {
}

void led_tick(void) {
}

void led_toggle_from_isr(void) {
}

#endif
