/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <common/debug.h>
#include <assert.h>
#include <errno.h>
#include <lib/utils_def.h>
#include <libfdt.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>
#include <plat_private.h>
#include <boot_cookie.h>

int32_t ambarella_fdt_init(void)
{
	uint64_t dtb_base;
	int32_t rval = 0;

	dtb_base = cookie_dtb_ram_start();
	if (dtb_base == 0)
		return -ENOENT;

	rval = mmap_add_dynamic_region(dtb_base, dtb_base, AMBARELLA_MAX_DTB_SIZE,
					MT_MEMORY | MT_RW | MT_SECURE);
	if (rval != 0) {
		ERROR("%s: Failed to map DTB region (%d)\n", __func__, rval);
		return rval;
	}

	rval = fdt_check_header((void *)dtb_base);
	if (rval != 0) {
		ERROR("%s: Invalid DTB header (%d)\n", __func__, rval);
		return rval;
	}

	NOTICE("Non Secure DTB at 0x%lx\n", dtb_base);

	return 0;
}

