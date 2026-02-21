/**
 * Radio implementation for STM32WLE5 via SubGHz HAL (STM32CubeWL).
 * Build with -DUSE_STM32WL_RADIO and path to STM32CubeWL to use HAL.
 * Without USE_STM32WL_RADIO — only empty radio_stm32wl_register().
 */
#include "radio_phy.h"
#include <string.h>

#if defined(USE_STM32WL_RADIO) && defined(STM32WLxx)
#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_subghz.h"
#include "rf_ctrl.h"

static SUBGHZ_HandleTypeDef hsubghz;
static int16_t last_rssi;
static int8_t  last_snr;
static uint8_t rx_buf[256];
static uint16_t rx_len;
static bool rx_ready;

static bool stm32wl_radio_init(void) {
    rf_ctrl_init();
    memset(&hsubghz, 0, sizeof(hsubghz));
    if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK)
        return false;
    rf_ctrl_set_rx();
    __HAL_SUBGHZ_SET_RX_MODE(&hsubghz, SUBGHZ_RX_SINGLE);
    return true;
}

static bool stm32wl_radio_set_freq(uint32_t freq_hz) {
    (void)freq_hz;
    /* HAL_SUBGHZ_SetRFFrequency(&hsubghz, freq_hz) or equivalent for your HAL version */
    return true;
}

static bool stm32wl_radio_set_lora(uint8_t sf, uint32_t bw_hz, uint8_t cr) {
    (void)sf;
    (void)bw_hz;
    (void)cr;
    /* HAL: set ModemParam (SF, BW, CR), PacketParam (preamble, header, payload len) */
    return true;
}

static bool stm32wl_radio_tx(const uint8_t *data, uint16_t len) {
    if (!data || len > 256) return false;
    if (HAL_SUBGHZ_SetPayload(&hsubghz, (uint8_t *)data, len) != HAL_OK)
        return false;
    rf_ctrl_set_tx();
    if (HAL_SUBGHZ_Transmit(&hsubghz, 0) != HAL_OK) {  /* 0 = timeout ms or blocking */
        rf_ctrl_set_rx();
        return false;
    }
    rf_ctrl_set_rx();
    return true;
}

static uint16_t stm32wl_radio_rx_poll(uint8_t *buf, uint16_t max_len) {
    if (!buf || max_len == 0) return 0;
    if (HAL_SUBGHZ_GetStatus(&hsubghz) == HAL_SUBGHZ_STATE_RX) {
        if (rx_ready && rx_len <= max_len) {
            memcpy(buf, rx_buf, rx_len);
            rx_ready = false;
            last_rssi = 0; /* HAL_SUBGHZ_GetRssiInst() when available */
            last_snr  = 0;
            return rx_len;
        }
    }
    return 0;
}

static void stm32wl_radio_get_rssi_snr(int16_t *rssi, int8_t *snr) {
    if (rssi) *rssi = last_rssi;
    if (snr)  *snr  = last_snr;
}

static const radio_phy_ops_t stm32wl_ops = {
    .init = stm32wl_radio_init,
    .set_freq = stm32wl_radio_set_freq,
    .set_lora = stm32wl_radio_set_lora,
    .tx = stm32wl_radio_tx,
    .rx_poll = stm32wl_radio_rx_poll,
    .get_last_rssi_snr = stm32wl_radio_get_rssi_snr,
};

void radio_stm32wl_register(void) {
    radio_phy_set_ops(&stm32wl_ops);
}

#else
void radio_stm32wl_register(void) { (void)0; }
#endif
