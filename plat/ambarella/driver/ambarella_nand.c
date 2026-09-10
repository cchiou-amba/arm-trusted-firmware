/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <arch_helpers.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>
#include <drivers/io/io_block.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat_private.h>
#include <boot_cookie.h>
#include "ambarella_nand.h"

/* ======================================================================== */

#define SPINAND_CMD_RESET		0xff
#define SPINAND_CMD_GET_FEATURE		0x0f
#define SPINAND_CMD_SET_FEATURE		0x1f
#define SPINAND_CMD_PAGE_READ		0x13
#define SPINAND_CMD_READ_CACHE		0x03
#define SPINAND_CMD_READ_CACHE_X2	0x3b
#define SPINAND_CMD_READ_CACHE_X4	0x6b
#define SPINAND_CMD_BLK_ERASE		0xd8
#define SPINAND_CMD_PROG_EXEC		0x10
#define SPINAND_CMD_PROG_LOAD		0x02
#define SPINAND_CMD_PROG_LOAD_X4	0x32
#define SPINAND_CMD_READ_ID		0x9f
#define SPINAND_CMD_WR_DISABLE		0x04
#define SPINAND_CMD_WR_ENABLE		0x06

/* ======================================================================== */

#define AMBARELLA_MAX_BBT_NUM			32
#define AMBARELLA_NAND_PAGE_PER_BLOCK		64
#define AMBARELLA_NAND_BBT_UNKNOWN		0xff

extern uint32_t ambarella_part_media_base;

typedef struct ambarella_nand_s {
	int pages_per_block;
	int page_size;
	int spare_size;
	uint8_t bbt[AMBARELLA_MAX_BBT_NUM];
	int is_spinand;
} ambarella_nand_t;

static ambarella_nand_t ambarella_nand;


/* spare buffer to check bad block: (4096+256) * 2 */
static uint8_t spare_buffer[8704];
static uint32_t fdma_dsm_ecc_ctrl;
static uint32_t fdma_dsm_no_ecc_ctrl;

static int ambarella_nand_read_pages(ambarella_nand_t *nand,
		uint32_t block, uint32_t page, uint32_t pages,
		uintptr_t main_buf, uintptr_t spare_buf)
{
	uint32_t mlen, slen, status, addr, val;
	int	bch_enable = 1;

	if ((nand->is_spinand == 0) && (!main_buf))
		bch_enable = 0;
	mlen = pages * nand->page_size;
	slen = pages * nand->spare_size;
	main_buf = main_buf ? : spare_buf + slen;
	spare_buf = spare_buf ? : main_buf + mlen;

	/* clear all of NAND Interrupt status */
	mmio_write_32(FIO_RAW_INT_STATUS_REG, 0xff);

	if (nand->is_spinand == 0) {
		if (bch_enable) {
			mmio_setbits_32(FIO_CTRL_REG, FIO_CTRL_ECC_BCH_ENABLE);
			mmio_write_32(FDMA_DSM_CTRL_REG, fdma_dsm_ecc_ctrl);
		} else {
			mmio_clrbits_32(FIO_CTRL_REG, FIO_CTRL_ECC_BCH_ENABLE);
			mmio_write_32(FDMA_DSM_CTRL_REG, fdma_dsm_no_ecc_ctrl);
		}
	}
	/* Setup FDMA engine transfer */
	mmio_write_32(FDMA_MN_MEM_ADDR_REG, main_buf);
	mmio_write_32(FDMA_SP_MEM_ADDR_REG, spare_buf);
	mmio_write_32(FDMA_MN_CTRL_REG,
				FDMA_CTRL_ENABLE | FDMA_CTRL_WRITE_MEM |
				FDMA_CTRL_BLK_SIZE_512B | (mlen + slen));

	/* Write start address for memory target to */
	addr = (block * nand->pages_per_block + page) * nand->page_size;
	if (nand->is_spinand) {
		mmio_write_32(NAND_CMD_REG, addr);

		val = NAND_CC_WORD_CMD1VAL0(SPINAND_CMD_PAGE_READ) |
			NAND_CC_WORD_CMD2VAL0(SPINAND_CMD_READ_CACHE_X2);
		mmio_write_32(NAND_CC_WORD_REG, val);

		mmio_write_32(SPINAND_ERR_PATTERN_REG,  SPINAND_ERR_PATTERN);
		mmio_write_32(SPINAND_DONE_PATTERN_REG,  SPINAND_DONE_PATTERN);

		val = SPINAND_LANE_NUM(2) | SPINAND_CC_RW_READ |
			SPINAND_CC_ADDR_SRC(2) | SPINAND_CC_ADDR_CYCLE(2) |
			SPINAND_CC_DUMMY_DATA_NUM(1) | SPINAND_CC_DATA_SRC_DMA |
			SPINAND_CC2_ENABLE | SPINAND_CC_AUTO_STSCHK;
		mmio_write_32(SPINAND_CC2_REG, val);

		val = SPINAND_LANE_NUM(1) | SPINAND_CC_ADDR_SRC(1) |
			SPINAND_CC_ADDR_CYCLE(3) | SPINAND_CC_DATA_SRC_DMA |
			SPINAND_CC_AUTO_STSCHK;
		mmio_write_32(SPINAND_CC1_REG, val);
	} else
		mmio_write_32(NAND_CMD_REG, addr | NAND_CMD_READ);

	do {
		status = mmio_read_32(FIO_RAW_INT_STATUS_REG);
	} while (!(status & (FIO_INT_ECC_RPT_UNCORR | FIO_INT_OPERATION_DONE | FIO_INT_SND_LOOP_TIMEOUT)));

	/* clear all of NAND Interrupt status */
	mmio_write_32(FIO_RAW_INT_STATUS_REG, 0xff);

	if (status & FIO_INT_ECC_RPT_UNCORR)
		return -1;

	status = mmio_read_32(FDMA_MN_STATUS_REG);
	assert(!(status & (FDMA_STATUS_DMA_BUS_ERR | FDMA_STATUS_DMA_ADDR_ERR)));

	return 0;
}

static int ambarella_nand_block_isbad(ambarella_nand_t *nand, int block)
{
	uint8_t *spare = spare_buffer;
	int is_bad, ret;

	/* use cache if available */
	if (block < ARRAY_SIZE(nand->bbt) &&
	    nand->bbt[block] != AMBARELLA_NAND_BBT_UNKNOWN)
		return nand->bbt[block];

	inv_dcache_range((uintptr_t)spare_buffer, nand->spare_size * 2);

	ret = ambarella_nand_read_pages(nand, block, 0, 2, 0, (uintptr_t)spare_buffer);
	if (ret < 0) {
		is_bad = 1;
		goto out;
	}

	inv_dcache_range((uintptr_t)spare_buffer, nand->spare_size * 2);

	is_bad = ((*spare) & *(spare + nand->spare_size)) != 0xff;

out:
	/* if possible, save the result for future re-use */
	if (block < ARRAY_SIZE(nand->bbt))
		nand->bbt[block] = is_bad;

	if (is_bad)
		WARN("found bad block at %d. skip.\n", block);

	return is_bad;
}

static size_t __ambarella_nand_read(ambarella_nand_t *nand, int lba,
				   uintptr_t buf, size_t size)
{
	int pages_per_block = nand->pages_per_block;
	int page_size = nand->page_size;
	int pages_to_read = div_round_up(size, page_size);
	int page = lba % pages_per_block;
	int blocks_to_skip = lba / pages_per_block;
	int part_start = ambarella_part_media_base / (page_size * pages_per_block);
	int block;
	int pages, ret;
	uintptr_t p = buf;

	block = part_start;
	while (blocks_to_skip > part_start) {
		ret = ambarella_nand_block_isbad(nand, block);
		if (!ret)
			blocks_to_skip--;

		block++;
	}

	while (pages_to_read) {
		ret = ambarella_nand_block_isbad(nand, block);
		if (ret) {
			block++;
			continue;
		}

		pages = MIN(pages_per_block - page, pages_to_read);

		ret = ambarella_nand_read_pages(nand, block, page, pages, p, 0);
		if (ret)
			goto out;

		block++;
		page = 0;
		p += page_size * pages;
		pages_to_read -= pages;
	}

out:
	/* number of read bytes */
	return MIN(size, p - buf);
}

static size_t ambarella_nand_read(int lba, uintptr_t buf, size_t size)
{
	size_t count;

	inv_dcache_range(buf, size);

	count = __ambarella_nand_read(&ambarella_nand, lba, buf, size);

	inv_dcache_range(buf, size);

	return count;
}

static io_block_dev_spec_t ambarella_nand_dev_spec = {
	.ops = {
		.read = ambarella_nand_read,
	},
	/* fill .buffer and .block_size at run-time */
};

static int ambarella_nand_hw_init(ambarella_nand_t *nand)
{
	uint32_t i, poc, sval, bch_enable = 0;

	for (i = 0; i < ARRAY_SIZE(nand->bbt); i++)
		nand->bbt[i] = AMBARELLA_NAND_BBT_UNKNOWN;

	nand->pages_per_block = AMBARELLA_NAND_PAGE_PER_BLOCK;

	mmio_write_32(FIO_INT_ENABLE_REG, 0x0);
	/* Reset FIO FIFO, and Exit random read mode */
	mmio_clrbits_32(FIO_CTRL_REG, FIO_CTRL_RANDOM_READ);

	poc = mmio_read_32(RCT_REG(SYS_CONFIG_OFFSET));

	if (poc & SYS_CONFIG_NAND_PAGE_SIZE) {
		nand->page_size = 2048;
		nand->spare_size = 64;
		fdma_dsm_no_ecc_ctrl = FDMA_DSM_MAIN_JP_SIZE_2KB | FDMA_DSM_SPARE_JP_SIZE_64B;
		mmio_clrbits_32(NAND_EXT_CTRL_REG, NAND_EXT_CTRL_4K_PAGE);
	} else {
		nand->page_size = 4096;
		nand->spare_size = 128;
		fdma_dsm_no_ecc_ctrl = FDMA_DSM_MAIN_JP_SIZE_4KB | FDMA_DSM_SPARE_JP_SIZE_128B;
		mmio_setbits_32(NAND_EXT_CTRL_REG, NAND_EXT_CTRL_4K_PAGE);
	}

	if (poc & SYS_CONFIG_NAND_ECC_SPARE_2X) {
		nand->spare_size *= 2;
		fdma_dsm_no_ecc_ctrl += 0x1;
		fdma_dsm_ecc_ctrl = FDMA_DSM_MAIN_JP_SIZE_512B | FDMA_DSM_SPARE_JP_SIZE_32B;
		mmio_setbits_32(NAND_EXT_CTRL_REG, NAND_EXT_CTRL_SPARE_2X);
	} else {
		fdma_dsm_ecc_ctrl = FDMA_DSM_MAIN_JP_SIZE_512B | FDMA_DSM_SPARE_JP_SIZE_16B;
		mmio_clrbits_32(NAND_EXT_CTRL_REG, NAND_EXT_CTRL_SPARE_2X);
	}

	if (poc & SYS_CONFIG_NAND_SPINAND) {
		mmio_setbits_32(FIO_CTRL2_REG, FIO_CTRL2_SPINAND);

		sval = SPINAND_CTRL_MAX_CMD_LOOP | SPINAND_CTRL_PS_SEL_6;
		if (poc & SYS_CONFIG_NAND_SCKMODE)
			sval |= SPINAND_CTRL_SCKMODE_3;
		mmio_write_32(SPINAND_CTRL_REG, sval);

		nand->is_spinand = 1;
	}

	bch_enable = !!(poc & SYS_CONFIG_NAND_ECC_BCH_EN);

	if (nand->is_spinand == 0)
		assert(bch_enable);

	/* Setup Flash IO Control Register */
	if (bch_enable) {
		mmio_setbits_32(FIO_CTRL_REG, FIO_CTRL_ECC_BCH_ENABLE);
		mmio_write_32(FDMA_DSM_CTRL_REG, fdma_dsm_ecc_ctrl);
	} else {
		mmio_clrbits_32(FIO_CTRL_REG, FIO_CTRL_ECC_BCH_ENABLE);
		if (nand->is_spinand)
			mmio_write_32(FDMA_DSM_CTRL_REG, fdma_dsm_no_ecc_ctrl);
	}

	mmio_write_32(NAND_CTRL_REG, NAND_CTRL_P3 | NAND_CTRL_SIZE_8G);

	return 0;
}

int ambarella_nand_init(uintptr_t *block_dev_spec)
{
	int ret;

	ret = ambarella_nand_hw_init(&ambarella_nand);
	if (ret)
		return ret;

	ambarella_nand_dev_spec.block_size = ambarella_nand.page_size;

	*block_dev_spec = (uintptr_t)&ambarella_nand_dev_spec;

	return 0;
}
