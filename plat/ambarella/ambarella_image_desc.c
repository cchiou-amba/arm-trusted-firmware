/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <common/desc_image_load.h>
#include <platform_def.h>

static struct bl_mem_params_node ambarella_image_descs[] = {
	{
		.image_id = BL31_IMAGE_ID,

		SET_STATIC_PARAM_HEAD(image_info, PARAM_EP,
				      VERSION_2, image_info_t, 0),
		.image_info.image_base = BL31_BASE,
		.image_info.image_max_size = BL31_LIMIT - BL31_BASE,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_EP,
				      VERSION_2, entry_point_info_t,
				      SECURE | EXECUTABLE | EP_FIRST_EXE),
		.ep_info.pc = BL31_BASE,
		.ep_info.spsr = SPSR_64(MODE_EL3, MODE_SP_ELX,
					DISABLE_ALL_EXCEPTIONS),

#ifndef SPD_none
		.next_handoff_image_id = BL32_IMAGE_ID,
#else
		.next_handoff_image_id = BL33_IMAGE_ID,
#endif
	},

#ifndef SPD_none
	{
		.image_id = BL32_IMAGE_ID,

		SET_STATIC_PARAM_HEAD(image_info, PARAM_EP,
				      VERSION_2, image_info_t, 0),
		.image_info.image_base = BL32_BASE,
		.image_info.image_max_size = BL32_LIMIT - BL32_BASE,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_EP,
				      VERSION_2, entry_point_info_t,
				      SECURE | EXECUTABLE),
		.ep_info.pc = BL32_BASE,
		.ep_info.spsr = SPSR_64(MODE_EL3, MODE_SP_ELX,
					DISABLE_ALL_EXCEPTIONS),

		.next_handoff_image_id = BL33_IMAGE_ID,
	},
#endif

	{
		.image_id = BL33_IMAGE_ID,

		SET_STATIC_PARAM_HEAD(image_info, PARAM_EP,
				      VERSION_2, image_info_t, 0),
		/* fill .image_info.image_base at run-time */
		.image_info.image_max_size = AMBARELLA_MAX_BLD_SIZE,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_EP,
				      VERSION_2, entry_point_info_t,
				      NON_SECURE | EXECUTABLE),
		/* fill .ep_info.pc at run-time */
		.ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX,
					DISABLE_ALL_EXCEPTIONS),

		.next_handoff_image_id = INVALID_IMAGE_ID,
	},

	{
		.image_id = SOC_FW_CONFIG_ID,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_IMAGE_BINARY,
				VERSION_2, entry_point_info_t,
				NON_SECURE | NON_EXECUTABLE),
		/* fill .image_info.image_base at run-time */
		.image_info.image_max_size = AMBARELLA_MAX_DTB_SIZE,

		SET_STATIC_PARAM_HEAD(image_info, PARAM_IMAGE_BINARY,
				VERSION_2, image_info_t, 0),

		.next_handoff_image_id = INVALID_IMAGE_ID,
	},
};
REGISTER_BL_IMAGE_DESCS(ambarella_image_descs);

