/**
 * RF control pins: PA4=RF_CTRL1, PA5=RF_CTRL2.
 * Used to drive antenna/RF switch for RX vs TX. Invert levels below if your schematic differs.
 */

#include "rf_ctrl.h"

#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"

#define RF_CTRL1_PORT   GPIOA
#define RF_CTRL1_PIN     GPIO_PIN_4
#define RF_CTRL2_PORT   GPIOA
#define RF_CTRL2_PIN    GPIO_PIN_5

/* RX/TX levels for antenna switch; change if board schematic uses different logic */
#define RF_RX_CTRL1   GPIO_PIN_RESET  /* 0 */
#define RF_RX_CTRL2   GPIO_PIN_SET    /* 1 */
#define RF_TX_CTRL1   GPIO_PIN_SET    /* 1 */
#define RF_TX_CTRL2   GPIO_PIN_RESET  /* 0 */

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
void rf_ctrl_set_rx(void) {}
void rf_ctrl_set_tx(void) {}

#endif
