/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <asm_macros.S>
#include <platform_def.h>

	.globl	platform_mem_init
	.globl	plat_reset_handler
	.globl	plat_crash_console_init
	.globl	plat_crash_console_putc
	.globl	plat_crash_console_flush
	.globl	plat_get_my_entrypoint
	.globl	ambarella_sec_entrypoint
	.globl	ambarella_fake_bootentry

	/* -----------------------------------------------------
	 * void platform_mem_init(void);
	 * Setup the secure memory region if secure boot is enabled.
	 * -----------------------------------------------------
	 */
func platform_mem_init
	ret
endfunc platform_mem_init


	/* -----------------------------------------------------
	 * void plat_reset_handler(void);
	 * -----------------------------------------------------
	 */
func plat_reset_handler
	b	plat_crash_console_init
	ret
endfunc plat_reset_handler


	/* ---------------------------------------------
	 * int plat_crash_console_init(void)
	 * Function to initialize the crash console
	 * without a C Runtime to print crash report.
	 * Clobber list : x0, x1, x2
	 * ---------------------------------------------
	 */
func plat_crash_console_init
	mov_imm	x0, UART0_BASE
	mov_imm	x1, UART_CLOCK
	mov_imm	x2, UART_BAUDRATE
	b	console_ambarella_core_init
	ret
endfunc plat_crash_console_init


	/* ---------------------------------------------
	 * int plat_crash_console_putc(void)
	 * Function to print a character on the crash
	 * console without a C Runtime.
	 * Clobber list : x1, x2
	 * ---------------------------------------------
	 */
func plat_crash_console_putc
	mov_imm x1, UART0_BASE
	b	console_ambarella_core_putc
	ret
endfunc plat_crash_console_putc


	/* ---------------------------------------------
	 * int plat_crash_console_flush()
	 * Function to force a write of all buffered
	 * data that hasn't been output.
	 * Out : return -1 on error else return 0.
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func plat_crash_console_flush
	mov	x0, #0
	ret
endfunc plat_crash_console_flush


	/* -----------------------------------------------------
	 * unsigned long plat_get_my_entrypoint (void);
	 *
	 * Main job of this routine is to distinguish between
	 * a cold and warm boot. If the ambarella_sec_entrypoint for
	 * this CPU is present, then it's a warm boot.
	 *
	 * -----------------------------------------------------
	 */
func plat_get_my_entrypoint
	mov	x29, x30
	bl	plat_my_core_pos
	mov	x3, x0
	adr	x1, ambarella_sec_entrypoint
	lsl	x2, x0, #3
1:	ldr	x0, [x1, x2]
	cbnz	x0, 2f
	cbz	x3, 2f	/* core0 must can go */
	wfe
	b	1b
2:	mov	x30, x29
	ret
endfunc plat_get_my_entrypoint


.text
/* CV5/N1 reset vector need aligned to 256 at least. use 512 here */
.balign 512
ambarella_fake_bootentry:
	b	.

/* data section */
.data
.align	3
ambarella_sec_entrypoint:
	.rept PLATFORM_CORE_COUNT
	.quad	0x0
	.endr
