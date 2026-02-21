/**
 * Main serial (USART1): all application exchange with the device.
 * PB6 (TX), PB7 (RX), 115200 8N1.
 */
#ifndef SERIAL_IO_H
#define SERIAL_IO_H

#include <stdbool.h>
#include <stdint.h>

#if defined(USE_HAL_DRIVER)

void serial_init(void);
void serial_puts(const char *s);
void serial_write(const uint8_t *data, uint16_t len);
void serial_put_int16(int16_t v);   /* decimal to UART (for RSSI, etc.) */
void serial_push_byte(uint8_t b);   /* from USART1 IRQ when RXNE */
bool serial_get_byte(uint8_t *out); /* non-blocking */

#else

static inline void serial_init(void) {}
static inline void serial_puts(const char *s) { (void)s; }
static inline void serial_write(const uint8_t *data, uint16_t len) { (void)data; (void)len; }
static inline void serial_put_int16(int16_t v) { (void)v; }
static inline void serial_push_byte(uint8_t b) { (void)b; }
static inline bool serial_get_byte(uint8_t *out) { (void)out; return false; }

#endif

#endif
