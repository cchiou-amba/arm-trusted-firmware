/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <asm_macros.S>
#include <console_macros.S>
#include <uart_ambarella.h>
#include <ambarella_def.h>

	/*
	 * "core" functions are low-level implementations that don't require
	 * writable memory and are thus safe to call in BL1 crash context.
	 */
	.globl console_ambarella_core_init
	.globl console_ambarella_core_putc
	.globl console_ambarella_core_getc

	.globl console_ambarella_register
	.globl console_ambarella_putc
	.globl console_ambarella_getc

	.macro uart_pin_cfg pin:req
	  ldr	x3, =(IOMUX_BASE + 0xC * (\pin / 32))
	  ldr	w4, [x3, 0x0]
	  orr	w4, w4, #(1 << (\pin % 32))
	  str	w4, [x3, 0x0]
	  ldr	w4, [x3, 0x4]
	  bic	w4, w4, #(1 << (\pin % 32))
	  str	w4, [x3, 0x4]
	  ldr	w4, [x3, 0x8]
	  bic	w4, w4, #(1 << (\pin % 32))
	  str	w4, [x3, 0x8]
	.endm


	/* -----------------------------------------------
	 * int console_ambarella_core_init(uintptr_t base_addr,
	 * unsigned int uart_clk, unsigned int baud_rate)
	 * Function to initialize the console without a
	 * C Runtime to print debug information. This
	 * function will be accessed by console_init and
	 * crash reporting.
	 * In: x0 - console base address
	 *     x1 - Uart clock in Hz
	 *     x2 - Baud rate
	 * Out: return 1 on success else 0 on error
	 * Clobber list : x1, x2, x3, x4
	 * -----------------------------------------------
	 */
func console_ambarella_core_init
	/* Check the input base address */
	cbz	x0, init_fail
	/* Check baud rate and uart clock for sanity */
	cbz	w1, init_fail
	cbz	w2, init_fail

	uart_pin_cfg	AMBARELLA_UART_PIN_RX
	uart_pin_cfg	AMBARELLA_UART_PIN_TX

	ldr	x3, =IOMUX_BASE
	mov	w4, #1
	str	w4, [x3, #IOMUX_CTRL_SET_OFFSET]
	str	wzr, [x3, #IOMUX_CTRL_SET_OFFSET]

	ldr	x3, =RCT_BASE
	str	w4, [x3, #CG_UART_OFFSET]

	strb	wzr, [x0, #UART_SRR_OFFSET]
	mov	w4, #0x80
	strb	w4, [x0, #UART_LC_OFFSET]
	mov	w4, #13
	strb	w4, [x0, #UART_DLL_OFFSET]
	strb	wzr, [x0, #UART_DLH_OFFSET]
	mov	w4, #0x3
	strb	w4, [x0, #UART_LC_OFFSET]
	mov	w0, #1
	ret
init_fail:
	mov	w0, #0
	ret
endfunc console_ambarella_core_init


	/* -----------------------------------------------
	 * int console_ambarella_register(uintptr_t baseaddr,
	 *     uint32_t clock, uint32_t baud,
	 *     console_t *console);
	 * Function to initialize and register a new Ambarella
	 * console. Storage passed in for the console struct
	 * *must* be persistent (i.e. not from the stack).
	 * In: x0 - UART register base address
	 *     w1 - UART clock in Hz
	 *     w2 - Baud rate
	 *     x3 - pointer to empty console_t struct
	 * Out: return 1 on success, 0 on error
	 * Clobber list : x0, x1, x2, x6, x7, x14
	 * -----------------------------------------------
	 */
func console_ambarella_register
	mov	x7, x30
	mov	x6, x3
	cbz	x6, register_fail
	str	x0, [x6, #CONSOLE_T_BASE]

	bl	console_ambarella_core_init
	cbz	x0, register_fail

	mov	x0, x6
	mov	x30, x7
	finish_console_register ambarella putc=1, getc=ENABLE_CONSOLE_GETC, flush=0

register_fail:
	ret	x7
endfunc console_ambarella_register


	/* --------------------------------------------------------
	 * int console_ambarella_core_putc(int c, uintptr_t base_addr)
	 * Function to output a character over the console. It
	 * returns the character printed on success or -1 on error.
	 * In : x0 - character to be printed
	 *      x1 - console base address
	 * Out : return -1 on error else return character.
	 * Clobber list : x2
	 * --------------------------------------------------------
	 */
func console_ambarella_core_putc
	/* Check the input parameter */
	cbz	x1, putc_error

0:	ldr	w2, [x1, #UART_LS_OFFSET]
	tbz	w2, #6, 0b
	mov	w2, #'\n'
	cmp	w0, w2
	b.ne	2f
	mov	w2, #'\r'
	str	w2, [x1, #UART_TH_OFFSET]
1:  	ldr	w2, [x1, #UART_LS_OFFSET]
	tbz	w2, #6, 1b
2:	str	w0, [x1, #UART_TH_OFFSET]
	ret
putc_error:
	mov	x0, #-1
	ret
endfunc console_ambarella_core_putc

	/* --------------------------------------------------------
	 * int console_ambarella_putc(int c, console_t *console)
	 * Function to output a character over the console. It
	 * returns the character printed on success or -1 on error.
	 * In : w0 - character to be printed
	 *      x1 - pointer to console_t structure
	 * Out : return -1 on error else return character.
	 * Clobber list : x2
	 * --------------------------------------------------------
	 */
func console_ambarella_putc
	ldr	x1, [x1, #CONSOLE_T_BASE]
	b	console_ambarella_core_putc
endfunc console_ambarella_putc


	/* ---------------------------------------------
	 * int console_ambarella_core_getc(uintptr_t base_addr)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or -1 on error.
	 * In : x0 - console base address
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_ambarella_core_getc
	mov	x0, #-1
	ret
endfunc console_ambarella_core_getc

	/* ---------------------------------------------
	 * int console_ambarella_getc(console_t *console)
	 * Function to get a character from the console.
	 * It returns the character grabbed on success
	 * or -1 on if no character is available.
	 * In :  x0 - pointer to console_t structure
	 * Out : w0 - character if available, else -1
	 * Clobber list : x0, x1
	 * ---------------------------------------------
	 */
func console_ambarella_getc
	ldr	x0, [x0, #CONSOLE_T_BASE]
	b	console_ambarella_core_getc
endfunc console_ambarella_getc

