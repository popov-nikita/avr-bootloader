#ifndef __USART_H
#define __USART_H 1

#include <io.h>

void usart_init(void);
uint16_t usart_read(uint8_t *buf, uint16_t bufsz);
void usart_write(const uint8_t *buf, uint16_t bufsz);
uint16_t usart_calc_csum(uint8_t *buf, uint16_t bufsz);

extern uint8_t usart_buffer[], usart_buffer_end[];

#endif
