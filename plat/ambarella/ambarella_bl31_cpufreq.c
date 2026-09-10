/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <lib/mmio.h>
#include <lib/libc/errno.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <bl31/bl31.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <drivers/generic_delay_timer.h>
#include <plat/common/platform.h>
#include <uart_ambarella.h>
#include <plat_private.h>
#include <boot_cookie.h>
#include <libfdt.h>
#include <drivers/delay_timer.h>

enum {
	CTRL_OFFSET,
	FRAC_OFFSET,
	CTRL2_OFFSET,
	CTRL3_OFFSET,
	PRES_OFFSET,
	POST_OFFSET,
	REG_NUM,
};

#define REF_CLK_FREQ		24000000ULL
#define CTRL_WRITE_ENABLE	BIT(0)
#define CTRL_BYPASS		BIT(2)
#define CTRL_FRAC_MODE		BIT(3)
#define CTRL_FORCE_RESET	BIT(4)
#define CTRL_POWER_DOWN		BIT(5)
#define CTRL_HALT_VCO		BIT(6)
#define CTRL_VCODIV_DIV2	BIT(8)	/* For pll_version >= 2 only */
#define CTRL_FSDIV_DIV2		BIT(9)	/* For pll_version >= 2 only */
#define CTRL_FSOUT_DIV2		BIT(10)	/* For pll_version >= 2 only */
#define CTRL_BYPASS_HSDIV	BIT(11)	/* For pll_version >= 2 only */

/* for pll_soc_data_v1 */
#define CTRL2_VCODIV_DIV2	BIT(8)
#define CTRL2_FSDIV_DIV2	BIT(9)
#define CTRL2_FSOUT_DIV2	BIT(11)
#define CTRL2_BYPASS_HSDIV	BIT(12)

#define CTRL3_VCO_RANGE_MASK	(0x6)
#define CTRL3_VCO_CLAMP		0x8	/* For pll_version >= 2 only */

/* Sensor PLL offsets used as calibration helper for WE workaround */
#define PLL_SENSOR_CTRL_OFFSET		0x024
#define PLL_SENSOR_FRAC_OFFSET		0x028
#define PLL_SENSOR_CTRL2_OFFSET		0x11c
#define PLL_SENSOR_CTRL3_OFFSET		0x120
#define CTRL2_OB_ADJ_MASK		(0x3 << 12)

struct amb_pll_soc_data {
	uint32_t pll_version;
	uint32_t fsout_mask;
	uint32_t fsout_val;
	uint32_t fsdiv_mask;
	uint32_t fsdiv_val;
	uint32_t vcodiv_mask;
	uint32_t vcodiv_val;
	uint32_t vco_max_mhz;
	uint32_t vco_min_mhz;
	uint32_t vco_range[4];
	uint32_t ctrl2_val;	/* For pll_version >= 2 only */
	uint32_t ctrl3_val;	/* For pll_version >= 2 only */
	uint32_t sensor_ob_offset; /* PLL_SENSOR_OB, for cortex/core WE workaround */
};

static const struct amb_pll_soc_data pll_soc_data_v0 = {
	.pll_version	= 0,
	.fsout_mask	= 0x00000f00,
	.fsout_val	= 0x00000400,
	.fsdiv_mask	= 0x000000f0,
	.fsdiv_val	= 0x00000040,
	.vcodiv_mask	= 0x0000000f,
	.vcodiv_val	= 0x00000004,
	.vco_max_mhz	= 1600UL, /* RCT doc said 1.8GHz, but we use 1.6GHz for margin */
	.vco_min_mhz	= 700UL,
	.vco_range	= {980UL, 700UL, 530UL, 0UL},
};

static const struct amb_pll_soc_data pll_soc_data_v1 = {
	.pll_version	= 1,
	.fsout_mask	= CTRL2_FSOUT_DIV2,
	.fsout_val	= CTRL2_FSOUT_DIV2,
	.fsdiv_mask	= CTRL2_FSDIV_DIV2,
	.fsdiv_val	= CTRL2_FSDIV_DIV2,
	.vcodiv_mask	= CTRL2_VCODIV_DIV2,
	.vcodiv_val	= CTRL2_VCODIV_DIV2,
	.vco_max_mhz	= 2600UL, /* RCT doc said 2.8GHz, but we use 2.6GHz for margin */
	.vco_min_mhz	= 850UL,
	.vco_range	= {1800UL, 1400UL, 1100UL, 0UL},
};

static const struct amb_pll_soc_data pll_soc_data_v2 = {
	.pll_version	= 2,
	.fsout_mask	= CTRL_FSOUT_DIV2,
	.fsout_val	= CTRL_FSOUT_DIV2,
	.fsdiv_mask	= CTRL_FSDIV_DIV2,
	.fsdiv_val	= CTRL_FSDIV_DIV2,
	.vcodiv_mask	= CTRL_VCODIV_DIV2,
	.vcodiv_val	= CTRL_VCODIV_DIV2,
	.vco_max_mhz	= 2500UL,
	.vco_min_mhz	= 1000UL,
	.ctrl2_val	= 0x00223005,
	.ctrl3_val	= 0x7f801703,
	.sensor_ob_offset = 0x900,	/* CV7 PLL_SENSOR_OB */
};

static const struct amb_pll_soc_data pll_soc_data_v3 = {
	.pll_version	= 3,
	.fsout_mask	= CTRL_FSOUT_DIV2,
	.fsout_val	= CTRL_FSOUT_DIV2,
	.fsdiv_mask	= CTRL_FSDIV_DIV2,
	.fsdiv_val	= CTRL_FSDIV_DIV2,
	.vcodiv_mask	= CTRL_VCODIV_DIV2,
	.vcodiv_val	= CTRL_VCODIV_DIV2,
	.vco_max_mhz	= 5000UL,
	.vco_min_mhz	= 2500UL,
	.ctrl2_val	= 0x00c25000,
	.ctrl3_val	= 0x40c03d20,
	.sensor_ob_offset = 0xAE4,	/* CV8 PLL_SENSOR_OB */
};

struct amb_clk_pll {
	char clk_name[32];
	uint32_t reg_offset[REG_NUM];

	uint32_t parsed_done : 1;
	uint32_t frac_mode : 1;
	uint32_t use_sensor_ob : 1; /* cortex/core: copy sensor OB, write-enable */

	uint32_t ctrl_val;
	uint32_t ctrl2_val;
	uint32_t ctrl3_val;
	uint32_t fix_divider;
	unsigned long vco_max_mhz;
	unsigned long vco_min_mhz;
	const struct amb_pll_soc_data *soc_data;
};

static uint32_t div64_32(uint64_t *n, uint32_t base)
{
	uint32_t rem;
	uint64_t divdend = (*n), part;

	part = divdend / base;
	rem = divdend % base;
	(*n) = part;

	return rem;
}

#define do_div(n,base) ({			\
			unsigned int __rem;	\
			__rem = div64_32((uint64_t *)&(n), base);	\
			__rem;						\
		 })

#define DIV_ROUND_CLOSEST_ULL(x, divisor)(		\
{							\
	typeof(divisor) __d = divisor;			\
	uint64_t _tmp = (x) + (__d) / 2;	\
	do_div(_tmp, __d);				\
	_tmp;						\
}							\
)

#define roundup(x, y)		round_up(x, y)
#define rounddown(x, y)		round_down(x, y)

static char *strncpy(char *dest, const char *src, unsigned int n)
{
	while (n > 0) {
		n--;
		if ((*dest++ = *src++) == '\0')
			break;
	}

	return dest;
}

#define BUG_ON(x, format...)						\
		if ((x)) {						\
			NOTICE("%s(%d): ", __func__, __LINE__);		\
			NOTICE("BUG_ON: "format);			\
			for (;;);					\
		}

static void rct_write_clkreg(uint32_t offset, uint32_t val)
{
	mmio_write_32(RCT_REG(offset), val);
}

static uint32_t rct_read_clkreg(uint32_t offset, uint32_t *value)
{
	uint32_t tmp = mmio_read_32(RCT_REG(offset));

	if (value)
		*value = tmp;

	return tmp;
}

static void rct_write_clkreg_en(uint32_t offset, uint32_t val)
{
	mmio_write_32(RCT_REG(offset), val);
	mmio_write_32(RCT_REG(offset), val | 1);
	mmio_write_32(RCT_REG(offset), val);
}

static signed long abs_val(signed long value)
{
	if (value < 0)
		return -value;
	else
		return value;
}

static unsigned long ambarella_pll_calc_vco(struct amb_clk_pll *clk_pll, unsigned long parent_rate)
{
	const struct amb_pll_soc_data *soc_data = clk_pll->soc_data;
	uint32_t *reg = clk_pll->reg_offset, pre_scaler = 1, ctrl_val, ctrl2_val, frac_val;
	uint32_t intp, sdiv, vcodiv, fsdiv, frac = 0UL;

	if (reg[PRES_OFFSET] != 0) {
		rct_read_clkreg(reg[PRES_OFFSET], &pre_scaler);
		pre_scaler >>= 4;
		pre_scaler++;
	}

	rct_read_clkreg(reg[CTRL_OFFSET], &ctrl_val);
	intp = ((ctrl_val >> 24) & 0x7f) + 1;
	sdiv = ((ctrl_val >> 12) & 0xf) + 1;

	if (soc_data->pll_version >= 2) {
		vcodiv = ((ctrl_val & soc_data->vcodiv_mask) == soc_data->vcodiv_val) ? 2 : 1;
		fsdiv = ((ctrl_val & soc_data->fsdiv_mask) == soc_data->fsdiv_val) ? 2 : 1;
	} else {
		rct_read_clkreg(reg[CTRL2_OFFSET], &ctrl2_val);
		vcodiv = ((ctrl2_val & soc_data->vcodiv_mask) == soc_data->vcodiv_val) ? 2 : 1;
		fsdiv = ((ctrl2_val & soc_data->fsdiv_mask) == soc_data->fsdiv_val) ? 2 : 1;
	}

	if (ctrl_val & CTRL_FRAC_MODE) {
		rct_read_clkreg(reg[FRAC_OFFSET], &frac_val);
		frac = (parent_rate / pre_scaler * vcodiv * sdiv * fsdiv * frac_val) >> 32;
	}

	return parent_rate / pre_scaler * vcodiv * fsdiv * intp * sdiv + frac;
}

void ambarella_pll_set_ctrl3(struct amb_clk_pll *clk_pll, unsigned long parent_rate)
{
	const struct amb_pll_soc_data *soc_data = clk_pll->soc_data;
	uint32_t fvco_mhz, range, ctrl3_val = clk_pll->ctrl3_val;

	if (ctrl3_val != 0) {
		rct_write_clkreg(clk_pll->reg_offset[CTRL3_OFFSET], ctrl3_val);
		return;
	}

	fvco_mhz = ambarella_pll_calc_vco(clk_pll, parent_rate) / 1000000UL;

	for (range = 0; range < ARRAY_SIZE(soc_data->vco_range); range++) {
		if (fvco_mhz > soc_data->vco_range[range])
			break;
	}
	range = ARRAY_SIZE(soc_data->vco_range) - range - 1;

	rct_write_clkreg(clk_pll->reg_offset[CTRL3_OFFSET], ctrl3_val);
	ctrl3_val &= ~CTRL3_VCO_RANGE_MASK;
	ctrl3_val |= range << 1;
	rct_write_clkreg(clk_pll->reg_offset[CTRL3_OFFSET], ctrl3_val);
}

unsigned long ambarella_pll_recalc_rate(struct amb_clk_pll *clk_pll, unsigned long parent_rate)
{
	const struct amb_pll_soc_data *soc_data = clk_pll->soc_data;
	uint32_t *reg = clk_pll->reg_offset, ctrl_val, ctrl2_val;
	uint32_t pre_scaler = 1, post_scaler = 1, vcodiv, fsout, sout;
	uint64_t fvco, rate;

	rct_read_clkreg(reg[CTRL_OFFSET], &ctrl_val);
	rct_read_clkreg(reg[CTRL2_OFFSET], &ctrl2_val);

	if (ctrl_val & (CTRL_POWER_DOWN | CTRL_HALT_VCO | CTRL_FORCE_RESET))
		return 0;

	if (reg[PRES_OFFSET] != 0) {
		rct_read_clkreg(reg[PRES_OFFSET], &pre_scaler);
		pre_scaler >>= 4;
		pre_scaler++;
	}

	if (reg[POST_OFFSET] != 0) {
		rct_read_clkreg(reg[POST_OFFSET], &post_scaler);
		post_scaler >>= 4;
		post_scaler++;
	}

	if (ctrl_val & CTRL_BYPASS)
		return parent_rate / pre_scaler / post_scaler;

	if (soc_data->pll_version >= 2) {
		vcodiv = ((ctrl_val & soc_data->vcodiv_mask) == soc_data->vcodiv_val) ? 2 : 1;
		fsout = ((ctrl_val & soc_data->fsout_mask) == soc_data->fsout_val) ? 2 : 1;
	} else {
		vcodiv = ((ctrl2_val & soc_data->vcodiv_mask) == soc_data->vcodiv_val) ? 2 : 1;
		fsout = ((ctrl2_val & soc_data->fsout_mask) == soc_data->fsout_val) ? 2 : 1;
	}
	sout = ((ctrl_val >> 16) & 0xf) + 1;

	fvco = ambarella_pll_calc_vco(clk_pll, parent_rate);

	if (soc_data->pll_version >= 2) {
		if (ctrl_val & CTRL_BYPASS_HSDIV)
			rate = fvco;
		else
			rate = fvco / vcodiv / fsout / sout;
	} else {
		if (ctrl2_val & CTRL2_BYPASS_HSDIV)
			rate = fvco;
		else
			rate = fvco / vcodiv / fsout / sout;
	}

	return rate / clk_pll->fix_divider / post_scaler;
}

/*
 * Normal PLL program sequence from BST rct_pll_set_value:
 *   ctrl2 -> clamp ctrl3 -> force reset -> delay 1us -> release reset
 *   -> delay 1us -> release clamp
 */
static void ambarella_pll_program_clamp_reset(uint32_t ctrl_off, uint32_t ctrl2_off,
			uint32_t ctrl3_off, uint32_t ctrl_val, uint32_t ctrl2_val,
			uint32_t ctrl3_val)
{
	rct_write_clkreg(ctrl2_off, ctrl2_val);
	rct_write_clkreg(ctrl3_off, ctrl3_val | CTRL3_VCO_CLAMP);
	rct_write_clkreg(ctrl_off, ctrl_val | CTRL_FORCE_RESET);
	udelay(1);
	rct_write_clkreg(ctrl_off, ctrl_val & ~CTRL_FORCE_RESET);
	udelay(1);
	rct_write_clkreg(ctrl3_off, ctrl3_val & ~CTRL3_VCO_CLAMP);
}

/*
 * Cortex/core PLL workaround from BST rct_pll_set_value_we:
 *   1) program sensor PLL with the target params (clamp/reset) to calibrate
 *   2) copy SENSOR_OB bits into ctrl3, clear ctrl2[13:12]
 *   3) program target PLL with write-enable (no force reset)
 */
static void ambarella_pll_program_sensor_ob_we(struct amb_clk_pll *clk_pll,
			uint32_t ctrl_val, uint32_t ctrl2_val, uint32_t ctrl3_val)
{
	const struct amb_pll_soc_data *soc_data = clk_pll->soc_data;
	uint32_t *reg = clk_pll->reg_offset;
	uint32_t saved_frac, saved_ctrl, saved_ctrl2, saved_ctrl3;
	uint32_t pll_ob, ctrl3;

	rct_read_clkreg(PLL_SENSOR_FRAC_OFFSET, &saved_frac);
	rct_read_clkreg(PLL_SENSOR_CTRL_OFFSET, &saved_ctrl);
	rct_read_clkreg(PLL_SENSOR_CTRL2_OFFSET, &saved_ctrl2);
	rct_read_clkreg(PLL_SENSOR_CTRL3_OFFSET, &saved_ctrl3);

	/* Match BST: clear sensor frac, then lock sensor at target params */
	rct_write_clkreg(PLL_SENSOR_FRAC_OFFSET, 0);
	ambarella_pll_program_clamp_reset(PLL_SENSOR_CTRL_OFFSET,
			PLL_SENSOR_CTRL2_OFFSET, PLL_SENSOR_CTRL3_OFFSET,
			ctrl_val, ctrl2_val, ctrl3_val);

	rct_read_clkreg(soc_data->sensor_ob_offset, &pll_ob);

	/* ctrl3[1:0] = pll_ob[29:28], ctrl3[30:24] = pll_ob[6:0] */
	ctrl3 = ctrl3_val;
	ctrl3 = (ctrl3 & ~0x3U) | ((pll_ob >> 28) & 0x3U);
	ctrl3 = (ctrl3 & ~(0x7fU << 24)) | ((pll_ob & 0x7fU) << 24);

	/* ctrl2[13:12] = 2'b00 */
	ctrl2_val &= ~CTRL2_OB_ADJ_MASK;

	rct_write_clkreg(reg[CTRL2_OFFSET], ctrl2_val);
	rct_write_clkreg(reg[CTRL3_OFFSET], ctrl3);
	rct_write_clkreg_en(reg[CTRL_OFFSET], ctrl_val & ~CTRL_FORCE_RESET);

	/* Restore sensor PLL so VIN is not left at borrowed settings */
	rct_write_clkreg(PLL_SENSOR_FRAC_OFFSET, saved_frac);
	ambarella_pll_program_clamp_reset(PLL_SENSOR_CTRL_OFFSET,
			PLL_SENSOR_CTRL2_OFFSET, PLL_SENSOR_CTRL3_OFFSET,
			saved_ctrl & ~(CTRL_FORCE_RESET | CTRL_WRITE_ENABLE),
			saved_ctrl2,
			saved_ctrl3 & ~CTRL3_VCO_CLAMP);
}

static void rational_best_approximation(
	unsigned long given_numerator, unsigned long given_denominator,
	unsigned long max_numerator, unsigned long max_denominator,
	unsigned long *best_numerator, unsigned long *best_denominator)
{
	unsigned long n0 = 0, d0 = 1;
	unsigned long n1 = 1, d1 = 0;
	unsigned long a, n2, d2, tmp;
	unsigned long num = given_numerator;
	unsigned long den = given_denominator;

	if (den == 0 || max_denominator == 0 || max_numerator == 0) {
		*best_numerator = 0;
		*best_denominator = 1;
		return;
	}

	while (1) {
		a = num / den;
		n2 = a * n1 + n0;
		d2 = a * d1 + d0;

		if (n2 > max_numerator || d2 > max_denominator)
			break;

		n0 = n1; d0 = d1;
		n1 = n2; d1 = d2;

		tmp = num;
		num = den;
		den = tmp % den;
		if (den == 0)
			break;
	}

	*best_numerator = n1;
	*best_denominator = d1;
}

static int ambarella_pll_set_rate(struct amb_clk_pll *clk_pll, unsigned long rate,
			unsigned long parent_rate)
{
	const struct amb_pll_soc_data *soc_data = clk_pll->soc_data;
	uint32_t *reg = clk_pll->reg_offset;
	uint32_t ctrl_val, frac_val, ctrl2_val, vcodiv, fsdiv, fsout;
	unsigned long max_numerator, max_denominator;
	unsigned long rate_tmp, rate_resolution, pre_scaler = 1, post_scaler = 1;
	unsigned long intp, sdiv = 1, sout = 1, intp_tmp, sout_tmp;
	uint64_t dividend, divider, diff;

	if (rate == 0) {
		rct_read_clkreg(reg[CTRL_OFFSET], &ctrl_val);
		ctrl_val |= CTRL_POWER_DOWN | CTRL_HALT_VCO;
		rct_write_clkreg_en(reg[CTRL_OFFSET], ctrl_val);
		return 0;
	}

	rate *= clk_pll->fix_divider;

	if (rate < parent_rate && reg[POST_OFFSET] != 0) {
		rate *= 16;
		post_scaler = 16;
	}

	if (rate < parent_rate) {
		NOTICE("%s: Error: target rate is too slow: %ld!\n",
				clk_pll->clk_name, rate);
		return -EINVAL;
	}

retry:
	rate_tmp = rate;

	if (soc_data->pll_version >= 2) {
		ctrl_val = clk_pll->ctrl_val; /* value is 0 if not specified in DTS */
		vcodiv = ((ctrl_val & soc_data->vcodiv_mask) == soc_data->vcodiv_val) ? 2 : 1;
		fsdiv = ((ctrl_val & soc_data->fsdiv_mask) == soc_data->fsdiv_val) ? 2 : 1;
		fsout = ((ctrl_val & soc_data->fsout_mask) == soc_data->fsout_val) ? 2 : 1;
	} else {
		clk_pll->ctrl2_val &= ~soc_data->fsdiv_mask;

		if (rate >= 3000000000UL) {
			clk_pll->ctrl2_val |= soc_data->fsdiv_val;
			rate_tmp = rate / 2;
		}

		if (clk_pll->ctrl2_val != 0)
			ctrl2_val = clk_pll->ctrl2_val;
		else
			ctrl2_val = mmio_read_32(RCT_REG(reg[CTRL2_OFFSET]));

		vcodiv = ((ctrl2_val & soc_data->vcodiv_mask) == soc_data->vcodiv_val) ? 2 : 1;
		fsdiv = ((ctrl2_val & soc_data->fsdiv_mask) == soc_data->fsdiv_val) ? 2 : 1;
		fsout = ((ctrl2_val & soc_data->fsout_mask) == soc_data->fsout_val) ? 2 : 1;
	}

	max_numerator = soc_data->vco_max_mhz / (REF_CLK_FREQ / 1000000UL) / vcodiv / fsdiv;
	max_numerator = MIN(128UL, max_numerator);
	max_denominator = 16;
	rational_best_approximation(rate_tmp, parent_rate,
			max_numerator, max_denominator, &intp, &sout);
	rate_resolution = parent_rate / post_scaler / 16;

	while (parent_rate * fsdiv * intp * sdiv / fsout / sout > rate) {
		rate_tmp -= rate_resolution;
		rational_best_approximation(rate_tmp, parent_rate,
				max_numerator, max_denominator, &intp, &sout);
	}

	intp_tmp = intp;
	sout_tmp = sout;

	while (parent_rate / 1000000 * vcodiv * fsdiv * intp * sdiv /
		pre_scaler < soc_data->vco_min_mhz) {
		if (sout > 8 || intp > 64) {
			if (reg[POST_OFFSET] != 0 && post_scaler == 1) {
				rate *= 16;
				post_scaler = 16;
				goto retry;
			}
			break;
		}
		intp += intp_tmp;
		sout += sout_tmp;
	}

	BUG_ON((intp > max_numerator || sout > max_denominator || sdiv > 16));
	BUG_ON((pre_scaler > 16 || post_scaler > 16));

	if (reg[PRES_OFFSET] != 0)
		rct_write_clkreg_en(reg[PRES_OFFSET], ((pre_scaler - 1) << 4));

	if (reg[POST_OFFSET] != 0)
		rct_write_clkreg_en(reg[POST_OFFSET], ((post_scaler - 1) << 4));

	if (clk_pll->ctrl2_val != 0 && !clk_pll->use_sensor_ob)
		rct_write_clkreg(reg[CTRL2_OFFSET], clk_pll->ctrl2_val);

	if (clk_pll->ctrl_val != 0) {
		ctrl_val = clk_pll->ctrl_val;
	} else {
		ctrl_val = ((intp - 1) & 0x7f) << 24;
		ctrl_val |= ((sdiv - 1) & 0xf) << 12;
		ctrl_val |= ((sout - 1) & 0xf) << 16;
		if (soc_data->pll_version >= 2) {
			ctrl_val |= (vcodiv == 2) ? soc_data->vcodiv_val : 0;
			ctrl_val |= (fsdiv == 2) ? soc_data->fsdiv_val : 0;
			ctrl_val |= (fsout == 2) ? soc_data->fsout_val : 0;
		}
	}
	/* Defer CTRL commit for sensor-OB WE PLLs until final sequence */
	if (!clk_pll->use_sensor_ob)
		rct_write_clkreg(reg[CTRL_OFFSET], ctrl_val);

	rct_write_clkreg(reg[FRAC_OFFSET], 0);

	if (clk_pll->frac_mode) {
		rate_tmp = ambarella_pll_recalc_rate(clk_pll, parent_rate);
		rate_tmp *= clk_pll->fix_divider * post_scaler;
		BUG_ON(rate_tmp > rate);

		diff = rate - rate_tmp;
		if (diff) {
			dividend = (diff * pre_scaler * sout * fsout) << 32;
			divider = (uint64_t)sdiv * (uint64_t)fsdiv * parent_rate;
			frac_val = DIV_ROUND_CLOSEST_ULL(dividend, divider);
			rct_write_clkreg(reg[FRAC_OFFSET], frac_val);

			ctrl_val |= CTRL_FRAC_MODE;
		}
	}

	/* critical PLL like cortex cannot be force-reset while running;
	 * use sensor-OB + write-enable sequence from BST pll.S instead.
	 */
	if (soc_data->pll_version >= 2) {
		uint32_t we_ctrl2 = clk_pll->ctrl2_val;
		uint32_t we_ctrl3 = clk_pll->ctrl3_val;

		if (!we_ctrl2)
			rct_read_clkreg(reg[CTRL2_OFFSET], &we_ctrl2);
		if (!we_ctrl3)
			we_ctrl3 = soc_data->ctrl3_val;

		if (clk_pll->use_sensor_ob && soc_data->sensor_ob_offset) {
			ambarella_pll_program_sensor_ob_we(clk_pll, ctrl_val,
							   we_ctrl2, we_ctrl3);
		} else {
			ambarella_pll_program_clamp_reset(reg[CTRL_OFFSET],
					reg[CTRL2_OFFSET], reg[CTRL3_OFFSET],
					ctrl_val, we_ctrl2, we_ctrl3);
		}
	} else {
		ambarella_pll_set_ctrl3(clk_pll, parent_rate);

		/* critical PLL like cortex cannot be stopped when system is running */
		if (clk_pll->frac_mode) {
			ctrl_val |= CTRL_FORCE_RESET;
			rct_write_clkreg_en(reg[CTRL_OFFSET], ctrl_val);
		}

		ctrl_val &= ~CTRL_FORCE_RESET;
		rct_write_clkreg_en(reg[CTRL_OFFSET], ctrl_val);
	}

	/* check if result rate is precise or not */
	rate_tmp = ambarella_pll_recalc_rate(clk_pll, parent_rate);
	if (abs_val(rate_tmp - rate / clk_pll->fix_divider / post_scaler) > 10) {
		NOTICE("[Warning] %s: request %ld, but got %ld\n",
			clk_pll->clk_name,
			rate / clk_pll->fix_divider / post_scaler, rate_tmp);
	}

	NOTICE(" cpufreq adjust done \n");
	return 0;
}

/*****************************************************************************/

static struct amb_clk_pll gclk_cortex_pll;	/* 0: cortex */
static struct amb_clk_pll gclk_core_pll;	/* 1: core */
static struct amb_clk_pll gclk_idsp_pll;	/* 2: idsp */
static struct amb_clk_pll *secure_clk_pll[] = {
	&gclk_cortex_pll,
	&gclk_core_pll,
	&gclk_idsp_pll,
	NULL,
};

static struct amb_clk_pll *ambarella_find_secure_clk_pll(unsigned long clk_idx)
{
	if (clk_idx > 3) {
		return NULL;
	}

	return secure_clk_pll[clk_idx];
}

static void security_cpufreq_parse(void *fdt, int32_t offset, struct amb_clk_pll *clk_pll)
{
	const fdt32_t *reg;
	int32_t i, lenp = 0;
	uint64_t base_offset = 0;
	const char *clkname = NULL;

	if (offset < 0)
		return;

	clkname = fdt_stringlist_get(fdt, offset, "clock-output-names", 0, &lenp);
	if (clkname)
		strncpy(clk_pll->clk_name, clkname, sizeof(clk_pll->clk_name) - 1);

	INFO("clk_pll->clk_name = %s \n", clk_pll->clk_name);

	clk_pll->frac_mode = !!fdt_get_property(fdt, offset, "amb,frac-mode", NULL);
	clk_pll->use_sensor_ob = !!fdt_get_property(fdt, offset, "amb,use-sensor-ob", NULL);

	reg = fdt_getprop(fdt, offset, "amb,ctrl-val", NULL);
	if (!reg)
		clk_pll->ctrl_val = 0;
	else
		clk_pll->ctrl_val = fdt32_to_cpu(*reg);

	reg = fdt_getprop(fdt, offset, "amb,ctrl2-val", NULL);
	if (!reg)
		clk_pll->ctrl2_val = clk_pll->soc_data->ctrl2_val;
	else
		clk_pll->ctrl2_val = fdt32_to_cpu(*reg);

	reg = fdt_getprop(fdt, offset, "amb,ctrl3-val", NULL);
	if (!reg)
		clk_pll->ctrl3_val = clk_pll->soc_data->ctrl3_val;
	else
		clk_pll->ctrl3_val = fdt32_to_cpu(*reg);

	reg = fdt_getprop(fdt, offset, "amb,fix-divider", NULL);
	if (!reg) {
		clk_pll->fix_divider = 1;
	} else
		clk_pll->fix_divider = fdt32_to_cpu(*reg);

	reg = fdt_getprop(fdt, offset, "amb,vco-max-mhz", NULL);
	if (reg)
		clk_pll->vco_max_mhz = fdt32_to_cpu(*reg);

	reg = fdt_getprop(fdt, offset, "amb,vco-min-mhz", NULL);
	if (reg)
		clk_pll->vco_min_mhz = fdt32_to_cpu(*reg);

	if (fdt_get_property(fdt, offset, "reg", &lenp)) {
		base_offset = ambarella_node_addr_base(fdt, offset);
		reg = fdt_getprop(fdt, offset, "reg", &lenp);
		for (i = 0; i < lenp / 8U; i++, reg += 2) {
			if (fdt32_to_cpu(*reg) != 0)
				clk_pll->reg_offset[i] = (uint32_t)(fdt32_to_cpu(*reg) + base_offset - RCT_BASE);
			else
				clk_pll->reg_offset[i] = 0;
		}
		clk_pll->parsed_done = 1;
	} else if (fdt_get_property(fdt, offset, "amb,clk-regmap", &lenp)) {
		reg = fdt_getprop(fdt, offset, "amb,clk-regmap", &lenp);
		/* skip the 'phandle' for the regmap case */
		reg++;
		lenp -= 4;
		for (i = 0; i < (lenp / 4U); i++, reg++)
			clk_pll->reg_offset[i] = fdt32_to_cpu(*reg);

		clk_pll->parsed_done = 1;
	}
}

void ambarella_cpufreq_parse(void)
{
	void *fdt;
	int32_t offset, lenp;
	const fdt32_t *phandle;

	if (!ambarella_is_secure_boot())
		return;

	memset(&gclk_cortex_pll, 0, sizeof(gclk_cortex_pll));
	memset(&gclk_core_pll, 0, sizeof(gclk_core_pll));
	memset(&gclk_idsp_pll, 0, sizeof(gclk_idsp_pll));

	fdt = (void *)(uintptr_t)(cookie_dtb_ram_start());
	if (!fdt)
		return;

	offset = fdt_path_offset(fdt, "/clocks");
	if (offset < 0)
		return;

	if (fdt_node_check_compatible(fdt, offset, "ambarella,clkpll-v0") == 0) {
		gclk_cortex_pll.soc_data = &pll_soc_data_v0;
		gclk_core_pll.soc_data = &pll_soc_data_v0;
		gclk_idsp_pll.soc_data = &pll_soc_data_v0;
	} else if (fdt_node_check_compatible(fdt, offset, "ambarella,clkpll-v1") == 0) {
		gclk_cortex_pll.soc_data = &pll_soc_data_v1;
		gclk_core_pll.soc_data = &pll_soc_data_v1;
		gclk_idsp_pll.soc_data = &pll_soc_data_v1;
	} else if (fdt_node_check_compatible(fdt, offset, "ambarella,clkpll-v2") == 0) {
		gclk_cortex_pll.soc_data = &pll_soc_data_v2;
		gclk_core_pll.soc_data = &pll_soc_data_v2;
		gclk_idsp_pll.soc_data = &pll_soc_data_v2;
	} else {
		/* version 3 (e.g. ambarella,clkpll-v3) */
		gclk_cortex_pll.soc_data = &pll_soc_data_v3;
		gclk_core_pll.soc_data = &pll_soc_data_v3;
		gclk_idsp_pll.soc_data = &pll_soc_data_v3;
	}

	offset = fdt_path_offset(fdt, "/cpufreq");
	if (offset < 0)
		return;

	phandle = fdt_getprop(fdt, offset, "clocks", &lenp);
	if (!phandle)
		return;

	if (lenp < (int32_t)(3U * sizeof(fdt32_t)))
		return;

	/* cortex */
	offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(phandle[0]));
	security_cpufreq_parse(fdt, offset, &gclk_cortex_pll);

	/* core */
	offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(phandle[1]));
	security_cpufreq_parse(fdt, offset, &gclk_core_pll);

	/* idsp */
	offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(phandle[2]));
	security_cpufreq_parse(fdt, offset, &gclk_idsp_pll);
}

int ambarella_secure_cpufreq_update(unsigned long clk_idx,
		unsigned long rate, unsigned long parent_rate)
{
	struct amb_clk_pll *clk_pll;

	clk_pll = ambarella_find_secure_clk_pll(clk_idx);
	if (!clk_pll) {
		ERROR(" No secure clock pll, Check again \n");
		return -EINVAL;
	}

	if (clk_pll->parsed_done)
		return ambarella_pll_set_rate(clk_pll, rate, parent_rate);
	else
		return -EINVAL;
}
