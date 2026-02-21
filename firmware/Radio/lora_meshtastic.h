/**
 * LoRa layer — Meshtastic parameters and abstraction for STM32WLE5.
 * Implementation: STM32WL SubGHz HAL or RadioLib (ModuleSTM32WL).
 */

#ifndef LORA_MESHTASTIC_H
#define LORA_MESHTASTIC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Frequency region (Meshtastic-compatible) */
typedef enum {
    REGION_EU_868,
    REGION_US_915,
    REGION_EU_433,
    REGION_LORA_24,
    REGION_COUNT
} lora_region_t;

/* Modem preset: SF, BW, CR combination */
typedef enum {
    MODEM_SHORT_FAST,
    MODEM_SHORT_SLOW,
    MODEM_MEDIUM_FAST,
    MODEM_MEDIUM_SLOW,
    MODEM_LONG_FAST,      /* default in Meshtastic */
    MODEM_LONG_SLOW,
    MODEM_VERY_LONG_SLOW,
    MODEM_COUNT
} lora_modem_preset_t;

typedef struct {
    uint8_t  sf;       /* 7..12 */
    uint32_t bw_hz;    /* 125000, 250000, 500000, etc. */
    uint8_t  cr;       /* 5..8 (denominator 4) */
    uint32_t freq_hz;
} lora_params_t;

/* Init radio (SubGHz HAL or RadioLib) */
bool lora_init(void);

/* Apply region + modem preset (Meshtastic-style) */
bool lora_set_region_preset(lora_region_t region, lora_modem_preset_t preset);

/* Current params (for debug/config) */
void lora_get_params(lora_params_t *out);

/* Transmit: buffer + length. Returns success. */
bool lora_tx(const uint8_t *data, uint16_t len);

/* Non-blocking receive: if packet available, copy to buf and return length; else 0. */
uint16_t lora_rx_poll(uint8_t *buf, uint16_t max_len);

/* RSSI/SNR of last received packet (optional) */
int16_t lora_last_rssi(void);
int8_t  lora_last_snr(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_MESHTASTIC_H */
