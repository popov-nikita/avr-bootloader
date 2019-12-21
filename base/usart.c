#include <comp-defs.h>
#include <board-info.h>
#include <usart.h>

void __text usart_init(void)
{
	uint8_t flags, high, low;
	flags = irq_save();

	high = (uint8_t) ((info.usart.ubrr & 0x0f00U) >> 8);
	low  = (uint8_t) ((info.usart.ubrr & 0x00ffU));
	io_write(ubrrh, high);
	io_write(ubrrl, low);

	/* set U2X */
	io_write(ucsra, (1U << u2x));
	/* Async mode, 1 stop bit, No Parity, 8 bit data */
	io_write(ucsrc, (1U << ursel) | (1U << ucsz1) | (1U << ucsz0));
	/* RX module is enabled by explicit call to usart_read routine */
	io_write(ucsrb, (1U << txen));

	irq_restore(flags);
}

static void __text usart_xmit(uint8_t c)
{
	while (!(io_read(ucsra) & (1U << udre))) ;

	io_write(udr, c);
}

static uint16_t __text usart_recv(void)
{
	uint8_t status, byte;

	status = io_read(ucsra);
	if (!(status & (1U << rxc)))
		return 0x0000U;

	byte = io_read(udr);
	/* If framing error occurs fake the received byte */
	if (status & (1U << fe))
		byte = 0x00U;
	/* Some characters were lost...
	   But this one is valid if no FRAMING ERROR occurs */
	if (status & (1U << dor)) {
		;
	}
	/* Leave only RX status which is 1 */
	status &= (1U << rxc);

	return ((((uint16_t) status) << 8) | ((uint16_t) byte));
}

static void __text usart_timer_start(void)
{
	uint8_t flags, tmp;
	flags = irq_save();

	/* Arm timer0 */
	io_write(tcnt0, 0x00U);
	/* The prescaler operates independently. Reset it now to be in somewhat known state */
	tmp  = io_read(sfior);
	tmp |= (1U << psr10);
	io_write(sfior, tmp);
	io_write(tccr0, (1U << cs02) | (0U << cs01) | (1U << cs00) | /* Divisor = 1024 */
	                (0U << com01) | (0U << com00) | /* OC0 disconnected */
	                (0U << wgm01) | (0U << wgm00) /* Normal mode */);
	/* Clear TOV */
	io_write(tifr, (1U << tov0));
	/* Unmask TOV IRQ, mask OC IRQ */
	tmp  = io_read(timsk);
	tmp |= (1U << toie0);
	tmp &= (~(1U << ocie0));
	io_write(timsk, tmp);

	irq_restore(flags);
}

static void __text usart_timer_stop(void)
{
	uint8_t flags, tmp;
	flags = irq_save();

	/* Stop timer */
	io_write(tccr0, (0U << cs02) | (0U << cs01) | (0U << cs00) /* No clock source */);
	/* Mask TOV IRQ */
	tmp  = io_read(timsk);
	tmp &= (~(1U << toie0));
	io_write(timsk, tmp);

	irq_restore(flags);
}

static void __text usart_rx_enable(void)
{
	uint8_t flags, tmp;
	flags = irq_save();

	tmp  = io_read(ucsrb);
	tmp |= (1U << rxen);
	io_write(ucsrb, tmp);

	irq_restore(flags);
}

static void __text usart_rx_disable(void)
{
	uint8_t flags, tmp;
	flags = irq_save();

	/* Disable receiver & flush FIFO */
	tmp  = io_read(ucsrb);
	tmp &= (~(1U << rxen));
	io_write(ucsrb, tmp);

	irq_restore(flags);
}

/* This type is atomic */
static volatile uint8_t usart_read_counter;

/* Timer/Counter0 Overflow interrupt handler */
void __interrupt __text usart_read_inc_counter()
{
	/* IRQs are disabled */
	usart_read_counter++;
}

/*
	Read upto bufsz characters from USART.
	The basic design is polling read with timeout.
	The timeout timer is reset upon each successful reception.
 */
uint16_t __text usart_read(uint8_t *buf, uint16_t bufsz)
{
	uint16_t c, nread;

	if (bufsz < 1U)
		return 0U;

	/* We are going to need this... */
	usart_rx_enable();

	/* Spin until first character is received */
	while (1) {
		c    = usart_recv();
		/* Only possible when no characters received from the hardware */
		if (!c)
			continue;
		*buf = (uint8_t) (c & 0x00ffU);
		break;
	}
	nread = 1U;

	/* Next steps heavily depend on IRQs enabled */
	if (bufsz < 2U ||
	    !((get_flags()) & (1U << bit_i)))
		goto done;

	/* This outer-inner loop design with identical condition
	   allows guaranteed timer deinitialization */
	do {
		usart_read_counter   = 0U;
		usart_timer_start();
		while (usart_read_counter < info.usart.timer_thres) {
			c            = usart_recv();
			if (!c)
				continue;
			buf[nread++] = (uint8_t) (c & 0x00ffU);
			if (nread >= bufsz)
				usart_read_counter = info.usart.timer_thres;
		}
		usart_timer_stop();
	} while (usart_read_counter < info.usart.timer_thres);
 done:
	usart_rx_disable();

	return nread;
}

void __text usart_write(const uint8_t *buf, uint16_t bufsz)
{
	uint16_t i;

	for (i = 0U; i < bufsz; i++)
		usart_xmit(buf[i]);
}

uint16_t __text usart_calc_csum(uint8_t *buf, uint16_t bufsz)
{
	uint32_t sum;
	uint16_t *ptr;

	ptr = (uint16_t *) buf;
	sum = 0U;

	while (bufsz > 1U) {
		sum   += *(ptr++);
		bufsz -= 2U;
	}

	if (bufsz > 0U)
		sum   += *((uint8_t *) ptr);

	while ((sum >> 16))
		sum    = (sum & 0xffffU) + (sum >> 16);

	return ~((uint16_t) sum);
}
