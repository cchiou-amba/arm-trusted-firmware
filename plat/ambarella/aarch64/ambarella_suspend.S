/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <platform_def.h>

	.globl ambarella_relocate_suspend
	.globl ambarella_do_suspend

.macro	delay_20ms
	mov	w0, #24000
	mov	w1, #20
	mul	w0, w0, w1
	mov	w1, #0x1
	str	w1, [x25, #RCT_TIMER_CTRL_OFFSET]
	dsb	sy
	str	wzr, [x25, #RCT_TIMER_CTRL_OFFSET]
0:	ldr	w1, [x25, #RCT_TIMER_OFFSET]
	cmp	w0, w1
	bhi	0b
.endm

.macro	gpio_send_srs_signal

	ldr	w4, [x27, #4]				/* LOW 32 */
	ldr	w5, [x27, #8]				/* High 32 */
	lsl	x5, x5, #32
	orr	x5, x5, x4
	cbz	x5, send_srs_done
	ldr	w3, [x27, #0]				/* MASK */

#if defined(PLATFORM_SUPPORT_CMVI)
	ldr	w4, [x27, #0x0C]			/* CMVI_SRS_NOTIFY_REG_L */
	ldr	w6, [x27, #0x10]			/* CMVI_SRS_NOTIFY_REG_H */
	lsl	x6, x6, #32
	orr     x6, x6, x4
	mov	w0, #CMVI_SRS_MARK
	str	w0, [x6, #0]
	dmb	st
	str	w3, [x5, #0]				/* cmvi: send IPI0 to cm3 */
#else
	/*
	 * different board design with different init pin level
	 * mcu can use the same logic to detect the signal if
	 * the start signal is same (high-level-long enough) for all boards.
	 */
	str	w3, [x5, #GPIO_DATA_OFFSET]	/* 1: long enough for signal aligned */
	delay_20ms
	delay_20ms
	delay_20ms
	str	wzr, [x5, #GPIO_DATA_OFFSET]	/* 0 */
	delay_20ms
	str	w3, [x5, #GPIO_DATA_OFFSET]	/* 1 */
	delay_20ms
	str	wzr, [x5, #GPIO_DATA_OFFSET]	/* 0 */
	delay_20ms
	str	w3, [x5, #GPIO_DATA_OFFSET]	/* 1 */
	delay_20ms
	str	wzr, [x5, #GPIO_DATA_OFFSET]	/* 0 */
	delay_20ms
	str	w3, [x5, #GPIO_DATA_OFFSET]	/* 1: compatile mode just in case */
	delay_20ms
	str	wzr, [x5, #GPIO_DIR_OFFSET]	/* input */
#endif

send_srs_done:

#if defined(DEBUG_SRS)	/* debug used only */
	mov	w0, 'S'
	str	w0, [x28, #0]
	delay_20ms
#endif

.endm

#if (DDRC_VERSION > 1)

.macro trigger_uinst
	str	w1, [x21, #0xac]	/* DRAM_UINST1 */
	str	w2, [x21, #0xb0]	/* DRAM_UINST2 */
	str	w4, [x21, #0xb8]	/* DRAM_UINST4 */
	mov	w0, #1
	str	w0, [x21, #0xbc]	/* DRAM_UINST5 */
	dsb	st
.endm

#if (DDRC_VERSION == 2)
.macro wait_complete, op, bit, label
	tbz	w10, #1, 100f
	\op	w0, \bit, \label
100:	tbz	w10, #2, 101f
	\op	w1, \bit, \label
101:
.endm

.macro dtte_stop, ddrcx
	mov	\ddrcx, \ddrcx
.endm
#endif

#if (DDRC_VERSION == 3)

#define DRAM_DTTE_ADDR				0x000000f8
#define DRAM_DTTE_DATA				0x000000fc
#define DTTE_TRAIN_CONFIG0_REG			0x00000008
#define DTTE_TRAIN_ENABLE_REG			0x0000001c
#define DTTE_TEMPORAL_FILTER_STRENGTH0_REG	0x000001c8
#define DTTE_TEMPORAL_FILTER_STRENGTH1_REG	0x000001cc
#define DTTE_GRID_STAT_READY_REG		0x000002e0
#define DTTE_ALGO_ERROR_ID_REG			0x00000148

#if defined(AMBARELLA_CV3AD655)
.macro wait_complete, op, bit, label
	\op	w0, \bit, \label
	\op	w1, \bit, \label
	\op	w2, \bit, \label
	\op	w3, \bit, \label
.endm
#elif defined(AMBARELLA_CV8)
.macro wait_complete, op, bit, label
	\op	w0, \bit, \label
	\op	w1, \bit, \label
.endm
#else
.macro wait_complete, op, bit, label
	\op	w0, \bit, \label
.endm
#endif

.macro dtte_write_single, regaddr, regdata, ddrcx
	str	\regaddr, [\ddrcx, #DRAM_DTTE_ADDR]
	dsb	st
	str	\regdata, [\ddrcx, #DRAM_DTTE_DATA]
	dsb	sy
	isb
.endm

.macro dtte_read_single, regaddr, regdata, ddrcx
	str	\regaddr, [\ddrcx, #DRAM_DTTE_ADDR]
	dsb	st
	ldr	\regdata, [\ddrcx, #DRAM_DTTE_DATA]
	dsb	ld
	isb
.endm

.macro dtte_stop, ddrc
	mov	w0, #DTTE_TRAIN_ENABLE_REG
	mov	w1, #DTTE_GRID_STAT_READY_REG
	mov	w2, #DTTE_ALGO_ERROR_ID_REG
	mov	x4, \ddrc
200:
	dtte_read_single w0, w3, x4
	tbnz	w3, #4, 201f		/* check dtte done? */
	dtte_write_single w0, wzr, x4	/* no done, stop dtte */
	mov w3, #DTTE_TRAIN_CONFIG0_REG
	dtte_write_single w3, wzr, x4	/* Clear train IDs to reduce stop time */
	mov w3, #DTTE_TEMPORAL_FILTER_STRENGTH0_REG
	dtte_write_single w3, wzr, x4	/* Clear temporal filter to reduce stop time */
	mov w3, #DTTE_TEMPORAL_FILTER_STRENGTH1_REG
	dtte_write_single w3, wzr, x4	/* Clear temporal filter to reduce stop time */
	mov	w3, #1
	dtte_write_single w1, w3, x4
	mvn	w3, wzr
	dtte_write_single w2, w3, x4	/* clear error */
	b	200b
201:
	dtte_write_single w0, wzr, x4	/* stop dtte */
202:
	dtte_read_single w0, w3, x4	/* re-poll dtte done to make sure */
	tbz	w3, #4, 202b
.endm
#endif

.align	3
ambarella_suspend_exec:
	add	x21, x20, #DDRCB	/* broadcast base register */
	add	x22, x20, #DDRC0	/* ddrc0 base regsiter */
	add	x23, x20, #DDRC1	/* ddrc1 base regsiter */
#if defined(AMBARELLA_CV3AD655)
	add	x29, x20, #DDRC2	/* ddrc2 base register */
	add	x30, x20, #DDRC3	/* ddrc3 base register */
#endif
	ldr	w10, [x20]		/* used for checking host num */

#if defined(AMBARELLA_CV5)
	tst	w10, #2			/* cv52: if host0(bit1) is zero */
	csel	x22, x23, x22, eq	/* CV52(DDRC0<=DDRC1) else cv5(keep) */
#endif

#if defined(AMBARELLA_CV7)
/* CV7 DDRC is special. e.g. CV7M only DDRC1 can access */
	and	w0, w10, #0x30
	cmp	w0, #0x30
	beq	next			/* both DDRC1,DDRC0 used */

	and	w0, w10, #0x2
	cbnz	w0, ddrc1_only

ddrc0_only:
	mov	x21, x22
	mov	x23, x22
	b	next

ddrc1_only:
	mov	x21, x23
	mov	x22, x23
next:
#endif

	ldr	w9, [x22, #DDRC_TYPE]	/* bit0:0(lpddr4); 1(lpddr5) */

	/* disable DRAM access && stop DTTE if possible */
#if (DDRC_VERSION == 3)
	/* Reduce DTTE throttle to speedup the stop */
	ldr	w0, [x22, #DRAM_DTTE_CONFIG3]
	bic	w0, w0, #0xff
	str	w0, [x21, #DRAM_DTTE_CONFIG3]
#endif

	ldr	w0, [x22]
	bic	w0, w0, #(1 << 0)
	str	w0, [x21]
	dsb	sy
	dtte_stop	x22
	dtte_stop	x23
#if defined(AMBARELLA_CV3AD655)
	dtte_stop	x29
	dtte_stop	x30
#endif

	/* wait for DDRC idle */
1:	ldr	w0, [x22, #DDRC_SRS_CTRL_STATUS]
	ldr	w1, [x23, #DDRC_SRS_CTRL_STATUS]
#if defined(AMBARELLA_CV3AD655)
	ldr	w2, [x29, #DDRC_SRS_CTRL_STATUS]
	ldr	w3, [x30, #DDRC_SRS_CTRL_STATUS]
#endif
	wait_complete tbz, #2, 1b

	/* wait for 8*tRFC to allow refresh credit to be maxed out */
	mov	w0, #0x1
	str	w0, [x25, #RCT_TIMER_CTRL_OFFSET]
	str	wzr, [x25, #RCT_TIMER_CTRL_OFFSET]
2:	ldr	w0, [x25, #RCT_TIMER_OFFSET]
	cmp	w0, #2400
	blo	2b

	/* disable auto-refresh */
	ldr	w0, [x22]
	bic	w0, w0, #(1 << 1)
	str	w0, [x21]
	dsb	st

	/* jump if LPDDR4 */
	tbz	w9, #0, 5f
	/* disable LP5 background ZQ calibration */
	mov	w0, #0x9f1c0000
	orr	w0, w0, #0x2
	str	w0, [x21, #0x30]
	/* wait for complete */
3:	ldr	w0, [x22, #0x30]
	ldr	w1, [x23, #0x30]
#if defined(AMBARELLA_CV3AD655)
	ldr	w2, [x29, #0x30]
	ldr	w3, [x30, #0x30]
#endif

	wait_complete tbnz, #31, 3b

	/* turnoff WCK Buffer in LP5 device */
	mov	w1, #0x0000000d
	mov	w2, #0x0000007c
	mov	w4, #0x00000303
	trigger_uinst
	/* wait for complete */
4:	ldr	w0, [x22, #0xbc]
	ldr	w1, [x23, #0xbc]
#if defined(AMBARELLA_CV3AD655)
	ldr	w2, [x29, #0xbc]
	ldr	w3, [x30, #0xbc]
#endif

	wait_complete tbnz, #0, 4b

5:	/* issue SRE to enter into SR */
	tst	w9, #1		/* check LPDDR5 or LPDDR4 */
	mov	w1, #0x00004068	/* LPDDR5 SRS Pd cmd */
	mov	w0, #0x00000018	/* LPDDR4 SRS Pd cmd */
	csel	w2, w0, w1, eq	/* w9.bit0 == 0 ? w0:w1 */
	mov	w1, #0x0000000d
	mov	w4, #0x00000303
	trigger_uinst
	/* wait for complete */
6:	ldr	w0, [x22, #0xbc]
	ldr	w1, [x23, #0xbc]
#if defined(AMBARELLA_CV3AD655)
	ldr	w2, [x29, #0xbc]
	ldr	w3, [x30, #0xbc]
#endif
	wait_complete tbnz, #0, 6b

	tbnz	w9, #0, 7f
	/* DRAM_CKE = 0; [LPDDR4 only] */
	ldr	w0, [x22, #DDRC_IO_CTRL]
	bic	w0, w0, #0xF
	str	w0, [x21, #DDRC_IO_CTRL]
	dsb	sy

7:
#if defined(PLAT_CFG_DRAMFREQ_SUPPORT)
	/* if dramfreq feature is enabled ?
	 * by check srambase + 0x8000 - 0x10
	 */
	mov	w1, #0x7FF0
	ldr	w0, [x27, x1]
	mov	w1, #0x5A5A
	cmp	w1, w0, uxth
	beq	dramfreq
#endif

	delay_20ms
	/* jump if RTC is broken */
	ldr	w0, [x24]
	tbz	w0, #2, 9f
	/* else make sure DDR in self-refresh state */
8:	ldr	w0, [x22, #DDRC_SRS_CTRL_STATUS]
	ldr	w1, [x23, #DDRC_SRS_CTRL_STATUS]
#if defined(AMBARELLA_CV3AD655)
	ldr	w2, [x29, #DDRC_SRS_CTRL_STATUS]
	ldr	w3, [x30, #DDRC_SRS_CTRL_STATUS]
#endif
	wait_complete tbz, #0, 8b

9:	gpio_send_srs_signal	/* send signal to mcu */

#if defined(AMBARELLA_CV8)
	/* Output last message: zzz */
	mov	w2, #0x7a
	movz	x3, #(UART0_BASE & 0xFFFF), lsl #0
	movk	x3, #((UART0_BASE >> 16) & 0xFFFF), lsl #16
	movk	x3, #((UART0_BASE >> 32) & 0xFFFF), lsl #32
	strb	w2, [x3, #UART_TH_OFFSET]
	delay_20ms
	strb	w2, [x3, #UART_TH_OFFSET]
	delay_20ms
	strb	w2, [x3, #UART_TH_OFFSET]
#endif

	isb
	wfi
	b	.

#if defined(PLAT_CFG_DRAMFREQ_SUPPORT)
dramfreq:
	/* mark it as 'SR' @ srambase + 0x8000 - 0x14 */
	mov	w1, #0x7FEC
	mov	w0, #0x5352
	str	w0, [x27, x1]

	/* clear force usb */
	ldr	w0, [x25, #0x34]
	bic	w0, w0, #0x400
	str	w0, [x25, #0x34]

	/* warm reboot */
	str	wzr, [x25, #0x68]
	mov	w0, #1
	str	w0, [x25, #0x68]
	b	.
#endif

#else

.align	3
ambarella_suspend_exec:
	add	x21, x20, #DDRCB	/* broadcast base register */
	add	x22, x20, #DDRC0	/* ddrc0 base regsiter */
	add	x23, x20, #DDRC1	/* ddrc1 base regsiter */

	ldr	w10, [x20, #0x00]	/* used for checking host num */

	/* disable DRAM access from DDRC0 */
	ldr	w2, [x22, #0x00]	/* DRAM_CTL */
	bic	w2, w2, #(1 << 0)
	str	w2, [x22, #0x00]

	tbz	w10, #0, 1f		/* check if 2 hosts */

	/* disable DRAM access from DDRC1 */
	ldr	w2, [x23, #0x00]
	bic	w2, w2, #(1 << 0)
	str	w2, [x23, #0x00]

	/* enter into SR for DDRC0 */
1:	ldr	w2, [x22, #DDRC_SRS_CTRL_STATUS]
	orr	w2, w2, #(1 << 31)
	str	w2, [x22, #DDRC_SRS_CTRL_STATUS]

2:	ldr	w2, [x22, #DDRC_SRS_CTRL_STATUS]
	tbz	w2, #28, 2b

	tbz	w10, #0, 4f		/* check if 2 hosts */

	/* enter into SR for DDRC1 */
	ldr	w2, [x23, #DDRC_SRS_CTRL_STATUS]
	orr	w2, w2, #(1 << 31)
	str	w2, [x23, #DDRC_SRS_CTRL_STATUS]

	delay_20ms
	/* jump if RTC is broken */
	ldr	w0, [x24]
	tbz	w0, #2, 4f
	/* else make sure DDR in self-refresh state */
3:	ldr	w2, [x23, #DDRC_SRS_CTRL_STATUS]
	tbz	w2, #28, 3b
4:
	gpio_send_srs_signal		/* send signal to mcu */

	ldr	w1, [x25, #0x50]	/* always do pwc-poweroff on cv2x */
	orr	w1, w1, #0x20
	str	w1, [x25, #0x50]

	isb
	wfi
	b	.
#endif

.align	3
ambarella_suspend_sz:
	.quad	. - ambarella_suspend_exec

ambarella_relocate_suspend:
	ldr	x0, =FIO_BASE
	ldr	x1, [x0]
	orr	x1, x1, #(1 << 1)
	str	x1, [x0]

	ldr	x0, =ambarella_suspend_exec
	ldr	x1, =FIO_4K_BASE

	/* x2 = (((ambarella_suspend_sz + 15) / 16) * 16) */
	ldr	x2, ambarella_suspend_sz
	add	x2, x2, #0x0f
	and	x2, x2, #(~0x0f)

1:	ldp	x3, x4, [x0], #16
	stp	x3, x4, [x1], #16
	sub	x2, x2, #16
	cbnz	x2, 1b
	dsb	sy

	ldr	x20, =DRAMC_BASE
#if defined(AMBARELLA_CV8)
	ldr 	x24, =DDRH0_SRS_REG
#else
	ldr 	x24, =PWC_RTC_SRS_REG
#endif
	ldr	x25, =RCT_BASE
	ldr	x26, =AXI_BASE
	ldr	x27, =SRAM_BASE

#if defined(DEBUG_SRS)
	ldr	x28, =UART0_BASE
#endif
	ret

ambarella_do_suspend:
	ldr	x1, =FIO_4K_BASE
	dsb	sy
	isb
	br	x1
