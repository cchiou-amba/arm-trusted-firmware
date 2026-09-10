/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <platform_def.h>
#include <asm_macros.S>

	.globl	el2_pgtable_base
	.globl	el2_stack_base
	.globl	el2_vector_base
	.globl	ambarella_el2_runtime_setup

.macro	__entry
	stp	x0, x1, [sp, #0x00]
	stp	x2, x3, [sp, #0x10]
	stp	x4, x5, [sp, #0x20]
	stp	x6, x7, [sp, #0x30]
	stp	x8, x9, [sp, #0x40]
	stp	x10, x11, [sp, #0x50]
	stp	x12, x13, [sp, #0x60]
	stp	x14, x15, [sp, #0x70]
	stp	x16, x17, [sp, #0x80]
	stp	x18, x19, [sp, #0x90]
	stp	x20, x21, [sp, #0xA0]
	stp	x22, x23, [sp, #0xB0]
	stp	x24, x25, [sp, #0xC0]
	stp	x26, x27, [sp, #0xD0]
	stp	x28, x29, [sp, #0xE0]
	stp	x30, xzr, [sp, #0xF0]
.endm

.macro	__exit
	ldp	x0, x1,  [sp, #0x00]
	ldp	x2, x3, [sp, #0x10]
	ldp	x4, x5, [sp, #0x20]
	ldp	x6, x7, [sp, #0x30]
	ldp	x8, x9, [sp, #0x40]
	ldp	x10, x11, [sp, #0x50]
	ldp	x12, x13, [sp, #0x60]
	ldp	x14, x15, [sp, #0x70]
	ldp	x16, x17, [sp, #0x80]
	ldp	x18, x19, [sp, #0x90]
	ldp	x20, x21, [sp, #0xA0]
	ldp	x22, x23, [sp, #0xB0]
	ldp	x24, x25, [sp, #0xC0]
	ldp	x26, x27, [sp, #0xD0]
	ldp	x28, x29, [sp, #0xE0]
	ldp	x30, xzr, [sp, #0xF0]
.endm

/*
 * x0 - page table base
 * x1 - vector base
 * x2 - limit
 *
 * Clobber list: x3, x4, x5
 */
func ambarella_el2_runtime_setup
	mov	x5, x30

	ldr	x0, =0x30C50830
	msr	sctlr_el2, x0
	isb

	ldr	x0, el2_pgtable_base
	cbz	x0, 3f			// branch out if no need EL2 pgtbl setup
	msr	vttbr_el2, x0

	ldr	x0, el2_vector_base
	msr	vbar_el2, x0

	ldr	x1, el2_stack_base
	sub	x2, x1, x0		// max vector size

	ldr	w0, __el2_vector_size	// check if enough space to hold EL2 vector table
	cmp	w0, w2
	ble	1f
	adr	x4, el2_err_str
	bl	asm_print_str
	b	.			// die here if insufficient space

1:	bl	plat_my_core_pos	// stack size 512 Bytes for each core
	lsl	x0, x0, #9
	ldr	x1, el2_stack_base
	add	x1, x1, x0
	msr	sp_el2, x1

	ldr	x1, =VTCR_EL2_FLAG
	msr	vtcr_el2, x1

	ldr	x1, =0x80000607
	msr	hcr_el2, x1

	msr	tpidr_el2, xzr

	cbnz	x0, 3f			// relocate EL2 vector table on primary core only

	ldr	x0, el2_vector_base
	ldr	x1, =__el2_vector_start
	ldr	x2, =__el2_vector_end

2:	ldp	x3, x4, [x1, #16]!
	stp	x3, x4, [x0, #16]!
	cmp	x1, x2
	b.lt	2b

3:	ret	x5
endfunc ambarella_el2_runtime_setup


.align 8
__el2_vector_start:
	b	.			// Synchronous EL2t
	.balign 0x80
	b	.			// IRQ EL2t
	.balign 0x80
	b	.			// FIQ EL2t
	.balign 0x80
	b	.			// Error EL2t
	.balign 0x80

	b	.			// Synchronous EL2h
	.balign 0x80
	b	.                       // IRQ EL2h
	.balign 0x80
	b	.                       // FIQ EL2h
	.balign 0x80
	b	.                       // Error EL2h
	.balign 0x80

	b	guest_sync		// Synchronous 64-bit Lower
	.balign 0x80
	b	.                       // IRQ 64-bit Lower
	.balign 0x80
	b	.                       // FIQ 64-bit Lower
	.balign 0x80
	b	.                       // Error 64-bit	Lower
	.balign 0x80

	b	.			// Synchronous 32-bit Lower
	.balign 0x80
	b	.                       // IRQ 32-bit Lower
	.balign 0x80
	b	.                       // FIQ 32-bit Lower
	.balign 0x80
	b	.                       // Error 32-bit	Lower
	.balign 0x80

guest_sync:
	__entry

	mrs	x0, elr_el2
	mrs	x1, spsr_el2
	mrs	x2, far_el2
	mrs	x3, esr_el2
	mrs	x4, hpfar_el2
	stp	x0, x1, [sp, #0x100]
	stp	x2, x3, [sp, #0x110]
	stp	x4, xzr,[sp, #0x120]

	mov	w0, #0xc2000000		// 0xc2000701 is AMBA_SIP_SVC_EL2_FAULT
	movk	w0, #0x0701
	mov	x1, sp
	smc	#0
	isb
	dsb	sy

	cbz	x0, 1f

	ldp	x0, x1, [sp, #0x130]
	ldp	x2, x3, [sp, #0x140]
	msr	elr_el1, x0
	msr	spsr_el1, x1
	msr	far_el1, x2
	msr	esr_el1, x3

1:	ldp	x0, x1, [sp, #0x100]
	msr	elr_el2, x0
	msr	spsr_el2, x1

	__exit
	dsb     sy
	isb
	eret

.align 4
__el2_vector_size:	.quad	__el2_vector_end - __el2_vector_start

__el2_vector_end:


/* data section */
.section .data
.align	4

el2_pgtable_base:	.quad	0
el2_stack_base:		.quad	0
el2_vector_base:	.quad	0

/* string section */
.section .rodata.el2_err_str, "aS"
el2_err_str:		.asciz "Relocate EL2 Out-of-range.\n"

