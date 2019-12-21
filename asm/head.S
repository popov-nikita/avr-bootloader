	.section	.head.text,"ax",@progbits

	.set		.Lspl,0x3d
	.set		.Lsph,0x3e

	# Unused interrupt vector
	.macro		int_stub
	reti
	nop
	.endm

	.globl		ivt
	.type		ivt,@function
ivt:
	jmp		entry			/* RESET */
	int_stub				/* INT0: External Interrupt Request 0 */
	int_stub				/* INT1: External Interrupt Request 1 */
	int_stub				/* TIMER2 COMP: Timer/Counter2 Compare Match */
	int_stub				/* TIMER2 OVF: Timer/Counter2 Overflow */
	int_stub				/* TIMER1 CAPT: Timer/Counter1 Capture Event */
	int_stub				/* TIMER1 COMPA: Timer/Counter1 Compare Match A */
	int_stub				/* TIMER1 COMPB: Timer/Counter1 Compare Match B */
	int_stub				/* TIMER1 OVF: Timer/Counter1 Overflow */
	jmp		usart_read_inc_counter	/* TIMER0 OVF: Timer/Counter0 Overflow */
	int_stub				/* SPI, STC: Serial Transfer Complete */
	int_stub				/* USART, RXC: USART, Rx Complete */
	#jmp		usart_rx		/* USART, RXC: USART, Rx Complete */
	int_stub				/* USART, UDRE: USART Data Register Empty */
	int_stub				/* USART, TXC: USART, Tx Complete */
	int_stub				/* ADC: ADC Conversion Complete */
	int_stub				/* EE_RDY: EEPROM Ready */
	int_stub				/* ANA_COMP: Analog Comparator */
	int_stub				/* TWI: Two-wire Serial Interface */
	int_stub				/* INT2: External Interrupt Request 2 */
	int_stub				/* TIMER0 COMP: Timer/Counter0 Compare Match */
	jmp		_spm_isr		/* SPM_RDY: Store Program Memory Ready */
	.size		ivt,. - ivt

	.globl		entry
	.type		entry,@function
entry:
	# Set the stack pointer
	ldi		r16,lo8(__init_sp)
	ldi		r17,hi8(__init_sp)
	out		.Lspl,r16
	out		.Lsph,r17
	# C ABI requires r1 to be always zero
	eor		r1,r1
	# Begin copying .data, .rodata into SRAM
	ldi		r30,lo8(__data_start)
	ldi		r31,hi8(__data_start)
	ldi		r26,lo8(__data_ram_start)
	ldi		r27,hi8(__data_ram_start)
	ldi		r16,lo8(__data_end)
	ldi		r17,hi8(__data_end)
.Lcopy_data_next:
	cp		r30,r16
	cpc		r31,r17
	brcc		.Lcopy_data_end
	lpm		r0,Z+
	st		X+,r0
	rjmp		.Lcopy_data_next
.Lcopy_data_end:
	# Begin clearing SRAM area allocated for .bss by linker
	ldi		r26,lo8(__bss_ram_start)
	ldi		r27,hi8(__bss_ram_start)
	ldi		r16,lo8(__bss_ram_end)
	ldi		r17,hi8(__bss_ram_end)
.Lclear_bss_next:
	cp		r26,r16
	cpc		r27,r17
	brcc		.Lclear_bss_end
	st		X+,r1
	rjmp		.Lclear_bss_next
.Lclear_bss_end:
	# Now jump to C code...
	rjmp		main
	.size		entry,. - entry
