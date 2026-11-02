#ifndef SERIAL_H
#define SERIAL_H
#include <stdbool.h>
#include <stdint.h>
void serial_init(uint32_t baud);
void serial_service(void);
bool serial_read(uint8_t *c);
bool serial_write(const char *text);
void serial_request_mode(bool interrupts);
bool serial_interrupts(void);
unsigned serial_tx_space(void);
extern volatile uint32_t serial_rx_dropped, serial_errors, serial_tx_dropped;
#endif
