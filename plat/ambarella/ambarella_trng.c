/*
 * Copyright (c) 2025 Ambarella International LP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <common/debug.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>

#include <ambarella_def.h>
#include <ambarella_trng.h>

#if defined(AMBARELLA_CV2)
#define TRNG_SECSP_CNT_OFF	0x40U
#define TRNG_SECSP_DATA0_OFF	0x44U
#elif defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_CV28) || defined(AMBARELLA_S6LM) || \
	defined(AMBARELLA_CV5) || defined(AMBARELLA_CV7)
#define TRNG_SECSP_CNT_OFF	0x00U
#define TRNG_SECSP_DATA0_OFF	0x04U
#elif defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75)
#define TRNG_SECSP_CNT_OFF	0x00U
#define TRNG_SECSP_DATA0_OFF	0x04U
#elif defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1) || defined(AMBARELLA_N1_655)
#define TRNG_SECSP_CNT_OFF	0xD0U
#define TRNG_SECSP_DATA0_OFF	0xD4U
#elif defined(AMBARELLA_CV8)
#define TRNG_SECSP_CNT_OFF	0xE0U
#define TRNG_SECSP_DATA0_OFF	0xE4U
#else
#define TRNG_SECSP_CNT_OFF	0x00U
#define TRNG_SECSP_DATA0_OFF	0x04U
#endif

#define RCT_RNG_PD_REG_OFF	0x1A4U
#define RCT_RNG_PD_BIT		U(0)
#define TRNG_BUSY_BIT		BIT(1)
#define TRNG_SAMPLE_RATE_SHIFT	4U
#define TRNG_SAMPLE_RATE_MASK	(U(0x3) << TRNG_SAMPLE_RATE_SHIFT)

#define TRNG_BUSY_POLL_MAX	100000000U

static int trng_hw_init_once(void)
{
	static uint8_t inited = 0;
	uint32_t val;
	uintptr_t cnt_reg;
	uintptr_t rct_pd;

	if (TRNG_BASE == 0UL) {
		ERROR("trng_hw_init_once, TRNG_BASE == 0?\n");
		return -1;
	}
	if (inited != 0U) {
		//VERBOSE("trng_hw_init_once, already inited\n");
		return 0;
	}

	cnt_reg = (uintptr_t)TRNG_REG(TRNG_SECSP_CNT_OFF);
	rct_pd = (uintptr_t)RCT_REG(RCT_RNG_PD_REG_OFF);

	val = mmio_read_32(rct_pd);
	val &= ~BIT(RCT_RNG_PD_BIT);
	mmio_write_32(rct_pd, val);

	val = mmio_read_32(cnt_reg);
	val &= ~TRNG_SAMPLE_RATE_MASK;
	val |= (0U << TRNG_SAMPLE_RATE_SHIFT);
	mmio_write_32(cnt_reg, val);

	inited = 1U;
	//VERBOSE("trng_hw_init_once, init done\n");
	return 0;
}

static int trng_start_wait_sample(void)
{
	uintptr_t cnt_reg;
	uint32_t v;
	uint32_t tmo;

	cnt_reg = (uintptr_t)TRNG_REG(TRNG_SECSP_CNT_OFF);
	v = mmio_read_32(cnt_reg);
	mmio_write_32(cnt_reg, v | TRNG_BUSY_BIT);

	for (tmo = 0U; tmo < TRNG_BUSY_POLL_MAX; tmo++) {
		if ((mmio_read_32(cnt_reg) & TRNG_BUSY_BIT) == 0U) {
			//VERBOSE("trng_start_wait_sample, wait done, tmo %d\n", tmo);
			return 0;
		}
	}

	ERROR("TRNG: busy timeout (cnt reg stuck)\n");
	return -2;
}

/* One hardware sample -> 4 x 32-bit (little-endian in memory) */
static int trng_read_sample_words(uint32_t w[4])
{
	uintptr_t d0;
	unsigned int k;

	if (trng_start_wait_sample() != 0) {
		ERROR("trng_read_sample_words, wait sample failed\n");
		return -2;
	}

	d0 = (uintptr_t)TRNG_REG(TRNG_SECSP_DATA0_OFF);
	for (k = 0U; k < 4U; k++) {
		w[k] = mmio_read_32(d0 + (uintptr_t)(k * 4U));
	}
	return 0;
}

int ambarella_trng_get_bytes(uint8_t *buf, size_t len)
{
	size_t off = 0;
	uint32_t w[4];

	if (!buf || !len) {
		ERROR("ambarella_trng_get_bytes, null params\n");
		return -1;
	}

	if (TRNG_BASE == 0UL) {
		ERROR("TRNG_BASE not defined for this chip\n");
		return -1;
	}

	if (trng_hw_init_once() != 0) {
		ERROR("ambarella_trng_get_bytes, init failed\n");
		return -1;
	}

	while (off + 16U <= len) {
		if (trng_read_sample_words(w) != 0) {
			ERROR("ambarella_trng_get_bytes, read sample failed\n");
			return -2;
		}
		//VERBOSE("w[0] %x, w[1] %x, w[2] %x, w[3] %x\n", w[0], w[1], w[2], w[3]);
		memcpy(buf + off, w, 16);
		off += 16U;
	}

	if (off < len) {
		if (trng_read_sample_words(w) != 0) {
			ERROR("ambarella_trng_get_bytes, read sample failed\n");
			return -2;
		}
		//VERBOSE("w[0] %x, w[1] %x, w[2] %x, w[3] %x\n", w[0], w[1], w[2], w[3]);
		memcpy(buf + off, w, len - off);
	}

	return 0;
}

