/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <lib/utils_def.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <drivers/arm/gicv2.h>
#include <drivers/delay_timer.h>
#include <plat/common/platform.h>
#include <plat_private.h>

#define DEAD(...)	do { wfi(); ERROR(__VA_ARGS__); panic();} while(0)

#define CORE_PWR_STATE(state) \
		((state)->pwr_domain_state[MPIDR_AFFLVL0])
#define SYSTEM_PWR_STATE(state) \
		((state)->pwr_domain_state[PLAT_MAX_PWR_LVL])

extern uint64_t ambarella_sec_entrypoint[PLATFORM_CORE_COUNT];

static uint32_t key_saved_once = 0;

static void ambarella_cpu_standby(plat_local_state_t cpu_state)
{
	unsigned int scr = read_scr_el3();

	assert(cpu_state == PLAT_MAX_RET_STATE);

	/* Enable Physical IRQ and FIQ bit for NS world to wake the CPU */
	write_scr_el3(scr | SCR_IRQ_BIT | SCR_FIQ_BIT);
	isb();
	dsb();
	wfi();

	/*
	 * Restore SCR to the original value, synchronisation of scr_el3 is
	 * done by eret while el3_exit to save some execution cycles.
	 */
	write_scr_el3(scr);
}


#if defined(PLAT_CFG_AMRTOS_ARM_TF) || defined(PLAT_CFG_FIX_WARM_REBOOT)
static void ambarella_jump_to_entry(uint32_t core)
{
	void (*entry)(void) = (void *)ambarella_sec_entrypoint[0];

	const uint32_t rvb_offset[] = {
		CORTEX_RVBARADDR0_OFFSET,
		CORTEX_RVBARADDR1_OFFSET,
		CORTEX_RVBARADDR2_OFFSET,
		CORTEX_RVBARADDR3_OFFSET
	};

	assert(core != 0);

	/* clear rvbar register, wait util calling cpu_on() again */
	mmio_write_32(AXI_BASE + rvb_offset[core], 0);
	while (!mmio_read_32(AXI_BASE + rvb_offset[core])) {
		dsb();
	}

	entry();
}
#endif

static int ambarella_pwr_domain_on(u_register_t mpidr)
{
	uint32_t cpuid, reset_bit;
	const uint32_t rvb_offset[] = {
		CORTEX_RVBARADDR0_OFFSET,
		CORTEX_RVBARADDR1_OFFSET,
		CORTEX_RVBARADDR2_OFFSET,
		CORTEX_RVBARADDR3_OFFSET
	};
	const uint32_t reset_mask [] = {
		CORTEX_CORE0_RESET_MASK,
		CORTEX_CORE1_RESET_MASK,
		CORTEX_CORE2_RESET_MASK,
		CORTEX_CORE3_RESET_MASK,
	};

	ambarella_sec_entrypoint[plat_core_pos_by_mpidr(mpidr)] = ambarella_sec_entrypoint[0];
	clean_dcache_range((uintptr_t)ambarella_sec_entrypoint,
			   sizeof(ambarella_sec_entrypoint));

	if (!ambarella_is_primary_cluster()) {
		sev();
		return PSCI_E_SUCCESS;
	}

	cpuid = plat_core_pos_by_mpidr(mpidr);
	assert(cpuid != 0);

	reset_bit = reset_mask[cpuid];

	mmio_write_32(AXI_BASE + rvb_offset[cpuid], CORTEX_RVBAR_ADDR(BL31_BASE));
	dsb();
#if defined(CORTEX_RESET_SECONDARY_WORKAROUND) || defined(PLAT_CFG_FIX_WARM_REBOOT)
	/* secondary CPU is in WFE state */
	sev();
#else
	/* put the secondary cores into reset state */
	do {
		mmio_setbits_32(AXI_BASE + CORTEX_RESET_OFFSET, reset_bit);
	} while (!(mmio_read_32(AXI_BASE + CORTEX_RESET_OFFSET) & reset_bit));
#endif

	mmio_clrbits_32(AXI_BASE + CORTEX_RESET_OFFSET, reset_bit);

	return PSCI_E_SUCCESS;
}

static void ambarella_pwr_domain_off(const psci_power_state_t *target_state)
{
	/* Prevent interrupts from spuriously waking up this cpu */
	gicv2_cpuif_disable();
}

static void ambarella_pwr_domain_suspend(const psci_power_state_t *target_state)
{
	if (CORE_PWR_STATE(target_state) != PLAT_MAX_OFF_STATE)
		return;

	/* Prevent interrupts from spuriously waking up this cpu */
	gicv2_cpuif_disable();

	if (SYSTEM_PWR_STATE(target_state) == PLAT_MAX_OFF_STATE) {
		/* SYSTEM_SUSPEND only on CPU0 */
		assert(plat_my_core_pos() == 0);

#if 0
		/* Turn off all of the secondary cpus */
		mmio_setbits_32(AXI_BASE + CORTEX_RESET_OFFSET,
					CORTEX_CORE1_RESET_MASK |
					CORTEX_CORE2_RESET_MASK |
					CORTEX_CORE3_RESET_MASK);
#endif
	}
}

static void ambarella_pwr_domain_on_finish(const psci_power_state_t *target_state)
{
	/* Initialize the GIC per-cpu and distributor interfaces */
	ambarella_gic_pcpu_init();
}

static void ambarella_pwr_domain_suspend_finish(const psci_power_state_t *target_state)
{
	if (SYSTEM_PWR_STATE(target_state) != PLAT_MAX_OFF_STATE)
		goto finish;

	ambarella_security_setup();
	ambarella_soc_fixup();

	/* Initialize the GIC distributor interfaces */
	ambarella_gic_distif_init();
finish:
	/* Initialize the GIC per-cpu and distributor interfaces */
	ambarella_gic_pcpu_init();
}

static void ambarella_pwc_poweroff(void)
{
#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) \
	|| defined(AMBARELLA_CV28) || defined(AMBARELLA_S6LM)
	mmio_setbits_32(RCT_BASE + ANA_PWR_OFFSET, 0x20);
#endif
}

static void __dead2 ambarella_system_off(void)
{
	ambarella_gpio_notify_mcu(NOTIFY_MCU_POWEROFF);
	ambarella_pwc_poweroff();
	DEAD("Ambarella System Off: operation not handled.\n");
}

static void rct_enable_force_usb(uint32_t enabled)
{
	if (enabled) {
		mmio_setbits_32(RCT_BASE + SYS_CONFIG_OFFSET, SYS_CONFIG_USB_BOOT);
	} else {
		mmio_clrbits_32(RCT_BASE + SYS_CONFIG_OFFSET, SYS_CONFIG_USB_BOOT);
	}
}

static void rct_reset_chip(void)
{
	mmio_clrbits_32(RCT_BASE + SOFT_OR_DLL_RESET_OFFSET, SOFT_OR_DLL_RESET_VALUE);
	isb();
	mmio_setbits_32(RCT_BASE + SOFT_OR_DLL_RESET_OFFSET, SOFT_OR_DLL_RESET_VALUE);

	dsb();
	isb();
}

#if defined(CORTEX_BOOT_SECONDARY_WORKAROUND)
static void ambarella_boot_seconadry_workaround(void)
{
	dcsw_op_all(DCCISW);
	/* special: amboot(el1) cold reboot(core1 must live before do warm reset) */
	mmio_write_32(AXI_BASE + CORTEX_RVBARADDR1_OFFSET,
	CORTEX_RVBAR_ADDR((uintptr_t)ambarella_fake_bootentry));
	dsb();
	mmio_clrbits_32(AXI_BASE + CORTEX_RESET_OFFSET, CORTEX_CORE1_RESET_MASK);
}
#endif

static void __dead2 ambarella_system_reset(void)
{
	rct_enable_force_usb(0);
#if defined(CORTEX_BOOT_SECONDARY_WORKAROUND)
	ambarella_boot_seconadry_workaround();
#endif
	rct_reset_chip();

	DEAD("Ambarella System Reset: operation not handled.\n");
}

static int ambarella_validate_power_state(unsigned int power_state,
				psci_power_state_t *req_state)
{
	int pstate = psci_get_pstate_type(power_state);

	assert(req_state);

	/* Sanity check the requested state */
	if (pstate == PSTATE_TYPE_STANDBY)
		req_state->pwr_domain_state[MPIDR_AFFLVL0] = PLAT_MAX_RET_STATE;
	else
		req_state->pwr_domain_state[MPIDR_AFFLVL0] = PLAT_MAX_OFF_STATE;

	/* We expect the 'state id' to be zero */
	if (psci_get_pstate_id(power_state))
		return PSCI_E_INVALID_PARAMS;

	return PSCI_E_SUCCESS;
}

static int ambarella_validate_ns_entrypoint(unsigned long ns_entrypoint)
{
	if ((ns_entrypoint & 0x3UL) != 0UL)
		return PSCI_E_INVALID_ADDRESS;

	return PSCI_E_SUCCESS;
}

static void ambarella_get_sys_suspend_power_state(psci_power_state_t *req_state)
{
	for (int i = MPIDR_AFFLVL0; i <= PLAT_MAX_PWR_LVL; i++)
		req_state->pwr_domain_state[i] = PLAT_MAX_OFF_STATE;
}

static void ambarella_cpucore_workaround(void)
{
	/* It is A53_Errata: 843819 workaround apply this before mmu off */
#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) \
	|| defined(AMBARELLA_CV28) || defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV2FS)

	unsigned long value;

	asm volatile("mrs %0, hcr_el2":"=r"(value): :"memory");
	VERBOSE("hcr_el2 = 0x%lx \n", value);
	value &= ~1; 		/* clear vm */
	asm volatile("msr hcr_el2, %0": :"r"(value):);
	dsb();
	isb();
#endif
}

static void ambarella_pwc_suspend_prepare(void)
{
#if !defined(AMBARELLA_CV8)
	uint32_t key_in, key_out;

	/* always set bit2 of pwc_status when suspend */
	mmio_setbits_32(PWC_REG(PWC_SET_STA_OFFSET), 0x1 << 2);
	mmio_write_32(PWC_REG(PWC_SET_RTC_OFFSET), mmio_read_32(PWC_REG(PWC_CUR_RTC_OFFSET)));
	mdelay(10); /* data must be valid >=4ms */
	mmio_write_32(PWC_REG(PWC_RESET_OFFSET), 0x1);
	mdelay(10); /* data and PCRST must be valid >=4ms */
	mmio_write_32(PWC_REG(PWC_RESET_OFFSET), 0x0);

	if (ambarella_is_secure_boot() && (!key_saved_once)) {
#if defined(AMBARELLA_CV75)
		mmio_write_32(PWC_DBG_REG(AHBSP_PWC_STROBE_OFFSET), 1);
		mdelay(10); /* at least 3ms, but we use 10ms for ensurance */
		mmio_write_32(PWC_DBG_REG(AHBSP_PWC_STROBE_OFFSET), 0);
#else
		mmio_write_32(SECURE_SCRATCHPAD_REG(AHBSP_PWC_STROBE_OFFSET), 1);
		mdelay(10); /* at least 3ms, but we use 10ms for ensurance */
		mmio_write_32(SECURE_SCRATCHPAD_REG(AHBSP_PWC_STROBE_OFFSET), 0);
#endif
		mdelay(10);
		key_in = mmio_read_32(PWC_DBG_REG(PWC_KEY_IN0_REG_OFFSET));
		key_out = mmio_read_32(PWC_DBG_REG(PWC_KEY_OUT0_REG_OFFSET));
		assert(key_in == key_out);
		key_saved_once = 1;
	}
#else
	/* CV8: No PWC & RTC on plat */
	uint32_t key_in, key_out;

	/* always set bit2 of pwc_status when suspend */
	mmio_setbits_32(DDRH0_REG(DDRH_STA_IN_OFFSET), 0x1 << 2);
	mmio_setbits_32(DDRH1_REG(DDRH_STA_IN_OFFSET), 0x1 << 2);
	mmio_write_32(DDRH0_REG(DDRH_UPDATE_EN_OFFSET), 0x0);
	mmio_write_32(DDRH1_REG(DDRH_UPDATE_EN_OFFSET), 0x0);
	mdelay(10);
	mmio_write_32(DDRH0_REG(DDRH_UPDATE_EN_OFFSET), 0x1);
	mmio_write_32(DDRH1_REG(DDRH_UPDATE_EN_OFFSET), 0x1);
	mdelay(10);
	mmio_write_32(DDRH0_REG(DDRH_UPDATE_EN_OFFSET), 0x0);
	mmio_write_32(DDRH1_REG(DDRH_UPDATE_EN_OFFSET), 0x0);

	if (ambarella_is_secure_boot() && (!key_saved_once)) {
		key_in = mmio_read_32(DDRH0_REG(DDRH_KEY_IN0_REG_OFFSET));
		key_out = mmio_read_32(DDRH0_REG(DDRH_KEY_OUT0_REG_OFFSET));
		assert(key_in == key_out);
		key_saved_once = 1;
	}
#endif
}

static void __dead2 ambarella_pwr_domain_pwr_down_wfi(const psci_power_state_t *target_state)
{
	if (SYSTEM_PWR_STATE(target_state) != PLAT_MAX_OFF_STATE) {
#if HW_ASSISTED_COHERENCY
		/* Flush all caches. */
		dcsw_op_all(DCCISW);

#if defined(PLAT_CFG_AMRTOS_ARM_TF) || defined(PLAT_CFG_FIX_WARM_REBOOT)
		/*
		 * Since p-channel is not implemented in CV3 and CV72, power-down secondary cpu by
		 * programming AXI register doesn't work.
		 *
		 * In AmRTOS case, secondary cpu would be booted twice. In AmRTOS, before jumping to
		 * kernel, secondary cpu would be shut down, and during kernel booting, boot secondary
		 * cpu by calling psci_cpu_on() again.
		 * */
		disable_mmu_el3();
		ambarella_jump_to_entry(plat_my_core_pos());
#endif
#endif
		psci_power_down_wfi();
		DEAD("CPU%d is not power down\n", plat_my_core_pos());
	}

	ambarella_pwc_suspend_prepare();
	ambarella_gpio_notify_mcu(NOTIFY_MCU_SUSPEND);
	ambarella_relocate_suspend();
	NOTICE("ambarella_relocate_suspend done \n");
	ambarella_cpucore_workaround();

	/* Flush all caches. */
	dcsw_op_all(DCCISW);
	/* disable mmu and icache */
	disable_mmu_icache_el3();
	tlbialle3();
	dsb();
	isb();

	if (ambarella_is_primary_cluster())
		ambarella_do_suspend();

	/* Should never reach here */
	DEAD("Ambarella System Power Down: operation not handled.\n");
}

/*******************************************************************************
 * Export the platform handlers to enable psci to invoke them
 ******************************************************************************/
static const struct plat_psci_ops ambarella_psci_ops = {
	.cpu_standby			= ambarella_cpu_standby,
	.pwr_domain_on			= ambarella_pwr_domain_on,
	.pwr_domain_on_finish		= ambarella_pwr_domain_on_finish,
	.pwr_domain_off			= ambarella_pwr_domain_off,
	.pwr_domain_suspend		= ambarella_pwr_domain_suspend,
	.pwr_domain_suspend_finish	= ambarella_pwr_domain_suspend_finish,
	.pwr_domain_pwr_down_wfi	= ambarella_pwr_domain_pwr_down_wfi,
	.system_off			= ambarella_system_off,
	.system_reset			= ambarella_system_reset,
	.validate_power_state		= ambarella_validate_power_state,
	.validate_ns_entrypoint		= ambarella_validate_ns_entrypoint,
	.get_sys_suspend_power_state	= ambarella_get_sys_suspend_power_state,
};


/*******************************************************************************
 * Export the platform specific power ops.
 ******************************************************************************/
int plat_setup_psci_ops(uintptr_t sec_entrypoint,
			const struct plat_psci_ops **psci_ops)
{
	ambarella_sec_entrypoint[0] = sec_entrypoint; /* i.e., bl31_warm_entrypoint */
	*psci_ops = &ambarella_psci_ops;

	return 0;
}
