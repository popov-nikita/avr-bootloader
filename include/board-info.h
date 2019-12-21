#ifndef __BOARD_INFO_H
#define __BOARD_INFO_H 1

#include <stdint.h>

/* System-wide configuration info, including FUSE bits */
struct board_info {
	struct {
		uint8_t low;
		uint8_t high;
	} fuses;
	int frequency;
	struct {
		uint16_t ubrr;
		enum {
			bps_9600  = 1,
			bps_38400 = 2,
		} bps;
		uint8_t timer_thres;
	} usart;
	const uint8_t cal_data[4];
};

extern struct board_info info;

#endif
