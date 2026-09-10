/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <errno.h>
#include <common/runtime_svc.h>
#include <plat/common/platform.h>
#include <libfdt.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <lib/extensions/ras_arch.h>
#include <plat_private.h>
#include <ambarella_smc.h>

extern uint64_t el2_pgtable_base;
extern uint64_t el2_stack_base;
extern uint64_t el2_vector_base;
extern void bl31_entrypoint(void);

static uint64_t el2_pgtable_level1;
static uint32_t el2_pgtable_free_index = 2;

/*****************************************************************************/

#define EL2_STACK_SIZE			(SZ_4K)
#define EL2_VECTOR_SIZE			(SZ_1M - EL2_STACK_SIZE)

#define FOUR_KB				(1ULL << FOUR_KB_SHIFT)
#define TWO_MB				(1ULL << TWO_MB_SHIFT)
#define ONE_GB				(1ULL << ONE_GB_SHIFT)

#define MEMORY_PROPERTIES		((0ULL << 54) | 0x7fc)
#define DEVICE_PROPERTIES		((1ULL << 54) | 0x6c4)

#define REG_ALLOW_READ			(1 << 0)
#define REG_ALLOW_WRITE			(1 << 1) /* currently not used */
#define REG_DBG_VERBOSE			(1 << 7)

struct register_permission_t {
	uint64_t start;
	uint32_t size;
	uint32_t flag;
};

/* The registers not defined in reg_permission[] are always accessible */
#define REG_PERM_MAX_NUM		(PAGE_SIZE / sizeof(struct register_permission_t))
static struct register_permission_t	reg_permission_ro[REG_PERM_MAX_NUM];
static struct register_permission_t	reg_permission_rw[REG_PERM_MAX_NUM];
static struct register_permission_t	reg_permission_na[REG_PERM_MAX_NUM];
static struct register_permission_t	reg_permission_na_implicit[REG_PERM_MAX_NUM];

static uint32_t reg_perm_num_ro = 0;
static uint32_t reg_perm_num_na = 0;
static uint32_t reg_perm_num_rw = 0;
static uint32_t reg_perm_num_na_implicit = 0;


/*****************************************************************************/
static uint64_t *ambarella_el2_pgtable_free(void)
{
	uint32_t pgtable_index = el2_pgtable_free_index++;

	assert(pgtable_index < (el2_vector_base - el2_pgtable_base)/ PAGE_SIZE);

	return (uint64_t *)(el2_pgtable_base + pgtable_index * PAGE_SIZE);
}

static uint64_t *ambarella_el2_pgtable_level2(uint64_t *pgtable_level1, uint64_t start)
{
	uint64_t *pgtable_level2, index = ONE_GB_INDEX(start);

	assert((pgtable_level1[index] & DESC_MASK) != INVALID_DESC);

	/* Create Level2 PageTable and replace the entry if it's block descriptor */
	if ((pgtable_level1[index] & DESC_MASK) == BLOCK_DESC) {
		pgtable_level2 = ambarella_el2_pgtable_free();
		pgtable_level1[index] = (uint64_t)pgtable_level2 | TABLE_DESC;

		for (index = 0; index < XLAT_TABLE_ENTRIES; index++) {
			pgtable_level2[index] = round_down(start, ONE_GB) + index * TWO_MB;
			pgtable_level2[index] |= DEVICE_PROPERTIES | BLOCK_DESC;
		}
	} else {
		pgtable_level2 = (uint64_t *)(pgtable_level1[index] & ~ULL(3));
	}

	return pgtable_level2;
}

static uint64_t *ambarella_el2_pgtable_level3(uint64_t *pgtable_level2, uint64_t start)
{
	uint64_t *pgtable_level3, index = TWO_MB_INDEX(start) % XLAT_TABLE_ENTRIES;

	assert((pgtable_level2[index] & DESC_MASK) != INVALID_DESC);

	/* Create Level3 PageTable and replace the entry if it's block descriptor */
	if ((pgtable_level2[index] & DESC_MASK) == BLOCK_DESC) {
		pgtable_level3 = ambarella_el2_pgtable_free();
		pgtable_level2[index] = (uint64_t)pgtable_level3 | TABLE_DESC;

		for (index = 0; index < XLAT_TABLE_ENTRIES; index++) {
			pgtable_level3[index] = round_down(start, TWO_MB) + index * FOUR_KB;
			pgtable_level3[index] |= DEVICE_PROPERTIES | PAGE_DESC;
		}
	} else {
		pgtable_level3 = (uint64_t *)(pgtable_level2[index] & ~ULL(3));
	}

	return pgtable_level3;
}

/* Invalidate the translation table of the region */
static void ambarella_el2_pgtable_update(uint64_t start, uint64_t len)
{
	uint64_t _start, *pgtable_level2, *pgtable_level3;
	uint32_t index, index_start, index_end;

	assert(start >= DEVICE_BASE && (start + len) < (DEVICE_BASE + DEVICE_SIZE));
	assert(ONE_GB_INDEX(start) == ONE_GB_INDEX(start + len));

	pgtable_level2 = ambarella_el2_pgtable_level2((uint64_t *)el2_pgtable_level1, start);

	for (_start = start; _start < start + len; _start += TWO_MB) {
		pgtable_level3 = ambarella_el2_pgtable_level3(pgtable_level2, _start);

		index_start = FOUR_KB_INDEX(round_down(_start, FOUR_KB));
		index_start %= XLAT_TABLE_ENTRIES;

		if (TWO_MB_INDEX(_start) < TWO_MB_INDEX(_start + len)) {
			index_end = XLAT_TABLE_ENTRIES;
			_start = round_down(_start, TWO_MB);
		} else {
			index_end = FOUR_KB_INDEX(_start + len);
			index_end %= XLAT_TABLE_ENTRIES;
		}

		for (index = index_start; index < index_end; index++)
			pgtable_level3[index] = 0;
	}
}

/*
 * get base address for node in case #address-cells > 1
 * by parsing #address-cells and ranges;
*/
uint64_t ambarella_node_addr_base(void *fdt, int32_t offset)
{
	const fdt32_t *ranges, *p_addr_cells, *p_parent_addr_cells;
	int32_t ranges_len, i, parent_offset;
	uint32_t addr_cells, parent_addr_cells;
	uint64_t base_addr = 0, child_addr = 0, parent_addr = 0;

	parent_offset = fdt_parent_offset(fdt, offset);
	if (parent_offset < 0)
		return base_addr;

	ranges = fdt_getprop(fdt, parent_offset, "ranges", &ranges_len);
	if (!ranges || !ranges_len)
		return base_addr;

	p_addr_cells = fdt_getprop(fdt, parent_offset, "#address-cells", NULL);
	if (!p_addr_cells)
		return base_addr;
	addr_cells = fdt32_to_cpu(p_addr_cells[0]);
	parent_offset = fdt_parent_offset(fdt, parent_offset);
	if (parent_offset < 0)
		return base_addr;

	p_parent_addr_cells = fdt_getprop(fdt, parent_offset, "#address-cells", NULL);
	if (!p_parent_addr_cells)
		return base_addr;
	parent_addr_cells = fdt32_to_cpu(p_parent_addr_cells[0]);
	/* ranges must hold child + parent address cells before walk */
	if (addr_cells > (uint32_t)ranges_len / sizeof(fdt32_t) ||
	    parent_addr_cells > ((uint32_t)ranges_len / sizeof(fdt32_t) - addr_cells))
		return base_addr;
	/* sub addr in ranges */
	for (i = 0; i < addr_cells; i++, ranges++)
		child_addr = (child_addr << (i * 32)) | fdt32_to_cpu(*ranges);
	/* parent's addr in ranges */
	for (i = 0; i < parent_addr_cells; i++, ranges++)
		parent_addr = (parent_addr << (i * 32)) | fdt32_to_cpu(*ranges);

	base_addr = parent_addr - child_addr;
	INFO("base_addr = 0x%lx \n", base_addr);

	return base_addr;
}

static int32_t ambarella_fdt_parse_gpio_security(void *fdt, int32_t offset)
{
	const char *pinctrl = "ambarella,pinctrl";
	const fdt32_t *gpio, *reg, *ctrl_bit;
	int32_t i, lenp, pinctrl_reg_idx, cell;
	uint32_t gpio_security_mask[GPIO_BANK] = {0};

	gpio = fdt_getprop(fdt, offset, "gpio", &lenp);
	if (gpio == NULL || lenp == 0)
		return 0;

	for (i = 0; i < lenp / 4U; i++, gpio++) {
		int32_t sec_gpio = fdt32_to_cpu(*gpio);
		int32_t bank = sec_gpio / NRGPIO_PER_BANK;
		int32_t bitoff = sec_gpio % NRGPIO_PER_BANK;

		if (bank < 0 || bank >= GPIO_BANK) {
			ERROR("'%s': secure GPIO bank %d out of range\n", pinctrl, bank);
			return -EINVAL;
		}

		gpio_security_mask[bank] |= (1 << bitoff);
	}

	offset = fdt_node_offset_by_compatible(fdt, -1, pinctrl);
	if (offset < 0)
		return offset;

	pinctrl_reg_idx = fdt_stringlist_search(fdt, offset, "reg-names", "iomux");
	if (pinctrl_reg_idx < 0) {
		ERROR("'%s': reg-names iomux not found\n", pinctrl);
		return 0;
	}
	cell = pinctrl_reg_idx;

	ctrl_bit = fdt_getprop(fdt, offset, "amb,secure-ctrl-bit", &lenp);
	if (ctrl_bit == NULL || lenp == 0) {
		ERROR("'%s': Secure control bit is not specified\n", pinctrl);
		return 0;
	}

	reg = fdt_getprop(fdt, offset, "reg", &lenp);
	if (reg == NULL || lenp == 0)
		return 0;
	else {
		uintptr_t pinctrl_reg[2];
		uintptr_t base_addr;

		base_addr = ambarella_node_addr_base(fdt, offset);
		pinctrl_reg[0] = fdt32_to_cpu(reg[2 * pinctrl_reg_idx]);
		pinctrl_reg[0] |= base_addr;
		pinctrl_reg[1] = fdt32_to_cpu(reg[2 * pinctrl_reg_idx + 1]);
		/* Enable IOMUX Security */
		ambarella_device_security_setup(fdt32_to_cpu(ctrl_bit[pinctrl_reg_idx]));
		ambarella_el2_pgtable_update(pinctrl_reg[0], pinctrl_reg[1]);
		ambarella_pinctrl_security_request(pinctrl_reg, gpio_security_mask);

			/* Enable GPIO BANK Security */
			for (i = 0; i < cell; i++, ctrl_bit++) {

				uintptr_t gpio_reg[2];

				if (!gpio_security_mask[i])
					continue;

				gpio_reg[0] = fdt32_to_cpu(reg[2 * i]);
				gpio_reg[0] |= base_addr;
				gpio_reg[1] = fdt32_to_cpu(reg[2 * i + 1]);

				ambarella_device_security_setup(fdt32_to_cpu(*ctrl_bit));
				ambarella_el2_pgtable_update(gpio_reg[0], gpio_reg[1]);
				ambarella_gpio_security_request(gpio_reg, i);
			}
	}

	return 0;
}

static int32_t ambarella_fdt_parse_device_security(void *fdt, int32_t offset)
{
	const fdt32_t *phandle, *ctrl_bit, *reg, *addr_rw, *addr_ro, *addr_na, *reg_ro, *reg_na;
	int32_t lenp, sub_lenp, i, j;
	const char *device_name;
	uintptr_t base_addr;

	phandle = fdt_getprop(fdt, offset, "device", &lenp);
	if (phandle == NULL)
		return 0;

	for (i = 0; i < lenp / 4U; i++, phandle++) {
		offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*phandle));
		if (offset < 0)
			return offset;

		device_name = fdt_getprop(fdt, offset, "compatible", NULL);
		if (device_name == NULL)
			continue;

		/* base-addr for this node(phandle point to) */
		base_addr = ambarella_node_addr_base(fdt, offset);
		/* If there is no control bit property, treat the device as NON-Secure. */
		ctrl_bit = fdt_getprop(fdt, offset, "amb,secure-ctrl-bit", NULL);
		if (ctrl_bit == NULL) {
			ERROR("'%s': Secure control bit is not specified\n", device_name);
			continue;
		}

		INFO("'%s': Secure device[%d].\n", device_name, fdt32_to_cpu(*ctrl_bit));

		/* Enable device Security */
		ambarella_device_security_setup(fdt32_to_cpu(*ctrl_bit));

		reg = fdt_getprop(fdt, offset, "reg", &sub_lenp);
		if (reg == NULL)
			continue;
		assert((sub_lenp / 4U) % 2 == 0);

		addr_na = fdt_getprop(fdt, offset, "amb,secure-addr-na", &sub_lenp);
		if (addr_na != NULL) {
			assert((sub_lenp / 4U) % 2 == 0);

			for (j = 0; j < sub_lenp / 4U; j += 2) {
				reg_permission_na[reg_perm_num_na].start = fdt32_to_cpu(addr_na[j]);
				reg_permission_na[reg_perm_num_na].start |= base_addr;
				reg_permission_na[reg_perm_num_na].size = fdt32_to_cpu(addr_na[j + 1]);

				INFO("'%s': noaccess reg: [0x%lx, 0x%x]\n", device_name,
					reg_permission_na[reg_perm_num_na].start,
					reg_permission_na[reg_perm_num_na].size);

				reg_perm_num_na++;
				assert(reg_perm_num_na < REG_PERM_MAX_NUM);
			}
		}

		addr_ro = fdt_getprop(fdt, offset, "amb,secure-addr-ro", &sub_lenp);
		if (addr_ro != NULL) {
			assert((sub_lenp / 4U) % 2 == 0);

			for (j = 0; j < sub_lenp / 4U; j += 2) {
				reg_permission_ro[reg_perm_num_ro].start = fdt32_to_cpu(addr_ro[j]);
				reg_permission_ro[reg_perm_num_ro].start |= base_addr;
				reg_permission_ro[reg_perm_num_ro].size = fdt32_to_cpu(addr_ro[j + 1]);

				INFO("'%s': readonly reg: [0x%lx, 0x%x]\n", device_name,
					reg_permission_ro[reg_perm_num_ro].start,
					reg_permission_ro[reg_perm_num_ro].size);

				reg_perm_num_ro++;
				assert(reg_perm_num_ro < REG_PERM_MAX_NUM);
			}
		}

		addr_rw = fdt_getprop(fdt, offset, "amb,secure-addr-rw", &sub_lenp);
		if (addr_rw != NULL) {
			assert((sub_lenp / 4U) % 2 == 0);

			for (j = 0; j < sub_lenp / 4U; j += 2) {
				reg_permission_rw[reg_perm_num_rw].start = fdt32_to_cpu(addr_rw[j]);
				reg_permission_rw[reg_perm_num_rw].start |= base_addr;
				reg_permission_rw[reg_perm_num_rw].size = fdt32_to_cpu(addr_rw[j + 1]);

				INFO("'%s': r/w reg: [0x%lx, 0x%x]\n", device_name,
					reg_permission_rw[reg_perm_num_rw].start,
					reg_permission_rw[reg_perm_num_rw].size);

				reg_perm_num_rw++;
				assert(reg_perm_num_rw < REG_PERM_MAX_NUM);
			}
		}

		/* set the whole range implicit can't access, ro and rw check before it */
		if (addr_rw || addr_ro ||addr_na) {
				reg_permission_na_implicit[reg_perm_num_na_implicit].start = fdt32_to_cpu(reg[0]);
				reg_permission_na_implicit[reg_perm_num_na_implicit].start |= base_addr;
				reg_permission_na_implicit[reg_perm_num_na_implicit].size = fdt32_to_cpu(reg[1]);

				INFO("'%s': implicit noaccess reg: [0x%lx, 0x%x]\n", device_name,
					reg_permission_na_implicit[reg_perm_num_na_implicit].start,
					reg_permission_na_implicit[reg_perm_num_na_implicit].size);

				reg_perm_num_na_implicit++;
				assert(reg_perm_num_na_implicit < REG_PERM_MAX_NUM);
		}

		if (!addr_rw && !addr_ro && !addr_na) {
			reg_ro = fdt_getprop(fdt, offset, "amb,secure-reg-ro", &sub_lenp);
			if (reg_ro != NULL) {
				assert((sub_lenp / 4U) % 2 == 0);

				for (j = 0; j < sub_lenp / 4U; j += 2) {
					reg_permission_ro[reg_perm_num_ro].start = fdt32_to_cpu(reg_ro[j]);
					reg_permission_ro[reg_perm_num_ro].start |= base_addr;
					reg_permission_ro[reg_perm_num_ro].size = fdt32_to_cpu(reg_ro[j+1]);

					INFO("'%s': readonly reg: [0x%lx, 0x%x]\n", device_name,
						reg_permission_ro[reg_perm_num_ro].start,
						reg_permission_ro[reg_perm_num_ro].size);

					reg_perm_num_ro++;
					assert(reg_perm_num_ro < REG_PERM_MAX_NUM);
				}
			}

			reg_na = fdt_getprop(fdt, offset, "amb,secure-reg-na", &sub_lenp);
			if (reg_na != NULL) {
				assert((sub_lenp / 4U) % 2 == 0);

				for (j = 0; j < sub_lenp / 4U; j += 2) {
					reg_permission_na[reg_perm_num_na].start = fdt32_to_cpu(reg_na[j]);
					reg_permission_na[reg_perm_num_na].start |= base_addr;
					reg_permission_na[reg_perm_num_na].size = fdt32_to_cpu(reg_na[j+1]);

					INFO("'%s': noaccess reg: [0x%lx, 0x%x]\n", device_name,
						reg_permission_na[reg_perm_num_na].start,
						reg_permission_na[reg_perm_num_na].size);

					reg_perm_num_na++;
					assert(reg_perm_num_na < REG_PERM_MAX_NUM);
				}
			}
			if (addr_rw == NULL && addr_ro == NULL && addr_na == NULL && reg_ro == NULL && reg_na == NULL)
				continue;
		}

		/* Update the stage2 translation table reg[0] */
		ambarella_el2_pgtable_update((fdt32_to_cpu(reg[0]) | base_addr), fdt32_to_cpu(reg[1]));
	}

	/*
	 * Monitor DDRC registers, and set it as read-only.
	 * Non-secure world BLD needs READ permission to access these registers.
	 */
#if (DDRC_DEBUG_SUPPORT > 0)
	reg_permission_rw[reg_perm_num_rw].start = DRAMC_BASE;
	reg_permission_rw[reg_perm_num_rw].size = DRAMC_SIZE;

	reg_perm_num_rw++;
	assert(reg_perm_num_rw < REG_PERM_MAX_NUM);
	if (ambarella_is_secure_boot())
		NOTICE("Warning: DDRC Debug Mode Under Secure Boot !!! \n");
#else
	reg_permission_ro[reg_perm_num_ro].start = DRAMC_BASE;
	reg_permission_ro[reg_perm_num_ro].size = DRAMC_SIZE;

	reg_perm_num_ro++;
	assert(reg_perm_num_ro < REG_PERM_MAX_NUM);
#endif

	return 0;
}

static int32_t ambarella_fdt_parse_security(void)
{
	void *fdt = (void *)(uintptr_t)(cookie_dtb_ram_start());
	int32_t offset;

	/*
	 * If secure boot is disabled, reg_permission[] will be empty,
	 * it means all registers are accessible.
	 */
	if (!ambarella_is_secure_boot())
		return 0;

	offset = fdt_path_offset(fdt, "/secure-monitor");
	if (offset < 0)
		return offset;

	ambarella_fdt_parse_gpio_security(fdt, offset);

	ambarella_fdt_parse_device_security(fdt, offset);

	return 0;
}

static uint64_t ambarella_fdt_get_dram_size(void)
{
	int32_t offset, len;
	void *fdt = (void *)(uintptr_t)(cookie_dtb_ram_start());
	const uint32_t *prop;
	uint64_t size = 0;

	offset = fdt_path_offset(fdt, "/memory");
	if (offset < 0)
		return 0;

	prop = fdt_getprop(fdt, offset, "total_size", &len);
	if (!prop)
		return 0;

	if (len == 4) {
		size = fdt32_to_cpu(prop[0]);
	} else if (len == 8) {
		size = fdt32_to_cpu(prop[0]);
		size = fdt32_to_cpu(prop[1]) | size << 32;
	}

	return size;
}

/*
 * Setup stage2 translation, and relocate EL2 exception vector
 */
void ambarella_el2_pgtable_setup(void)
{
	uint64_t *pgtable_level1, *pgtable_level2, el2_base, el2_end;
	uint64_t start, dram_size = 0;
	int32_t index, rval;

	if (!ambarella_el2_is_used())
		return;
	/*
	 * Only the CPUs in the primary cluster can access the DRAM configuration registers.
	 * It is assumed that the DTB contains the correct DRAM size information.
	 * For other clusters, this information can be read from the DTB.
	 */
	if (ambarella_is_primary_cluster())
		dram_size = ambarella_dram_size();
	else {
		dram_size = ambarella_fdt_get_dram_size();
	}

	assert(dram_size);

	ambarella_el2_rsvd_mem(&el2_base, &el2_end);
	NOTICE("EL2 RSVD: %lx - %lx, DRAM: %lu %ciB\n", el2_base, el2_end - 1,
	       dram_size >> 30 ? : dram_size >> 20,
	       dram_size >> 30 ? 'G':'M');

	rval = mmap_add_dynamic_region(el2_base, el2_base, el2_end - el2_base,
					MT_MEMORY | MT_RW | MT_NS);
	assert(rval == 0);

	el2_pgtable_base = el2_base;
	el2_stack_base = el2_end - EL2_STACK_SIZE;
	el2_vector_base = el2_stack_base - EL2_VECTOR_SIZE;

	/*
	 *  + ------------ + ---> Pgtable
	 *  |              |
	 *  |     ...      |
	 *  |              |
	 *  + ------------ + ---> Vector
	 *  |              |
	 *  |  1MB - 4KB   |
	 *  |              |
	 *  + ------------ + ---> Stack
	 *  |     4KB      |
	 *  + ------------ + ---> END
	 */
	memset((uint64_t *)el2_pgtable_base, 0, el2_end - el2_base);

	el2_pgtable_level1 = el2_pgtable_base;

	pgtable_level1 = (uint64_t *)el2_pgtable_level1;

	/* dram */
	for (start = DRAM_BASE; start < DRAM_BASE + dram_size; start += ONE_GB) {
		index = ONE_GB_INDEX(start);
		pgtable_level1[index] = start | MEMORY_PROPERTIES | BLOCK_DESC;
	}

	/* device */
	for (start = DEVICE_BASE; start < DEVICE_BASE + DEVICE_SIZE; start += ONE_GB) {
		index = ONE_GB_INDEX(start);
		pgtable_level1[index] = start | DEVICE_PROPERTIES | BLOCK_DESC;
	}

	/*
	 * Stage2 MMU
	 *
	 *  + ------------- +
	 *  | Normal memory |
	 *  + ------------- +
	 *  | EL2 Reserved  | Invalid
	 *  + ------------- +
	 *  | Normal memory |
	 *  + ------------- +
	 *  |      DDRC     | Invalid
	 *  + ------------- +
	 *  | Device memory |
	 *  + ------------- +
	 */

	/* Invalid [el2_base, el2_end] */

	assert(ONE_GB_INDEX(el2_base) == ONE_GB_INDEX(el2_end));
	pgtable_level2 = ambarella_el2_pgtable_free();
	pgtable_level1[ONE_GB_INDEX(el2_base)] = (uint64_t)pgtable_level2 | TABLE_DESC;

	for (index = 0; index < XLAT_TABLE_ENTRIES; index++) {
		start = round_down(el2_base, ONE_GB) + index * TWO_MB;

		if (start < el2_base || start >= el2_end)
			pgtable_level2[index] = start | MEMORY_PROPERTIES | BLOCK_DESC;
		else
			pgtable_level2[index] = 0;
	}

	/* Invalid DDRC registers */

	assert(ONE_GB_INDEX(DRAMC_BASE) == ONE_GB_INDEX(DRAMC_BASE + DRAMC_SIZE));
	pgtable_level2 = ambarella_el2_pgtable_free();
	pgtable_level1[ONE_GB_INDEX(DRAMC_BASE)] = (uint64_t)pgtable_level2 | TABLE_DESC;

	for (index = 0; index < XLAT_TABLE_ENTRIES; index++) {
		start = round_down(DRAMC_BASE, ONE_GB) + index * TWO_MB;

#if (DEVICE_BASE > (1ULL << 32))
		if ((start + TWO_MB) <= DRAMC_BASE || start >= DRAMC_BASE + DRAMC_SIZE)
			pgtable_level2[index] = start | DEVICE_PROPERTIES | BLOCK_DESC;
		else
			pgtable_level2[index] = 0;
#else
		if ((start + TWO_MB) <= DRAMC_BASE) /* [start, DRAMC_BASE) -->2MB */
			pgtable_level2[index] = start | MEMORY_PROPERTIES | BLOCK_DESC;
		else if (start >= DRAMC_BASE + DRAMC_SIZE)
			pgtable_level2[index] = start | DEVICE_PROPERTIES | BLOCK_DESC;
		else
			pgtable_level2[index] = 0;
#endif
	}

	clean_dcache_range((uintptr_t)el2_base, EL2_RSVD_SIZE);

	/* Parse secure monitor */
	ambarella_fdt_parse_security();

	/* Setup EL2 system register */
	ambarella_el2_runtime_setup();
}

void ambarella_el2_rsvd_mem(uint64_t *base, uint64_t *end)
{
	uint64_t bl31_base = (uint64_t)bl31_entrypoint;
	uint64_t el2_base, el2_end;

	if (ambarella_is_primary_cluster()) {
		el2_base = EL2_RSVD_BASE;
		el2_end = EL2_RSVD_BASE + EL2_RSVD_SIZE;
	} else {
		el2_base = (bl31_base + (TWO_MB)) & ~(TWO_MB - 1);
		el2_end = el2_base + EL2_RSVD_SIZE;
	}

	if ((el2_base & (TWO_MB - 1))
		|| (el2_end & (TWO_MB - 1))) {
		ERROR("EL2 reserved memory must be 2MB aligned!\n");
		panic();
	}

	*base = el2_base;
	*end = el2_end;
}

/*****************************************************************************/

/* ISS encoding for EXCEPTION from a Data Abort */
typedef struct __data_abort {
	/* Data Fault status Code. */
	uint32_t dfsc:6;
	/*  Write not Read */
	uint32_t wnr: 1;
	uint32_t s1ptw:1;
	uint32_t cm:1;
	uint32_t ea:1;
	uint32_t fnv:1;
	uint32_t res0:3;
	uint32_t ar:1;
	uint32_t sf:1;
	/* register number of Rt */
	uint32_t srt:5;
	uint32_t sse:1;
	/*
	 * Syndrome Access size.
	 * 00 - Byte
	 * 01 - Halfword
	 * 10 - word
	 * 11 - Doubleword
	 * */
	uint32_t sas:2;
	uint32_t isv:1;
	uint32_t il:1;
	uint32_t ec:6;
} da_esr_t;

/* Context when calling SMC */
typedef struct __pt_reg {
	uint64_t reg[32];

	uint64_t elr_el2;
	uint64_t spsr_el2;
	uint64_t far_el2;
	uint64_t esr_el2;
	uint64_t hpfar_el2;
	uint64_t res0;

	uint64_t elr_el1;
	uint64_t spsr_el1;
	uint64_t far_el1;
	uint64_t esr_el1;
} pt_reg_t;

static int32_t pinctrl_handle_mmio(pt_reg_t *pt_reg, u_register_t r, da_esr_t *esr)
{
	return ambarella_pinctrl_security_handle(&pt_reg->reg[esr->srt], r, esr->sas, esr->wnr);
}

static int32_t gpio_handle_mmio(pt_reg_t *pt_reg, u_register_t r, da_esr_t *esr)
{
	return ambarella_gpio_security_handle(&pt_reg->reg[esr->srt], r, esr->sas, esr->wnr);
}

static bool iomem_permitted(u_register_t r, uint32_t wnr)
{
	u_register_t start;
	uint32_t idx, size;

	/*
	 * 1 check explicit na
	 * 2 check explicit read only
	 * 3 check explicit r/w
	 * 4 check implicit na
	 */
	for (idx = 0; idx < ARRAY_SIZE(reg_permission_na); idx++) {
		start = reg_permission_na[idx].start;
		size = reg_permission_na[idx].size;

		if (start == 0 && size == 0)
			break;

		if (r >= start && r < start + size)
			goto failure;
	}

	for (idx = 0; idx < ARRAY_SIZE(reg_permission_ro); idx++) {
		start = reg_permission_ro[idx].start;
		size = reg_permission_ro[idx].size;

		if (start == 0 && size == 0)
			break;

		if (r >= start && r < start + size) {
			if(wnr)
				goto failure;
			else
				goto success;
		}
	}

	for (idx = 0; idx < ARRAY_SIZE(reg_permission_rw); idx++) {
		start = reg_permission_rw[idx].start;
		size = reg_permission_rw[idx].size;

		if (start == 0 && size == 0)
			break;

		if (r >= start && r < start + size)
			goto success;
	}

	for (idx = 0; idx < ARRAY_SIZE(reg_permission_na_implicit); idx++) {
		start = reg_permission_na_implicit[idx].start;
		size = reg_permission_na_implicit[idx].size;

		if (start == 0 && size == 0)
			break;

		if (r >= start && r < start + size)
			goto failure;
	}

success:
	return true;

failure:
	ERROR("%s register[0x%08lx] denied\n", wnr ? "Write" : "Read", r);

	return false;
}

static int32_t handle_read(pt_reg_t *pt_reg, u_register_t r, da_esr_t *esr)
{
	uint64_t value;

	switch(esr->sas) {
	case 0:
		value = mmio_read_8(r);
		break;
	case 1:
		value = mmio_read_16(r);
		break;
	case 2:
		value = mmio_read_32(r);
		break;
	case 3:
		value = mmio_read_64(r);
		break;
	default:
		panic();
	}

	pt_reg->reg[esr->srt] = value;

	return 0;
}

static int32_t handle_write(pt_reg_t *pt_reg, u_register_t r, da_esr_t *esr)
{
	uint64_t value = pt_reg->reg[esr->srt];

	switch(esr->sas) {
	case 0:
		mmio_write_8(r, (uint8_t)value);
		break;
	case 1:
		mmio_write_16(r, (uint16_t)value);
		break;
	case 2:
		mmio_write_32(r, (uint32_t)value);
		break;
	case 3:
		mmio_write_64(r, (uint64_t)value);
		break;
	default:
		panic();
	}

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
		defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
	if (r >= SRAM_BASE && r < (SRAM_BASE + SRAM_SIZE))
		clean_dcache_range(r, sizeof(uint64_t));
#endif

	return 0;
}

static int32_t handle_mmio(pt_reg_t *pt_reg, u_register_t r, da_esr_t *esr)
{
	if (!iomem_permitted(r, esr->wnr))
		return -EPERM;

	if (!pinctrl_handle_mmio(pt_reg, r, esr))
		return 0;

	if (!gpio_handle_mmio(pt_reg, r, esr))
		return 0;

	return esr->wnr ? handle_write(pt_reg, r, esr) : handle_read(pt_reg, r, esr);
}

/*
 * Stage2 Translation SIP handler
 */
uintptr_t ambarella_el2_fault_handler(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	pt_reg_t *pt_reg = (pt_reg_t *)x1;
	bool user_mode;
	da_esr_t *esr;
	u_register_t r;
	int32_t rval = SMC_UNK;
	bool valid_range = false;

	assert(FNID_OF_SMC(smc_fid) == AMBA_SIP_EL2_DATA_ABORT);

	inv_dcache_range(x1, sizeof(pt_reg_t));

	esr = (da_esr_t *)&pt_reg->esr_el2;

	r = ((pt_reg->hpfar_el2 >> 4) << 12) | (pt_reg->far_el2 & PAGE_SIZE_MASK);

	if (!esr->isv)
		ERROR("No valid instruction syndrome: %lx \n", r);

	if ((r >= DEVICE_BASE) && (r < (DEVICE_BASE + DEVICE_SIZE)))
		valid_range = true;

	if ((r >= DRAMC_BASE) && (r < DRAMC_BASE  + DRAMC_SIZE))
		valid_range = true;

	/* Handle device memory from EL1t/h */
	if (esr->ec == EC_DABORT_LOWER_EL && esr->isv && valid_range) {
		rval = handle_mmio(pt_reg, r, esr);
		if (!rval) {
			pt_reg->elr_el2 += 4;	/* Skip the PC to fix the EXCEPTION */
			goto exit;
		}
	}

	/* Route DATA Abort to EL1t EXCEPTION */
	pt_reg->far_el1 = pt_reg->far_el2;
	pt_reg->elr_el1 = pt_reg->elr_el2;
	pt_reg->esr_el1 = pt_reg->esr_el2;

	pt_reg->esr_el1 &= ~(ESR_EC_MASK << ESR_EC_SHIFT);			// clear EC
	pt_reg->esr_el1 &= ~(EABORT_DFSC_MASK << EABORT_DFSC_SHIFT);		// clear DFSC
	pt_reg->esr_el1 |= SYNC_EA_FSC;						// Synchronous External Abort

	user_mode = (GET_EL(pt_reg->spsr_el2) == MODE_EL0) ? true : false;

	if (user_mode)
		pt_reg->esr_el1 |= (EC_DABORT_LOWER_EL << ESR_EC_SHIFT);
	else
		pt_reg->esr_el1 |= (EC_DABORT_CUR_EL << ESR_EC_SHIFT);

	pt_reg->spsr_el1 = pt_reg->spsr_el2;
	pt_reg->spsr_el2 = 0x3c5;
	pt_reg->elr_el2 = read_vbar_el1() + (user_mode ? 0x400 : 0x200);

exit:
	clean_dcache_range(x1, sizeof(pt_reg_t));
	SMC_RET1(handle, rval);
}
