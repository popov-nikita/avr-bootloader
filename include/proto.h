#ifndef __PROTO_H
#define __PROTO_H 1

#include <stdint.h>
#include <comp-defs.h>

/* Header of the UART packet */
struct hdr {
	/* IPv4 checksum. Everything below this field is checksummed. */
	uint16_t csum;
	/* Length of the current packet */
	uint16_t len;
	/* Size of the file.
	   File is splitted into several packets.
	   Must be equal for all packets. */
	uint16_t filesz;
	/* Payload */
} __packed;

/*
 * AVR MCU is a slave device. It either ACKs or NACKs the received packet.
 * To avoid excessive error recovery we distinguish these two states
 * by number of bytes sent. The exact values are somewhat random.
 * But we stick to convention where `0` indicates success and '-1' -- failure.
 */
#define ANSWER_ACK    { 0x00U }
#define ANSWER_NACK   { 0xffU, 0xffU }

#endif
