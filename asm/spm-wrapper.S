	# C ABI:
	#  Call-used registers (r18-r27, r30-r31) -- free to use in assembly
	#  Call-saved registers (r2-r17, r28-r29) -- we must preserve them
	#  Fixed registers (r0, r1):
	#   r0 - temporary register               -- free to use in assembly
	#   r1 - zero reg                         -- upon returning to C code we must zero it
	#  Function call conventions:
	#   Arguments - allocated r25 to r8.
	#   All arguments are aligned to start in even-numbered registers
	#   (odd-sized arguments, including char, have one free register above them).
	#   This allows making better use of the movw instruction on the enhanced core.
	#   Values are passed in little endian order:
	#    Register with lesser number has less significant byte.
	.section	.text,"ax",@progbits

	.set		.Leecr,		0x1c
	.set		.Leere,		0     ; EEPROM Read Enable
	.set		.Leewe,		1     ; EEPROM Write Enable
	.set		.Leemwe,	2     ; EEPROM Master Write Enable
	.set		.Leerie,	3     ; EEPROM Ready Interrupt Enable

	.set		.Lspmcr,	0x37
	.set		.Lspmen,	0     ; Store Program Memory Enable
	.set		.Lpgers,	1     ; Page Erase
	.set		.Lpgwrt,	2     ; Page Write
	.set		.Lblbset,	3     ; Boot Lock Bit Set
	.set		.Lrwwsre,	4     ; Read-While-Write Section Read Enable
	.set		.Lrwwsb,	6     ; Read-While-Write Section Busy
	.set		.Lspmie,	7     ; SPM Interrupt Enable

	.set		.Lsreg,		0x3f

	.macro		irq_save	reg:req
	in		\reg,.Lsreg
	cli
	.endm

	.macro		irq_restore	reg:req
	out		.Lsreg,\reg
	.endm

	.macro		spm_spin	func:req,reg:req
.L\func\()_spin:
	in		\reg,.Lspmcr
	sbrc		\reg,.Lspmen
	rjmp		.L\func\()_spin
	.endm

	# void _erase_page(uint16_t flash_addr)
	.globl		_erase_page
	.type		_erase_page,@function
_erase_page:
	irq_save	r18
	spm_spin	_erase_page,r0
	movw		r30,r24
	andi		r30,lo8(__page_mask)
	andi		r31,hi8(__page_mask)
	ldi		r19,(1 << .Lspmie) | (1 << .Lpgers) | (1 << .Lspmen)
	out		.Lspmcr,r19
	spm
	irq_restore	r18
	ret
	.size		_erase_page,. - _erase_page

	# void _store_temp_buffer(uint16_t flash_addr, uint16_t data)
	.globl		_store_temp_buffer
	.type		_store_temp_buffer,@function
_store_temp_buffer:
	irq_save	r18
	spm_spin	_store_temp_buffer,r0
	movw		r0,r22
	movw		r30,r24
	ldi		r19,(1 << .Lspmen)
	out		.Lspmcr,r19
	spm
	eor		r1,r1
	irq_restore	r18
	ret
	.size		_store_temp_buffer,. - _store_temp_buffer

	# void _write_page(uint16_t flash_addr)
	.globl		_write_page
	.type		_write_page,@function
_write_page:
	irq_save	r18
	spm_spin	_write_page,r0
	movw		r30,r24
	andi		r30,lo8(__page_mask)
	andi		r31,hi8(__page_mask)
	ldi		r19,(1 << .Lspmie) | (1 << .Lpgwrt) | (1 << .Lspmen)
	out		.Lspmcr,r19
	spm
	irq_restore	r18
	ret
	.size		_write_page,. - _write_page

	# void _enable_rww_sect(void)
	.globl		_enable_rww_sect
	.type		_enable_rww_sect,@function
_enable_rww_sect:
	irq_save	r18
	spm_spin	_enable_rww_sect,r0
	ldi		r19,(1 << .Lrwwsre) | (1 << .Lspmen)
	out		.Lspmcr,r19
	spm
	irq_restore	r18
	ret
	.size		_enable_rww_sect,. - _enable_rww_sect

	# void _set_lock_bits(uint8_t bits)
	.globl		_set_lock_bits
	.type		_set_lock_bits,@function
_set_lock_bits:
	irq_save	r18
	spm_spin	_set_lock_bits,r0
	# honor 'future compatibility' requirement
	ori		r24,(1 << 7) | (1 << 6) | (1 << 1) | (1 << 0)
	mov		r0,r24
	ldi		r30,0x01
	ldi		r31,0x00
	ldi		r19,(1 << .Lspmie) | (1 << .Lblbset) | (1 << .Lspmen)
	out		.Lspmcr,r19
	spm
	irq_restore	r18
	ret
	.size		_set_lock_bits,. - _set_lock_bits

	# This 3 functions share the common part
	# So they are not C-alike - body of the function is not contiguous
	# Therefore set symbols sizes to 0, so code analyzing tools skip them
	.globl		_get_lock_bits
	# uint8_t _get_lock_bits(void)
	.type		_get_lock_bits,@function
_get_lock_bits:
	ldi		r30,0x01
	ldi		r31,0x00
	rjmp		.Ldo_lpm
	.size		_get_lock_bits,0

	.globl		_get_lfuse_bits
	# uint8_t _get_lfuse_bits(void)
	.type		_get_lfuse_bits,@function
_get_lfuse_bits:
	ldi		r30,0x00
	ldi		r31,0x00
	rjmp		.Ldo_lpm
	.size		_get_lfuse_bits,0

	.globl		_get_hfuse_bits
	# uint8_t _get_hfuse_bits(void)
	.type		_get_hfuse_bits,@function
_get_hfuse_bits:
	ldi		r30,0x03
	ldi		r31,0x00
	rjmp		.Ldo_lpm
	.size		_get_hfuse_bits,0

.Ldo_lpm:
	irq_save	r18
	spm_spin	do_lpm,r0
	ldi		r19,(1 << .Lblbset) | (1 << .Lspmen)
	out		.Lspmcr,r19
	lpm		r24,Z
	irq_restore	r18
	ret

	.globl		_spm_isr
	.type		_spm_isr,@function
_spm_isr:
	# stash call-used registers and temporary registers
	.irp		regno,0,18,19,20,21,22,23,24,25,26,27,30,31
	push		r\regno
	.endr
	in		r0,.Lsreg
	push		r0
	# Always disable SPM ready interrupt
	in		r18,.Lspmcr
	andi		r18,~(1 << .Lspmie)
	out		.Lspmcr,r18
	# Just to be sure that jumping to C code is safe
	eor		r1,r1
	call		spm_handler
	pop		r0
	out		.Lsreg,r0
	.irp		regno,31,30,27,26,25,24,23,22,21,20,19,18,0
	pop		r\regno
	.endr
	reti
	.size		_spm_isr,. - _spm_isr
