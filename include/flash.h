#ifndef __FLASH_H
#define __FLASH_H 1

#include <io.h>

void write_page(callback_t cb);
void set_lock_bits(callback_t cb, uint8_t bits);

/* Linker managed buffer */
extern uint16_t spm_buffer[], spm_buffer_end[];
/* Absolute constants defined in linker script */
extern uint16_t __flash_page, __text_start, __page_offset_mask, __page_mask;

#endif
