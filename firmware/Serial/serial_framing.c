/**
 * Receive state machine: look for 0x94 0xC3, read length, then body.
 * Implement uart_tx in your project (HAL_UART_Transmit etc.).
 */

#include "serial_framing.h"
#include <string.h>

enum { SYNC, LEN_MSB, LEN_LSB, BODY };
static uint8_t  rx_state;
static uint16_t rx_len;
static uint16_t rx_idx;
static uint8_t  rx_buf[SERIAL_MAX_PAYLOAD];
static serial_packet_cb_t rx_cb;

/* External: send buffer to UART. Implement in project (HAL_UART_Transmit etc.). */
extern void uart_tx(const uint8_t *data, uint16_t len);

void serial_send_packet(const uint8_t *body, uint16_t body_len) {
    if (!body || body_len > SERIAL_MAX_PAYLOAD) return;
    uint8_t hdr[4] = { SERIAL_START1, SERIAL_START2, (uint8_t)(body_len >> 8), (uint8_t)(body_len & 0xFF) };
    uart_tx(hdr, 4);
    uart_tx(body, body_len);
}

int serial_rx_byte(uint8_t byte, serial_packet_cb_t callback) {
    rx_cb = callback;
    switch (rx_state) {
        case SYNC:
            if (byte == SERIAL_START1) rx_state = LEN_MSB;
            break;
        case LEN_MSB:
            rx_len = (uint16_t)byte << 8;
            rx_state = LEN_LSB;
            break;
        case LEN_LSB:
            rx_len |= byte;
            if (rx_len > SERIAL_MAX_PAYLOAD) { rx_state = SYNC; break; }
            rx_idx = 0;
            rx_state = BODY;
            break;
        case BODY:
            rx_buf[rx_idx++] = byte;
            if (rx_idx >= rx_len) {
                rx_state = SYNC;
                if (rx_cb) rx_cb(rx_buf, rx_len);
                return 1;
            }
            break;
    }
    return 0;
}
