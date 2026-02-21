/**
 * RF control pins: PA4=RF_CTRL1, PA5=RF_CTRL2.
 * On STM32WLE5 the CPU talks to the integrated SX1262 over internal SUBGHZSPI (no SPI pins).
 * The only radio-related GPIOs are these two: they drive the antenna/RF switch for RX vs TX.
 *
 * Default aligned with radio_pair: TX = PA4=0 PA5=1, RX = PA4=1 PA5=0.
 * Set WIO_E5_RF_SWAP=1 for the opposite: TX = PA4=1 PA5=0, RX = PA4=0 PA5=1.
 */

#include "rf_ctrl.h"

#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"

#define RF_CTRL1_PORT   GPIOA
#define RF_CTRL1_PIN     GPIO_PIN_4
#define RF_CTRL2_PORT   GPIOA
#define RF_CTRL2_PIN    GPIO_PIN_5

#ifndef WIO_E5_RF_SWAP
#define WIO_E5_RF_SWAP  0
#endif
#if WIO_E5_RF_SWAP
/* TX: PA4=1 PA5=0, RX: PA4=0 PA5=1, Off = PA4=0 PA5=0 */
#define RF_RX_CTRL1   GPIO_PIN_RESET
#define RF_RX_CTRL2   GPIO_PIN_SET
#define RF_TX_CTRL1   GPIO_PIN_SET
#define RF_TX_CTRL2   GPIO_PIN_RESET
#define RF_OFF_CTRL1  GPIO_PIN_RESET
#define RF_OFF_CTRL2  GPIO_PIN_RESET
#else
/* Default (radio_pair): TX = PA4=0 PA5=1, RX = PA4=1 PA5=0, Off = PA4=0 PA5=0 */
#define RF_RX_CTRL1   GPIO_PIN_SET    /* PA4=1 */
#define RF_RX_CTRL2   GPIO_PIN_RESET  /* PA5=0 */
#define RF_TX_CTRL1   GPIO_PIN_RESET  /* PA4=0 */
#define RF_TX_CTRL2   GPIO_PIN_SET    /* PA5=1 */
#define RF_OFF_CTRL1  GPIO_PIN_RESET  /* PA4=0 */
#define RF_OFF_CTRL2  GPIO_PIN_RESET  /* PA5=0 */
#endif

void rf_ctrl_init(void) {
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pin = RF_CTRL1_PIN;
    HAL_GPIO_Init(RF_CTRL1_PORT, &g);
    g.Pin = RF_CTRL2_PIN;
    HAL_GPIO_Init(RF_CTRL2_PORT, &g);

    rf_ctrl_set_rx();
}

void rf_ctrl_set_off(void) {
    HAL_GPIO_WritePin(RF_CTRL1_PORT, RF_CTRL1_PIN, RF_OFF_CTRL1);
    HAL_GPIO_WritePin(RF_CTRL2_PORT, RF_CTRL2_PIN, RF_OFF_CTRL2);
}

void rf_ctrl_set_rx(void) {
    HAL_GPIO_WritePin(RF_CTRL1_PORT, RF_CTRL1_PIN, RF_RX_CTRL1);
    HAL_GPIO_WritePin(RF_CTRL2_PORT, RF_CTRL2_PIN, RF_RX_CTRL2);
}

void rf_ctrl_set_tx(void) {
    HAL_GPIO_WritePin(RF_CTRL1_PORT, RF_CTRL1_PIN, RF_TX_CTRL1);
    HAL_GPIO_WritePin(RF_CTRL2_PORT, RF_CTRL2_PIN, RF_TX_CTRL2);
}

#else

void rf_ctrl_init(void) {}
void rf_ctrl_set_off(void) {}
void rf_ctrl_set_rx(void) {}
void rf_ctrl_set_tx(void) {}

#endif
