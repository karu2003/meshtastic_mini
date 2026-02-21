/**
 * Serial API framing: 0x94 0xC3 + length (2 bytes, MSB first), max 512 bytes body.
 */

#ifndef SERIAL_FRAMING_H
#define SERIAL_FRAMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_START1  0x94
#define SERIAL_START2  0xC3
#define SERIAL_MAX_PAYLOAD  512

/**
 * Write one packet to UART: 4-byte header + body.
 * body_len must be <= SERIAL_MAX_PAYLOAD.
 */
void serial_send_packet(const uint8_t *body, uint16_t body_len);

/**
 * Non-blocking receive: feed next byte from UART.
 * On complete packet calls callback and returns 1; otherwise 0.
 * callback(buf, len) is called with packet body (without 4-byte header).
 */
typedef void (*serial_packet_cb_t)(const uint8_t *buf, uint16_t len);
int serial_rx_byte(uint8_t byte, serial_packet_cb_t callback);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_FRAMING_H */
