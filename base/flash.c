#include <comp-defs.h>
#include <spm-wrapper.h>
#include <flash.h>

enum spm_state {
	/* No current action */
	spm_noop    = 0,
	/* Erasing initiated */
	spm_erasing = 1,
	/* Writing initiated */
	spm_writing = 2,
	/* Setting lock bits initiated */
	spm_locking = 3,
};

struct spm_data {
	enum spm_state state;
	uint16_t address;
	callback_t cb;
};

static volatile struct spm_data ___data = {
	.state = spm_noop,
	.address = 0x0000U,
};

void __text spm_handler(void)
{
	callback_t cb;

	switch (___data.state) {
	case spm_erasing:
		_write_page(___data.address);
		___data.state = spm_writing;
		break;
	case spm_writing:
		_enable_rww_sect();
		___data.address += ((uint16_t) (&__flash_page));
		/* FALLTHROUGH */
	case spm_locking:
		cb = ___data.cb;
		___data.state = spm_noop;
		___data.cb    = (callback_t) ((uint16_t) 0);
		cb();
		break;
	default:
		die();
	}
}

/*
	The routine initiates non-blocking flash write operation consisting of
	filling hardware buffer, page erasing and page writing.
	The function is written in a way so spm_buffer may be instantly reused upon
	function return.
 */
void __text write_page(callback_t cb)
{
	uint16_t *p;
	uint16_t addr;
	uint8_t flags;

	flags = irq_save();

	addr = ___data.address;
	if ((___data.state != spm_noop) ||
	    (addr >= ((uint16_t) (&__text_start))))
		die();

	for (p = spm_buffer;
	     p < spm_buffer_end;
	     p++, addr += 2U) {
		_store_temp_buffer(addr, *p);
	}
	_erase_page(___data.address);

	___data.state = spm_erasing;
	___data.cb = cb;

	irq_restore(flags);
}

void __text set_lock_bits(callback_t cb, uint8_t bits)
{
	uint8_t flags;

	flags = irq_save();

	if (___data.state != spm_noop)
		die();
	___data.state = spm_locking;
	___data.cb = cb;
	_set_lock_bits(bits);

	irq_restore(flags);
}
