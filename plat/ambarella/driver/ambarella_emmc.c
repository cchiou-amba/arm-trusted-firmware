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
#include <plat_private.h>
#include <boot_cookie.h>
#include "ambarella_emmc.h"

#define EMMC_BLOCK_SIZE			512
#define EMMC_BLOCK_MASK			(EMMC_BLOCK_SIZE - 1)
#define EMMC_MAX_BLOCK_CNT		65535
/*===========================================================================*/
/*
 * cmd_x:  normal command,
 * acmd_x: app command,
 * scmd_x: sd command,
 * mcmd_x: mmc command
 */
#define cmd_0	0x0000	/* GO_IDLE_STATE	no response */
#define cmd_1	0x0102	/* SEND_OP_COND		expect 48 bit response (R3) */
#define cmd_2	0x0209	/* ALL_SEND_CID		expect 136 bit response (R2) */
#define cmd_3	0x031a	/* SET_RELATIVE_ADDR	expect 48 bit response (R6/R1) */
#define acmd_6	0x061a	/* SET_BUS_WIDTH	expect 48 bit response (R1) */
#define acmd_23	0x171a	/* SET_WR_BLK_ERASE_CNT	expect 48 bit response (R1) */
#define scmd_6	0x063a	/* SWITCH_BUSWIDTH	expect 48 bit response (R1) */
#define mcmd_6	0x061b	/* SET_EXT_CSD		expect 48 bit response (R1B) */
#define cmd_7	0x071b	/* SELECT_CARD		expect 48 bit response (R1B) */
#define cmd_8	0x081a	/* SEND_IF_COND		expect 48 bit response (R7) */
#define mcmd_8	0x083a	/* GET_EXT_CSD		expect 48 bit response (R1) */
#define cmd_9	0x0909	/* GET_THE_CSD		expect 136 bit response (R2) */
#define cmd_11	0x0b1a	/* SWITCH VOLTAGE	expect 48 bit response (R1) */
#define cmd_13	0x0d1a	/* SEND_STATUS		expect 48 bit response (R1) */
#define acmd_13	0x0d3a	/* SEND_STATUS		expect 48 bit response (R1) */
#define cmd_16	0x101a	/* SET_BLOCKLEN		expect 48 bit response (R1) */
#define cmd_17	0x113a	/* READ_SINGLE_BLOCK	expect 48 bit response (R1) */
#define cmd_18	0x123a	/* READ_MULTIPLE_BLOCK	expect 48 bit response (R1) */
#define scmd_19 0x133a  /* SEND_TUNING		expect 48 bit response (R1) */
#define mcmd_21 0x153a  /* SEND_TUNING		expect 48 bit response (R1) */
#define cmd_24	0x183a	/* WRITE_BLOCK		expect 48 bit response (R1) */
#define cmd_25	0x193a	/* WRITE_MULTIPLE_BLOCK	expect 48 bit response (R1) */
#define cmd_32	0x201a	/* ERASE_WR_BLK_START	expect 48 bit response (R1) */
#define cmd_33	0x211a	/* ERASE_WR_BLK_END	expect 48 bit response (R1) */
#define cmd_35	0x231a	/* ERASE_GROUP_START	expect 48 bit response (R1) */
#define cmd_36	0x241a	/* ERASE_GROUP_END	expect 48 bit response (R1) */
#define cmd_38	0x261b	/* ERASE		expect 48 bit response (R1B) */
#define acmd_41	0x2902	/* SD_SEND_OP_COND	expect 48 bit response (R3) */
#define acmd_42	0x2a1b	/* LOCK_UNLOCK		expect 48 bit response (R1B) */
#define acmd_51	0x333a	/* SEND_SCR		expect 48 bit response (R1) */
#define cmd_55	0x371a	/* APP_CMD		expect 48 bit response (R1) */


#define SDMMC_CARD_BUSY		0x80000000	/**< Card Power up status bit */
#define SDMMC_READ_ERROR	0xc0200000	/**< Card/Device read error bit */
#define SDMMC_PROGRAM_ERROR	0xe4000000	/**< Card/Device program error bit */
#define SDMMC_ERASE_ERROR	0xd800a000	/**< Card/Device erase error bit */
#define SDMMC_STATUS_MASK	0xfdf9a080	/**< Card/Device status mask */

/*===========================================================================*/
static unsigned int emmc_sector_mode;

/**
 * Reset the CMD line.
 */
static void sdmmc_reset_cmd_line(void)
{
	/* Reset the CMD line */
	mmio_write_8(EMMC0_BASE + SD_RESET_OFFSET, SD_RESET_CMD);

	/* wait command line ready */
	while (mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_CMD);
}

/**
 * Reset the DATA line.
 */
static void sdmmc_reset_data_line(void)
{
	/* Reset the DATA line */
	mmio_write_8(EMMC0_BASE + SD_RESET_OFFSET, SD_RESET_DAT);

	/* wait data line ready */
	while (mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_DAT);
}

/*
 * Clean interrupts status.
 */
static void sdmmc_clean_interrupt(void)
{
	mmio_write_16(EMMC0_BASE + SD_NIS_OFFSET, mmio_read_16(EMMC0_BASE + SD_NIS_OFFSET));
	mmio_write_16(EMMC0_BASE + SD_EIS_OFFSET, mmio_read_16(EMMC0_BASE + SD_EIS_OFFSET));
}

/* Wait for command to complete, return -1 if error happened */
static int ambarella_emmc_wait_command_done(void)
{
	uint32_t nis, eis;
	int rval = 0;

	while(1) {
		nis = mmio_read_16(EMMC0_BASE + SD_NIS_OFFSET);
		eis = mmio_read_16(EMMC0_BASE + SD_EIS_OFFSET);

		if ((nis & SD_NIS_CMD_DONE) && !(nis & SD_NIS_ERROR))
			break;

		if ((nis & SD_NIS_ERROR) || (eis != 0x0)) {
			sdmmc_reset_cmd_line();
			sdmmc_reset_data_line();

			ERROR("CMD error: cmd=0x%x, eis=0x%x, nis=0x%x\n",
				mmio_read_16(EMMC0_BASE + SD_CMD_OFFSET), eis, nis);
			rval = -EIO;
			break;
		}
	}

	/* clear interrupts */
	sdmmc_clean_interrupt();

	return rval;
}

/* Wait for data transfer to complete, return -1 if error happened */
static int ambarella_emmc_wait_data_done(void)
{
	uint32_t nis, eis;
	int rval = 0;

	while(1) {
		nis = mmio_read_16(EMMC0_BASE + SD_NIS_OFFSET);
		eis = mmio_read_16(EMMC0_BASE + SD_EIS_OFFSET);

		if ((nis & SD_NIS_CMD_DONE) && !(nis & SD_NIS_ERROR))
			break;

		if ((nis & SD_NIS_ERROR) || (eis != 0x0)) {
			sdmmc_reset_cmd_line();
			sdmmc_reset_data_line();

			ERROR("CMD error: cmd=0x%x, eis=0x%x, nis=0x%x\n",
				mmio_read_16(EMMC0_BASE + SD_CMD_OFFSET), eis, nis);

			rval = -EIO;
			break;
		}
	}

	if(rval == 0) {
		while (1) {
			nis = mmio_read_16(EMMC0_BASE + SD_NIS_OFFSET);
			eis = mmio_read_16(EMMC0_BASE + SD_EIS_OFFSET);

			if ((nis & SD_NIS_XFR_DONE) && !(nis & SD_NIS_ERROR))
				break;

			if ((nis & SD_NIS_ERROR) || (eis != 0x0)) {
				sdmmc_reset_cmd_line();
				sdmmc_reset_data_line();

				ERROR("DATA error: cmd=0x%x, eis=0x%x, nis=0x%x \n",
					mmio_read_16(EMMC0_BASE + SD_CMD_OFFSET), eis, nis);

				rval = -EIO;
				break;
			}
		}
	}
	/* clear interrupts */
	sdmmc_clean_interrupt();

	return rval;
}

static int ambarella_emmc_send_command(int command, int argument)
{
	/* wait CMD line ready */
	while ((mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_CMD));

	/* argument */
	mmio_write_32(EMMC0_BASE + SD_ARG_OFFSET, argument);
	mmio_write_16(EMMC0_BASE + SD_XFR_OFFSET, 0x0);

	/* command */
	mmio_write_16(EMMC0_BASE + SD_CMD_OFFSET, command);

	return ambarella_emmc_wait_command_done();
}

static uint32_t ambarella_emmc_send_status_cmd(uint32_t wait_busy, uint32_t wait_data_ready)
{
	uint32_t resp0 = 0x0;
	int rval;

	do {
		rval = ambarella_emmc_send_command(cmd_13, 0x01 << 16);
		if (rval < 0)
			goto emmc_send_status_exit;

		resp0 = mmio_read_32(EMMC0_BASE + SD_RSP0_OFFSET);
	} while ((wait_data_ready && !(resp0 & (1 << 8))) ||
		(wait_busy && (((resp0 & 0x00001E00) >> 9) == 7)));

	if (resp0 & SDMMC_STATUS_MASK)
		NOTICE("Warning: sdmmc card error: 0x%08x\n", resp0 & SDMMC_STATUS_MASK);

emmc_send_status_exit:
	return resp0;
}

/*****************************************************************************/
/*
 * Read single/multi sector from SD/MMC.
 */
static int _sdmmc_read_sector_DMA(int sector, int sectors, uintptr_t target)
{
	int rval = 0;

	uint32_t start_512kb = (uintptr_t)target & 0xfff80000;
	uint32_t end_512kb = ((uintptr_t)target + (sectors << 9) - 1) & 0xfff80000;

	if (start_512kb != end_512kb) {
		WARN("WARNING: crosses 512KB DMA boundary!\n");
		return -1;
	}

	/* wait CMD line ready */
	while ((mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_CMD));

	/* wait DAT line ready */
	while ((mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_DAT));

	mmio_write_32(EMMC0_BASE + SD_DMA_ADDR_OFFSET, (uintptr_t)target);

	if (emmc_sector_mode)			/* argument */
		mmio_write_32(EMMC0_BASE + SD_ARG_OFFSET, sector);
	else
		mmio_write_32(EMMC0_BASE + SD_ARG_OFFSET, sector << 9);

	if (sectors == 1) {
		/* single sector *********************************************/
		mmio_write_16(EMMC0_BASE + SD_BLK_CNT_OFFSET, 0x0);

		mmio_write_16(EMMC0_BASE + SD_XFR_OFFSET,   SD_XFR_CTH_SEL | SD_XFR_DMA_EN);

		mmio_write_16(EMMC0_BASE + SD_CMD_OFFSET, cmd_17);
	} else {
		/* multi sector **********************************************/
		mmio_write_16(EMMC0_BASE + SD_BLK_CNT_OFFSET, sectors);

		mmio_write_16(EMMC0_BASE + SD_XFR_OFFSET, SD_XFR_MUL_SEL |
						SD_XFR_CTH_SEL |
						SD_XFR_AC12_EN |
						SD_XFR_BLKCNT_EN |
						SD_XFR_DMA_EN);

		mmio_write_16(EMMC0_BASE + SD_CMD_OFFSET, cmd_18);
	}

	rval = ambarella_emmc_wait_data_done();
	if (rval < 0)
		goto done;

	/* wait CMD line ready */
	while ((mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_CMD));

	/* wait DAT line ready */
	while ((mmio_read_32(EMMC0_BASE + SD_STA_OFFSET) & SD_STA_CMD_INHIBIT_DAT));

	rval = ambarella_emmc_send_status_cmd(1, 1);
	if (rval & SDMMC_READ_ERROR)
		rval = -1;

done:
	return rval < 0 ? rval : 0;
}

static size_t ambarella_emmc_read(int lba, uintptr_t buf, size_t size)
{
	int ret = 0;
	uint32_t nsectors = size / EMMC_BLOCK_SIZE;

	/* size must be n * EMMC_BLOCK_SIZE */
	assert(!(size & (EMMC_BLOCK_SIZE - 1)));

	/* The size should be multiple EMMC_BLOCK_SIZE */
	inv_dcache_range(buf, size);

	assert(nsectors < EMMC_MAX_BLOCK_CNT);
	ret = _sdmmc_read_sector_DMA(lba, nsectors, buf);
	inv_dcache_range(buf, size);

	return ret ? 0 : size;
}

static io_block_dev_spec_t ambarella_emmc_dev_spec = {
	.ops = {
		.read = ambarella_emmc_read,
	},
	/* fill .buffer and .block_size at run-time */
};


static int ambarella_emmc_hw_init(void)
{
	/* Use sector mode as default */
	emmc_sector_mode = 1;
	return 0;
}

int ambarella_emmc_init(uintptr_t *block_dev_spec)
{
	int ret;

	ret = ambarella_emmc_hw_init();
	if (ret)
		return ret;

	ambarella_emmc_dev_spec.block_size = EMMC_BLOCK_SIZE;

	*block_dev_spec = (uintptr_t)&ambarella_emmc_dev_spec;

	return 0;
}

