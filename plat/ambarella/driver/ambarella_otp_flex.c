/*
 * ambarella_otp_flex.c - CV8 / sec_v3 layout-driven OTP driver
 *
 * Copyright (c) 2026 Ambarella International LP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <common/debug.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <plat_private.h>
#include <ambarella_def.h>
#include <ambarella_trng.h>

#include "ambarella_otp_flex.h"
#include <crypto_utils.h>

#define D_INTERNAL_CHECK

extern int ed25519_sha512_verify(const uint8_t *message, uint32_t message_len,
		const uint8_t *signature, uint32_t signature_len,
		const uint8_t *public_key);

typedef struct {
	unsigned int lock_bits;
	unsigned int invalid_bits;
	unsigned int sysconfig;
	unsigned int sysconfig_mask;

	unsigned int secure_boot_permanent_en : 1;
	unsigned int jtag_efuse : 1;
	unsigned int sysconfig_locked : 1;
	unsigned int huk_locked : 1;
	unsigned int customer_id_locked : 1;
	unsigned int zone_a_locked : 1;
	unsigned int cst_seed_cuk_locked : 1;
	unsigned int usr_cuk_locked : 1;
	unsigned int anti_rollback_en : 1;
	unsigned int secure_usb_boot_dis : 1;
	unsigned int all_rot_key_lock_together : 1;

	unsigned int otp_layout_ver : 4;
	unsigned int mono_cnts_disabled : 1;
	unsigned int reserved : 3;
	unsigned int not_revokeable_key_index : 8;
	unsigned int has_bst_anti_rollback : 1;
	unsigned int need_fill_and_lock_amba_reserved : 1;
	unsigned int amba_reserved_locked : 1;
	unsigned int mono_cnt_keep_ori_inc_bit_written : 1;
	unsigned int user_data_keep_ori_inc_bit_written : 1;

	unsigned int rot_pubkey_lock_base;
	unsigned int aes_key_lock_base;
	unsigned int ecc_key_lock_base;
	unsigned int usr_slot_g0_lock_base;
	unsigned int usr_slot_g1_lock_base;

	unsigned int num_of_rot_pub_keys;
	unsigned int num_of_aes_keys;
	unsigned int num_of_ecc_keys;
	unsigned int num_of_usr_slot_g0;
	unsigned int num_of_usr_slot_g1;
} otp_setting_t;

static spinlock_t otp_access_lock;
static spinlock_t otp_auth_lock;
static uint8_t otp_stored_challenge[AMB_OTP_CHALLENGE_BYTES];
static uint32_t otp_challenge_valid;
static volatile int g_otp_auth_internal;

static void otp_auth_invalidate(void)
{
	memset(otp_stored_challenge, 0, sizeof(otp_stored_challenge));
	otp_challenge_valid = 0U;
}

void ambarella_otp_init(void)
{
	/*
	 * Match amboot otp_init() + CV8 Config-OTP defaults.
	 * CTRL2 holds external VPP setup/hold counters (reset default 0xf0/0xf0).
	 */
	mmio_write_32(OTP_CTRL2_REG, OTP_CTRL2_DEFAULT);
}

static void ambarella_otp_write_enable(void)
{
}

static void ambarella_otp_write_disable(void)
{
}

/*
 * Low-level 32-bit OTP read — keep in sync with amboot otp_drv.c / CV8
 * "Software Programming Guide" Read mode.
 */
static int otp_hw_read32(uint32_t bit_addr, uint32_t *value)
{
	uint32_t val;

	if ((bit_addr >= OTP_BIT_SIZE) || (bit_addr & 0x1fU)) {
		ERROR("otp read address 0x%x is invalid\n", bit_addr);
		return -31;
	}

	mmio_write_32(OTP_CTRL1_REG, 0);
	/* Read mode: DBG_READ_MODE | READ_FSM_ENABLE => 0x00A0_0000 */
	mmio_write_32(OTP_CTRL1_REG, DBG_READ_MODE);
	mmio_write_32(OTP_CTRL1_REG, DBG_READ_MODE | READ_FSM_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & READ_OBSV_RDY))
		;

	val = DBG_READ_MODE | READ_FSM_ENABLE;
	val |= (bit_addr & OTP_BIT_MASK);
	mmio_write_32(OTP_CTRL1_REG, val);

	/* READ_ENABLE => 0x00E0_xxxx */
	mmio_write_32(OTP_CTRL1_REG, val | READ_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & READ_OBSV_DONE))
		;

	*value = mmio_read_32(OTP_READ_DOUT_REG);

	mmio_write_32(OTP_CTRL1_REG, val);

	while (!(mmio_read_32(OTP_OBSV_REG) & READ_OBSV_RDY))
		;

	return 0;
}

/*
 * Low-level 1-bit OTP program — keep in sync with amboot otp_write_bit().
 * Sequence (absolute CTRL1 values):
 *   0x0 -> 0x0008_0000 -> 0x000A_0000 -> wait RDY
 *   0x000A_xxxx -> INV=~(0x000E_xxxx) -> 0x000E_xxxx -> wait DONE
 *   0x000A_xxxx
 */
static int otp_hw_write_bit(uint32_t bit_addr)
{
	uint32_t ctrl;

	if (bit_addr >= OTP_BIT_SIZE) {
		ERROR("otp write address 0x%x is invalid\n", bit_addr);
		return -41;
	}

	ambarella_otp_write_enable();

	mmio_write_32(OTP_CTRL1_REG, 0);
	/* FSM_WRITE_MODE => 0x0008_0000 */
	mmio_write_32(OTP_CTRL1_REG, FSM_WRITE_MODE);
	/* + PROG_FSM_ENABLE => 0x000A_0000 */
	mmio_write_32(OTP_CTRL1_REG, FSM_WRITE_MODE | PROG_FSM_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_RDY))
		;

	ctrl = FSM_WRITE_MODE | PROG_FSM_ENABLE;
	ctrl |= (bit_addr & OTP_BIT_MASK);
	/* 0x000A_xxxx */
	mmio_write_32(OTP_CTRL1_REG, ctrl);

	/*
	 * Match amboot otp_write_bit(): write INV for (ctrl|PROG_ENABLE) first,
	 * then assert PROG_ENABLE so CTRL1 and INV become consistent.
	 */
	mmio_write_32(OTP_CTRL1_INVERT_REG, ~(ctrl | PROG_ENABLE));
	mmio_write_32(OTP_CTRL1_REG, ctrl | PROG_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_DONE))
		;

	if (mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_FAIL) {
		ERROR("otp write at 0x%x failed\n", bit_addr);
		mmio_write_32(OTP_CTRL1_REG, ctrl);
		ambarella_otp_write_disable();
		return -44;
	}

	/* clear PROG_ENABLE => 0x000A_xxxx */
	mmio_write_32(OTP_CTRL1_REG, ctrl);

	while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_RDY))
		;

	ambarella_otp_write_disable();
	return 0;
}

int ambarella_otp_read(uint32_t bit_addr, uint32_t length, uint32_t *value)
{
	uint32_t _bit_addr = round_down(bit_addr, 32);
	uint32_t dout;
	int ret;

#ifdef D_INTERNAL_CHECK
	if (_bit_addr != round_down(bit_addr + length - 1, 32) ||
	    bit_addr > OTP_BIT_SIZE) {
		ERROR("Invalid OTP read address: 0x%x - 0x%x\n",
				bit_addr, bit_addr + length);
		return -31;
	}
#endif

	spin_lock(&otp_access_lock);
	ambarella_otp_init();
	ret = otp_hw_read32(_bit_addr, &dout);
	spin_unlock(&otp_access_lock);

	if (ret < 0) {
		return ret;
	}

	*value = (dout >> (bit_addr % 32)) & ((1ULL << length) - 1);
	return 0;
}

int ambarella_otp_write(uint32_t bit_addr, uint32_t length, uint32_t value)
{
	uint32_t _bit_addr = round_down(bit_addr, 32);
	uint32_t i, val;
	int ret = 0;

#ifdef D_INTERNAL_CHECK
	if (_bit_addr != round_down(bit_addr + length - 1, 32) ||
	    bit_addr > OTP_BIT_SIZE) {
		ERROR("Invalid OTP write address: 0x%x - 0x%x\n",
				bit_addr, bit_addr + length);
		return -41;
	}
#endif

	if (ambarella_otp_read(bit_addr, length, &val) < 0) {
		return -42;
	}

	spin_lock(&otp_access_lock);
	ambarella_otp_init();

	for (i = 0; i < length; i++) {
		if (((val ^ value) & (1U << i)) == 0) {
			continue;
		}

		if (val & (1U << i)) {
			ERROR("cannot clear OTP bit at 0x%x\n", bit_addr + i);
			ret = -43;
			break;
		}

		ret = otp_hw_write_bit(bit_addr + i);
		if (ret < 0) {
			/* one retry like legacy ATF driver */
			ret = otp_hw_write_bit(bit_addr + i);
			if (ret < 0) {
				break;
			}
		}
	}

	spin_unlock(&otp_access_lock);
	return ret;
}

static int ambarella_otp_write_fast_u32(uint32_t start_bit_addr, uint32_t num,
		uint32_t *p_u32)
{
	uint32_t cur_bit_addr, i, j;
	uint32_t cur_val;
	int ret = 0;

	if ((!num) || (!p_u32)) {
		return -50;
	}

	if (start_bit_addr & 0x1FU) {
		return -51;
	}

	if ((start_bit_addr + (num * 32U)) > OTP_BIT_SIZE) {
		return -52;
	}

	spin_lock(&otp_access_lock);
	ambarella_otp_init();

	cur_bit_addr = start_bit_addr;
	for (j = 0; j < num; j++, cur_bit_addr += 32U) {
		cur_val = p_u32[j];

		for (i = 0; i < 32U; i++) {
			if (!(cur_val & (1U << i))) {
				continue;
			}

			ret = otp_hw_write_bit(cur_bit_addr + i);
			if (ret < 0) {
				ret = otp_hw_write_bit(cur_bit_addr + i);
				if (ret < 0) {
					ret = -53;
					goto out;
				}
			}
		}
	}

out:
	spin_unlock(&otp_access_lock);
	return ret;
}

static int otp_read_field(uint32_t addr, uint32_t len, void *v)
{
	uint32_t i;
	uint32_t *pu32 = v;

	for (i = 0; i < len; i += 32U) {
		if (ambarella_otp_read(addr + i, 32, pu32++) < 0) {
			ERROR("OTP read 0x%x failed\n", addr + i);
			return -10;
		}
	}

	return 0;
}

static int otp_write_field(uint32_t addr, uint32_t len, void *v,
		uint32_t lock_bank, uint32_t lock_index,
		uint32_t write_content, uint32_t write_lock,
		uint32_t simulate_write, uint32_t fast_write,
		const char *op_str)
{
	uint32_t i, lock_bit, value;
	uint32_t *pu32;
	uint32_t lock_addr;
	int ret;

	if (simulate_write) {
		if (write_content) {
			NOTICE("[Simulate OTP write]: %s: addr 0x%08x, length %d\n",
				op_str, addr, len);
		}
		if (write_lock) {
			NOTICE("[Simulate OTP write]: %s lock bit: %u\n",
				op_str, lock_index);
		}
		return 0;
	}

	if (write_content) {
		if (fast_write) {
			ret = ambarella_otp_write_fast_u32(addr, len >> 5,
					(uint32_t *)v);
			if (ret) {
				return ret;
			}
		} else {
			for (i = 0, pu32 = v; i < len; i += 32U) {
				if (ambarella_otp_write(addr + i, 32, *pu32++) < 0) {
					return -20;
				}
			}

			for (i = 0, pu32 = v; i < len; i += 32U) {
				if (ambarella_otp_read(addr + i, 32, &value) < 0) {
					return -21;
				}
				if (value != *pu32++) {
					return -22;
				}
			}
		}
	}

	if (write_lock) {
		lock_addr = (lock_bank == OTP_LOCK_BANK_WRITE_LOCK_2) ?
			WRITE_LOCK_2_BIT_ADDR : WRITE_LOCK_BIT_ADDR;

		if (ambarella_otp_write(lock_addr + lock_index, 1, 1) < 0) {
			return -23;
		}

		ret = ambarella_otp_read(lock_addr + lock_index, 1, &lock_bit);
		if ((ret < 0) || (lock_bit == 0U)) {
			return -24;
		}
	}

	return 0;
}

static int otp_read_lock_bit(uint32_t lock_bank, uint32_t lock_bit_index,
		uint32_t *lock_bit)
{
	uint32_t lock_addr;

	if (lock_bank == OTP_LOCK_BANK_WRITE_LOCK_2) {
		lock_addr = WRITE_LOCK_2_BIT_ADDR;
	} else {
		lock_addr = WRITE_LOCK_BIT_ADDR;
	}

	return ambarella_otp_read(lock_addr + lock_bit_index, 1, lock_bit);
}

static int otp_region_is_locked_idx(otp_region_idx_t idx)
{
	const otp_region_desc_t *reg = otp_region_get(idx);
	uint32_t lock_bit = 0;
	int ret;

	if (!reg || !reg->has_lock) {
		return 0;
	}

	ret = otp_read_lock_bit(reg->lock_bank, reg->lock_bit, &lock_bit);
	if (ret < 0) {
		return ret;
	}

	return lock_bit ? 1 : 0;
}

static int otp_region_is_invalid_idx(otp_region_idx_t idx)
{
	const otp_region_desc_t *reg = otp_region_get(idx);
	uint32_t invalid_bit = 0;
	int ret;

	if (!reg || !reg->has_invalid_bit) {
		return 0;
	}

	ret = ambarella_otp_read(DATA_INVALID_BIT_ADDR + reg->invalid_bit,
			1, &invalid_bit);
	if (ret < 0) {
		return ret;
	}

	return invalid_bit ? 1 : 0;
}

static int otp_enforce_access_control(void)
{
	uint32_t v = 0;

	if (ambarella_otp_read(ENFORCE_ACCESS_CONTROL_BITMASK_BIT, 1, &v) < 0) {
		return 1;
	}

	return v ? 1 : 0;
}

static int otp_enforce_access_auth(void)
{
	uint32_t v = 0;

	if (ambarella_otp_read(ENFORCE_ACCESS_AUTH_BIT, 1, &v) < 0) {
		return 1;
	}

	return v ? 1 : 0;
}

static int otp_acl_blocked(otp_region_idx_t idx)
{
	const otp_region_desc_t *reg = otp_region_get(idx);
	uint32_t mask_word;
	uint32_t bit;

	if (!reg || reg->acl_bit == OTP_ACL_NONE) {
		return 0;
	}

	if (!otp_enforce_access_control()) {
		return 0;
	}

	bit = reg->acl_bit;
	mask_word = 0U;
	if (ambarella_otp_read(ACCESS_CONTROL_BITMASK_ADDR + bit, 1,
			&mask_word) < 0) {
		return 1;
	}

	return mask_word ? 1 : 0;
}

static int otp_check_caller_access(const otp_region_desc_t *reg,
		otp_caller_t caller, int is_write)
{
	uint16_t need;

	if (!reg) {
		return -1;
	}

	if (caller == OTP_CALLER_NW) {
		if (otp_acl_blocked(reg->index)) {
			ERROR("NW access blocked for %s by acl\n", reg->name);
			return -84;
		}
	}

	need = is_write ?
		((caller == OTP_CALLER_NW) ? OTP_CAP_NW_WR : OTP_CAP_SW_WR) :
		((caller == OTP_CALLER_NW) ? OTP_CAP_NW_RD : OTP_CAP_SW_RD);

	if ((reg->caps & need) == 0U) {
		ERROR("%s access denied for caller %d write=%d\n",
			reg->name, caller, is_write);
		return -85;
	}

	return 0;
}

static int otp_check_write_auth(void)
{
	if (!otp_enforce_access_auth()) {
		return 0;
	}

	if (g_otp_auth_internal) {
		return 0;
	}

	return D_AUTH_REQUIRED_RETCODE;
}

static int otp_region_read_idx(otp_region_idx_t idx, void *buf,
		uint32_t byte_len, otp_caller_t caller)
{
	const otp_region_desc_t *reg = otp_region_get(idx);
	int ret;

	ret = otp_check_caller_access(reg, caller, 0);
	if (ret) {
		return ret;
	}

	if (byte_len < (reg->bit_size / 8U)) {
		ERROR("buffer too small for %s\n", reg->name);
		return -2;
	}

	return otp_read_field(reg->bit_addr, reg->bit_size, buf);
}

static int otp_region_write_idx(otp_region_idx_t idx, void *buf,
		uint32_t byte_len, otp_caller_t caller,
		uint32_t write_content, uint32_t write_lock,
		uint32_t simulate, uint32_t fast_write)
{
	const otp_region_desc_t *reg = otp_region_get(idx);
	int ret;

	ret = otp_check_caller_access(reg, caller, 1);
	if (ret) {
		return ret;
	}

	ret = otp_check_write_auth();
	if (ret) {
		return ret;
	}

	if (reg->has_lock) {
		ret = otp_region_is_locked_idx(idx);
		if (ret > 0) {
			return D_LOCKED_RETCODE;
		}
		if (ret < 0) {
			return ret;
		}
	}

	if (write_content) {
		if ((!buf) || (byte_len < (reg->bit_size / 8U))) {
			return -5;
		}
	}

	return otp_write_field(reg->bit_addr, reg->bit_size, buf,
			reg->lock_bank, reg->lock_bit,
			write_content, write_lock, simulate, fast_write,
			reg->name);
}

static int store_embedded_flag(unsigned char *p, unsigned int length,
		unsigned int content_length, otp_region_idx_t idx)
{
	uint32_t *p_embed_flag = NULL;
	int ret;

	if (length == content_length) {
		p_embed_flag = NULL;
	} else if (length == (content_length + 4U)) {
		p_embed_flag = (uint32_t *)((unsigned long)p + content_length);
	} else {
		ERROR("length not expected (%u)\n", length);
		return -1;
	}

	if (p_embed_flag) {
		*p_embed_flag = 0U;
		ret = otp_region_is_locked_idx(idx);
		if (ret < 0) {
			return ret;
		}
		if (ret > 0) {
			*p_embed_flag |= D_FLAG_LOCKED;
		}

		if (otp_region_is_invalid_idx(idx) > 0) {
			*p_embed_flag |= D_FLAG_REVOKED;
		}
	}

	return 0;
}

static int check_length_get_embedded_flag(unsigned char *p,
		unsigned int length, unsigned int content_length,
		uint32_t *write_content, uint32_t *write_lock,
		uint32_t *simulate, uint32_t *fast_write)
{
	uint32_t *p_embed_flag;

	if (length == content_length) {
		*write_content = 1U;
		*write_lock = 0U;
		*simulate = 0U;
		*fast_write = 0U;
		return 0;
	}

	if (length != (content_length + 4U)) {
		return -1;
	}

	p_embed_flag = (uint32_t *)((unsigned long)p + content_length);

	*write_content = ((*p_embed_flag) & D_IN_FLAG_WRITE_CONTENT) ? 1U : 0U;
	*write_lock = ((*p_embed_flag) & D_IN_FLAG_WRITE_LOCK) ? 1U : 0U;
	*simulate = ((*p_embed_flag) & D_IN_FLAG_SIMULATE_WRITE) ? 1U : 0U;
	*fast_write = ((*p_embed_flag) & D_IN_FLAG_FAST_WRITE) ? 1U : 0U;

	return 0;
}

static int count_one_bit_number(uint32_t addr, uint32_t size, uint32_t *cnt)
{
	uint32_t i, val, total = 0U;
	int ret;

	for (i = 0; i < size; i += 32U) {
		ret = ambarella_otp_read(addr + i, 32, &val);
		if (ret < 0) {
			return ret;
		}
		total += __builtin_popcount(val);
	}

	*cnt = total;
	return 0;
}

static int program_one_bit(uint32_t addr, uint32_t size, uint32_t simulate)
{
	uint32_t i, j, val;
	int ret;

	for (i = 0; i < size; i += 32U) {
		ret = ambarella_otp_read(addr + i, 32, &val);
		if (ret < 0) {
			return ret;
		}

		for (j = 0; j < 32U; j++) {
			if (!(val & (1U << j))) {
				if (!simulate) {
					ret = ambarella_otp_write(addr + i + j, 1, 1);
					if (ret < 0) {
						return ret;
					}
				}
				return 0;
			}
		}
	}

	return -70;
}

static int otp_read_anchoring_pubkey(uint8_t pubkey[32])
{
	uint8_t raw[ANCHORING_KEY_1_BITS / 8];
	int ret;

	ret = otp_region_read_idx(OTP_IDX_ANCHORING_KEY_1, raw, sizeof(raw),
			OTP_CALLER_SW);
	if (ret < 0) {
		return ret;
	}

	memcpy(pubkey, raw, 32U);
	return 0;
}

static int otp_auth_verify_challenge_suffix(const uint8_t *msg, size_t msg_len)
{
	const uint8_t *chal_in;

	if (!otp_challenge_valid) {
		return -1;
	}

	if (msg_len < AMB_OTP_CHALLENGE_BYTES) {
		return -1;
	}

	chal_in = msg + (msg_len - AMB_OTP_CHALLENGE_BYTES);
	if (crypto_memcmp(chal_in, otp_stored_challenge,
			AMB_OTP_CHALLENGE_BYTES) != 0) {
		return -1;
	}

	return 0;
}

static int otp_auth_run_op(const struct otp_auth_op_hdr *hdr,
		const uint8_t *data, uint32_t *result_out)
{
	otp_region_idx_t idx;
	int ret = 0;

	if (hdr->region_idx >= OTP_IDX_REGION_COUNT) {
		return -1;
	}

	idx = (otp_region_idx_t)hdr->region_idx;
	g_otp_auth_internal = 1;

	switch (hdr->opcode) {
	case OTP_AUTH_OP_WRITE:
		ret = otp_region_write_idx(idx, (void *)data, hdr->data_len,
				OTP_CALLER_SW, 1U, 0U, 0U, 0U);
		break;
	case OTP_AUTH_OP_LOCK:
	{
		const otp_region_desc_t *reg = otp_region_get(idx);
		uint32_t lock_addr;

		if (!reg || !reg->has_lock) {
			ret = -1;
			break;
		}
		lock_addr = (reg->lock_bank == OTP_LOCK_BANK_WRITE_LOCK_2) ?
			WRITE_LOCK_2_BIT_ADDR : WRITE_LOCK_BIT_ADDR;
		ret = ambarella_otp_write(lock_addr + reg->lock_bit, 1, 1);
		break;
	}
	case OTP_AUTH_OP_INC_MONO:
		ret = program_one_bit(g_otp_regions[idx].bit_addr,
				g_otp_regions[idx].bit_size, 0U);
		break;
	case OTP_AUTH_OP_REVOKE_KEY:
		ret = ambarella_otp_revoke_key(hdr->sub_index, 0U);
		break;
	case OTP_AUTH_OP_WRITE_BIT:
		if (hdr->data_len < sizeof(uint32_t)) {
			ret = -1;
			break;
		}
		{
			uint32_t bit_val;

			memcpy(&bit_val, data, sizeof(bit_val));
			ret = ambarella_otp_write(hdr->sub_index, 1, bit_val);
		}
		break;
	default:
		ret = -1;
		break;
	}

	g_otp_auth_internal = 0;

	if (result_out) {
		*result_out = (ret < 0) ? 0U : 1U;
	}

	return ret;
}

int ambarella_otp_auth_issue_challenge(uint8_t *challenge_out)
{
	int ret;

	if (!challenge_out) {
		return -1;
	}

	spin_lock(&otp_auth_lock);

	ret = ambarella_trng_get_bytes(challenge_out, AMB_OTP_CHALLENGE_BYTES);
	if (ret != 0) {
		spin_unlock(&otp_auth_lock);
		return ret;
	}

	memcpy(otp_stored_challenge, challenge_out, AMB_OTP_CHALLENGE_BYTES);
	otp_challenge_valid = 1U;

	spin_unlock(&otp_auth_lock);
	return 0;
}

int ambarella_otp_auth_execute(const uint8_t *message, size_t message_len,
		const uint8_t signature[AMB_OTP_SIGNATURE_BYTES],
		uint32_t *result_out)
{
	uint8_t pubkey[32];
	struct otp_auth_op_hdr hdr;
	const uint8_t *payload;
	const uint8_t *data;
	size_t payload_len;
	int vret;
	int ret;

	if (!message || !signature ||
	    message_len < (sizeof(hdr) + AMB_OTP_CHALLENGE_BYTES)) {
		return -1;
	}

	spin_lock(&otp_auth_lock);

	if (otp_auth_verify_challenge_suffix(message, message_len) != 0) {
		spin_unlock(&otp_auth_lock);
		return -1;
	}

	payload_len = message_len - AMB_OTP_CHALLENGE_BYTES;
	if (payload_len < sizeof(hdr)) {
		spin_unlock(&otp_auth_lock);
		return -1;
	}

	payload = message;
	memcpy(&hdr, payload, sizeof(hdr));

	if (payload_len < (sizeof(hdr) + hdr.data_len)) {
		spin_unlock(&otp_auth_lock);
		return -1;
	}

	data = payload + sizeof(hdr);

	if (otp_enforce_access_auth()) {
		ret = otp_read_anchoring_pubkey(pubkey);
		if (ret < 0) {
			spin_unlock(&otp_auth_lock);
			return -1;
		}

		vret = ed25519_sha512_verify(message, (uint32_t)message_len,
				signature, 64U, pubkey);
		if (vret != 0) {
			spin_unlock(&otp_auth_lock);
			return -1;
		}
	}

	ret = otp_auth_run_op(&hdr, data, result_out);

	otp_auth_invalidate();
	memset(pubkey, 0, sizeof(pubkey));
	spin_unlock(&otp_auth_lock);

	return ret;
}

int ambarella_otp_read_amba_unique_id(uint8_t *p, uint32_t length)
{
	int ret;

	if ((!p) || (!length)) {
		return -1;
	}

	ret = store_embedded_flag(p, length, UNIQUE_ID_BITS / 8U,
			OTP_IDX_UUID);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_UUID, p, length, OTP_CALLER_SW);
}

int ambarella_otp_read_rot_pubkey(uint8_t *p, uint32_t length,
		uint32_t key_index)
{
	otp_region_idx_t idx;
	int ret;

	if (key_index >= ROT_KEY_NUM) {
		return -3;
	}

	idx = (otp_region_idx_t)(OTP_IDX_ROT_KEY_0 + key_index);

	ret = store_embedded_flag(p, length, ROT_PUBKEY_BITS / 8U, idx);
	if (ret < 0) {
		return ret;
	}

	if (otp_region_is_invalid_idx(idx) > 0) {
		if (length >= (ROT_PUBKEY_BITS / 8U + 4U)) {
			uint32_t *flag = (uint32_t *)(p + ROT_PUBKEY_BITS / 8U);

			*flag |= D_FLAG_REVOKED;
		}
	}

	return otp_region_read_idx(idx, p, length, OTP_CALLER_SW);
}

int ambarella_otp_write_rot_pubkey(uint8_t *p, uint32_t length,
		uint32_t key_index)
{
	otp_region_idx_t idx;
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	if (key_index >= ROT_KEY_NUM) {
		return -3;
	}

	idx = (otp_region_idx_t)(OTP_IDX_ROT_KEY_0 + key_index);

	ret = check_length_get_embedded_flag(p, length, ROT_PUBKEY_BITS / 8U,
			&write_content, &write_lock, &simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(idx, p, length, OTP_CALLER_SW,
			write_content, write_lock, simulate, fast_write);
}

int ambarella_otp_lock_rot_pubkey(uint32_t key_index, uint32_t simulate)
{
	otp_region_idx_t idx;
	const otp_region_desc_t *reg;
	uint32_t lock_addr;

	if (key_index >= ROT_KEY_NUM) {
		return -3;
	}

	idx = (otp_region_idx_t)(OTP_IDX_ROT_KEY_0 + key_index);
	reg = otp_region_get(idx);
	if (!reg) {
		return -1;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	if (simulate) {
		return 0;
	}

	lock_addr = WRITE_LOCK_2_BIT_ADDR + reg->lock_bit;
	return ambarella_otp_write(lock_addr, 1, 1);
}

int ambarella_otp_read_customer_id(uint8_t *p, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(p, length, CUSTOMER_ID_BITS / 8U,
			OTP_IDX_SERIAL_NUMBER);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_SERIAL_NUMBER, p, length,
			OTP_CALLER_SW);
}

int ambarella_otp_write_customer_id(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	ret = check_length_get_embedded_flag(p, length,
			CUSTOMER_ID_BITS / 8U, &write_content, &write_lock,
			&simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(OTP_IDX_SERIAL_NUMBER, p, length,
			OTP_CALLER_SW, write_content, write_lock, simulate,
			fast_write);
}

int ambarella_otp_read_mono_counter(uint32_t *p, uint32_t length,
		uint32_t mono_cnt_index)
{
	otp_region_idx_t idx;
	uint32_t tot_cnt = 0U;
	int ret;

	if ((!p) || (length != sizeof(uint32_t)) ||
	    (mono_cnt_index >= NUM_OF_MONO_CNT)) {
		return -1;
	}

	idx = (otp_region_idx_t)(OTP_IDX_MONO_CNT_0 + mono_cnt_index);
	ret = count_one_bit_number(g_otp_regions[idx].bit_addr,
			g_otp_regions[idx].bit_size, &tot_cnt);
	if (ret < 0) {
		return ret;
	}

	*p = tot_cnt;
	return 0;
}

int ambarella_otp_increase_mono_counter(uint32_t mono_cnt_index,
		uint32_t simulate)
{
	otp_region_idx_t idx;
	int ret;

	if (mono_cnt_index >= NUM_OF_MONO_CNT) {
		return -3;
	}

	ret = otp_check_write_auth();
	if (ret) {
		return ret;
	}

	idx = (otp_region_idx_t)(OTP_IDX_MONO_CNT_0 + mono_cnt_index);
	return program_one_bit(g_otp_regions[idx].bit_addr,
			g_otp_regions[idx].bit_size, simulate);
}

int ambarella_otp_permanently_enable_secure_boot(uint32_t simulate)
{
	uint32_t lock_bits = 0U;
	int ret;

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
		return -1;
	}

	if (!(lock_bits & (1U << LOCK_BIT_SYS_CONFIG))) {
		if (!simulate) {
			ret = ambarella_otp_write(SYS_CONFIG_BIT_ADDR +
					SECURE_BOOT_BIT, 1, 1);
			if (ret < 0) {
				return ret;
			}
			ret = ambarella_otp_write(SYS_CONFIG_BIT_ADDR +
					SECURE_BOOT_BIT + 32U, 1, 1);
			if (ret < 0) {
				return ret;
			}
		}
	}

	return 0;
}

int ambarella_otp_read_huk(uint8_t *p, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(p, length, HUK_BITS / 8U, OTP_IDX_HUK);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_HUK, p, length, OTP_CALLER_SW);
}

int ambarella_otp_read_huk_nonce(uint8_t *p, uint32_t length)
{
	/* sec_v3: no separate nonce region; HUK itself is 256-bit */
	return ambarella_otp_read_huk(p, length);
}

int ambarella_otp_write_huk_nonce(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	/*
	 * sec_v3: program HUK only (256-bit). Legacy SIP name kept for ABI.
	 * When ENFORCE_ACCESS_AUTH is set, caller must use OTP auth execute.
	 */
	ret = check_length_get_embedded_flag(p, length, HUK_BITS / 8U,
			&write_content, &write_lock, &simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(OTP_IDX_HUK, p, length, OTP_CALLER_SW,
			write_content, write_lock, simulate, fast_write);
}

static int otp_read_key_slot(otp_region_idx_t base_idx, uint8_t *p,
		uint32_t length, uint32_t key_index, uint32_t key_bits)
{
	otp_region_idx_t idx;
	int ret;

	if (key_index >= 4U) {
		return -3;
	}

	idx = (otp_region_idx_t)(base_idx + key_index);
	ret = store_embedded_flag(p, length, key_bits / 8U, idx);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(idx, p, length, OTP_CALLER_SW);
}

static int otp_write_key_slot(otp_region_idx_t base_idx, uint8_t *p,
		uint32_t length, uint32_t key_index, uint32_t key_bits)
{
	otp_region_idx_t idx;
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	if (key_index >= 4U) {
		return -3;
	}

	idx = (otp_region_idx_t)(base_idx + key_index);
	ret = check_length_get_embedded_flag(p, length, key_bits / 8U,
			&write_content, &write_lock, &simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(idx, p, length, OTP_CALLER_SW,
			write_content, write_lock, simulate, fast_write);
}

int ambarella_otp_read_aes_key(uint8_t *p, uint32_t length,
		uint32_t key_index)
{
	return otp_read_key_slot(OTP_IDX_AES_KEY_0, p, length, key_index,
			AES_KEY_BITS);
}

int ambarella_otp_write_aes_key(uint8_t *p, uint32_t length,
		uint32_t key_index)
{
	return otp_write_key_slot(OTP_IDX_AES_KEY_0, p, length, key_index,
			AES_KEY_BITS);
}

int ambarella_otp_read_ecc_key(uint8_t *p, uint32_t length,
		uint32_t key_index)
{
	return otp_read_key_slot(OTP_IDX_ECC_KEY_0, p, length, key_index,
			ECC_KEY_BITS);
}

int ambarella_otp_write_ecc_key(uint8_t *p, uint32_t length,
		uint32_t key_index)
{
	return otp_write_key_slot(OTP_IDX_ECC_KEY_0, p, length, key_index,
			ECC_KEY_BITS);
}

int ambarella_otp_read_user_slot_g0(uint8_t *p, uint32_t length,
		uint32_t slot_index)
{
	otp_region_idx_t idx;
	int ret;

	if (slot_index >= USR_SLOT_G0_NUM) {
		return -3;
	}

	idx = (otp_region_idx_t)(OTP_IDX_USER_SLOT_0 + slot_index);
	ret = store_embedded_flag(p, length, USR_SLOT_G0_BITS / 8U, idx);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(idx, p, length, OTP_CALLER_SW);
}

int ambarella_otp_write_user_slot_g0(uint8_t *p, uint32_t length,
		uint32_t slot_index)
{
	otp_region_idx_t idx;
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	if (slot_index >= USR_SLOT_G0_NUM) {
		return -3;
	}

	idx = (otp_region_idx_t)(OTP_IDX_USER_SLOT_0 + slot_index);
	ret = check_length_get_embedded_flag(p, length,
			USR_SLOT_G0_BITS / 8U, &write_content, &write_lock,
			&simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(idx, p, length, OTP_CALLER_SW,
			write_content, write_lock, simulate, fast_write);
}

int ambarella_otp_read_user_slot_g1(uint8_t *p, uint32_t length,
		uint32_t slot_index)
{
	(void)p;
	(void)length;
	(void)slot_index;
	return D_NOT_AVAILABLE_RETCODE;
}

int ambarella_otp_write_user_slot_g1(uint8_t *p, uint32_t length,
		uint32_t slot_index)
{
	(void)p;
	(void)length;
	(void)slot_index;
	return D_NOT_AVAILABLE_RETCODE;
}

int ambarella_otp_read_user_data_g0(uint8_t *p, uint32_t length,
		uint32_t data_index)
{
	(void)p;
	(void)length;
	(void)data_index;
	return D_NOT_AVAILABLE_RETCODE;
}

int ambarella_otp_write_user_data_g0(uint8_t *p, uint32_t length,
		uint32_t data_index)
{
	(void)p;
	(void)length;
	(void)data_index;
	return D_NOT_AVAILABLE_RETCODE;
}

int ambarella_otp_read_test_region(uint8_t *p, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(p, length, TEST_REGION_BITS / 8U,
			OTP_IDX_TEST_REGION);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_TEST_REGION, p, length,
			OTP_CALLER_SW);
}

int ambarella_otp_write_test_region(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	ret = check_length_get_embedded_flag(p, length,
			TEST_REGION_BITS / 8U, &write_content, &write_lock,
			&simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(OTP_IDX_TEST_REGION, p, length,
			OTP_CALLER_SW, write_content, write_lock, simulate,
			fast_write);
}

int ambarella_otp_revoke_key(uint32_t index, uint32_t simulate)
{
	if (index >= ROT_KEY_NUM) {
		return -3;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	if (simulate) {
		return 0;
	}

	return ambarella_otp_write(DATA_INVALID_BIT_ADDR + index, 1, 1);
}

int ambarella_otp_get_chip_repair_info(uint32_t *p, uint32_t length)
{
	if ((!p) || (!length)) {
		return -1;
	}

	return otp_read_field(CHIP_REPAIR_INFO_ADDR, CHIP_REPAIR_INFO_BITS, p);
}

int ambarella_otp_query_otp_setting(uint32_t *p, uint32_t length)
{
	otp_setting_t *setting = (otp_setting_t *)p;
	uint32_t v;

	if (!p || length != sizeof(otp_setting_t)) {
		return -1;
	}

	memset(setting, 0, sizeof(*setting));
	setting->otp_layout_ver = OTP_LAYOUT_VERSION;
	setting->not_revokeable_key_index = NON_REVOKABLE_KEY_INDEX;
	setting->has_bst_anti_rollback = 1;
	setting->need_fill_and_lock_amba_reserved = 0;
	setting->all_rot_key_lock_together = 0;

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &setting->lock_bits) < 0) {
		return -2;
	}
	if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32,
			&setting->invalid_bits) < 0) {
		return -3;
	}
	if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR, 32, &setting->sysconfig) < 0) {
		return -4;
	}
	if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR + 32U, 32,
			&setting->sysconfig_mask) < 0) {
		return -5;
	}

	setting->secure_boot_permanent_en =
		((setting->sysconfig & (1U << SECURE_BOOT_BIT)) &&
		 (setting->sysconfig_mask & (1U << SECURE_BOOT_BIT))) ? 1U : 0U;

	if (ambarella_otp_read(FUNCTION_DISABLE_ADDR + JTAG_DIS_BIT, 1, &v) < 0) {
		return -6;
	}
	setting->jtag_efuse = v ? 1U : 0U;

	setting->sysconfig_locked =
		(setting->lock_bits & (1U << LOCK_BIT_SYS_CONFIG)) ? 1U : 0U;
	setting->huk_locked =
		(setting->lock_bits & (1U << LOCK_BIT_HUK_NONCE)) ? 1U : 0U;
	setting->customer_id_locked =
		(setting->lock_bits & (1U << LOCK_BIT_CUSTOMER_ID)) ? 1U : 0U;
	setting->zone_a_locked =
		(setting->lock_bits & (1U << LOCK_BIT_A)) ? 1U : 0U;
	setting->cst_seed_cuk_locked =
		(setting->lock_bits & (1U << LOCK_BIT_CST_PLANTED_SEED_CUK)) ?
		1U : 0U;
	setting->usr_cuk_locked =
		(setting->lock_bits & (1U << LOCK_BIT_USR_PLANTED_CUK)) ? 1U : 0U;

	if (ambarella_otp_read(EN_ANTI_ROLLBACK_BIT, 1, &v) < 0) {
		return -7;
	}
	setting->anti_rollback_en = v ? 1U : 0U;

	if (ambarella_otp_read(DIS_SECURE_USB_BOOT_BIT, 1, &v) < 0) {
		return -8;
	}
	setting->secure_usb_boot_dis = v ? 1U : 0U;

	setting->amba_reserved_locked =
		(setting->lock_bits & (1U << LOCK_BIT_AMBA_RESERVED)) ? 1U : 0U;

	setting->rot_pubkey_lock_base = LOCK_BIT_ROT_BASE;
	setting->aes_key_lock_base = LOCK_BIT_AES_KEY_BASE;
	setting->ecc_key_lock_base = LOCK_BIT_ECC_KEY_BASE;
	setting->usr_slot_g0_lock_base = LOCK_BIT_USR_SLOT_G0_BASE;
	setting->usr_slot_g1_lock_base = 0;
	setting->num_of_rot_pub_keys = ROT_KEY_NUM;
	setting->num_of_aes_keys = AES_KEY_NUM;
	setting->num_of_ecc_keys = ECC_KEY_NUM;
	setting->num_of_usr_slot_g0 = USR_SLOT_G0_NUM;
	setting->num_of_usr_slot_g1 = 0;

	return 0;
}

int ambarella_otp_set_jtag_efuse(uint32_t simulate)
{
	int ret;

	ret = otp_check_write_auth();
	if (ret) {
		return ret;
	}

	if (simulate) {
		return 0;
	}

	return ambarella_otp_write(FUNCTION_DISABLE_ADDR + JTAG_DIS_BIT, 1, 1);
}

int ambarella_otp_lock_zone_a(uint32_t simulate)
{
	if (simulate) {
		return 0;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	return ambarella_otp_write(WRITE_LOCK_BIT_ADDR + LOCK_BIT_A, 1, 1);
}

int ambarella_otp_read_sysconfig(uint8_t *p, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(p, length, SYS_CONFIG_BITS / 8U,
			OTP_IDX_SYS_CONFIG);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_SYS_CONFIG, p, length,
			OTP_CALLER_SW);
}

int ambarella_otp_write_sysconfig(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	ret = check_length_get_embedded_flag(p, length, SYS_CONFIG_BITS / 8U,
			&write_content, &write_lock, &simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(OTP_IDX_SYS_CONFIG, p, length,
			OTP_CALLER_SW, write_content, write_lock, simulate,
			fast_write);
}

int ambarella_otp_read_cst_planted_seed(uint8_t *p, uint32_t length)
{
	return otp_region_read_idx(OTP_IDX_CST_SEED, p, length, OTP_CALLER_SW);
}

int ambarella_otp_read_cst_planted_cuk(uint8_t *p, uint32_t length)
{
	return otp_region_read_idx(OTP_IDX_CST_CUK, p, length, OTP_CALLER_SW);
}

int ambarella_otp_write_cst_planted_seed_and_cuk(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	if ((!p) || (length < ((CST_PLANTED_SEED_BITS + CST_PLANTED_CUK_BITS) / 8U))) {
		return -6;
	}

	ret = check_length_get_embedded_flag(p, length,
			(CST_PLANTED_SEED_BITS + CST_PLANTED_CUK_BITS) / 8U,
			&write_content, &write_lock, &simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	ret = otp_region_write_idx(OTP_IDX_CST_SEED, p,
			CST_PLANTED_SEED_BITS / 8U, OTP_CALLER_SW,
			write_content, 0U, simulate, fast_write);
	if (ret < 0) {
		return ret;
	}

	return otp_region_write_idx(OTP_IDX_CST_CUK,
			p + CST_PLANTED_SEED_BITS / 8U,
			CST_PLANTED_CUK_BITS / 8U, OTP_CALLER_SW,
			write_content, write_lock, simulate, fast_write);
}

int ambarella_otp_read_user_planted_cuk(uint8_t *p, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(p, length, USR_PLANTED_CUK_BITS / 8U,
			OTP_IDX_USER_CUK);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_USER_CUK, p, length, OTP_CALLER_SW);
}

int ambarella_otp_write_user_planted_cuk(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	ret = check_length_get_embedded_flag(p, length,
			USR_PLANTED_CUK_BITS / 8U, &write_content, &write_lock,
			&simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(OTP_IDX_USER_CUK, p, length, OTP_CALLER_SW,
			write_content, write_lock, simulate, fast_write);
}

int ambarella_otp_read_bst_ver(uint32_t *p, uint32_t length)
{
	if ((!p) || (length < (BST_VER_BITS / 32U))) {
		return -1;
	}

	return otp_region_read_idx(OTP_IDX_BST_VERSION_VALID, p,
			length * sizeof(uint32_t), OTP_CALLER_SW);
}

int ambarella_otp_increase_bst_ver(uint32_t simulate)
{
	int ret;

	ret = otp_check_write_auth();
	if (ret) {
		return ret;
	}

	return program_one_bit(g_otp_regions[OTP_IDX_BST_VERSION_VALID].bit_addr,
			g_otp_regions[OTP_IDX_BST_VERSION_VALID].bit_size,
			simulate);
}

int ambarella_otp_set_bst_anti_rollback(uint32_t simulate)
{
	if (simulate) {
		return 0;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	return ambarella_otp_write(EN_ANTI_ROLLBACK_BIT, 1, 1);
}

int ambarella_otp_disable_secure_usb_boot(uint32_t simulate)
{
	if (simulate) {
		return 0;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	return ambarella_otp_write(DIS_SECURE_USB_BOOT_BIT, 1, 1);
}

int ambarella_otp_disable_mono_counters(uint32_t simulate)
{
	(void)simulate;
	return D_NOT_AVAILABLE_RETCODE;
}

int ambarella_otp_read_misc_config(uint32_t *p)
{
	uint32_t bits[2];
	int ret;

	if (!p) {
		return -1;
	}

	ret = ambarella_otp_read(MISC_CONFIG_ADDR, MISC_CONFIG_BITS, bits);
	if (ret < 0) {
		return ret;
	}

	p[0] = bits[0];
	p[1] = MISC_CONFIG_BITS;
	return 0;
}

int ambarella_otp_write_misc_config(uint32_t misc_config, uint32_t simulate)
{
	if (simulate) {
		return 0;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	return ambarella_otp_write(MISC_CONFIG_ADDR, MISC_CONFIG_BITS,
			misc_config);
}

int ambarella_otp_read_function_disable(uint8_t *p, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(p, length, FUNCTION_DISABLE_BITS / 8U,
			OTP_IDX_FUNCTION_DISABLE);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_FUNCTION_DISABLE, p, length,
			OTP_CALLER_SW);
}

int ambarella_otp_write_function_disable(uint8_t *p, uint32_t length)
{
	uint32_t write_content, write_lock, simulate, fast_write;
	int ret;

	ret = check_length_get_embedded_flag(p, length,
			FUNCTION_DISABLE_BITS / 8U, &write_content, &write_lock,
			&simulate, &fast_write);
	if (ret < 0) {
		return -5;
	}

	return otp_region_write_idx(OTP_IDX_FUNCTION_DISABLE, p, length,
			OTP_CALLER_SW, write_content, write_lock, simulate,
			fast_write);
}

int ambarella_otp_write_and_lock_amba_reserved(uint32_t simulate)
{
	(void)simulate;
	return D_NOT_AVAILABLE_RETCODE;
}

int ambarella_otp_lock_amba_reserved(uint32_t simulate)
{
	if (simulate) {
		return 0;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	return ambarella_otp_write(WRITE_LOCK_BIT_ADDR + LOCK_BIT_AMBA_RESERVED,
			1, 1);
}

int ambarella_otp_diagnosis(uint32_t *p, uint32_t length)
{
	(void)p;
	(void)length;
	return D_NOT_AVAILABLE_RETCODE;
}

static int is_generic_otp_addr_accessible(uint32_t bit_addr_u32)
{
	unsigned int i;

	for (i = 0; i < OTP_IDX_REGION_COUNT; i++) {
		const otp_region_desc_t *reg = &g_otp_regions[i];

		if (reg->acl_bit == OTP_ACL_NONE) {
			continue;
		}

		if ((bit_addr_u32 >= reg->bit_addr) &&
		    ((bit_addr_u32 + 32U) <= (reg->bit_addr + reg->bit_size))) {
			if (otp_acl_blocked(reg->index)) {
				return 0;
			}
			return 1;
		}
	}

	return 0;
}

int ambarella_otp_generic_read_u32(uint8_t *p, uint32_t length,
		uint32_t bit_addr_u32)
{
	if ((!p) || (length != 4U) || (bit_addr_u32 & 0x1FU)) {
		return -1;
	}

	if (!is_generic_otp_addr_accessible(bit_addr_u32)) {
		return -5;
	}

	return ambarella_otp_read(bit_addr_u32, 32, (uint32_t *)p);
}

int ambarella_otp_generic_write_u32(uint32_t value, uint32_t continue_write,
		uint32_t bit_addr_u32)
{
	uint32_t cur_val = 0;
	int ret;

	if (bit_addr_u32 & 0x1FU) {
		return -3;
	}

	if (!is_generic_otp_addr_accessible(bit_addr_u32)) {
		return -5;
	}

	ret = otp_check_write_auth();
	if (ret) {
		return ret;
	}

	ret = ambarella_otp_read(bit_addr_u32, 32, &cur_val);
	if (ret) {
		return -5;
	}

	if (!continue_write) {
		if (cur_val) {
			return -6;
		}
	} else {
		uint32_t diff_mask = value ^ cur_val;
		uint32_t check_mask = diff_mask & cur_val;

		if (check_mask) {
			return -7;
		}
	}

	return ambarella_otp_write(bit_addr_u32, 32, value);
}

int ambarella_otp_read_temp_sensor_params(uint32_t *p, uint32_t length)
{
	if ((!p) || (!length)) {
		return -1;
	}

	return otp_read_field(ON_DIE_TEMP_SENSOR_PARAMS_ADDR,
			ON_DIE_TEMP_SENSOR_PARAMS_BITS, p);
}

#define NW_QUERY_SUB_INDEX_MASK		0xff00U
#define NW_QUERY_SUB_INDEX_SHIFT	8U
#define NW_QUERY_SUB_TYPE_MASK		0x00ffU

#define NW_QUERY_OTP_SYSCONFIG		0x01U
#define NW_QUERY_OTP_WRITELOCK		0x02U
#define NW_QUERY_OTP_DATAINV		0x03U
#define NW_QUERY_OTP_UNIQUEID		0x04U
#define NW_QUERY_OTP_CUSTOMERID		0x05U
#define NW_QUERY_OTP_ROTKEY		0x07U

int ambarella_otp_nw_query_otp(unsigned int type_index,
		unsigned int *p_buf, unsigned int buf_size,
		unsigned int *is_locked, unsigned int *is_invalid)
{
	uint32_t lock_bits = 0U;
	uint32_t invalid_bits = 0U;
	uint32_t key_index;

	if ((!p_buf) || (!buf_size)) {
		return -1;
	}

	if (type_index == NW_QUERY_OTP_SYSCONFIG) {
		if (buf_size != 8U) {
			return -2;
		}
		if (otp_acl_blocked(OTP_IDX_SYS_CONFIG)) {
			return -84;
		}
		if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR, 32, p_buf) < 0) {
			return -3;
		}
		if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR + 32U, 32,
				p_buf + 1) < 0) {
			return -4;
		}
		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
			return -5;
		}
		*is_locked = (lock_bits & (1U << LOCK_BIT_SYS_CONFIG)) ? 1U : 0U;
		return 0;
	}

	if (type_index == NW_QUERY_OTP_WRITELOCK) {
		if (buf_size != 4U) {
			return -2;
		}
		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, p_buf) < 0) {
			return -3;
		}
		return 0;
	}

	if (type_index == NW_QUERY_OTP_DATAINV) {
		if (buf_size != 4U) {
			return -2;
		}
		if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32, p_buf) < 0) {
			return -3;
		}
		return 0;
	}

	if ((type_index & NW_QUERY_SUB_TYPE_MASK) == NW_QUERY_OTP_ROTKEY) {
		uint32_t i, limit;
		uint32_t rot_addr;
		otp_region_idx_t idx;

		key_index = (type_index & NW_QUERY_SUB_INDEX_MASK) >>
			NW_QUERY_SUB_INDEX_SHIFT;
		if (key_index >= ROT_KEY_NUM) {
			return -5;
		}

		idx = (otp_region_idx_t)(OTP_IDX_ROT_KEY_0 + key_index);
		if (otp_acl_blocked(idx)) {
			return -84;
		}

		rot_addr = ROT_PUBKEY_ADDR + key_index * ROT_PUBKEY_BITS;
		if (buf_size < (ROT_PUBKEY_BITS / 8U)) {
			return -5;
		}

		if (ambarella_otp_read(WRITE_LOCK_2_BIT_ADDR, 32, &lock_bits) < 0) {
			return -5;
		}
		*is_locked = (lock_bits & (1U << key_index)) ? 1U : 0U;

		if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32,
				&invalid_bits) < 0) {
			return -3;
		}
		*is_invalid = (invalid_bits & (1U << key_index)) ? 1U : 0U;

		limit = ROT_PUBKEY_BITS / 32U;
		for (i = 0; i < limit; i++, rot_addr += 32U) {
			if (ambarella_otp_read(rot_addr, 32, p_buf++) < 0) {
				return -10;
			}
		}
		return 0;
	}

	if (type_index == NW_QUERY_OTP_UNIQUEID) {
		if (buf_size < 16U) {
			return -5;
		}
		return ambarella_otp_read_amba_unique_id((uint8_t *)p_buf, buf_size);
	}

	if (type_index == NW_QUERY_OTP_CUSTOMERID) {
		if (buf_size < 16U) {
			return -5;
		}
		if (otp_acl_blocked(OTP_IDX_SERIAL_NUMBER)) {
			return -84;
		}
		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
			return -5;
		}
		*is_locked = (lock_bits & (1U << LOCK_BIT_CUSTOMER_ID)) ? 1U : 0U;
		return ambarella_otp_read_customer_id((uint8_t *)p_buf, buf_size);
	}

	return -6;
}

#if defined(AMBA_SECURITY_ARCH_sec_v3) && defined(PLAT_CFG_TCG_DICE)

/*
 * Match setup_dice_uds_layer() in cv8_rom dice.c:
 *   HKDF-SHA512(salt="AMBA_Salt_Gen_Keypair", ikm=UDS,
 *               info="Gen_Keypair") -> 32-byte Ed25519 seed/priv
 *   ed25519_public_from_private(pub, priv)
 */
static const unsigned char uds_kdf_salt[] = "AMBA_Salt_Gen_Keypair";
static const unsigned char uds_kdf_info[] = "Gen_Keypair";

extern int hkdf_sha2_512(const unsigned char *p_salt, unsigned int salt_len,
		const unsigned char *p_ikm, unsigned int ikm_len,
		const unsigned char *p_info, unsigned int info_len,
		unsigned char *p_okm, unsigned int okm_len);
extern void ed25519_public_from_private(uint8_t out_public_key[32],
		const uint8_t private_key[32]);

static int uds_derive_keypair(const uint8_t uds[UDS_BYTES],
		uint8_t priv[UDS_PRIVKEY_BYTES],
		uint8_t pub[UDS_PUBKEY_BYTES])
{
	int ret;

	ret = hkdf_sha2_512(uds_kdf_salt, sizeof(uds_kdf_salt) - 1U,
			uds, UDS_BYTES,
			uds_kdf_info, sizeof(uds_kdf_info) - 1U,
			priv, UDS_PRIVKEY_BYTES);
	if (ret < 0) {
		return ret;
	}

	ed25519_public_from_private(pub, priv);
	return 0;
}

/*
 * GEN_UDS_DEBUG: TRNG -> UDS, derive keypair, output UDS|priv|pub (128B).
 * Does not write OTP.
 */
int ambarella_otp_gen_uds_debug(uint8_t *out, uint32_t length)
{
	uint8_t uds[UDS_BYTES];
	uint8_t priv[UDS_PRIVKEY_BYTES];
	uint8_t pub[UDS_PUBKEY_BYTES];
	int ret;

	if ((!out) || (length < (UDS_BYTES + UDS_PRIVKEY_BYTES +
			UDS_PUBKEY_BYTES))) {
		return -1;
	}

	ret = ambarella_trng_get_bytes(uds, UDS_BYTES);
	if (ret != 0) {
		return ret;
	}

	ret = uds_derive_keypair(uds, priv, pub);
	if (ret < 0) {
		memset(uds, 0, sizeof(uds));
		return ret;
	}

	memcpy(out, uds, UDS_BYTES);
	memcpy(out + UDS_BYTES, priv, UDS_PRIVKEY_BYTES);
	memcpy(out + UDS_BYTES + UDS_PRIVKEY_BYTES, pub, UDS_PUBKEY_BYTES);

	memset(uds, 0, sizeof(uds));
	memset(priv, 0, sizeof(priv));
	return 0;
}

/*
 * GEN_UDS: TRNG -> UDS, write+lock OTP, output public key only (32B).
 */
int ambarella_otp_gen_uds(uint8_t *out, uint32_t length)
{
	uint8_t uds[UDS_BYTES];
	uint8_t priv[UDS_PRIVKEY_BYTES];
	uint8_t pub[UDS_PUBKEY_BYTES];
	int ret;

	if ((!out) || (length < UDS_PUBKEY_BYTES)) {
		return -1;
	}

	/* Refuse if already locked */
	ret = otp_region_is_locked_idx(OTP_IDX_UDS);
	if (ret > 0) {
		return D_LOCKED_RETCODE;
	}
	if (ret < 0) {
		return ret;
	}

	ret = ambarella_trng_get_bytes(uds, UDS_BYTES);
	if (ret != 0) {
		return ret;
	}

	ret = uds_derive_keypair(uds, priv, pub);
	if (ret < 0) {
		memset(uds, 0, sizeof(uds));
		return ret;
	}

	ret = otp_region_write_idx(OTP_IDX_UDS, uds, UDS_BYTES, OTP_CALLER_SW,
			1U, 1U, 0U, 0U);
	memset(uds, 0, sizeof(uds));
	memset(priv, 0, sizeof(priv));
	if (ret < 0) {
		memset(pub, 0, sizeof(pub));
		return ret;
	}

	memcpy(out, pub, UDS_PUBKEY_BYTES);
	return 0;
}

/*
 * READ_UDS: read OTP UDS; optional +4B embedded lock flag (D_FLAG_LOCKED).
 * length == 64: UDS only; length == 68: UDS + lock status.
 */
int ambarella_otp_read_uds(uint8_t *out, uint32_t length)
{
	int ret;

	ret = store_embedded_flag(out, length, UDS_BYTES, OTP_IDX_UDS);
	if (ret < 0) {
		return ret;
	}

	return otp_region_read_idx(OTP_IDX_UDS, out, length, OTP_CALLER_SW);
}

int ambarella_otp_enable_dice(uint32_t simulate)
{
	if (simulate) {
		return 0;
	}

	if (otp_check_write_auth()) {
		return D_AUTH_REQUIRED_RETCODE;
	}

	return ambarella_otp_write(OTP_ENABLE_DICE_BIT, 1, 1);
}

#endif /* AMBA_SECURITY_ARCH_sec_v3 && PLAT_CFG_TCG_DICE */
