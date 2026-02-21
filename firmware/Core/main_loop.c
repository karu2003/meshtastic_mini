/**
 * Main loop: LoRa RX → decrypt → protobuf decode → UART;
 *            UART line → protobuf encode → encrypt → LoRa TX.
 *
 * Over-the-air format (Meshtastic-compatible):
 *   [16-byte header] [AES-128-CTR( protobuf(Data{portnum, payload}) )]
 */

#include "led.h"
#include "serial_io.h"
#include "../Radio/lora_meshtastic.h"
#if defined(USE_HAL_DRIVER)
#include "stm32wlxx_hal.h"
#endif
#include "../Mesh/mesh_packet.h"
#include "../Mesh/flood_router.h"
#include "../Config/config_store.h"
#include "../Crypto/aes_meshtastic.h"
#include <string.h>

#define LORA_BUF_SIZE 256
#define LINE_BUF_SIZE 200

#define PORTNUM_TEXT_MESSAGE 1

static uint8_t lora_rx_buf[LORA_BUF_SIZE];
static uint8_t lora_tx_buf[LORA_BUF_SIZE];
static device_config_t g_config;
static uint32_t next_packet_id;

static uint8_t line_buf[LINE_BUF_SIZE];
static uint16_t line_len;

/* --- Minimal protobuf encode/decode for Meshtastic Data message --- */

/* Encode Data{portnum, payload} → protobuf bytes.
 * Returns encoded length, 0 on error. */
static uint16_t pb_encode_data(uint8_t *out, uint16_t max_out,
                               uint8_t portnum,
                               const uint8_t *payload, uint16_t payload_len)
{
    uint16_t need = 2 + 2 + payload_len;
    if (need > max_out || payload_len > 127) return 0;
    uint16_t p = 0;
    out[p++] = 0x08;              /* field 1 (portnum), wire=varint */
    out[p++] = portnum;
    out[p++] = 0x12;              /* field 2 (payload), wire=length-delimited */
    out[p++] = (uint8_t)payload_len;
    memcpy(out + p, payload, payload_len);
    return (uint16_t)(p + payload_len);
}

/* Decode Data protobuf → portnum + payload pointer/length.
 * Returns true if payload field found. */
static bool pb_decode_data(const uint8_t *data, uint16_t len,
                           uint8_t *portnum,
                           const uint8_t **payload, uint16_t *payload_len)
{
    *portnum = 0;
    *payload = NULL;
    *payload_len = 0;

    uint16_t pos = 0;
    while (pos < len) {
        if (pos >= len) break;
        uint8_t tag = data[pos++];
        uint8_t field = tag >> 3;
        uint8_t wire  = tag & 0x07;

        if (wire == 0) {          /* varint */
            uint32_t val = 0;
            unsigned shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                val |= (uint32_t)(b & 0x7F) << shift;
                if (!(b & 0x80)) break;
                shift += 7;
            }
            if (field == 1) *portnum = (uint8_t)val;
        } else if (wire == 2) {   /* length-delimited */
            if (pos >= len) break;
            uint32_t flen = 0;
            unsigned shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                flen |= (uint32_t)(b & 0x7F) << shift;
                if (!(b & 0x80)) break;
                shift += 7;
            }
            if (field == 2 && flen <= (uint32_t)(len - pos)) {
                *payload = data + pos;
                *payload_len = (uint16_t)flen;
            }
            pos += flen;
        } else if (wire == 5) {   /* fixed32 */
            pos += 4;
        } else if (wire == 1) {   /* fixed64 */
            pos += 8;
        } else {
            break;
        }
    }
    return (*payload != NULL);
}

/* --- Packet send/receive with encryption --- */

static bool send_lora_packet(uint32_t to_id, const uint8_t *text, uint16_t text_len) {
    /* Encode Data protobuf */
    uint8_t pb_buf[LORA_BUF_SIZE - MESH_HEADER_SIZE];
    uint16_t pb_len = pb_encode_data(pb_buf, sizeof(pb_buf),
                                     PORTNUM_TEXT_MESSAGE, text, text_len);
    if (pb_len == 0) return false;

    mesh_lora_header_t h = {
        .to_id     = to_id,
        .from_id   = g_config.node_id,
        .packet_id = next_packet_id++,
        .flags     = 3,
        .channel   = 0,
        .next_hop  = 0,
        .relay     = 0,
    };
    mesh_header_to_buf(&h, lora_tx_buf);

    memcpy(lora_tx_buf + MESH_HEADER_SIZE, pb_buf, pb_len);

    /* Encrypt payload (after header) with AES-CTR */
    aes_ctr_crypt(lora_tx_buf + MESH_HEADER_SIZE, pb_len,
                  h.packet_id, h.from_id);

    flood_seen(h.from_id, h.packet_id);
    return lora_tx(lora_tx_buf, MESH_HEADER_SIZE + pb_len);
}

static void uart_rx_line_poll(void) {
    uint8_t b;
    while (serial_get_byte(&b)) {
        if (b == '\r' || b == '\n') {
            if (line_len == 0) continue;
            line_buf[line_len] = '\0';

            if (line_len >= 2 && (line_buf[0] == 'N' || line_buf[0] == 'n')) {
                if (line_buf[1] >= '1' && line_buf[1] <= '9') {
                    g_config.node_id = (uint32_t)(line_buf[1] - '0');
                    serial_puts("node_id set\r\n");
                }
                line_len = 0;
                continue;
            }

            if (line_len == 4 && line_buf[0] == 'h' && line_buf[1] == 'e' &&
                line_buf[2] == 'l' && line_buf[3] == 'p') {
                serial_puts("Commands: N1..N9, info, help. Any other text = send over LoRa.\r\n");
                line_len = 0;
                continue;
            }

            if (line_len == 4 && line_buf[0] == 'i' && line_buf[1] == 'n' &&
                line_buf[2] == 'f' && line_buf[3] == 'o') {
                lora_params_t params;
                lora_get_params(&params);
                serial_puts("Freq: ");
                serial_put_int16((int16_t)(params.freq_hz / 1000000));
                serial_puts(" MHz  SF: ");
                serial_put_int16((int16_t)params.sf);
                serial_puts("  NodeId: ");
                serial_put_int16((int16_t)g_config.node_id);
                serial_puts("  Last RSSI: ");
                serial_put_int16(lora_last_rssi());
                serial_puts(" dBm\r\n");
                line_len = 0;
                continue;
            }

            if (send_lora_packet(MESH_BROADCAST_ID, line_buf, line_len))
                serial_puts("Sent.\r\n");
            else
                serial_puts("TX failed.\r\n");
            line_len = 0;
            continue;
        }
        if (line_len < LINE_BUF_SIZE - 1)
            line_buf[line_len++] = b;
    }
}

void mesh_mini_loop(void) {
    led_tick();
    uart_rx_line_poll();

    uint16_t n = lora_rx_poll(lora_rx_buf, LORA_BUF_SIZE);
    if (n <= MESH_HEADER_SIZE) return;

    mesh_lora_header_t h;
    mesh_header_from_buf(&h, lora_rx_buf);
    bool should_fwd = flood_should_forward(lora_rx_buf, n);
    flood_seen(h.from_id, h.packet_id);

    if (h.to_id == MESH_BROADCAST_ID || h.to_id == g_config.node_id) {
        uint16_t enc_len = n - MESH_HEADER_SIZE;
        uint8_t dec_buf[LORA_BUF_SIZE];
        memcpy(dec_buf, lora_rx_buf + MESH_HEADER_SIZE, enc_len);

        /* Decrypt with AES-CTR (same function for encrypt/decrypt) */
        aes_ctr_crypt(dec_buf, enc_len, h.packet_id, h.from_id);

        /* Decode Data protobuf */
        uint8_t portnum = 0;
        const uint8_t *text = NULL;
        uint16_t text_len = 0;

        if (pb_decode_data(dec_buf, enc_len, &portnum, &text, &text_len) &&
            text_len > 0)
        {
            serial_puts("RX: ");
            serial_write(text, text_len);
            serial_puts("  RSSI: ");
            serial_put_int16(lora_last_rssi());
            serial_puts(" dBm  SNR: ");
            serial_put_int16((int16_t)lora_last_snr());
            serial_puts(" dB\r\n");

            /* Auto-reply "pong" (unless we received "pong") */
            if (text_len != 4 || memcmp(text, "pong", 4) != 0) {
                const char pong[] = "pong";
                send_lora_packet(h.from_id, (const uint8_t *)pong, 4);
            }
        }
    }

    if (should_fwd) {
        flood_prepare_forward(lora_rx_buf, n, (uint8_t)(g_config.node_id & 0xFF));
        lora_tx(lora_rx_buf, n);
    }
}

void mesh_mini_init(void) {
    led_init();
    config_set_defaults(&g_config);
    config_load(&g_config);
    lora_init();
    lora_set_region_preset(REGION_EU_868, MODEM_LONG_FAST);
    aes_set_channel_key(g_config.channel_psk);
}
