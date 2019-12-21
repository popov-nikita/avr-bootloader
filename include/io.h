#ifndef __IO_H
#define __IO_H 1

#include <stdint.h>

enum io_location {
	sreg   = 0x3f,
	sph    = 0x3e,
	spl    = 0x3d,
	ocr0   = 0x3c,
	gicr   = 0x3b,
	gifr   = 0x3a,
	timsk  = 0x39,
	tifr   = 0x38,
	spmcr  = 0x37,
	/* skip */
	mcucr  = 0x35,
	mcucsr = 0x34,
	tccr0  = 0x33,
	tcnt0  = 0x32,
	osccal = 0x31,
	sfior  = 0x30,
	/* skip */
	tccr2  = 0x25,
	tcnt2  = 0x24,
	ocr2   = 0x23,
	assr   = 0x22,
	wdtcr  = 0x21,
	ubrrh  = 0x20,
	ucsrc  = 0x20,
	eearh  = 0x1f,
	eearl  = 0x1e,
	eedr   = 0x1d,
	eecr   = 0x1c,
	porta  = 0x1b,
	ddra   = 0x1a,
	pina   = 0x19,
	/* skip */
	udr    = 0x0c,
	ucsra  = 0x0b,
	ucsrb  = 0x0a,
	ubrrl  = 0x09,
	/* skip */
};

enum tccr0_bits {
	cs00   = 0, /* Clock Select */
	cs01   = 1, /* Clock Select */
	cs02   = 2, /* Clock Select */
	wgm01  = 3, /* Waveform Generation Mode */
	com00  = 4, /* Compare Match Output Mode */
	com01  = 5, /* Compare Match Output Mode */
	wgm00  = 6, /* Waveform Generation Mode */
	foc0   = 7, /* Force Output Compare */
};

enum timsk_bits {
	toie0  = 0, /* Timer/Counter0 Overflow Interrupt Enable */
	ocie0  = 1, /* Timer/Counter0 Output Compare Match Interrupt Enable */
	/* SKIP */
};

enum tifr_bits {
	tov0   = 0, /* Timer/Counter0 Overflow Flag */
	ocf0   = 1, /* Output Compare Flag 0 */
	/* SKIP */
};

enum sfior_bits {
	psr10  = 0, /* Prescaler Reset Timer/Counter1 and Timer/Counter0 */
	psr2   = 1, /* Prescaler Reset Timer/Counter2 */
	/* SKIP */
};

enum spmcr_bits {
	spmen  = 0, /* Store Program Memory Enable */
	pgers  = 1, /* Page Erase */
	pgwrt  = 2, /* Page Write */
	blbset = 3, /* Boot Lock Bit Set */
	rwwsre = 4, /* Read-While-Write Section Read Enable */
	rwwsb  = 6, /* Read-While-Write Section Busy */
	spmie  = 7, /* SPM Interrupt Enable */
};

enum eecr_bits {
	eere   = 0, /* EEPROM Read Enable */
	eewe   = 1, /* EEPROM Write Enable */
	eemwe  = 2, /* EEPROM Master Write Enable */
	eerie  = 3, /* EEPROM Ready Interrupt Enable */
};

enum gicr_bits {
	ivce   = 0, /* Interrupt Vector Change Enable */
	ivsel  = 1, /* Interrupt Vector Select */
	int2   = 5, /* External Interrupt Request 2 Enable */
	int0   = 6, /* External Interrupt Request 0 Enable */
	int1   = 7, /* External Interrupt Request 1 Enable */
};

enum ucsra_bits {
	mpcm   = 0, /* Multi-processor Communication Mode */
	u2x    = 1, /* Double the USART Transmission Speed */
	pe     = 2, /* Parity Error */
	dor    = 3, /* Data OverRun */
	fe     = 4, /* Frame Error */
	udre   = 5, /* USART Data Register Empty */
	txc    = 6, /* USART Transmit Complete */
	rxc    = 7, /* USART Receive Complete */
};

enum ucsrb_bits {
	txb8   = 0, /* Transmit Data Bit 8 */
	rxb8   = 1, /* Receive Data Bit 8 */
	ucsz2  = 2, /* Character Size */
	txen   = 3, /* Transmitter Enable */
	rxen   = 4, /* Receiver Enable */
	udrie  = 5, /* USART Data Register Empty Interrupt Enable */
	txcie  = 6, /* TX Complete Interrupt Enable */
	rxcie  = 7, /* RX Complete Interrupt Enable */
};

enum ucsrc_bits {
	ucpol  = 0, /* Clock Polarity. Datasheet says write this bit to 0 for async mode! */
	ucsz0  = 1, /* Character Size */
	ucsz1  = 2, /* Character Size */
	usbs   = 3, /* Stop Bit Select */
	upm0   = 4, /* Parity Mode */
	upm1   = 5, /* Parity Mode */
	umsel  = 6, /* USART Mode Select */
	ursel  = 7, /* Register Select */
};

enum sreg_bits {
	bit_c  = 0, /* Carry Flag */
	bit_z  = 1, /* Zero Flag */
	bit_n  = 2, /* Negative Flag */
	bit_v  = 3, /* Two’s Complement Overflow Flag */
	bit_s  = 4, /* Sign Bit */
	bit_h  = 5, /* Half Carry Flag */
	bit_t  = 6, /* Bit Copy Storage */
	bit_i  = 7, /* Global Interrupt Enable */
};

static inline void cli(void)
{
	asm volatile ( "cli" ::: "memory", "cc" );
}

static inline void sei(void)
{
	asm volatile ( "sei" ::: "memory", "cc" );
}

static inline void wdr(void)
{
	asm volatile ( "wdr" ::: "memory" );
}

static inline void sleep(void)
{
	asm volatile ( "sleep" ::: "memory" );
}

static inline void barrier(void)
{
	asm volatile ( "" ::: "memory" );
}

static inline uint8_t lpm(uint16_t adr)
{
	uint8_t value;

	asm volatile ( "lpm %[value],Z\n\t"
		       : [value] "=&r" (value)
		       : "z" (adr)
		       : "memory" );

	return value;
}

/* We want opcodes here to be exact. So overhead for I/O is small */
static inline uint8_t io_read(uint8_t adr)
{
	uint8_t value;

	adr &= (1U << 6) - 1;
	if (__builtin_constant_p(adr)) {
		asm volatile ( "in %[value],%[adr]\n\t"
			       : [value] "=r" (value)
			       : [adr] "I" (adr)
			       : "memory" );
	} else {
		uint16_t _adr = (uint16_t) adr;
		asm volatile ( "ldd %[value],Z + 0x20\n\t"
			       : [value] "=&r" (value)
			       : "z" (_adr)
			       : "memory" );
	}

	return value;
}

static inline void io_write(uint8_t adr, uint8_t value)
{
	adr &= (1U << 6) - 1;
	if (__builtin_constant_p(adr)) {
		asm volatile ( "out %[adr],%[value]\n\t"
			       : /* no output operands */
			       : [adr] "I" (adr),
				 [value] "r" (value)
			       : "memory", "cc" );
	} else {
		uint16_t _adr = (uint16_t) adr;
		asm volatile ( "std Z + 0x20,%[value]\n\t"
			       : /* no output operands */
			       : "z" (_adr),
				 [value] "r" (value)
			       : "memory", "cc" );
	}
}

static inline uint8_t get_flags(void)
{
	return io_read(sreg);
}

static inline void set_flags(uint8_t f)
{
	io_write(sreg, f);
}

static inline uint8_t irq_save(void)
{
	uint8_t f;

	f = get_flags();
	cli();

	return f;
}

static inline void irq_restore(uint8_t f)
{
	set_flags(f);
}

/* Safely halt CPU */
static inline void die(void)
{
	cli();
	for (;;) {
		;
	}
}

typedef void (*callback_t) (void);

#endif
