/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <errno.h>
#include <plat_private.h>

#if (AMBARELLA_SUPPORT_SEURITY_CTRL == 1)

static uint32_t devic_security_ctrl[AXI_SEC_CTRL_NUM] = {0};

static void ambarella_device_security_restore(void)
{
	mmio_write_32(AXI_CFG_SEC0_REG, devic_security_ctrl[0]);
	mmio_write_32(AXI_CFG_SEC1_REG, devic_security_ctrl[1]);
	mmio_write_32(AXI_CFG_SEC2_REG, devic_security_ctrl[2]);
	mmio_write_32(AXI_CFG_SEC3_REG, devic_security_ctrl[3]);
	mmio_write_32(AXI_CFG_SEC4_REG, devic_security_ctrl[4]);
}

#endif

/* Setup specified device into secure world */
void ambarella_device_security_setup(uint32_t ctrl_bit)
{
	if (!ambarella_is_primary_cluster())
		return;

	INFO("Setup security control bit %u\n", ctrl_bit);

#if (AMBARELLA_SUPPORT_SEURITY_CTRL == 1)
	assert(ctrl_bit < AXI_SEC_CTRL_NUM * 32);

	devic_security_ctrl[ctrl_bit >> 5] |= 1 << (ctrl_bit % 32);

	switch (ctrl_bit >> 5) {
	case 0:
		mmio_setbits_32(AXI_CFG_SEC0_REG, 1 << (ctrl_bit % 32));
		break;
	case 1:
		mmio_setbits_32(AXI_CFG_SEC1_REG, 1 << (ctrl_bit % 32));
		break;
	case 2:
		mmio_setbits_32(AXI_CFG_SEC2_REG, 1 << (ctrl_bit % 32));
		break;
	case 3:
		mmio_setbits_32(AXI_CFG_SEC3_REG, 1 << (ctrl_bit % 32));
		break;
	case 4:
		mmio_setbits_32(AXI_CFG_SEC4_REG, 1 << (ctrl_bit % 32));
		break;
	}
#endif
}

static void ambarella_secure_memory_setup(void)
{
	uint64_t secure_limit, page_sz;

	/* secure and non secure memory access partitioning */
	secure_limit = SECURE_MEM_PATITIONING;

	page_sz = ambarella_dram_vpn_page_size();

#if (AMBARELLA_SUPPORT_AST == 0)
	mmio_write_32(DRAMC_REG(DRAM_SECMEM_BASE_OFFSET), BL2_BASE / page_sz);
	mmio_write_32(DRAMC_REG(DRAM_SECMEM_LIMIT_OFFSET), (secure_limit / page_sz) - 1);
	mmio_setbits_32(DRAMC_REG(DRAM_SECMEM_CTRL_OFFSET), 0x1);
#else
	uint32_t i;
	uint64_t dram_size = ambarella_dram_size();
	mmio_write_32(DRAMC_REG(AST_SEG_RELOC_OFFSET), 0x00000000);
	mmio_write_32(DRAMC_REG(AST_SEG_LIMIT_OFFSET), (secure_limit / page_sz) - 1);
#if defined(AMBARELLA_N1)
	mmio_write_32(DRAMC_REG(AST_SEG_RIGHT_OFFSET), 0xfffffff9);
#else
	mmio_write_32(DRAMC_REG(AST_SEG_RIGHT_OFFSET), 0xfffffff1);
#endif
	mmio_write_32(DRAMC_REG(AST_SEG_RIGHT_OFFSET + 4), 0xffffffff);

	for (i = 1; i < 16; i++) {
		mmio_write_32(DRAMC_REG(AST_SEG_RELOC_OFFSET + 4 * i), 0x00000000);
		mmio_write_32(DRAMC_REG(AST_SEG_LIMIT_OFFSET + 4 * i), (dram_size / page_sz) - 1);
	}
#endif
}

/*
 * Security setup:
 * - Setup all devices non-secure, and let DTS determine the secure devices.
 * - Setup secure memory if secure boot.
 * - Setup ATT to prevent EL2 reserved memory from corrupted by peripherals if
 *   EL2 is used, i.e., the DTB is signed.
 */
void ambarella_security_setup(void)
{
	uint32_t i;

	if (!ambarella_is_primary_cluster())
		return;

	/*
	 * re-eanble jtag via jtag_en register, after jtag_efuse bit is set
	 * this is only for secure analysis firmware, comment out it by default
	 */
	//mmio_setbits_32(AHBSP_JTAG_EN_REG, BIT(0));

	/* Omit checking the BL33_BASE while booting from PCI-e
	 *
	 * In PCI-e boot, BL33 is Linux kernel rather than bootloader. Linux is
	 * built with position independent.
	 */
	if (!ambarella_is_pcie_boot()) {
		assert(cookie_bld_ram_start() == BL33_BASE);
	}

#if (AMBARELLA_SUPPORT_SEURITY_CTRL == 1)
	/* Disable all devices's security by default in cold boot */
	ambarella_device_security_restore();
#endif

	for (i = 0; i <= NIC_GPV_MASTER_PORT; i++)
		mmio_write_32(NIC_GPV_REG(0x08 + i * 4), NON_SECURE);

	/* if secure-boot, make AXI secure */
	if (ambarella_is_secure_boot())
		mmio_write_32(NIC_GPV_REG(NIC_GPV_SLAVE_PORT_AXI), SECURE);
	else
	 	mmio_write_32(NIC_GPV_REG(NIC_GPV_SLAVE_PORT_AXI), NON_SECURE);

	/* NIC400-GPV-PWI to none-secure if exists */
#if (NIC_GPV_PWI_PORTS > 0)
	for (i = 0; i < NIC_GPV_PWI_PORTS; i++)
		mmio_write_32(NIC_GPV_PWI_REG(0x08 + i * 4), NON_SECURE);
#endif

	isb();
	dsb();

	/* setup platform ATT (Address Translation Table) */
	ambarella_att_setup();

	/* If secure boot is disabled, no need to setup Secure Memory */
	if (!ambarella_is_secure_boot())
		return;

	ambarella_secure_memory_setup();
}
