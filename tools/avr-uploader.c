#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

/* FIX ME */
#define AVR_SPEED    B38400
#define USART_BUFSZ  ((64 * 2) * 2)
#define FLASH_SZ     (0x1c00U * 2)

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
} __attribute__((packed));

uint16_t usart_calc_csum(uint8_t *buf, uint16_t bufsz)
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

static inline void die(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);

	exit(-1);
}

static void upload_program(int tty_fd, const char *path)
{
	uint8_t ack_data[2], msgbuf[USART_BUFSZ];
	struct hdr *hdr = (struct hdr *) msgbuf;
	uint8_t *pld = (uint8_t *) (&hdr[1]);
	const unsigned int pldsz = sizeof(msgbuf) - sizeof(struct hdr);
	struct {
		const char *path;
		int fd;
		unsigned int size;
		uint8_t *ptr;
	} program;
	struct stat st;
	uint8_t *saved_ptr;
	unsigned int saved_size;
	program.path = path;
	program.fd   = open(program.path, O_RDONLY);
	if (program.fd < 0)
		die("UPLOAD PROGRAM (open): \"%s\"\n", strerror(errno));
	if (fstat(program.fd, &st) < 0)
		die("UPLOAD PROGRAM (fstat): \"%s\"\n", strerror(errno));
	program.size = st.st_size;
	if (program.size > FLASH_SZ)
		die("UPLOAD PROGRAM (invalid size): %u\n", program.size);
	program.ptr  = mmap(NULL, program.size, PROT_READ, MAP_PRIVATE, program.fd, 0);
	if (program.ptr == ((const uint8_t *) MAP_FAILED))
		die("UPLOAD PROGRAM (mmap): \"%s\"\n", strerror(errno));
	saved_ptr    = program.ptr;
	saved_size   = program.size;
	close(program.fd);
	program.fd   = -1;

	hdr->filesz = program.size;
	while (program.size > 0) {
		unsigned int msgsz;
		int nread;

		msgsz = (program.size > pldsz) ? pldsz : program.size;
		memcpy(pld, program.ptr, msgsz);
		hdr->len  = msgsz + sizeof(struct hdr);
		hdr->csum = usart_calc_csum((uint8_t *) &hdr->len,
		                            hdr->len - offsetof(struct hdr, len));
		if (write(tty_fd, hdr, hdr->len) != hdr->len)
			die("UPLOAD PROGRAM (write): \"%s\"\n", "failure");
		nread     = read(tty_fd, ack_data, sizeof(ack_data));
		if (nread <= 0)
			die("UPLOAD PROGRAM (read): \"%s\"\n", "failure");
		if (nread == 1) {
			/* Success */
			printf("COMPLETE transmission of %u - %u part\n",
			       (unsigned int) (program.ptr - saved_ptr),
			       (unsigned int) (program.ptr - saved_ptr) + msgsz);
			program.ptr  += msgsz;
			program.size -= msgsz;
		} else {
			/* Failure. Try to re-transmit current part */
			printf("REPEAT transmission of %u - %u part\n",
			       (unsigned int) (program.ptr - saved_ptr),
			       (unsigned int) (program.ptr - saved_ptr) + msgsz);
		}
	}

	munmap(saved_ptr, saved_size);
}

int main(int argc, char **argv)
{
	int tty_fd, flags;
	struct termios tios;

	if (argc < 3)
		die("USAGE: %s <tty device> <file name to flash>\n", argv[0]);

	errno  = 0;

	tty_fd = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (tty_fd < 0)
		die("ERROR (open): \"%s\"\n", strerror(errno));

	flags  = fcntl(tty_fd, F_GETFL);
	if (flags < 0)
		die("ERROR (fcntl#1): \"%s\"\n", strerror(errno));
	flags &= ~(O_NONBLOCK);
	if (fcntl(tty_fd, F_SETFL, flags) < 0)
		die("ERROR (fcntl#2): \"%s\"\n", strerror(errno));

	if (tcgetattr(tty_fd, &tios) < 0)
		die("ERROR (tcgetattr): \"%s\"\n", strerror(errno));

	cfsetispeed(&tios, AVR_SPEED);
	cfsetospeed(&tios, AVR_SPEED);

	tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);
	tios.c_oflag &= ~(OPOST);
	tios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tios.c_cflag &= ~(CSIZE | PARENB);
	tios.c_cflag |= CS8;
	/* 2 bytes - maximum message length */
	tios.c_cc[VMIN]  = 2;
	/* 1 seconds interbyte timeout */
	tios.c_cc[VTIME] = 10;

	if (tcsetattr(tty_fd, TCSANOW, &tios) < 0)
		die("ERROR (tcsetattr): \"%s\"\n", strerror(errno));

	upload_program(tty_fd, argv[2]);

	exit(0);
}
