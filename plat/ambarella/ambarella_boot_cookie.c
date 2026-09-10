/**
 *
 * History:
 *    2019/10/22 - [Cao Rongrong] created file
 *
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <string.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <plat_private.h>
#include <boot_cookie.h>

static boot_cookie_t g_boot_cookie;

boot_cookie_t *boot_cookie_init(void)
{
	uintptr_t data3;
	boot_cookie_t *boot_cookie;

	if (ambarella_is_primary_cluster() && (BOOT_COOKIE_REG != 0))
		data3 = BOOT_COOKIE_REG;
	else
		data3 = mmio_read_32(SCRATCHPAD_BASE + AHBSP_DATA3_OFFSET);

	if (ambarella_is_primary_cluster())
		boot_cookie = (boot_cookie_t *)data3;
	else
		boot_cookie = (boot_cookie_t *)(data3 << 8);

	memset(&g_boot_cookie, 0, sizeof(boot_cookie_t));

	/* santiy check, valid value must be 4B aligned. */
	if ((data3 & 0x3) || boot_cookie->magic != BOOT_COOKIE_MAGIC_NUM)
		return NULL;

	/* bld can never be at 0x00000000 which is used by bl2. */
	if (!boot_cookie->bld_ram_start && !boot_cookie->bld_ram_start_hi)
		boot_cookie->bld_ram_start = BL33_BASE;

	if (boot_cookie->version > BOOT_COOKIE_V200) {
		if (!boot_cookie->dtb_ram_start && !boot_cookie->dtb_ram_start_hi)
			boot_cookie->dtb_ram_start = BL33_BASE + SZ_64M;

		if (!boot_cookie->fip_media_start) {
#if defined(PLAT_CFG_FIP_MEDIA_OFFSET)
			if (!PLAT_CFG_FIP_MEDIA_OFFSET) {
				ERROR("PLAT_CFG_FIP_MEDIA_OFFSET is 0\n");
				return NULL;
			}
			boot_cookie->fip_media_start = PLAT_CFG_FIP_MEDIA_OFFSET;
			NOTICE("BL2: FIP offset is 0x%08x\n", PLAT_CFG_FIP_MEDIA_OFFSET);
#else
			ERROR("PLAT_CFG_FIP_MEDIA_OFFSET is not set\n");
			return NULL;
#endif
		}
	}

	memcpy(&g_boot_cookie, boot_cookie, sizeof(boot_cookie_t));

	return &g_boot_cookie;
}

boot_cookie_t *boot_cookie_ptr(void)
{
	return (g_boot_cookie.magic == BOOT_COOKIE_MAGIC_NUM) ? &g_boot_cookie : NULL;
}
