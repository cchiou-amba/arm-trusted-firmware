/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <plat_private.h>
#include <boot_cookie.h>
#include <drivers/delay_timer.h>

static void cluster_set_rvba_by_core(uint64_t cluster, uint32_t core, uint64_t addr)
{
	uint64_t axi_rvba_reg = 0;

	switch(core) {
	case 0: axi_rvba_reg = NX_RVBARADDR0_REG(cluster); break;
	case 1: axi_rvba_reg = NX_RVBARADDR1_REG(cluster); break;
	case 2: axi_rvba_reg = NX_RVBARADDR2_REG(cluster); break;
	case 3: axi_rvba_reg = NX_RVBARADDR3_REG(cluster); break;
	default: return;
	}

#if NX_RVBARADDR_IS_64BIT
	mmio_write_32(axi_rvba_reg, addr & UINT_MAX);
	mmio_write_32(axi_rvba_reg + 4, (addr >> 32) & 0xFF);
#else
	mmio_write_32(axi_rvba_reg, addr >> 8);
#endif
}
__unused
static uint64_t cluster_get_rvba_by_core(uint64_t cluster, uint32_t core)
{
	uint64_t axi_rvba_reg = 0;
	uint64_t rvba = 0;

	switch(core) {
	case 0: axi_rvba_reg = NX_RVBARADDR0_REG(cluster); break;
	case 1: axi_rvba_reg = NX_RVBARADDR1_REG(cluster); break;
	case 2: axi_rvba_reg = NX_RVBARADDR2_REG(cluster); break;
	case 3: axi_rvba_reg = NX_RVBARADDR3_REG(cluster); break;
	default: return 0;
	}

#if NX_RVBARADDR_IS_64BIT
	rvba = mmio_read_32(axi_rvba_reg + 4);
	rvba = mmio_read_32(axi_rvba_reg) | rvba << 32;
#else
	rvba = mmio_read_32(axi_rvba_reg);
	rvba <<= 8;
#endif
	return rvba;
}

int ambarella_cluster_cpu_on(u_register_t cluster,
			     u_register_t boot_entry,
			     u_register_t x3,
			     u_register_t x4)
{
	int i;

	assert(CORTEX_CLUSTER_NUM > 1);

	if (cluster == 0)
		return -1;
	if (cluster >= CORTEX_CLUSTER_NUM)
		return -1;

	for (i = 0; i < CLUSTER_CORE_NUM; i++)
		cluster_set_rvba_by_core(cluster, i, boot_entry);

	mmio_clrbits_32(CORTEX_RESET_REG, CORTEX_RESET_MASK(cluster));
	return 0;
}

int ambarella_cluster_cpu_off(u_register_t cluster,
			     u_register_t x2,
			     u_register_t x3,
			     u_register_t x4)
{
	return -1;
}
