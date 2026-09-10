/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <stddef.h>
#include <common/debug.h>
#include <common/runtime_svc.h>
#include <lib/pmf/pmf.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat_private.h>
#include <ambarella_smc.h>
#include <ambarella_secure_debug.h>
#if defined(AMBA_SECURITY_ARCH_sec_v3)
#include "driver/ambarella_otp_flex.h"
#else
#include "driver/ambarella_otp.h"
#endif

static int ambarella_boot_cluster(uint32_t smc_fid,
				  u_register_t x1,
				  u_register_t x2,
				  u_register_t x3,
				  u_register_t x4,
				  void *cookie,
				  void *handle,
				  u_register_t flags)
{
	int ret = SMC_OK;

	uint32_t fn = FNID_OF_SMC(smc_fid);

	switch(fn) {
	case AMBA_SIP_FUNC_CPU_ON:
		if (ambarella_cluster_cpu_on(x1, x2, x3, x4))
			ret = SMC_UNK;
		break;
	case AMBA_SIP_FUNC_CPU_OFF:
		if (ambarella_cluster_cpu_off(x1, x2, x3, x4))
			ret = SMC_UNK;
		break;
	default:
		ret = SMC_PREEMPTED;
	}

	SMC_RET1(handle, ret);
}

static uint64_t ambarella_freq_setup(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);

	switch (fn) {
	case AMBA_SCM_CNTFRQ_SETUP_CMD:
		write_cntfrq_el0(plat_get_syscnt_freq2());
		SMC_RET1(handle, SMC_OK);

	default:
		SMC_RET1(handle, SMC_UNK);
	}
}

static uint64_t ambarella_switch_to_aarch32(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);

	switch (fn) {
	case AMBA_SIP_AARCH32_KERNEL:
		if (psci_secondaries_brought_up())
			SMC_RET1(handle, -2);

		bl31_plat_prepare_kernel32_entry(x1, x2, x3, x4);
		SMC_RET0(handle);

	default:
		SMC_RET1(handle, SMC_UNK);
	}
}

static uint64_t ambarella_lp5_adjust(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);
	uint32_t rval;

	switch (fn) {
	case AMBA_SIP_LP5_ADJUST_ISLP5:
		rval = ambarella_lp5_adjust_islp5();
		SMC_RET1(handle, rval);

	case AMBA_SIP_LP5_ADJUST_INIT:
		ambarella_lp5_adjust_init();
		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_LP5_ADJUST_RUN:
		ambarella_lp5_adjust_run();
		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_LP5_ADJUST_SET_PVAL:
		ambarella_lp5_adjust_set_pval(x1);
		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_LP5_ADJUST_GET_PVAL:
		rval = ambarella_lp5_adjust_get_pval();
		SMC_RET1(handle, rval);

	case AMBA_SIP_LP5_ADJUST_SET_NVAL:
		ambarella_lp5_adjust_set_nval(x1);
		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_LP5_ADJUST_GET_NVAL:
		rval = ambarella_lp5_adjust_get_nval();
		SMC_RET1(handle, rval);

	case AMBA_SIP_LP5_ADJUST_SHOW_SWITCH:
		ambarella_lp5_adjust_show_switch();
		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_LP5_ADJUST_SET_WCK2DQI_TIMER:
		ambarella_lp5_adjust_set_wck2dqi_timer();
		SMC_RET1(handle, SMC_OK);

	default:
		SMC_RET1(handle, SMC_UNK);
	}
}

static uint64_t ambarella_secure_debug_smc(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);
	unsigned long base, end, map_size;
	unsigned long addr_min, addr_max;
	uint32_t mem_attr;
	int ret;
	uint32_t result = 0U;

	(void)x4;
	(void)cookie;

	mem_attr = is_caller_secure(flags) ?
			(MT_SECURE | MT_RW_DATA) : (MT_NS | MT_RW_DATA);

	switch (fn) {
	case AMBA_SIP_SD_GET_CHALLENGE:
		if (!x1 || x2 != AMB_SD_CHALLENGE_BYTES) {
			ERROR("bl31 sd_smc: GET_CHALLENGE bad args x1=%lx x2=%lx\n",
			      (unsigned long)x1, (unsigned long)x2);
			SMC_RET1(handle, SMC_UNK);
		}
		if (check_uptr_overflow((uintptr_t)x1, (uintptr_t)x2)) {
			ERROR("secure debug: challenge buffer overflow x1=%lx x2=%lx\n",
			      (unsigned long)x1, (unsigned long)x2);
			SMC_RET1(handle, SMC_UNK);
		}
		base = round_down(x1, PAGE_SIZE);
		end = round_up(x1 + x2, PAGE_SIZE);
		map_size = end - base;
		if (end < base) {
			ERROR("secure debug: challenge map size wrap\n");
			SMC_RET1(handle, SMC_UNK);
		}
		ret = mmap_add_dynamic_region(base, base, map_size, mem_attr);
		if (ret) {
			ERROR("secure debug: map challenge buffer failed %d\n", ret);
			SMC_RET1(handle, SMC_UNK);
		}

		inv_dcache_range(x1, x2);
		ret = ambarella_secure_debug_issue_challenge((uint8_t *)x1);

		flush_dcache_range(x1, x2);
		mmap_remove_dynamic_region(base, map_size);
		if (ret != 0) {
			SMC_RET1(handle, SMC_UNK);
		}

		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_SD_EXECUTE:
		if ((!x1) || (!x2)) {
			ERROR("zero x1(%lx) or x2(%lx)\n", x1, x2);
			SMC_RET1(handle, SMC_UNK);
		}
		if (x2 < (AMB_SD_CHALLENGE_BYTES + AMB_SD_SIGNATURE_BYTES + sizeof(uint32_t))) {
			ERROR("too small x2 (%ld)\n", x2);
			SMC_RET1(handle, SMC_UNK);
		}
		if (check_uptr_overflow((uintptr_t)x1, (uintptr_t)x2)) {
			ERROR("secure debug: exec buffer overflow x1=%lx x2=%lx\n",
			      (unsigned long)x1, (unsigned long)x2);
			SMC_RET1(handle, SMC_UNK);
		}
		addr_min = (unsigned long) x1;
		addr_max = (unsigned long) x1 + (unsigned long) x2;
		base = round_down(addr_min, PAGE_SIZE);
		end = round_up(addr_max, PAGE_SIZE);
		map_size = end - base;
		if (end < base) {
			ERROR("secure debug: exec map size wrap\n");
			SMC_RET1(handle, SMC_UNK);
		}
		ret = mmap_add_dynamic_region(base, base, map_size, mem_attr);
		if (ret) {
			ERROR("secure debug: map exec buffer failed %d\n", ret);
			SMC_RET1(handle, SMC_UNK);
		}
		inv_dcache_range(x1, x2);
		ret = ambarella_secure_debug_execute((const uint8_t *)x1, (size_t) x2 - AMB_SD_SIGNATURE_BYTES,
				(const uint8_t *) (x1 + x2 - AMB_SD_SIGNATURE_BYTES), &result);
		flush_dcache_range(x1, x2);
		mmap_remove_dynamic_region(base, map_size);
		if (ret != 0) {
			SMC_RET1(handle, SMC_UNK);
		}
		SMC_RET1(handle, result);

	default:
		SMC_RET1(handle, SMC_UNK);
	}
}

#if defined(AMBA_SECURITY_ARCH_sec_v3)
static uint64_t ambarella_otp_auth_smc(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);
	unsigned long base, end, map_size;
	unsigned long addr_min, addr_max;
	uint32_t mem_attr;
	int ret;
	uint32_t result = 0U;

	(void)x3;
	(void)x4;
	(void)cookie;

	mem_attr = is_caller_secure(flags) ?
			(MT_SECURE | MT_RW_DATA) : (MT_NS | MT_RW_DATA);

	switch (fn) {
	case AMBA_SIP_OTP_GET_CHALLENGE:
		if (!x1 || x2 != AMB_OTP_CHALLENGE_BYTES) {
			SMC_RET1(handle, SMC_UNK);
		}
		if (check_uptr_overflow((uintptr_t)x1, (uintptr_t)x2)) {
			SMC_RET1(handle, SMC_UNK);
		}
		base = round_down(x1, PAGE_SIZE);
		end = round_up(x1 + x2, PAGE_SIZE);
		map_size = end - base;
		if (end < base) {
			SMC_RET1(handle, SMC_UNK);
		}
		ret = mmap_add_dynamic_region(base, base, map_size, mem_attr);
		if (ret) {
			SMC_RET1(handle, SMC_UNK);
		}
		inv_dcache_range(x1, x2);
		ret = ambarella_otp_auth_issue_challenge((uint8_t *)x1);
		flush_dcache_range(x1, x2);
		mmap_remove_dynamic_region(base, map_size);
		if (ret != 0) {
			SMC_RET1(handle, SMC_UNK);
		}
		SMC_RET1(handle, SMC_OK);

	case AMBA_SIP_OTP_EXECUTE:
		if ((!x1) || (!x2)) {
			SMC_RET1(handle, SMC_UNK);
		}
		if (x2 < (AMB_OTP_CHALLENGE_BYTES + AMB_OTP_SIGNATURE_BYTES +
				sizeof(struct otp_auth_op_hdr))) {
			SMC_RET1(handle, SMC_UNK);
		}
		if (check_uptr_overflow((uintptr_t)x1, (uintptr_t)x2)) {
			SMC_RET1(handle, SMC_UNK);
		}
		addr_min = (unsigned long)x1;
		addr_max = (unsigned long)x1 + (unsigned long)x2;
		base = round_down(addr_min, PAGE_SIZE);
		end = round_up(addr_max, PAGE_SIZE);
		map_size = end - base;
		if (end < base) {
			SMC_RET1(handle, SMC_UNK);
		}
		ret = mmap_add_dynamic_region(base, base, map_size, mem_attr);
		if (ret) {
			SMC_RET1(handle, SMC_UNK);
		}
		inv_dcache_range(x1, x2);
		ret = ambarella_otp_auth_execute((const uint8_t *)x1,
				(size_t)x2 - AMB_OTP_SIGNATURE_BYTES,
				(const uint8_t *)(x1 + x2 - AMB_OTP_SIGNATURE_BYTES),
				&result);
		flush_dcache_range(x1, x2);
		mmap_remove_dynamic_region(base, map_size);
		if (ret != 0) {
			SMC_RET1(handle, SMC_UNK);
		}
		SMC_RET1(handle, result);

	default:
		SMC_RET1(handle, SMC_UNK);
	}
}
#endif

static uint64_t ambarella_vp_config(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);
	uint64_t rval = SMC_UNK;

	switch (fn) {
	case AMBA_SIP_VP_CONFIG_RESET:
		rct_soft_reset_vp_cluster();
		rval = SMC_OK;
		break;
	default:
		break;
	}

	SMC_RET1(handle, rval);
}

static uint64_t ambarella_get_otp_huk(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t fn = FNID_OF_SMC(smc_fid);
	unsigned long base = 0, end = 0, size = 0;
	int ret;
	int map_buf = 1;

	switch (fn) {
	case AMBA_SIP_SET_MISC_CONFIG:
	case AMBA_SIP_LOCK_PUKEY:
	case AMBA_SIP_ADD_COUNTER:
	case AMBA_SIP_PERMANENTLY_ENABLE_SECURE_BOOT:
	case AMBA_SIP_REVOKE_KEY:
	case AMBA_SIP_SET_JTAG_EFUSE:
	case AMBA_SIP_LOCK_ZONA_A:
	case AMBA_SIP_INCREASE_BST_VER:
	case AMBA_SIP_EN_ANTI_ROLLBACK:
	case AMBA_SIP_DIS_SECURE_USB_BOOT:
	case AMBA_SIP_DIS_MONO_CNTS:
	case AMBA_SIP_SET_AND_LOCK_AMBA_RESERVED:
	case AMBA_SIP_LOCK_AMBA_RESERVED:
	case AMBA_SIP_GENERIC_OTP_WRITE:
		/* Scalar SMC args — must not treat x1/x2 as a buffer */
		map_buf = 0;
		break;
	default:
		break;
	}

	if (map_buf && x1 && x2) {
		if (check_uptr_overflow((uintptr_t)x1, (uintptr_t)x2)) {
			ERROR("otp huk: buffer overflow x1=%lx x2=%lx\n",
			      (unsigned long)x1, (unsigned long)x2);
			SMC_RET1(handle, SMC_UNK);
		}
		base = round_down(x1, PAGE_SIZE);
		end = round_up(x1 + x2, PAGE_SIZE);
		size = end - base;
		if (end < base) {
			ERROR("otp huk: map size wrap\n");
			SMC_RET1(handle, SMC_UNK);
		}

		ret = mmap_add_dynamic_region(base, base, size, MT_SECURE | MT_RW_DATA);
		if (ret) {
			ERROR("failed to map memory for UUID/CHIP_ID: %d\n", ret);
			SMC_RET1(handle, SMC_UNK);
		}

		inv_dcache_range(x1, x2);
	}

	switch (fn) {
	case AMBA_SIP_GET_AMBA_UNIQUE_ID:
		ret = ambarella_otp_read_amba_unique_id((uint8_t *) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_GET_PUKEY:
		ret = ambarella_otp_read_rot_pubkey((uint8_t *) x1,
			(uint32_t) x2, (uint32_t) x3);
		break;

	case AMBA_SIP_SET_PUKEY:
		ret = ambarella_otp_write_rot_pubkey((uint8_t *) x1,
			(uint32_t) x2, (uint32_t) x3);
		break;

	case AMBA_SIP_LOCK_PUKEY:
		ret = ambarella_otp_lock_rot_pubkey((uint32_t) x1, (uint32_t) x3);
		break;

	case AMBA_SIP_GET_CUSTOMER_ID:
		ret = ambarella_otp_read_customer_id((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_SET_CUSTOMER_ID:
		ret = ambarella_otp_write_customer_id((uint8_t *) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_GET_COUNTER:
		ret = ambarella_otp_read_mono_counter((uint32_t *) x1,
			(uint32_t) x2, (uint32_t) x3);
		break;

	case AMBA_SIP_ADD_COUNTER:
		ret = ambarella_otp_increase_mono_counter((uint32_t) x1, (uint32_t) x3);
		break;

	case AMBA_SIP_PERMANENTLY_ENABLE_SECURE_BOOT:
		ret = ambarella_otp_permanently_enable_secure_boot((uint32_t) x1);
		break;

	case AMBA_SIP_GET_HUK:
		ret = ambarella_otp_read_huk((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_SET_HUK_NONCE:
		ret = ambarella_otp_write_huk_nonce((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_GET_HUK_NONCE:
		ret = ambarella_otp_read_huk_nonce((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_GET_AES_KEY:
		ret = ambarella_otp_read_aes_key((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_SET_AES_KEY:
		ret = ambarella_otp_write_aes_key((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_GET_ECC_KEY:
		ret = ambarella_otp_read_ecc_key((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_SET_ECC_KEY:
		ret = ambarella_otp_write_ecc_key((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_GET_USR_SLOT_G0:
		ret = ambarella_otp_read_user_slot_g0((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_SET_USR_SLOT_G0:
		ret = ambarella_otp_write_user_slot_g0((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_GET_USR_SLOT_G1:
		ret = ambarella_otp_read_user_slot_g1((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_SET_USR_SLOT_G1:
		ret = ambarella_otp_write_user_slot_g1((uint8_t *) x1, x2, x3);
		break;

	case AMBA_SIP_GET_TEST_REGION:
		ret = ambarella_otp_read_test_region((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_SET_TEST_REGION:
		ret = ambarella_otp_write_test_region((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_REVOKE_KEY:
		ret = ambarella_otp_revoke_key((uint32_t) x1, (uint32_t) x3);
		break;

	case AMBA_SIP_GET_CHIP_REPAIR_INFO:
		ret = ambarella_otp_get_chip_repair_info((uint32_t *) x1, x2);
		break;

	case AMBA_SIP_QUERY_OTP_SETTING:
		ret = ambarella_otp_query_otp_setting((uint32_t *) x1, x2);
		break;

	case AMBA_SIP_SET_JTAG_EFUSE:
		ret = ambarella_otp_set_jtag_efuse((uint32_t) x1);
		break;

	case AMBA_SIP_LOCK_ZONA_A:
		ret = ambarella_otp_lock_zone_a((uint32_t) x1);
		break;

	case AMBA_SIP_GET_SYSCONFIG:
		ret = ambarella_otp_read_sysconfig((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_SET_SYSCONFIG:
		ret = ambarella_otp_write_sysconfig((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_GET_CST_SEED:
		ret = ambarella_otp_read_cst_planted_seed((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_GET_CST_CUK:
		ret = ambarella_otp_read_cst_planted_cuk((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_SET_CST_SEED_CUK:
		ret = ambarella_otp_write_cst_planted_seed_and_cuk((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_GET_USR_CUK:
		ret = ambarella_otp_read_user_planted_cuk((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_SET_USR_CUK:
		ret = ambarella_otp_write_user_planted_cuk((uint8_t *) x1, x2);
		break;

	case AMBA_SIP_GET_BST_VER:
		ret = ambarella_otp_read_bst_ver((uint32_t *) x1, x2);
		break;

	case AMBA_SIP_INCREASE_BST_VER:
		ret = ambarella_otp_increase_bst_ver((uint32_t) x1);
		break;

	case AMBA_SIP_EN_ANTI_ROLLBACK:
		ret = ambarella_otp_set_bst_anti_rollback((uint32_t) x1);
		break;

	case AMBA_SIP_DIS_SECURE_USB_BOOT:
		ret = ambarella_otp_disable_secure_usb_boot((uint32_t) x1);
		break;

	case AMBA_SIP_DIS_MONO_CNTS:
		ret = ambarella_otp_disable_mono_counters((uint32_t) x1);
		break;

	case AMBA_SIP_GET_MISC_CONFIG: {
		uint32_t misc_tmp[2];
		unsigned long mbase = 0, mend = 0, msize = 0;
		const unsigned long misc_bytes = 2U * sizeof(uint32_t);

		ret = ambarella_otp_read_misc_config(misc_tmp);
		if (ret < 0) {
			break;
		}
		if (!x1 || check_uptr_overflow((uintptr_t)x1, misc_bytes)) {
			ret = -1;
			break;
		}
		mbase = round_down(x1, PAGE_SIZE);
		mend = round_up(x1 + misc_bytes, PAGE_SIZE);
		msize = mend - mbase;
		if (mend < mbase) {
			ret = -1;
			break;
		}
		/* Map at least two words even when caller x2 was 0/<8 */
		if (mmap_add_dynamic_region(mbase, mbase, msize,
				MT_SECURE | MT_RW_DATA) != 0) {
			ret = -1;
			break;
		}
		((uint32_t *)x1)[0] = misc_tmp[0];
		((uint32_t *)x1)[1] = misc_tmp[1];
		flush_dcache_range(x1, misc_bytes);
		mmap_remove_dynamic_region(mbase, msize);
		break;
	}

	case AMBA_SIP_SET_MISC_CONFIG:
		ret = ambarella_otp_write_misc_config((uint32_t) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_SET_AND_LOCK_AMBA_RESERVED:
		ret = ambarella_otp_write_and_lock_amba_reserved((uint32_t) x1);
		break;

	case AMBA_SIP_LOCK_AMBA_RESERVED:
		ret = ambarella_otp_lock_amba_reserved((uint32_t) x1);
		break;

	case AMBA_SIP_GET_DIAGNOSIS_REPORT:
		ret = ambarella_otp_diagnosis((uint32_t *) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_GENERIC_OTP_READ:
		ret = ambarella_otp_generic_read_u32((uint8_t *) x1, (uint32_t) x2, (uint32_t) x3);
		break;

	case AMBA_SIP_GENERIC_OTP_WRITE:
		ret = ambarella_otp_generic_write_u32((uint32_t) x1, (uint32_t) x3 & 0x80000000U, (uint32_t) x3 & 0x7FFFFFFFU);
		break;

	case AMBA_SIP_GET_TEMP_SENSOR_PARAMS:
		ret = ambarella_otp_read_temp_sensor_params((uint32_t *) x1, (uint32_t) x2);
		break;

#if defined(AMBA_SECURITY_ARCH_sec_v3) && defined(PLAT_CFG_TCG_DICE)
	case AMBA_SIP_GEN_UDS_DEBUG:
		ret = ambarella_otp_gen_uds_debug((uint8_t *) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_GEN_UDS:
		ret = ambarella_otp_gen_uds((uint8_t *) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_READ_UDS:
		ret = ambarella_otp_read_uds((uint8_t *) x1, (uint32_t) x2);
		break;

	case AMBA_SIP_ENABLE_DICE:
		ret = ambarella_otp_enable_dice((uint32_t) x1);
		break;
#endif

	default:
		ERROR("not expected fn 0x%08x\n", fn);
		ret = -1;
		break;
	}

	if (map_buf && x1 && x2) {
		flush_dcache_range(x1, x2);
		mmap_remove_dynamic_region(base, size);
	}

	if (ret < 0) {
		ERROR("fn (%d) failed, ret %d\n", fn, ret);
	}

	SMC_RET1(handle, (ret < 0) ? SMC_UNK : SMC_OK);

}

/*
 * This function handles ARM defined SiP Calls
 */
static uintptr_t ambarella_sip_handler(uint32_t smc_fid,
		u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4,
		void *cookie, void *handle, u_register_t flags)
{
	uint32_t svc = SVC_OF_SMC(smc_fid);
	uint32_t fn = FNID_OF_SMC(smc_fid);
	int ret = 0;

	/* Determine which security state this SMC originated from */
#if defined(AMBA_SECURITY_ARCH_sec_v3)
	if (svc == AMBA_SIP_ACCESS_OTP &&
	    (fn == AMBA_SIP_OTP_GET_CHALLENGE || fn == AMBA_SIP_OTP_EXECUTE)) {
		return ambarella_otp_auth_smc(smc_fid, x1, x2, x3, x4, cookie,
				handle, flags);
	}
#endif

	if (is_caller_secure(flags)) {
		switch (svc) {
		case AMBA_SIP_ACCESS_OTP:
			return ambarella_get_otp_huk(smc_fid, x1, x2, x3, x4, cookie, handle, flags);
		default:
			SMC_RET1(handle, SMC_UNK);
		}
	} else {
		uint32_t uuid[4] = {0,};
		uint32_t customid[4] = {0,};
		u_register_t *p = NULL;

		if (svc == AMBA_SIP_ACCESS_OTP && fn == AMBA_SIP_GET_AMBA_UNIQUE_ID) {
			ret = ambarella_otp_read_amba_unique_id((uint8_t *)uuid, sizeof(uuid));
			if (ret < 0) {
				SMC_RET1(handle, SMC_UNK);
			}
			p = (u_register_t *)uuid;
			SMC_RET3(handle, 0, p[0], p[1]);
		}

		if (svc == AMBA_SIP_ACCESS_OTP && fn == AMBA_SIP_GET_CUSTOMER_ID) {
			ret = ambarella_otp_read_customer_id((uint8_t *)customid,
				sizeof(customid));
			if (ret < 0) {
				SMC_RET1(handle, SMC_UNK);
			}
			p = (u_register_t *)customid;
			SMC_RET3(handle, 0, p[0], p[1]);
		}

		if (svc == AMBA_SIP_ACCESS_OTP && fn == AMBA_SIP_NW_QUERY_OTP) {
			unsigned int is_locked = 0;
			unsigned int is_invalid = 0;

			unsigned long base = 0, end = 0;
			unsigned long map_size = 0;

			if ((!x2) || (!x3)) {
				ERROR("nw_query_otp: bad buffer x2=%lx x3=%lx\n",
				      (unsigned long)x2, (unsigned long)x3);
				SMC_RET1(handle, SMC_UNK);
			}
			base = round_down(x2, PAGE_SIZE);
			end = round_up(x2 + x3, PAGE_SIZE);
			map_size = end - base;
			if (end < base) {
				ERROR("nw_query_otp: map size wrap\n");
				SMC_RET1(handle, SMC_UNK);
			}

			ret = mmap_add_dynamic_region(base, base, map_size, MT_NS | MT_RW_DATA);
			if (ret) {
				ERROR("failed to map memory, ret %d\n", ret);
				SMC_RET1(handle, SMC_UNK);
			}

			inv_dcache_range(x2, x3);

			ret = ambarella_otp_nw_query_otp(
				(unsigned int) x1,
				(unsigned int *) x2,
				(unsigned int) x3,
				&is_locked,
				&is_invalid);

			if (x2 && x3) {
				flush_dcache_range(x2, x3);
				mmap_remove_dynamic_region(base, map_size);
			}

			if (ret < 0) {
				SMC_RET1(handle, SMC_UNK);
			}
			SMC_RET4(handle, 0, is_locked, is_invalid, 0);
		}
#ifdef HAS_ON_DIE_TEMPERATURE_SENSOR
		if (svc == AMBA_SIP_ACCESS_OTP && fn == AMBA_SIP_GET_TEMP_SENSOR_PARAMS) {
			uint32_t ts_params[8] = {0};

			ret = ambarella_otp_read_temp_sensor_params(ts_params, ON_DIE_TEMP_SENSOR_PARAMS_BITS / 8);
			if (ret < 0) {
				SMC_RET1(handle, SMC_UNK);
			}
			p = (u_register_t *)ts_params;
			/* Return 256 bits (8 x 32-bit values) via a0, a1, a2, a3
			 * a0: ts_params[0] (low 32-bit) | ts_params[1] (high 32-bit)
			 * a1: ts_params[2] (low 32-bit) | ts_params[3] (high 32-bit)
			 * a2: ts_params[4] (low 32-bit) | ts_params[5] (high 32-bit)
			 * a3: ts_params[6] (low 32-bit) | ts_params[7] (high 32-bit)
			 */
			SMC_RET5(handle, 0, p[0], p[1], p[2], p[3]);
		}
#endif
		if (svc == AMBA_SIP_ACCESS_OTP && fn == AMBA_SIP_GET_MISC_CONFIG) {
			uint32_t misc_config[2] = {0};

			ret = ambarella_otp_read_misc_config(misc_config);
			if (ret < 0) {
				SMC_RET1(handle, SMC_UNK);
			}
			SMC_RET3(handle, 0, (u_register_t)misc_config[0],
				 (u_register_t)misc_config[1]);
		}
	}

	switch (svc) {
	case AMBA_SCM_SVC_QUERY:
		SMC_RET1(handle, SMC_64);

	case AMBA_SCM_SVC_FREQ:
		return ambarella_freq_setup(smc_fid, x1, x2, x3, x4, cookie, handle, flags);

	case AMBA_SIP_SWITCH_TO_AARCH32:
		return ambarella_switch_to_aarch32(smc_fid, x1, x2, x3, x4, cookie, handle, flags);

	case AMBA_SIP_SVC_EL2_FAULT:
		return ambarella_el2_fault_handler(smc_fid, x1, x2, x3, x4, cookie, handle, flags);

	case AMBA_SIP_VP_CONFIG:
		return ambarella_vp_config(smc_fid, x1, x2, x3, x4, cookie, handle, flags);

	case AMBA_SIP_SECURITY_CPUFREQ:
		ret = ambarella_secure_cpufreq_update(x1, x2, x3);
		if (ret < 0) {
			SMC_RET1(handle, SMC_UNK);
		} else {
			SMC_RET1(handle, SMC_OK);
		}

	case AMBA_SIP_LP5_ADJUST:
		return ambarella_lp5_adjust(smc_fid, x1, x2, x3, x4, cookie, handle, flags);
	case AMBA_SIP_SVC_CLUSTER:
		return ambarella_boot_cluster(smc_fid, x1, x2, x3, x4, cookie, handle, flags);
	case AMBA_SIP_SECURE_DEBUG:
		return ambarella_secure_debug_smc(smc_fid, x1, x2, x3, x4, cookie, handle, flags);
	default:
		NOTICE("Unimplemented ARM SiP Service Call: 0x%x \n", smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}
}

/* Define a runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	ambarella_sip_svc,
	OEN_SIP_START,
	OEN_SIP_END,
	SMC_TYPE_FAST,
	NULL,
	ambarella_sip_handler
);

