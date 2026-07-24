/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/serial.h — COM1 serial output for headless diagnostics
 * ==========================================================================*/

#ifndef AURELIAN_SERIAL_H
#define AURELIAN_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write(const char *s);
void serial_write_u64(uint64_t v);
void serial_write_hex(uint64_t v);

#endif /* AURELIAN_SERIAL_H */
