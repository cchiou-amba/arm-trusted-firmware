/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <arch_helpers.h>
#include <lib/utils_def.h>
#include <lib/mmio.h>
#include <drivers/io/io_block.h>
#include <plat_private.h>
#include <boot_cookie.h>
#include "ambarella_spinor.h"

#define SPINOR_DMA_BLK_SIZE			32
/* Temp use 256Bytes for page size */
#define SPINOR_PAGE_SIZE		256
#define SPINOR_ERASE_SIZE		(64 * 1024)
/* Use channel 0 for dma read in ATF */
#define SPINOR_READ_DMA_CHANNEL		0

/* Use 16MB for read cmd mode choice */
#define SPINOR_SIZE_BOUND		(16 * 1024 * 1024)

/* ===SPINOR CMD===== */
#define SPINOR_CMD_READ					0x03
#define SPINOR_CMD_READ4B				0x13
#define SPINOR_CMD_OCTA_DDR_READ		0x8B
#define SPINOR_OCTA_DDR_BOOT			0x00004000
/* ============ */

static uint32_t octa_ddr_boot = 0;

static void spinor_setup_dma_devmem(uintptr_t buf, size_t size)
{
	unsigned long reg;

	reg = DMA0_CHAN_STA_REG(SPINOR_READ_DMA_CHANNEL);
	mmio_write_32(reg, 0x0);

	reg = DMA0_CHAN_SRC_REG(SPINOR_READ_DMA_CHANNEL);
	mmio_write_32(reg, SPINOR_RXDATA_REG & GENMASK_64(31, 0)); /* fix for 64-bit reg chips */

	reg = DMA0_CHAN_DST_REG(SPINOR_READ_DMA_CHANNEL);
	mmio_write_32(reg, buf);

	reg = DMA0_CHAN_CTR_REG(SPINOR_READ_DMA_CHANNEL);
	mmio_write_32(reg, DMA_CHANX_CTR_EN | DMA_CHANX_CTR_WM |
			DMA_CHANX_CTR_NI | DMA_CHANX_CTR_BLK_32B |
			DMA_CHANX_CTR_TS_4B | (size & (~0x1f)));

	/* set dma enable reg 0x18 */
	mmio_write_32(SPINOR_DMACTRL_REG, SPINOR_DMACTRL_RXEN);
}

static uint32_t ambarella_spinor_send_cmd(uint32_t cmd, uint32_t addr,
					uint32_t length, uint32_t addr_len, uintptr_t buf)
{
	uint32_t val;

	/* reset tx/rx fifo */
	mmio_write_32(SPINOR_TXFIFORST_REG, 0x1);
	mmio_write_32(SPINOR_RXFIFORST_REG, 0x1);

	/* set tx/rx fifo threshold level 0x1c- 0x20 */
	mmio_write_32(SPINOR_TXFIFOTHLV_REG, 31);
	mmio_write_32(SPINOR_RXFIFOTHLV_REG, 31);

	/* set Length reg 0x00 */
	val = SPINOR_LENGTH_CMD(1) | SPINOR_LENGTH_ADDR(addr_len) | SPINOR_LENGTH_DATA(length);
	if (octa_ddr_boot)
		val |= SPINOR_LENGTH_DUMMY(16);
	mmio_write_32(SPINOR_LENGTH_REG, val);

	/* set ctr reg 0x04 */
	if (octa_ddr_boot) {
		val = SPINOR_CTRL_ADDRDTR | SPINOR_CTRL_DATADTR | SPINOR_CTRL_CMD8LANE | \
				SPINOR_CTRL_ADDR8LANE | SPINOR_CTRL_DATA8LANE | SPINOR_CTRL_RDEN;
	} else {
		val = SPINOR_CTRL_CMD1LANE | SPINOR_CTRL_ADDR1LANE | SPINOR_CTRL_DATA2LANE | \
				SPINOR_CTRL_RXLANE_TXRX | SPINOR_CTRL_RDEN;
	}
	mmio_write_32(SPINOR_CTRL_REG, val);

	/* set cmd reg 0x0c */
	mmio_write_32(SPINOR_CMD_REG, cmd);

	/* set addr reg 0x10-0x14 */
	val = addr;
	mmio_write_32(SPINOR_ADDRLO_REG, val);
	mmio_write_32(SPINOR_ADDRHI_REG, 0);

	spinor_setup_dma_devmem(buf, length);

	/* clear all pending interrupts */
	mmio_write_32(SPINOR_CLRINTR_REG, SPINOR_INTR_ALL);

	/* start tx/rx transaction */
	mmio_write_32(SPINOR_START_REG, 0x1);

	/* wait for spi done */
	while((mmio_read_32(SPINOR_RAWINTR_REG) & SPINOR_INTR_DATALENREACH) == 0x0);
	/* wait for dma done */
	while(!(mmio_read_32(DMA0_REG(DMA_INT_OFFSET)) & DMA_INT_CHAN(SPINOR_READ_DMA_CHANNEL)));
	mmio_write_32(DMA0_REG(DMA_INT_OFFSET), 0x0);	/* clear */
	/* disable spi-nor dma */
	mmio_write_32(SPINOR_DMACTRL_REG, 0x0);

	return 0;
}

static size_t __ambarella_spinor_read(int lba, uintptr_t buf, size_t size)
{
	uint32_t read_cmd, start_addr, end_addr, addr_len;

	start_addr = lba * SPINOR_PAGE_SIZE;
	end_addr = start_addr + size;

	if (octa_ddr_boot) {
		addr_len = 4;
		read_cmd = SPINOR_CMD_OCTA_DDR_READ;
	} else {
		addr_len = 3;
		read_cmd = SPINOR_CMD_READ;

		if (end_addr > SPINOR_SIZE_BOUND) {
			read_cmd = SPINOR_CMD_READ4B;
			addr_len = 4;
		}
	}

	ambarella_spinor_send_cmd(read_cmd, start_addr, size, addr_len, buf);

	return 0;
}

static size_t ambarella_spinor_read(int lba, uintptr_t buf, size_t size)
{
	size_t count;

	inv_dcache_range(buf, size);

	count = __ambarella_spinor_read(lba, buf, size);

	inv_dcache_range(buf, size);

	return count ? 0 : size;
}

static io_block_dev_spec_t ambarella_spinor_dev_spec = {
	.ops = {
		.read = ambarella_spinor_read,
	},
	/* fill .buffer and .block_size at run-time */
};

static int ambarella_spinor_hw_init(void)
{

	/* use channel 0 for spinor dma read channel */
	mmio_write_32(AHBSP_DMA0_SEL0_REG, NOR_SPI_RX_DMA_REQ_IDX);

	/* mask all interrupts */
	mmio_write_32(SPINOR_INTRMASK_REG, SPINOR_INTR_ALL);

	/* reset tx/rx fifo */
	mmio_write_32(SPINOR_TXFIFORST_REG, 0x1);
	mmio_write_32(SPINOR_RXFIFORST_REG, 0x1);

	/* read rx fifo manually to clear tx fifo, it's must have. */
	while (mmio_read_32(SPINOR_RXFIFOLV_REG) != 0) {
		uint32_t tmp = mmio_read_32(SPINOR_RXDATA_REG);
		(void)tmp; /* avoid gcc warning "unused variable" */
	}

	mmio_write_32(SPINOR_TXFIFOTHLV_REG, 31);
	mmio_write_32(SPINOR_RXFIFOTHLV_REG, 31);

	return 0;
}

int ambarella_spinor_init(uintptr_t *block_dev_spec)
{
	int ret;
	uint32_t poc = mmio_read_32(RCT_REG(SYS_CONFIG_OFFSET));

	if (poc & SPINOR_OCTA_DDR_BOOT)
		octa_ddr_boot = 1;

	ret = ambarella_spinor_hw_init();
	if (ret)
		return ret;

	ambarella_spinor_dev_spec.block_size = SPINOR_PAGE_SIZE;

	*block_dev_spec = (uintptr_t)&ambarella_spinor_dev_spec;

	return 0;
}
