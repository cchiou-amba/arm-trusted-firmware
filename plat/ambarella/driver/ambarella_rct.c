/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <lib/mmio.h>
#include <plat_private.h>
#include <drivers/delay_timer.h>

#define REF_CLK_FREQ			24000000UL

#define PLL_CTRL_INTPROG(x)		((x >> 24) & 0x7F)
#define PLL_CTRL_SOUT(x)		((x >> 16) & 0xF)
#define PLL_CTRL_SDIV(x)		((x >> 12) & 0xF)
#define PLL_CTRL_FRAC_MODE(x)		(x & 0x8)
#define PLL_FRAC_VAL(f)			(f & 0x7FFFFFFF)
#define PLL_FRAC_VAL_NEGA(f)		(f & 0x80000000)
#define PLL_SCALER_JDIV(x)		(((x >> 4) & 0xF) + 1)

static uint64_t rct_get_integer_pll_freq(uint32_t ctrl, uint32_t ctrl2, uint32_t pres, uint32_t posts)
{
	uint32_t vcodiv, fsdiv, fsout, outsel;
	uint32_t intp, sout, sdiv;
	uint64_t fvco, freq;

	if (ctrl & 0x30)
		return 0;

	if (ctrl & 0x4)
		return REF_CLK_FREQ / pres / posts;

#if (PLL_VERSION == 1)
	vcodiv = ((ctrl2 >> 8) & 0x1) + 1;
	fsdiv = ((ctrl2 >> 9) & 0x1) + 1;
	fsout = ((ctrl2 >> 11) & 0x1) + 1;
	outsel = ((ctrl2 >> 12) & 0x1);
#else
	vcodiv = ((ctrl >> 8) & 0x1) + 1;
	fsdiv = ((ctrl >> 9) & 0x1) + 1;
	fsout = ((ctrl >> 10) & 0x1) + 1;
	outsel = ((ctrl >> 11) & 0x1);
#endif

	intp = ((ctrl >> 24) & 0x7f) + 1;
	sout = ((ctrl >> 16) & 0xf) + 1;
	sdiv = ((ctrl >> 12) & 0xf) + 1;

	fvco = (uint64_t)REF_CLK_FREQ * vcodiv * fsdiv * sdiv * intp;

	if (outsel)
		freq = fvco;
	else
		freq = fvco / vcodiv / fsout / sout;

	return freq / posts;
}

uint32_t get_sys_timer_parent_freq_hz(void)
{
	uint32_t ctrl, ctrl2;

#if (AXI_SYS_TIMER_INDEPENDENT == 1)
	ctrl = mmio_read_32(RCT_REG(PLL_ENET_CTRL_OFFSET));
	ctrl2 = mmio_read_32(RCT_REG(PLL_ENET_CTRL2_OFFSET));
#else
	ctrl = mmio_read_32(RCT_REG(PLL_CORTEX_CTRL_OFFSET));
	ctrl2 = mmio_read_32(RCT_REG(PLL_CORTEX_CTRL2_OFFSET));
#endif

	return rct_get_integer_pll_freq(ctrl, ctrl2, 1, 1);
}

uint32_t get_core_bus_freq_hz(void)
{
	return rct_get_integer_pll_freq(mmio_read_32(RCT_REG(PLL_CORE_CTRL_OFFSET)),
					mmio_read_32(RCT_REG(PLL_CORE_CTRL2_OFFSET)), 1, 1);
}

uint32_t get_ahb_bus_freq_hz(void)
{
	return get_core_bus_freq_hz() / 2;
}

uint32_t get_apb_bus_freq_hz(void)
{
	return get_ahb_bus_freq_hz() / 2;
}

uint32_t get_ddr_freq_hz(void)
{
	uintptr_t reg_ctrl, reg_ctrl2;

#if (DDRC_VERSION > 1)
	reg_ctrl = DDRH0_REG(PLL_DDR_CTRL_OFFSET);
	reg_ctrl2 = DDRH0_REG(PLL_DDR_CTRL2_OFFSET);
#else
	reg_ctrl = RCT_REG(PLL_DDR_CTRL_OFFSET);
	reg_ctrl2 = RCT_REG(PLL_DDR_CTRL2_OFFSET);
#endif

	return rct_get_integer_pll_freq(mmio_read_32(reg_ctrl),
					mmio_read_32(reg_ctrl2),
					1, 1) / 2;
}

void rct_soft_reset_vp_cluster(void)
{
	mmio_clrbits_32(RCT_REG(CLUSTER_SOFT_RESET_OFFSET), CLUSTER_SOFT_RESET_VP);
	mmio_setbits_32(RCT_REG(CLUSTER_SOFT_RESET_OFFSET), CLUSTER_SOFT_RESET_VP);
	mmio_clrbits_32(RCT_REG(CLUSTER_SOFT_RESET_OFFSET), CLUSTER_SOFT_RESET_VP);
}
