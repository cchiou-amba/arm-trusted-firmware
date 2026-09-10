/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <drivers/arm/gicv2.h>
#include <plat/common/platform.h>
#include <plat_private.h>

static const interrupt_prop_t ambarella_interrupt_props[] = {
	AMBARELLA_G1S_IRQ_PROPS(GICV2_INTR_GROUP0),
	AMBARELLA_G0_IRQ_PROPS(GICV2_INTR_GROUP0)
};

static unsigned int target_mask_array[PLATFORM_CORE_COUNT];

static const gicv2_driver_data_t ambarella_gic_data = {
	.gicd_base = GICD_BASE,
	.gicc_base = GICC_BASE,
	.interrupt_props = ambarella_interrupt_props,
	.interrupt_props_num = ARRAY_SIZE(ambarella_interrupt_props),
	.target_masks = target_mask_array,
	.target_masks_num = ARRAY_SIZE(target_mask_array),
};

/******************************************************************************
 * Helper to initialize the GICv2 only driver.
 *****************************************************************************/
void ambarella_gic_driver_init(void)
{
	gicv2_driver_init(&ambarella_gic_data);
}

void ambarella_gic_distif_init(void)
{
	gicv2_distif_init();
}

/******************************************************************************
 * Helper to initialize the per cpu distributor interface in GICv2
 *****************************************************************************/
void ambarella_gic_pcpu_init(void)
{
	gicv2_pcpu_distif_init();
	gicv2_set_pe_target_mask(plat_my_core_pos());
	gicv2_cpuif_enable();
}

