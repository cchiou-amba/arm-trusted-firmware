/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <drivers/console.h>
#include <common/desc_image_load.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>
#include <plat_private.h>
#include <uart_ambarella.h>
#include <boot_cookie.h>

static console_t bl2_console;

void bl2_el3_early_platform_setup(u_register_t x0, u_register_t x1,
				  u_register_t x2, u_register_t x3)
{
	boot_cookie_t *cookie;

	/* Initialize the console to provide early debug support */
	console_ambarella_register(UART0_BASE, UART_CLOCK, UART_BAUDRATE, &bl2_console);
	printf("\x1b[4l\r\n");	/* Set terminal to replacement mode */

	cookie = boot_cookie_init();
	assert(cookie);

	NOTICE("BL2: up at %u msec(s)\n", apb_timer_get_msec());
}

void bl2_el3_plat_arch_setup(void)
{
	bl_mem_params_node_t *bl_mem_params;
	int rval;

	ambarella_mmap_setup(NULL);

	/* setup BL33 image info */
	bl_mem_params = get_bl_mem_params_node(BL33_IMAGE_ID);
	bl_mem_params->image_info.image_base = cookie_bld_ram_start();
	bl_mem_params->ep_info.pc = cookie_bld_ram_start();

	/* setup SOC_FW_CONFIG image info */
	bl_mem_params = get_bl_mem_params_node(SOC_FW_CONFIG_ID);
	bl_mem_params->image_info.image_base = cookie_dtb_ram_start();
	if (bl_mem_params->image_info.image_base == 0)
		bl_mem_params->image_info.h.attr = IMAGE_ATTRIB_SKIP_LOADING;

	rval = ambarella_io_setup();
	if (rval) {
		ERROR("failed to setup io devices: %d\n", rval);
		panic();
	}
}

void bl2_platform_setup(void)
{
}

void plat_flush_next_bl_params(void)
{
	flush_bl_params_desc();
}

bl_load_info_t *plat_get_bl_image_load_info(void)
{
	return get_bl_load_info_from_mem_params_desc();
}

bl_params_t *plat_get_next_bl_params(void)
{
	return get_next_bl_params_from_mem_params_desc();
}

int bl2_plat_handle_pre_image_load(unsigned int image_id)
{
	struct image_info *image_info;
	int ret = 0;

	image_info = &get_bl_mem_params_node(image_id)->image_info;

	if (image_info->h.attr & IMAGE_ATTRIB_SKIP_LOADING)
		return 0;

	ret = mmap_add_dynamic_region(image_info->image_base,
				      image_info->image_base,
				      image_info->image_max_size,
				      MT_MEMORY | MT_RW | MT_SECURE);
	if (ret < 0) {
		ERROR("map memory failed(%d) for IMAGE_ID(%d): 0x%lx, 0x%x\n", ret,
			image_id, image_info->image_base, image_info->image_max_size);
	}

	return ret;
}

int bl2_plat_handle_post_image_load(unsigned int image_id)
{
	boot_cookie_t *cookie = boot_cookie_ptr();
	uintptr_t recovery_flag;

	switch (image_id) {
	case BL33_IMAGE_ID:
		/* let BLD to know it's boot from recovery */
		recovery_flag = cookie_bld_ram_start() + DEVFW_FLAG_OFFSET;
		if (cookie->bak_bld_media_start)
			*(uint32_t *)recovery_flag = DEVFW_MAGIC;
		break;
	}

	return 0;
}

void bl2_el3_plat_prepare_exit(void)
{
	NOTICE("BL2: exit at %u msec(s)\n", apb_timer_get_msec());
}
