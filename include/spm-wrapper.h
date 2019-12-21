#ifndef __SPM_WRAPPER_H
#define __SPM_WRAPPER_H 1

#include <stdint.h>

/*
	Erases page at @flash_addr.
*/
void _erase_page(uint16_t flash_addr);

/*
	Stores @data into flash hardware buffer at the position determined by @flash_addr.
*/
void _store_temp_buffer(uint16_t flash_addr, uint16_t data);

/*
	Writes page at @flash_addr.
*/
void _write_page(uint16_t flash_addr);

/*
	Enables RWW section.
*/
void _enable_rww_sect(void);

/*
	Sets lock bits according to @bits.
*/
void _set_lock_bits(uint8_t bits);

uint8_t _get_lock_bits(void);
uint8_t _get_lfuse_bits(void);
uint8_t _get_hfuse_bits(void);

#endif
