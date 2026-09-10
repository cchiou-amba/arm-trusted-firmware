/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <plat/common/platform.h>

static const uint8_t plat_power_domain_tree_desc[] = {
	PLATFORM_PWR_DOMAIN_TREE
};

const uint8_t *plat_get_power_domain_tree_desc(void)
{
	return plat_power_domain_tree_desc;
}

/*******************************************************************************
 * This function implements a part of the critical interface between the psci
 * generic layer and the platform that allows the former to query the platform
 * to convert an MPIDR to a unique linear index. An error code (-1) is returned
 * in case the MPIDR is invalid.
 ******************************************************************************/
int32_t plat_core_pos_by_mpidr(u_register_t mpidr)
{
	uint32_t cpuid = 0;
	unsigned long __mpidr = read_mpidr_el1();


	if (__mpidr & MPIDR_MT_MASK) {
		cpuid = MPIDR_AFFLVL1_VAL(mpidr);
	} else {
		cpuid = MPIDR_AFFLVL0_VAL(mpidr);
	}

	if (cpuid >= PLATFORM_CORE_COUNT) {
		return -1;
	}

	return cpuid;
}

uint32_t plat_my_core_pos(void)
{
	uint32_t cpuid = 0;
	unsigned long mpidr = read_mpidr_el1();

	if (mpidr & MPIDR_MT_MASK) {
		cpuid = MPIDR_AFFLVL1_VAL(mpidr);
	} else {
		cpuid = MPIDR_AFFLVL0_VAL(mpidr);
	}

	return cpuid;
}

uint32_t plat_my_cluster_pos(void)
{
	uint32_t n = 0;
	unsigned long mpidr = read_mpidr_el1();

	if (mpidr & MPIDR_MT_MASK) {
		n = MPIDR_AFFLVL2_VAL(mpidr);
	}
#if defined(AMBARELLA_CV8)
	/* for cv8 MT=0, use b[8-15] aff1 as cluster id
	 *     cv8 CA53 core1 MPIDR_EL1 0x80000100
	 *     cv8 CA78 core1 MPIDR_EL1 0x81000100
	 */
	else {
		n = MPIDR_AFFLVL1_VAL(mpidr);
	}
#endif
	return n;
}
