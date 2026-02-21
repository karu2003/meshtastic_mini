/**
 * Weak radio driver stubs. radio_stm32wl.c overrides them when linked.
 */

#include "radio_phy.h"

static const radio_phy_ops_t *s_ops;

void radio_phy_set_ops(const radio_phy_ops_t *ops) {
    s_ops = ops;
}

static bool default_init(void) { (void)0; return true; }
static bool default_set_freq(uint32_t f) { (void)f; return true; }
static bool default_set_lora(uint8_t a, uint32_t b, uint8_t c) { (void)a;(void)b;(void)c; return true; }
static bool default_tx(const uint8_t *d, uint16_t l) { (void)d;(void)l; return true; }
static uint16_t default_rx_poll(uint8_t *b, uint16_t m) { (void)b;(void)m; return 0; }
static void default_rssi_snr(int16_t *r, int8_t *s) { if (r) *r = 0; if (s) *s = 0; }

static const radio_phy_ops_t default_ops = {
    .init = default_init,
    .set_freq = default_set_freq,
    .set_lora = default_set_lora,
    .tx = default_tx,
    .rx_poll = default_rx_poll,
    .get_last_rssi_snr = default_rssi_snr,
};

bool radio_phy_init(void) {
    const radio_phy_ops_t *ops = s_ops ? s_ops : &default_ops;
    return ops->init();
}

bool radio_phy_set_freq(uint32_t freq_hz) {
    const radio_phy_ops_t *ops = s_ops ? s_ops : &default_ops;
    return ops->set_freq(freq_hz);
}

bool radio_phy_set_lora(uint8_t sf, uint32_t bw_hz, uint8_t cr) {
    const radio_phy_ops_t *ops = s_ops ? s_ops : &default_ops;
    return ops->set_lora(sf, bw_hz, cr);
}

bool radio_phy_tx(const uint8_t *data, uint16_t len) {
    const radio_phy_ops_t *ops = s_ops ? s_ops : &default_ops;
    return ops->tx(data, len);
}

uint16_t radio_phy_rx_poll(uint8_t *buf, uint16_t max_len) {
    const radio_phy_ops_t *ops = s_ops ? s_ops : &default_ops;
    return ops->rx_poll(buf, max_len);
}

void radio_phy_get_last_rssi_snr(int16_t *rssi, int8_t *snr) {
    const radio_phy_ops_t *ops = s_ops ? s_ops : &default_ops;
    ops->get_last_rssi_snr(rssi, snr);
}
