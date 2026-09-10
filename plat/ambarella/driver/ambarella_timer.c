/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <lib/mmio.h>
#include <plat_private.h>

#define TIMER1_STATUS_OFFSET		0x00
#define TIMER1_RELOAD_OFFSET		0x04
#define TIMER1_MATCH1_OFFSET		0x08
#define TIMER1_MATCH2_OFFSET		0x0c
#define TIMER2_STATUS_OFFSET		0x10
#define TIMER2_RELOAD_OFFSET		0x14
#define TIMER2_MATCH1_OFFSET		0x18
#define TIMER2_MATCH2_OFFSET		0x1c
#define TIMER3_STATUS_OFFSET		0x20
#define TIMER3_RELOAD_OFFSET		0x24
#define TIMER3_MATCH1_OFFSET		0x28
#define TIMER3_MATCH2_OFFSET		0x2c
#define TIMER4_STATUS_OFFSET		0x34
#define TIMER4_RELOAD_OFFSET		0x38
#define TIMER4_MATCH1_OFFSET		0x3c
#define TIMER4_MATCH2_OFFSET		0x40
#define TIMER5_STATUS_OFFSET		0x44
#define TIMER5_RELOAD_OFFSET		0x48
#define TIMER5_MATCH1_OFFSET		0x4c
#define TIMER5_MATCH2_OFFSET		0x50
#define TIMER6_STATUS_OFFSET		0x54
#define TIMER6_RELOAD_OFFSET		0x58
#define TIMER6_MATCH1_OFFSET		0x5c
#define TIMER6_MATCH2_OFFSET		0x60
#define TIMER7_STATUS_OFFSET		0x64
#define TIMER7_RELOAD_OFFSET		0x68
#define TIMER7_MATCH1_OFFSET		0x6c
#define TIMER7_MATCH2_OFFSET		0x70
#define TIMER8_STATUS_OFFSET		0x74
#define TIMER8_RELOAD_OFFSET		0x78
#define TIMER8_MATCH1_OFFSET		0x7c
#define TIMER8_MATCH2_OFFSET		0x80
#define TIMER_CTRL_OFFSET		0x30

uint32_t apb_timer_get_msec(void)
{
	uint32_t clk_freq, timer_tick;
	uint32_t tick_per_msec;

	clk_freq = get_apb_bus_freq_hz();
	tick_per_msec = clk_freq / 1000;

	timer_tick = ~0U - mmio_read_32(TIMER_REG(TIMER8_STATUS_OFFSET));

	return timer_tick / tick_per_msec;
}
