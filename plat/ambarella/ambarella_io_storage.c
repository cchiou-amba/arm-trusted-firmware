/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <tools_share/firmware_image_package.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <drivers/io/io_block.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_fip.h>
#include <uart_ambarella.h>
#include <plat_private.h>
#include <boot_cookie.h>

static const io_dev_connector_t *fip_dev_con;
static uintptr_t fip_dev_handle;

static const io_dev_connector_t *block_dev_con;
static uintptr_t block_dev_handle;

static io_block_spec_t ambarella_fip_spec = {
	/* .offset will be set by the io_setup func */
	.length = 0x00400000,
};

static const io_uuid_spec_t ambarella_bl31_spec = {
	.uuid = UUID_EL3_RUNTIME_FIRMWARE_BL31,
};

static const io_uuid_spec_t ambarella_bl32_spec = {
	.uuid = UUID_SECURE_PAYLOAD_BL32,
};

static const io_uuid_spec_t ambarella_bl33_spec = {
	.uuid = UUID_NON_TRUSTED_FIRMWARE_BL33,
};

#if TRUSTED_BOARD_BOOT
static const io_uuid_spec_t ambarella_trusted_key_cert_spec = {
	.uuid = UUID_TRUSTED_KEY_CERT,
};

static const io_uuid_spec_t ambarella_soc_fw_key_cert_spec = {
	.uuid = UUID_SOC_FW_KEY_CERT,
};

static const io_uuid_spec_t ambarella_tos_fw_key_cert_spec = {
	.uuid = UUID_TRUSTED_OS_FW_KEY_CERT,
};

static const io_uuid_spec_t ambarella_nt_fw_key_cert_spec = {
	.uuid = UUID_NON_TRUSTED_FW_KEY_CERT,
};

static const io_uuid_spec_t ambarella_soc_fw_cert_spec = {
	.uuid = UUID_SOC_FW_CONTENT_CERT,
};

static const io_uuid_spec_t ambarella_tos_fw_cert_spec = {
	.uuid = UUID_TRUSTED_OS_FW_CONTENT_CERT,
};

static const io_uuid_spec_t ambarella_nt_fw_cert_spec = {
	.uuid = UUID_NON_TRUSTED_FW_CONTENT_CERT,
};
#endif /* TRUSTED_BOARD_BOOT */

static const io_uuid_spec_t ambarella_soc_fw_config_spec = {
	.uuid = UUID_SOC_FW_CONFIG,
};

struct ambarella_io_policy {
	uintptr_t *dev_handle;
	uintptr_t image_spec;
	uintptr_t init_params;
};

static const struct ambarella_io_policy ambarella_io_policies[] = {
	[FIP_IMAGE_ID] = {
		.dev_handle = &block_dev_handle,
		.image_spec = (uintptr_t)&ambarella_fip_spec,
	},
	[BL31_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_bl31_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[BL32_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_bl32_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[BL33_IMAGE_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_bl33_spec,
		.init_params = FIP_IMAGE_ID,
	},
#if TRUSTED_BOARD_BOOT
	[TRUSTED_KEY_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_trusted_key_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[SOC_FW_KEY_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_soc_fw_key_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[TRUSTED_OS_FW_KEY_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_tos_fw_key_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[NON_TRUSTED_FW_KEY_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_nt_fw_key_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[SOC_FW_CONTENT_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_soc_fw_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[TRUSTED_OS_FW_CONTENT_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_tos_fw_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
	[NON_TRUSTED_FW_CONTENT_CERT_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_nt_fw_cert_spec,
		.init_params = FIP_IMAGE_ID,
	},
#endif
	[SOC_FW_CONFIG_ID] = {
		.dev_handle = &fip_dev_handle,
		.image_spec = (uintptr_t)&ambarella_soc_fw_config_spec,
		.init_params = FIP_IMAGE_ID,
	},
};

uint32_t ambarella_part_media_base;

int ambarella_io_setup(void)
{
	boot_cookie_t *cookie = boot_cookie_ptr();
	uintptr_t block_dev_spec, block_buf;
	uint32_t boot_media;
	int ret = 0;

	boot_media = mmio_read_32(RCT_REG(SYS_CONFIG_OFFSET)) & POC_BOOT_FROM_MASK;
	switch (boot_media) {
	case POC_BOOT_FROM_EMMC:
		ret = ambarella_emmc_init(&block_dev_spec);
		break;
	case POC_BOOT_FROM_SPINOR:
		ret = ambarella_spinor_init(&block_dev_spec);
		break;
	case POC_BOOT_FROM_NAND:
		ret = ambarella_nand_init(&block_dev_spec);
		break;
	default:
		panic();
		break;
	}

	assert(ret == 0);

	block_buf = round_up(cookie_bld_ram_start() + AMBARELLA_MAX_BLD_SIZE, PAGE_SIZE);

	ret = mmap_add_dynamic_region(block_buf, block_buf,
				AMBARELLA_BLOCK_BUF_SIZE,
				MT_MEMORY | MT_RW | MT_NS);
	assert(ret == 0);

	((io_block_dev_spec_t *)block_dev_spec)->buffer.offset = block_buf;
	((io_block_dev_spec_t *)block_dev_spec)->buffer.length = AMBARELLA_BLOCK_BUF_SIZE;

	if (cookie->bak_bld_media_start)
		ambarella_fip_spec.offset = cookie->bak_bld_media_start;
	else
		ambarella_fip_spec.offset = cookie->fip_media_start;

	/* use this for nand bad block skip check */
	ambarella_part_media_base = ambarella_fip_spec.offset;

	ret = register_io_dev_fip(&fip_dev_con);
	if (ret)
		goto mmap_remove;

	ret = register_io_dev_block(&block_dev_con);
	if (ret)
		goto mmap_remove;

	ret = io_dev_open(fip_dev_con, 0, &fip_dev_handle);
	if (ret)
		goto mmap_remove;

	ret = io_dev_open(block_dev_con, block_dev_spec, &block_dev_handle);
	if (ret)
		goto mmap_remove;

	return 0;

mmap_remove:
	mmap_remove_dynamic_region(block_buf, AMBARELLA_BLOCK_BUF_SIZE);
	return ret;
}

int plat_get_image_source(unsigned int image_id, uintptr_t *dev_handle,
			  uintptr_t *image_spec)
{
	uintptr_t init_params;

	VERBOSE("%s:(image_id = %d)\n", __func__, image_id);

	assert(image_id < ARRAY_SIZE(ambarella_io_policies));

	*dev_handle = *(ambarella_io_policies[image_id].dev_handle);
	*image_spec = ambarella_io_policies[image_id].image_spec;
	init_params = ambarella_io_policies[image_id].init_params;

	return io_dev_init(*dev_handle, init_params);
}

