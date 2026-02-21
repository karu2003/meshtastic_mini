/**
 * Main loop example: LoRa RX → flood → decrypt → Serial;
 * Serial RX → ToRadio → encrypt → LoRa TX.
 * Real init (HAL, UART, flash) is in your project's main().
 */

#include "led.h"
#include "../Radio/lora_meshtastic.h"
#include "../Mesh/mesh_packet.h"
#include "../Mesh/flood_router.h"
#include "../Serial/serial_framing.h"
#include "../Config/config_store.h"
#include "../Crypto/aes_meshtastic.h"
#include <string.h>

#define LORA_BUF_SIZE 256
static uint8_t lora_rx_buf[LORA_BUF_SIZE];
static device_config_t g_config;

/* Called when a full packet is received from Serial (ToRadio) */
static void on_serial_packet(const uint8_t *buf, uint16_t len) {
    (void)buf;
    (void)len;
    /* TODO: nanopb decode ToRadio; if packet — build MeshPacket in LoRa format and lora_tx */
}

/* Pseudo-main: call from your main() after init. */
void mesh_mini_loop(void) {
    led_tick();

    /* 1) LoRa receive */
    uint16_t n = lora_rx_poll(lora_rx_buf, LORA_BUF_SIZE);
    if (n >= MESH_HEADER_SIZE) {
        mesh_lora_header_t h;
        mesh_header_from_buf(&h, lora_rx_buf);
        bool should_fwd = flood_should_forward(lora_rx_buf, n);
        flood_seen(h.from_id, h.packet_id);

        if (h.to_id == MESH_BROADCAST_ID || h.to_id == g_config.node_id) {
            uint16_t payload_len = n - MESH_HEADER_SIZE;
            if (payload_len > 0) {
                /* aes_decrypt_payload(lora_rx_buf + MESH_HEADER_SIZE, payload_len); */
                /* serial_send_packet(from_radio_buf, from_radio_len); — FromRadio.packet */
            }
        }

        if (should_fwd) {
            flood_prepare_forward(lora_rx_buf, n, (uint8_t)(g_config.node_id & 0xFF));
            lora_tx(lora_rx_buf, n);
        }
    }

    /* 2) Serial receive — call from UART RX callback per byte: */
    /* serial_rx_byte(byte, on_serial_packet); */
}

void mesh_mini_init(void) {
    led_init();
    config_set_defaults(&g_config);
    config_load(&g_config);
    lora_init();
    lora_set_region_preset(REGION_EU_868, MODEM_LONG_FAST);
    aes_set_channel_key(g_config.channel_psk);
}
