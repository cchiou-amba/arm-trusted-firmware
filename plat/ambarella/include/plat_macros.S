/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#ifndef __PLAT_MACROS_S__
#define __PLAT_MACROS_S__

/* ---------------------------------------------
 * The below required platform porting macro
 * prints out relevant platform registers
 * whenever an unhandled exception is taken in
 * BL31.
 * Clobbers: x0 - x10, x16, x17, sp
 * ---------------------------------------------
 */
.macro plat_crash_print_regs

.endm


/* ---------------------------------------------
 * AMBARELLA image header
 * ---------------------------------------------
 */
.macro ambarella_image_header image_size, image_entry, image_ver
b       skip_header

.org 0x50, 0
image_size:		.word \image_size
image_entry:		.word \image_entry
image_magic:		.quad 0x414D424F4F54
image_version:		.word \image_ver

.pushsection ".image.fixup", "a"
image_fixup:		.byte 0x0
.popsection

skip_header:

.endm

#endif /* __PLAT_MACROS_S__ */
