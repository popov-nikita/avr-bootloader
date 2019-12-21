#include <comp-defs.h>
#include <board-info.h>
#include <io.h>
#include <spm-wrapper.h>
#include <flash.h>
#include <usart.h>
#include <proto.h>

#define DEBUG    1

struct board_info info = {
	/* Unfortunately, there is no way to obtain calibration data at run-time :( */
	.cal_data = {
		0xa9,0xa9,0xa7,0xa7
	},
};

static void __head __noinline setup(void)
{
	uint8_t f_low, f_high, cal_data;

	f_low  = _get_lfuse_bits();
	f_high = _get_hfuse_bits();

	info.fuses.low  = f_low;
	info.fuses.high = f_high;

	switch (f_low & ((uint8_t) 0x0f)) {
	case ((uint8_t) 0x01):
		info.frequency  = 1;
		info.usart.ubrr        = 12U;
		info.usart.bps         = bps_9600;
		info.usart.timer_thres = 4U;
		cal_data               = info.cal_data[0];
		break;
	case ((uint8_t) 0x02):
		info.frequency         = 2;
		info.usart.ubrr        = 25U;
		info.usart.bps         = bps_9600;
		info.usart.timer_thres = 8U;
		cal_data               = info.cal_data[1];
		break;
	case ((uint8_t) 0x03):
		info.frequency         = 4;
		info.usart.ubrr        = 12U;
		info.usart.bps         = bps_38400;
		info.usart.timer_thres = 16U;
		cal_data               = info.cal_data[2];
		break;
	case ((uint8_t) 0x04):
		info.frequency         = 8;
		info.usart.ubrr        = 25U;
		info.usart.bps         = bps_38400;
		info.usart.timer_thres = 32U;
		cal_data               = info.cal_data[3];
		break;
	default:
		/* Unknown frequency. Panic. */
		die();
	}

	io_write(osccal, cal_data);
}

static inline void __head move_ivt_2_bls(void)
{
	uint8_t enable, set;

	enable = 1U << ivce;
	set    = 1U << ivsel;
	asm volatile ( "out %[io_loc],%[enable]\n\t"
		       "out %[io_loc],%[set]\n\t"
		       : /* no output operands */
		       : [io_loc] "I" (gicr),
			 [enable] "r" (enable),
			 [set] "r" (set)
		       : "memory" );
}

static volatile uint8_t may_continue = 0x00U;

static void __head locking_cb(void)
{
	uint8_t lock_bits;

	/* If lock bits didn't actually change -- panic */
	lock_bits = _get_lock_bits();
	if (lock_bits != 0xefU)
		die();

	may_continue = 0x01U;
}

#if 0
/* Makes spm_buffer consisting of 0xFF only */
static inline void __text flash_ut__make_ff(void)
{
	uint16_t *p;

	for (p = spm_buffer;
	     p < spm_buffer_end;
	     p++)
		*p = 0xffffU;
}

static inline void __text flash_ut__strcpy(char *dst, const char *src)
{
	for (; *src; src++, dst++)
		*dst = *src;
}

static void __text flash_ut_null_cb(void)
{
	/* stub */
	;
}

static void __text flash_ut_second_cb(void)
{
	uint8_t *ptr, *end;
	uint16_t addr;

	ptr = (uint8_t *) spm_buffer;
	end = (uint8_t *) spm_buffer_end;
	addr = 0U;
	for (; ptr < end; ptr++, addr++)
		*ptr = lpm(addr);

	write_page(flash_ut_null_cb);
}

static void __text flash_ut_first_cb(void)
{
	/* Here we check that we can actually write page in ISR context */
	write_page(flash_ut_second_cb);
}

/* Unit test */
static void __text flash_ut(void)
{
	static const char data_ut[] = "flash_ut data";
	static const char data_ut_first_cb[] = "flash_ut_first_cb data";

	flash_ut__make_ff();
	flash_ut__strcpy((char *) spm_buffer, data_ut);
	write_page(flash_ut_first_cb);
	/* Check that it is actually posible to change spm_buffer upon `write_page` return */
	flash_ut__make_ff();
	flash_ut__strcpy((char *) spm_buffer, data_ut_first_cb);
}
#else
static inline void __text flash_ut(void)
{
	;
}
#endif

#if 0
static inline void __text usart_ut__ushort2ascii(char *s, uint16_t x)
{
	static const char hex2ascii[] = "0123456789ABCDEF";

	s[0] = hex2ascii[((x >> 12) & 0x000fU)];
	s[1] = hex2ascii[((x >> 8) & 0x000fU)];
	s[2] = hex2ascii[((x >> 4) & 0x000fU)];
	s[3] = hex2ascii[((x) & 0x000fU)];
	s[4] = '\0';
}

static inline uint16_t __text usart_ut__strlen(const char *s)
{
	const char *e;

	for (e = s; *e; e++)
		;

	return (uint16_t) (e - s);
}

static void __text usart_ut(void)
{
	uint16_t nread, csum;
	char s1[5], s2[5];
	const uint16_t bufsz = (uint16_t) (usart_buffer_end - usart_buffer);
	static const char msg_csum[] = "Checksum is ";
	static const char msg_nl[]   = "\n\r";
	static const char msg_recv[] = "Data received: ";
	static const char msg_nr[]   = "Number of characters: ";

	nread = usart_read(usart_buffer, bufsz);
	csum  = usart_calc_csum(usart_buffer, nread);
	usart_ut__ushort2ascii(s1, csum);
	usart_ut__ushort2ascii(s2, nread);

	usart_write((uint8_t *) msg_csum, usart_ut__strlen(msg_csum));
	usart_write((uint8_t *) s1, usart_ut__strlen(s1));
	usart_write((uint8_t *) msg_nl, usart_ut__strlen(msg_nl));
	usart_write((uint8_t *) msg_recv, usart_ut__strlen(msg_recv));
	usart_write(usart_buffer, nread);
	usart_write((uint8_t *) msg_nl, usart_ut__strlen(msg_nl));
	usart_write((uint8_t *) msg_nr, usart_ut__strlen(msg_nr));
	usart_write((uint8_t *) s2, usart_ut__strlen(s2));
	usart_write((uint8_t *) msg_nl, usart_ut__strlen(msg_nl));
}
#else
static inline void __text usart_ut(void)
{
	;
}
#endif

static const uint8_t load_program_ack[]  = ANSWER_ACK;
static const uint8_t load_program_nack[] = ANSWER_NACK;

static void __text load_program_cb(void)
{
	may_continue = 0x01U;
}

static inline void load_program_wait(void)
{
	while (!may_continue) ;
}

static inline void load_program_wr_page(void)
{
	load_program_wait();
	may_continue = 0x00U;
	write_page(load_program_cb);
}

static void __text __noinline load_program(void)
{
	const uint16_t usart_bufsz = (uint16_t) (usart_buffer_end - usart_buffer);
	const uint16_t pg_off_mask = (uint16_t) (&__page_offset_mask);
	uint16_t filesz, nr, pld_nr, csum;
	struct hdr *hdr;
	uint8_t *src, *dst;

	/* Unknown file size yet */
	filesz = 0U;
	/* How many bytes of the file we've received already? */
	pld_nr = 0U;
	/* Mark flash module free */
	load_program_cb();

	while (1) {
		nr      = usart_read(usart_buffer, usart_bufsz);
		if (nr <= sizeof(*hdr))
			goto nack;
		hdr     = (struct hdr *) usart_buffer;
		/* Sanity check the header */
		if (hdr->len != nr)
			goto nack;
		/* Skip packets with zero filesz field */
		if (!hdr->filesz)
			goto nack;
		/* Further packets should have the same filesz value */
		if (filesz && hdr->filesz != filesz)
			goto nack;
		/* Finally check csum correctness */
		csum    = usart_calc_csum((uint8_t *) &hdr->len,
		                          nr - offsetof(struct hdr, len));
		if (csum != hdr->csum)
			goto nack;
		/* Check that no extra data is present... */
		if (nr + pld_nr > sizeof(*hdr) + hdr->filesz)
			goto nack;
		/* At this stage packet appears to be ok... */
		filesz  = hdr->filesz;
		src     = (uint8_t *) (&hdr[1]);
		dst     = &((uint8_t *) spm_buffer)[pld_nr & pg_off_mask];
		pld_nr += (nr - sizeof(*hdr));
		while (src < &usart_buffer[nr]) {
			*(dst++)     = *(src++);
			if (dst < ((uint8_t *) spm_buffer_end))
				continue;
			load_program_wr_page();
			dst          = (uint8_t *) spm_buffer;
		}
		/* We've consumed current packet. Send acknowledgment */
		usart_write(load_program_ack, sizeof(load_program_ack));
		/* There are still packets left */
		if (pld_nr < filesz)
			continue;
		/* No need to flash another page */
		if (dst == (uint8_t *) spm_buffer)
			break;
		/* Fill the rest of the flash page with 0xFF */
		while (dst < ((uint8_t *) spm_buffer_end))
			*(dst++)     = 0xffU;
		load_program_wr_page();
		break;
	nack:
		usart_write(load_program_nack, sizeof(load_program_nack));
		continue;
	}

	/* Wait until any remaining flash operation is complete */
	load_program_wait();
}

#if !DEBUG
asm ( ".pushsection\t.head.text,\"ax\",@progbits\n\t"
     ".type\t\tapp_code_trampoline,@function\n"
      "app_code_trampoline:\n\t"
      ".set\t\tapp_code_entry,0x0000\n\t"
      "jmp\t\tapp_code_entry\n\t"
      ".size\t\tapp_code_trampoline,. - app_code_trampoline\n\t"
      ".popsection\n\t"
     );
void __noreturn app_code_trampoline();
#endif

void __noreturn __head main(void)
{
	move_ivt_2_bls();
	setup();
	sei();
	/* SPM is not allowed to write to the Boot Loader section */
	set_lock_bits(locking_cb, 0xefU);
	/* Here we have to wait before proceeding :( */
	load_program_wait();
	flash_ut();
	usart_init();
	load_program();
#if !DEBUG
	/* Application program is loaded and ready for execution.
	   Disable interrupts and jump to its entry point (at 0x0000U).
	*/
	cli();
	app_code_trampoline();
#else
	/* To debug everything, just spin around forever */
	for (;;) ;
#endif
}
