/**
 * Newlib syscall stubs for bare-metal ARM build (no OS).
 * Removes linker warnings: _close, _lseek, _read, _write not implemented.
 */
#if defined(USE_HAL_DRIVER)

int _write(int fd, const void *buf, unsigned int count) {
    (void)fd;
    (void)buf;
    (void)count;
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
