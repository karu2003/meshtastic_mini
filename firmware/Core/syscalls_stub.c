/**
 * Newlib syscall stubs for bare-metal ARM build (no OS).
 * Removes linker warnings: _close, _lseek, _read, _write not implemented.
 * _write sends stdout/stderr to UART via uart_tx (implemented in serial_io.c).
 */
#if defined(USE_HAL_DRIVER)

#include <stddef.h>
#include <stdint.h>

extern void uart_tx(const uint8_t *data, uint16_t len);

int _write(int fd, const void *buf, unsigned int count) {
    if (fd == 1 || fd == 2) {
        if (buf != NULL && count > 0) {
            uint16_t n = (count > 0xFFFFu) ? 0xFFFFu : (uint16_t)count;
            uart_tx((const uint8_t *)buf, n);
        }
    }
    return (int)count;
}

int _close(int fd) {
    (void)fd;
    return 0;
}

int _read(int fd, void *buf, unsigned int count) {
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

long _lseek(int fd, long offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;
}
#else
/* Host build: no stubs needed */
void syscalls_stub_placeholder(void) { (void)0; }
#endif
