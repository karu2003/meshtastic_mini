/**
 * LoRa physical layer abstraction for STM32WLE5.
 * Implementation from radio_stm32wl.c (HAL SubGHz) or stub when built without HAL.
 */

#ifndef RADIO_PHY_H
#define RADIO_PHY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool (*init)(void);
    bool (*set_freq)(uint32_t freq_hz);
    bool (*set_lora)(uint8_t sf, uint32_t bw_hz, uint8_t cr);
    bool (*tx)(const uint8_t *data, uint16_t len);
    uint16_t (*rx_poll)(uint8_t *buf, uint16_t max_len);
    void (*get_last_rssi_snr)(int16_t *rssi, int8_t *snr);
} radio_phy_ops_t;

/* Set driver (called from lora_init when implementation is present). */
void radio_phy_set_ops(const radio_phy_ops_t *ops);

/* Call current driver. Weak stubs — if radio_stm32wl is not linked, these are used. */
bool radio_phy_init(void);
bool radio_phy_set_freq(uint32_t freq_hz);
bool radio_phy_set_lora(uint8_t sf, uint32_t bw_hz, uint8_t cr);
bool radio_phy_tx(const uint8_t *data, uint16_t len);
uint16_t radio_phy_rx_poll(uint8_t *buf, uint16_t max_len);
void radio_phy_get_last_rssi_snr(int16_t *rssi, int8_t *snr);

#ifdef __cplusplus
}
#endif

#endif /* RADIO_PHY_H */
