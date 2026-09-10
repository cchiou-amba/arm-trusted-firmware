/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>
#include <plat_private.h>
#include <ambarella_def.h>


static const mmap_region_t ambarella_device_mmap[] = {
	MAP_REGION_FLAT(DEVICE_BASE, DEVICE_SIZE, MT_DEVICE | MT_RW | MT_SECURE),
#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
	MAP_REGION_FLAT(DRAMC_BASE, DRAMC_SIZE, MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(SRAM_BASE, SRAM_SIZE, MT_MEMORY | MT_RW | MT_SECURE),
#endif
	{0}
};

void ambarella_mmap_setup(const struct mmap_region *mmap)
{
	VERBOSE("Trusted RAM seen by this BL image: %p - %p\n",
		(void *)BL_CODE_BASE, (void *)BL_END);
	mmap_add_region(BL_CODE_BASE, BL_CODE_BASE,
			round_up(BL_END, PAGE_SIZE) - BL_CODE_BASE,
			MT_MEMORY | MT_RW | MT_SECURE);

	/* remap the code section */
	VERBOSE("Code region: %p - %p\n",
		(void *)BL_CODE_BASE, (void *)BL_CODE_END);
	mmap_add_region(BL_CODE_BASE, BL_CODE_BASE,
			round_up(BL_CODE_END, PAGE_SIZE) - BL_CODE_BASE,
			MT_CODE | MT_SECURE);

	/* Re-map the read-only data section */
	VERBOSE("Read-only data region: %p - %p\n",
		(void *)BL_RO_DATA_BASE, (void *)BL_RO_DATA_END);
	mmap_add_region(BL_RO_DATA_BASE, BL_RO_DATA_BASE,
			BL_RO_DATA_END - BL_RO_DATA_BASE,
			MT_RO_DATA | MT_SECURE);

#if USE_COHERENT_MEM
	/* remap the coherent memory region */
	VERBOSE("Coherent region: %p - %p\n",
		(void *)BL_COHERENT_RAM_BASE, (void *)BL_COHERENT_RAM_END);
	mmap_add_region(BL_COHERENT_RAM_BASE, BL_COHERENT_RAM_BASE,
			BL_COHERENT_RAM_END - BL_COHERENT_RAM_BASE,
			MT_DEVICE | MT_RW | MT_SECURE);
#endif
	/* peripherals device registers */
	mmap_add(ambarella_device_mmap);

	/* additional regions if needed */
	if (mmap)
		mmap_add(mmap);

	init_xlat_tables();
	enable_mmu(0);
}
