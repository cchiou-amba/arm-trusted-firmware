/**
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#ifndef __AMB_BOOT_COOKIE_H__
#define __AMB_BOOT_COOKIE_H__

/*
 * WARNING:
 * This header file is used by BST, BLD and ATF, please sync between:
 * amboot/include/boot_cookie.h
 * arm-trusted-firmware/plat/ambarella/include/boot_cookie.h
 */

/*===========================================================================*/

#define BOOT_COOKIE_MAGIC_NUM			(0x40455641)
#define BOOT_COOKIE_V200			(0x00020000)
#define BOOT_COOKIE_V300			(0x00030000)

#define BOOT_COOKIE_VERSION			BOOT_COOKIE_V200
#define BOOT_COOKIE_VERSION_MAJOR(v)		((v) >> 16 & 0xFF)
#define BOOT_COOKIE_VERSION_MINOR(v)		((v) & 0xFF)

#define BOOT_COOKIE_MAGIC_OFFSET		(0x00)
#define BOOT_COOKIE_VERSION_OFFSET		(0x04)
#define BOOT_COOKIE_CHIP_OFFSET			(0x08)
#define BOOT_COOKIE_SOC_NOTIFY_OFFSET		(0x0c)
#define BOOT_COOKIE_BLD_RAM_START_OFFSET	(0x10)
#define BOOT_COOKIE_RSVD1_OFFSET		(0x14)
#define BOOT_COOKIE_FIP_MEDIA_OFFSET		(0x18)
#define BOOT_COOKIE_BAK_BLD_MEDIA_OFFSET	(0x1c)
#define BOOT_COOKIE_BST_BOOT_FLAG		(0x20)
#define BOOT_COOKIE_DRAM_TRAINING_OFFSET	(0x24)
#define BOOT_COOKIE_DRAM_PRAM_OFFSET		(0x28)
#define BOOT_COOKIE_DTB_RAM_START_OFFSET	(0x2c)
#define BOOT_COOKIE_DRAM_CFG_OFFSET		(0x30)
#define BOOT_COOKIE_KEYINDX_OFFSET		(0x34)
#define BOOT_COOKIE_DRAM_MODE_OFFSET		(0x38)

#define BOOT_COOKIE_MAX_SIZE			(0x80)

#ifndef __ASM__

typedef struct {
	unsigned int magic;
	unsigned int version;
	unsigned int chip;
	unsigned int soc_notify_gpioaddr;	/* soc notify --> mcu */
	unsigned int bld_ram_start;
	unsigned int rsvd1;
	unsigned int fip_media_start;
	unsigned int bak_bld_media_start;
#define BST_BOOT_F_PUBLIC_KEY_UNLOCK		(1 << 0)
	unsigned int bst_boot_flag;
	unsigned int dram_training;
	unsigned int dram_param_media_start;
	unsigned int dtb_ram_start;
	unsigned int dram_config_reg_value;
	unsigned int bst_key_index;
	unsigned int dram_mode_reg_value;
	unsigned int bld_ram_start_hi;
	unsigned int dtb_ram_start_hi;
	unsigned int padding[32-17];
} boot_cookie_t;

boot_cookie_t *boot_cookie_init(void);	/* called only once at very beginning */
boot_cookie_t *boot_cookie_ptr(void);

static inline uint64_t cookie_bld_ram_start(void)
{
	return ((uint64_t)boot_cookie_ptr()->bld_ram_start |
		((uint64_t)boot_cookie_ptr()->bld_ram_start_hi << 32));
}

static inline uint64_t cookie_dtb_ram_start(void)
{
	return ((uint64_t)boot_cookie_ptr()->dtb_ram_start |
		((uint64_t)boot_cookie_ptr()->dtb_ram_start_hi << 32));
}

#endif

#endif	/* __AMB_BOOT_COOKIE_H__ */
