/**
 * LoRa with Meshtastic parameters: region/preset tables and radio_phy calls.
 */

#include "lora_meshtastic.h"
#include "radio_phy.h"
#include <string.h>

/* Default frequency for region (first slot), Hz */
static const uint32_t region_freq_default[REGION_COUNT] = {
    [REGION_EU_868]  = 869525000,
    [REGION_US_915]  = 906875000,
    [REGION_EU_433]  = 433875000,
    [REGION_LORA_24] = 2403000000,
};

/* Presets: SF, BW (Hz), CR (5=4/5, 6=4/6, 7=4/7, 8=4/8). Meshtastic-compatible. */
typedef struct { uint8_t sf; uint32_t bw; uint8_t cr; } modem_preset_t;
static const modem_preset_t modem_presets[MODEM_COUNT] = {
    [MODEM_SHORT_FAST]     = { .sf = 7,  .bw = 250000,  .cr = 5 },  /* Short Fast */
    [MODEM_SHORT_SLOW]     = { .sf = 8,  .bw = 250000,  .cr = 5 },  /* Short Slow */
    [MODEM_MEDIUM_FAST]    = { .sf = 9,  .bw = 250000,  .cr = 5 },  /* Medium Fast */
    [MODEM_MEDIUM_SLOW]    = { .sf = 10, .bw = 250000,  .cr = 5 },  /* Medium Slow */
    [MODEM_LONG_FAST]      = { .sf = 11, .bw = 250000,  .cr = 5 },  /* Long Fast (default) */
    [MODEM_LONG_SLOW]      = { .sf = 12, .bw = 125000,  .cr = 8 },  /* Long Slow */
    [MODEM_VERY_LONG_SLOW] = { .sf = 12, .bw = 125000,  .cr = 8 },  /* Very Long Slow */
};

static lora_params_t s_params;
static bool s_inited;

bool lora_init(void) {
    if (s_inited) return true;
    s_params.freq_hz = region_freq_default[REGION_EU_868];
    s_params.sf = 11;
    s_params.bw_hz = 250000;
    s_params.cr = 5;
    if (!radio_phy_init()) return false;
    s_inited = true;
    return true;
}

bool lora_set_region_preset(lora_region_t region, lora_modem_preset_t preset) {
    if (region >= REGION_COUNT || preset >= MODEM_COUNT) return false;
    s_params.freq_hz = region_freq_default[region];
    const modem_preset_t *m = &modem_presets[preset];
    s_params.sf = m->sf;
    s_params.bw_hz = m->bw;
    s_params.cr = m->cr;
    if (!radio_phy_set_freq(s_params.freq_hz)) return false;
    if (!radio_phy_set_lora(s_params.sf, s_params.bw_hz, s_params.cr)) return false;
    return true;
}

void lora_get_params(lora_params_t *out) {
    if (out) memcpy(out, &s_params, sizeof(s_params));
}

bool lora_tx(const uint8_t *data, uint16_t len) {
    if (!data) return false;
    return radio_phy_tx(data, len);
}

uint16_t lora_rx_poll(uint8_t *buf, uint16_t max_len) {
    if (!buf || max_len == 0) return 0;
    return radio_phy_rx_poll(buf, max_len);
}

int16_t lora_last_rssi(void) {
    int16_t r; int8_t s;
    radio_phy_get_last_rssi_snr(&r, &s);
    return r;
}

int8_t lora_last_snr(void) {
    int16_t r; int8_t s;
    radio_phy_get_last_rssi_snr(&r, &s);
    return s;
}
