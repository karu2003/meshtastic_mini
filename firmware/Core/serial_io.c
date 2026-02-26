/**
 * Main serial: USART1 on PB6 (TX), PB7 (RX), 115200 8N1.
 * Used for all exchange with the device (commands, LoRa payloads, RX print).
 */
#if defined(USE_HAL_DRIVER)

#include <stdbool.h>
#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_gpio_ex.h"

static UART_HandleTypeDef huart1;

#define RX_RING_SIZE 256
static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

void serial_push_byte(uint8_t b) {
    uint16_t next = (rx_head + 1) % RX_RING_SIZE;
    if (next != rx_tail) {
        rx_ring[rx_head] = b;
        rx_head = next;
    }
}

bool serial_get_byte(uint8_t *out) {
    if (out == NULL) return false;
    if (rx_tail == rx_head) return false;
    *out = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1) % RX_RING_SIZE;
    return true;
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
        return;

    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &g);
}

void serial_init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)
        return;

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    NVIC_SetPriority(USART1_IRQn, 2);
    NVIC_EnableIRQ(USART1_IRQn);
}

void serial_puts(const char *s)
{
    if (s == NULL || huart1.Instance == NULL)
        return;
    uint16_t n = 0;
    while (s[n] != '\0' && n < 256)
        n++;
    if (n > 0)
        (void)HAL_UART_Transmit(&huart1, (const uint8_t *)s, n, 100);
}

void serial_write(const uint8_t *data, uint16_t len)
{
    if (data == NULL || huart1.Instance == NULL || len == 0)
        return;
    (void)HAL_UART_Transmit(&huart1, data, len, 100);
}

void serial_put_int16(int16_t v)
{
    if (huart1.Instance == NULL) return;
    uint8_t buf[8];
    uint8_t n = 0;
    uint16_t u;
    if (v < 0) {
        buf[n++] = '-';
        u = (uint16_t)(-(int32_t)v);
    } else {
        u = (uint16_t)v;
    }
    if (u == 0) {
        buf[n++] = '0';
    } else {
        uint8_t digits[5];
        uint8_t d = 0;
        while (u) { digits[d++] = (uint8_t)(u % 10u); u /= 10u; }
        while (d) buf[n++] = (uint8_t)('0' + digits[--d]);
    }
    (void)HAL_UART_Transmit(&huart1, buf, n, 100);
}

void uart_tx(const uint8_t *data, uint16_t len)
{
    if (data == NULL || huart1.Instance == NULL)
        return;
    (void)HAL_UART_Transmit(&huart1, data, len, 500);
}

#endif
