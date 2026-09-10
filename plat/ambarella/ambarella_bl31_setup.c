/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#include <assert.h>
#include <lib/mmio.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <bl31/bl31.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <drivers/generic_delay_timer.h>
#include <plat/common/platform.h>
#include <uart_ambarella.h>
#include <plat_private.h>
#include <boot_cookie.h>

static console_t bl31_console;

static entry_point_info_t bl33_image_ep_info;
static entry_point_info_t bl32_image_ep_info;

/*
 * Return a pointer to the 'entry_point_info' structure of the next image for
 * the security state specified. BL33 corresponds to the non-secure image type
 * while BL32 corresponds to the secure image type. A NULL pointer is returned
 * if the image does not exist.
 */
entry_point_info_t *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	entry_point_info_t *next_image_info;

	next_image_info = (type == NON_SECURE) ? &bl33_image_ep_info : &bl32_image_ep_info;

	/* None of the images on this platform can have 0x0 as the entrypoint */
	if (next_image_info->pc)
		return next_image_info;
	else
		return NULL;
}

/*******************************************************************************
 * Perform any BL3-1 early platform setup, such as console init and deciding on
 * memory layout.
 ******************************************************************************/
void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
		u_register_t arg2, u_register_t arg3)
{
	boot_cookie_t *cookie;

	if (ambarella_is_primary_cluster()) {
		console_ambarella_register(UART0_BASE, UART_CLOCK, UART_BAUDRATE, &bl31_console);
	} else {
		/* TODO */
	}
	printf("\x1b[4l\r\n");	/* Set terminal to replacement mode */

	cookie = boot_cookie_init();
	assert(cookie);

	NOTICE("BL31: up at %u msec(s)\n", apb_timer_get_msec());

#ifndef SPD_none
	if (ambarella_is_primary_cluster()) {
		SET_PARAM_HEAD(&bl32_image_ep_info, PARAM_EP, VERSION_1, 0);
		SET_SECURITY_STATE(bl32_image_ep_info.h.attr, SECURE);
		bl32_image_ep_info.pc = BL32_BASE;
		bl32_image_ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
		/* Pass DTB address to BL32 (OP-TEE) via arg2 */
		bl32_image_ep_info.args.arg2 = cookie_dtb_ram_start();
		NOTICE("BL31: Secure code at 0x%08lx \n", bl32_image_ep_info.pc);
		NOTICE("BL31: DTB address for BL32: 0x%016llx \n",
		       (unsigned long long)bl32_image_ep_info.args.arg2);
	}
#endif

	SET_PARAM_HEAD(&bl33_image_ep_info, PARAM_EP, VERSION_1, 0);
	SET_SECURITY_STATE(bl33_image_ep_info.h.attr, NON_SECURE);
	bl33_image_ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
	bl33_image_ep_info.pc = cookie_bld_ram_start();
	bl33_image_ep_info.args.arg0 = cookie_dtb_ram_start();
	NOTICE("BL31: Non Secure code at 0x%08lx \n", bl33_image_ep_info.pc);
}

/*******************************************************************************
 * Perform the very early platform specific architectural setup here. At the
 * moment this is only intializes the mmu in a quick and dirty way.
 ******************************************************************************/
void bl31_plat_arch_setup(void)
{
	ambarella_security_setup();
	ambarella_soc_fixup();

	ambarella_mmap_setup(NULL);
}

/*******************************************************************************
 * Perform any BL3-1 platform setup code
 ******************************************************************************/
void bl31_platform_setup(void)
{
	ambarella_fdt_init();

	generic_delay_timer_init();

	/* Initialize the GIC driver, cpu and distributor interfaces */
	ambarella_gic_driver_init();
	ambarella_gic_distif_init();
	ambarella_gic_pcpu_init();

	ambarella_gpio_init();
}

unsigned int plat_get_syscnt_freq2(void)
{
	return get_sys_timer_parent_freq_hz() / AXI_SYS_TIMER_DIVISOR;
}

void bl31_plat_prepare_kernel32_entry(uint64_t x1, uint64_t x2,
		uint64_t x3, uint64_t x4)
{
	SET_SECURITY_STATE(bl33_image_ep_info.h.attr, NON_SECURE);
	bl33_image_ep_info.spsr = SPSR_MODE32(MODE32_hyp, SPSR_T_ARM,
			SPSR_E_LITTLE, DAIF_FIQ_BIT | DAIF_IRQ_BIT | DAIF_ABT_BIT);
	bl33_image_ep_info.pc = x1;
	bl33_image_ep_info.args.arg0 = 0;	/* r0 */
	bl33_image_ep_info.args.arg1 = 0;	/* r1 */
	bl33_image_ep_info.args.arg2 = x2;	/* r2, i.e., dtb */

	/* Entrypoint must be non-zero and 4-byte aligned (AArch32 ARM mode) */
	assert(bl33_image_ep_info.pc &&
	       !(bl33_image_ep_info.pc & 0x3));

	cm_init_my_context(&bl33_image_ep_info);
	cm_prepare_el3_exit(NON_SECURE);
}

static void bl31_plat_shmem_init(void)
{
	if (!ambarella_is_primary_cluster())
		return;
#if defined(AMBARELLA_NEED_SHMEM_INIT)

	/* R52_A78 SHMEM can only accessed by A78-EL3 or HSM */
	uint32_t i = 0;
	uintptr_t shmem_base = 0xff00000000UL;

	for (i = 0; i < 16; i++)
		mmio_write_8(shmem_base + 0x1e000 + i, 0x05);

	for (i = 0; i < 96; i += 4)
		mmio_write_32(shmem_base + 0x1e100 + i, 0x20202020);

	for (i = 0; i < 32; i += 4)
		mmio_write_32(shmem_base + 0x1e160 + i, 0x20202020);

	dsbsy();
	VERBOSE("configure shmem done \n");
#endif
}

void bl31_plat_runtime_setup(void)
{
	ambarella_cpufreq_parse();
	ambarella_el2_pgtable_setup();
	bl31_plat_shmem_init();

#if 0
#if (DEBUG == 0)
	console_switch_state(CONSOLE_FLAG_RUNTIME);
#endif
#endif
	NOTICE("BL31: exit at %u msec(s)\n", apb_timer_get_msec());
}

