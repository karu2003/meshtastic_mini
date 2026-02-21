/**
 * Debug UART: USART1 on PB6 (TX), PB7 (RX), 115200 8N1.
 * Implements uart_tx() and sends startup message after init.
 */
#if defined(USE_HAL_DRIVER)

#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_gpio_ex.h"

static UART_HandleTypeDef huart1;

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

void debug_uart_init(void)
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

    static const char msg[] = "\r\nMeshtastic_mini started\r\n";
    (void)HAL_UART_Transmit(&huart1, (const uint8_t *)msg, (uint16_t)(sizeof(msg) - 1), 100);
}

void debug_uart_puts(const char *s)
{
    if (s == NULL || huart1.Instance == NULL)
        return;
    uint16_t n = 0;
    while (s[n] != '\0' && n < 256)
        n++;
    if (n > 0)
        (void)HAL_UART_Transmit(&huart1, (const uint8_t *)s, n, 100);
}

void uart_tx(const uint8_t *data, uint16_t len)
{
    if (data == NULL || huart1.Instance == NULL)
        return;
    (void)HAL_UART_Transmit(&huart1, data, len, 500);
}

#endif
