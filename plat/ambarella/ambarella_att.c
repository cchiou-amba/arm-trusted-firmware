/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <errno.h>
#include <libfdt.h>
#include <plat_private.h>

#if defined(AMBARELLA_CV5)
void ambarella_att_setup(void)
{

#define PAGE_ENTRY_SHIFT		(14)
#define PAGE_ENTRY_TOTAL		(1 << 14)
#define PAGE_ENTRY_MASK			(PAGE_ENTRY_TOTAL - 1)
#define PAGE_ATTR_RO			(1 << 15)

	int i, j, offset, len, count, verbose;
	const char *compatible = "ambarella,att-regmap";
	const unsigned int *prop;
	unsigned int bitmap, page_size, regval;
	unsigned int entry_start, entry_count, page_start;
	unsigned long dram_size;
	void *fdt;
	struct segment_regmap {
		unsigned long virt_addr;
		unsigned long phys_addr;
		unsigned long size;
	} regmap[8];



	fdt = (void *)(uintptr_t)(cookie_dtb_ram_start());
	if (!fdt)
		return;

	offset = fdt_node_offset_by_compatible(fdt, -1, compatible);
	if (offset < 0)
		return ;

	prop = fdt_getprop(fdt, offset, "amb,att-debug", &len);
	if (!prop)
		verbose = 0;
	else
		verbose = 1;

	prop = fdt_getprop(fdt, offset, "dram-size", &len);
	if (!prop || len < 0) {
		ERROR("%s: get property 'dram-size' error\n", __func__);
		return ;
	}

	dram_size = ((unsigned long)fdt32_to_cpu(prop[0]) << 32) | fdt32_to_cpu(prop[1]);
	page_size = (unsigned int)(dram_size >> PAGE_ENTRY_SHIFT);

	prop = fdt_getprop(fdt, offset, "client-bitmap", &len);
	if (!prop || len < 0) {
		ERROR("%s: get property 'client-bitmap' error\n", __func__);
		return ;
	}
	bitmap = fdt32_to_cpu(prop[0]);

	prop = fdt_getprop(fdt, offset, "segment-regmap", &len);
	if (!prop || len < 0){
		ERROR("%s: get property 'segment-regmap' error\n", __func__);
		return ;
	}

	if ((len % 24) != 0 || (len / 24) > 8) {
		ERROR("%s: Invalid segment-regmap length %d\n", __func__, len);
		return;
	}

	for (i = 0; i < len / 24; i ++) {
		regmap[i].virt_addr = ((unsigned long)fdt32_to_cpu(prop[i * 6 + 0]) << 32);
		regmap[i].virt_addr |= fdt32_to_cpu(prop[i * 6 + 1]);
		regmap[i].phys_addr = ((unsigned long)fdt32_to_cpu(prop[i * 6 + 2]) << 32);
		regmap[i].phys_addr |= fdt32_to_cpu(prop[i * 6 + 3]);
		regmap[i].size = ((unsigned long)fdt32_to_cpu(prop[i * 6 + 4]) << 32);
		regmap[i].size |= fdt32_to_cpu(prop[i * 6 + 5]);

		if (!regmap[i].size || regmap[i].size < page_size){
			ERROR("%s: Invalid regmap size 0x%lx\n", __func__, regmap[i].size);
			return;
		}
	}

	count = i;

	for (i = 0; verbose && (i < count); i++) {
		NOTICE("ATT: vaddr 0x%lx, paddr 0x%lx, size 0x%lx\n",
				regmap[i].virt_addr, regmap[i].phys_addr, regmap[i].size);
	}

	for (i = 0; i < 32; i++) {
		if (!(bitmap & (((unsigned int) 1) << i)))
			continue;

		mmio_write_32(DRAMC_REG(DRAM_VPN_BASE_OFFSET + i * 4), 0);
		mmio_write_32(DRAMC_REG(DRAM_VPN_BOUND_OFFSET + i * 4), PAGE_ENTRY_MASK);
	}

	regval = PAGE_ATTR_RO | PAGE_ENTRY_MASK;
	regval |=regval << 16;
	for (i = 0; i < PAGE_ENTRY_TOTAL; i += 2)
		mmio_write_32(DRAMC_REG(DRAM_ATT_OFFSET +  i * 2), regval);

	for (i = 0; i < count; i++) {
		entry_start = regmap[i].virt_addr / page_size;
		entry_count = regmap[i].size / page_size;
		page_start = regmap[i].phys_addr / page_size;

		if (entry_count == 0 || entry_start >= PAGE_ENTRY_TOTAL ||
		    entry_count > PAGE_ENTRY_TOTAL - entry_start) {
			ERROR("%s: ATT entry range out of bounds start=%u count=%u\n",
			      __func__, entry_start, entry_count);
			return;
		}

		for (j = entry_start; j < entry_start + entry_count; j += 2) {
			regval = page_start++;
			regval |= page_start++ << 16;
			mmio_write_32(DRAMC_REG(DRAM_ATT_OFFSET +  j * 2), regval);
		}
	}

	mmio_write_32(DRAMC_REG(DRAM_ACCESS_VIRTUAL_OFFSET), bitmap);

}

#elif defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	 defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)

void ambarella_att_setup(void)
{
	uint64_t el2_base, el2_limit;
	uint32_t client, page, entry;

	if (!ambarella_is_secure_boot() || !ambarella_el2_is_used())
		return;

	/*
	 * Setup Address Translation Table to protect EL2 Reserved Memory
	 * from access by peripherals.
	 * Note:
	 *   1) Client 0 is AXI0 which cannot be set to virtual
	 *   2) VPN page size is 256KB
	 *   3) We should make sure all other clients than AXI0 cannot
	 *      access EL2 rsvd memory. Once accessed, ATT will convert
	 *      it to [0x00000, 0x40000] which belong to BL2_BASE.
	 */
	mmio_write_32(DRAMC_REG(DRAM_ACCESS_VIRTUAL_OFFSET), 0xfffffffe);

	for (client = 1; client < 32; client++) {
		mmio_write_32(DRAMC_REG(DRAM_VPN_BASE_OFFSET + client * 4), 0x0000);
		mmio_write_32(DRAMC_REG(DRAM_VPN_BOUND_OFFSET + client * 4), 0x3fff);
	}

	ambarella_el2_rsvd_mem(&el2_base, &el2_limit);

	for(page = 0; page < 0x3fff; page += 2) {
		if (page >= (el2_base >> 18) && page < (el2_limit >> 18))
			entry = 0;
		else
			entry = page | ((page + 1) << 14);
		mmio_write_32(DRAMC_REG(DRAM_ATT_OFFSET + (page >> 1) * 4), entry);
	}
}

#else

void ambarella_att_setup(void)
{
}

#endif

