ENTRY(ivt)
OUTPUT_FORMAT(elf32-avr)
OUTPUT_ARCH(avr:5)
TARGET(elf32-avr)

__ram_start        = 0x60;
__ram_size         = 0x400;
__init_sp          = __ram_start + __ram_size - 1;
__flash_size       = (0x2000 * 2);
__flash_page       = (64 * 2);
__page_offset_mask = __flash_page - 1;
__page_mask        = ~(__page_offset_mask);

PHDRS
{
	/* RX */
	.text PT_LOAD FLAGS ( 5 ) ;
	/* RW */
	.data PT_LOAD FLAGS ( 6 ) ;
}

SECTIONS {
	.head.text (0x1c00 * 2) : ALIGN(2) {
		/*
			File `asm/head.o` contains IVT in its .head.text section.
			So it should always be first.
		 */
		__text_start     = ABSOLUTE(.);
		__pc_start       = ABSOLUTE(__text_start >> 1);
		*asm/head.o(.head.text)
		*(.head.text)
	} :.text

	.text : ALIGN(2) {
		KEEP(*(*.text))
		__text_end       = ABSOLUTE(.);
		__pc_end         = ABSOLUTE(__text_end >> 1);
	} :.text

	.data : ALIGN(2) {
		__data_start     = ABSOLUTE(.);
		__data_ram_start = ABSOLUTE(__ram_start);
		*(*.data)
		*(*.rodata*)
		__data_end       = ABSOLUTE(.);
		__data_ram_end   = ABSOLUTE(__data_end - __data_start + __data_ram_start);
	} :.data

	/* This should always be last */
	.bss : ALIGN(2) {
		__bss_start      = ABSOLUTE(.);
		__bss_ram_start  = ABSOLUTE(__bss_start - __data_end + __data_ram_end);
		/* Buffer used to hold flash data for SPM instruction */
		spm_buffer       = .;
		.               += __flash_page;
		spm_buffer_end   = .;
		/*
			Buffer where USART characters are stored.
			See `include/proto.h:struct hdr` definition.
		*/
		usart_buffer     = .;
		.               += (2 * __flash_page);
		usart_buffer_end = .;
		*(*.bss)
		*(COMMON)
		__bss_end        = ABSOLUTE(.);
		__bss_ram_end    = ABSOLUTE(__bss_end - __bss_start + __bss_ram_start);
	} :.data

	/DISCARD/ : {
		*(*)
	}
}

ASSERT(__data_end <= __flash_size, "Flash space overflow!")
/*
	We allocate no more than half of available RAM for DATA and BSS.
	Everything else goes to stack and other needs.
*/
ASSERT((__bss_ram_end - __data_ram_start) <= (__ram_size / 2), "RAM space overflow!")
ASSERT((__flash_page & __page_offset_mask) == 0, "__flash_page is not power of 2!")
ASSERT(__text_start == (__pc_start * 2), "__text_start is not aligned!")
ASSERT(__text_end == (__pc_end * 2), "__text_end is not aligned!")
