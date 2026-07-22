#ifndef AURELIAN_SERIAL_H
#define AURELIAN_SERIAL_H

#include <stdint.h>

#define COM1_PORT 0x3F8

void aurelion_serial_init(void);
void aurelion_serial_write(const char *text);
void aurelion_serial_putc(char c);

#endif
