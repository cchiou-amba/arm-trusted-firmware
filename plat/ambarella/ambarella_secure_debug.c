/*
 * Copyright (c) 2025 Ambarella International LP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <common/debug.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>

#include <ambarella_def.h>
#include <plat_private.h>
#include <ambarella_secure_debug.h>
#include <ambarella_trng.h>
#include <crypto_utils.h>
#if defined(AMBA_SECURITY_ARCH_sec_v3)
#include "driver/ambarella_otp_flex.h"
#else
#include "driver/ambarella_otp.h"
#endif

#define D_EXPOSE_DDRC_MR_READ
#define D_EXPOSE_JTAG_EN_WRITE
#define D_EXPOSE_JTAG_EN_READ

extern int ed25519_sha512_verify(const uint8_t *message, uint32_t message_len,
		const uint8_t *signature, uint32_t signature_len,
		const uint8_t *public_key);

// use ECC key slot[3] by default
#define SD_ECC_PUBKEY_SLOT		3U

static spinlock_t sd_lock;
static uint8_t sd_stored_challenge[AMB_SD_CHALLENGE_BYTES];
static uint32_t sd_challenge_valid = 0;

static void sd_invalidate_challenge(void)
{
	memset(sd_stored_challenge, 0, sizeof(sd_stored_challenge));
	sd_challenge_valid = 0U;
}

int ambarella_secure_debug_issue_challenge(uint8_t *challenge_out)
{
	int ret;

	if (!challenge_out) {
		return -1;
	}

	spin_lock(&sd_lock);

	ret = ambarella_trng_get_bytes(challenge_out, AMB_SD_CHALLENGE_BYTES);
	if (ret != 0) {
		ERROR("secure debug: TRNG failed %d\n", ret);
		spin_unlock(&sd_lock);
		return ret;
	}

	memcpy(sd_stored_challenge, challenge_out, AMB_SD_CHALLENGE_BYTES);
	sd_challenge_valid = 1U;

	spin_unlock(&sd_lock);
	return 0;
}

static int sd_verify_challenge_suffix(const uint8_t *msg, size_t msg_len)
{
	const uint8_t *chal_in;

	if (!sd_challenge_valid) {
		ERROR("secure debug: no active challenge\n");
		return -1;
	}

	if (msg_len < AMB_SD_CHALLENGE_BYTES + sizeof(uint32_t)) {
		ERROR("secure debug: message too short\n");
		return -1;
	}

	chal_in = msg + (msg_len - AMB_SD_CHALLENGE_BYTES);
	if (crypto_memcmp(chal_in, sd_stored_challenge,
			AMB_SD_CHALLENGE_BYTES) != 0) {
		ERROR("secure debug: challenge mismatch\n");
		return -1;
	}

	return 0;
}

extern uint32_t ddrc_mrr(uint32_t ddrc, uint32_t addr, uint32_t did, uint32_t cid);

static int sd_run_op(const uint8_t *op_blob, size_t op_len, uint32_t *result_out)
{
	uint32_t opcode;

	if (op_len < sizeof(uint32_t)) {
		return -1;
	}

	memcpy(&opcode, op_blob, sizeof(uint32_t));

	switch (opcode) {
	case SD_OP_DDRC_READ_MR: {
#ifdef D_EXPOSE_DDRC_MR_READ
		struct sd_op_read_ddrc_mr op;
		uint32_t v = 0;

		if (op_len < sizeof(op)) {
			ERROR("bug, op_len (%ld) too small\n", op_len);
			return -1;
		}
		memcpy(&op, op_blob, sizeof(op));
		// currently expose mr 5~7
		if ((op.mr_idx < 5U) || (op.mr_idx > 7U)) {
			ERROR("secure debug: ddrc mr_idx %u out of range\n", op.mr_idx);
			return -1;
		}
		v = ddrc_mrr(0U, op.mr_idx, 0U, 0U);
		NOTICE("read ddrc_mr[%d]: 0x%x\n", op.mr_idx, v);
		if (result_out) {
			*result_out = v;
		}
		return 0;
#else
		ERROR("read ddrc_mr is not permitted, D_EXPOSE_DDRC_MR_READ is not enabled\n");
		return -10;
#endif
	}
	case SD_OP_WRITE_JTAG_EN: {
#ifdef D_EXPOSE_JTAG_EN_WRITE
		struct sd_op_write_jtag_en op;

		if (op_len < sizeof(op)) {
			ERROR("bug, op_len (%ld) too small\n", op_len);
			return -1;
		}
		memcpy(&op, op_blob, sizeof(op));

		{
			uint32_t r = mmio_read_32(AHBSP_JTAG_EN_REG);

			r = (r & ~BIT(0)) | (op.value & BIT(0));
			mmio_write_32(AHBSP_JTAG_EN_REG, r);
		}
		NOTICE("secure debug: JTAG_EN bit0 <= %u\n",
				(op.value & BIT(0)) != 0U ? 1U : 0U);
		return 0;
#else
		ERROR("write jtag_en is not permitted, SD_OP_WRITE_JTAG_EN is not enabled\n");
		return -10;
#endif
	}
	case SD_OP_READ_JTAG_EN: {
#ifdef D_EXPOSE_JTAG_EN_READ
		struct sd_op_read_jtag_en op;
		uint32_t v = 0;

		if (op_len < sizeof(op)) {
			ERROR("bug, op_len (%ld) too small\n", op_len);
			return -1;
		}
		memcpy(&op, op_blob, sizeof(op));

		v = mmio_read_32(AHBSP_JTAG_EN_REG);
		NOTICE("read jtag_en: 0x%x\n", v);
		if (result_out) {
			*result_out = v;
		}
		return 0;
#else
		ERROR("read jtag_en is not permitted, D_EXPOSE_JTAG_EN_READ is not enabled\n");
		return -10;
#endif
	}
	default:
		ERROR("secure debug: unknown opcode %u\n", opcode);
		return -1;
	}
}

int ambarella_secure_debug_execute(const uint8_t *message, size_t message_len,
		const uint8_t signature[AMB_SD_SIGNATURE_BYTES],
		uint32_t *result_out)
{
	uint32_t pubkey[ECC_KEY_BITS / 32 + 1];
	int vret;
	unsigned int * p_embed_flag = pubkey + (ECC_KEY_BITS / 32);
	size_t op_len;
	int ret;

	if (!message || message_len < (AMB_SD_CHALLENGE_BYTES + sizeof(uint32_t)) ||
	    !signature) {
	    ERROR("bad params\n");
		return -1;
	}

	spin_lock(&sd_lock);

	if (sd_verify_challenge_suffix(message, message_len) != 0) {
		spin_unlock(&sd_lock);
		return -1;
	}

	ret = ambarella_otp_read_ecc_key((uint8_t *) pubkey,
		(uint32_t) sizeof(pubkey), SD_ECC_PUBKEY_SLOT);
	if (ret != 0) {
		ERROR("secure debug: OTP ECC pubkey slot %u read failed %d\n",
				SD_ECC_PUBKEY_SLOT, ret);
		spin_unlock(&sd_lock);
		return -1;
	}

	if ((*p_embed_flag) & D_FLAG_LOCKED) {

		vret = ed25519_sha512_verify(message, (uint32_t) message_len,
				signature, 64U, (uint8_t *) pubkey);
		if (vret != 0) {
			ERROR("secure debug: ED25519 signature verify failed %d\n", vret);
			spin_unlock(&sd_lock);
			return -1;
		}
		NOTICE("secure debug: Ed25519 signature verify pass\n");
	} else {
		/* No provisioned / locked pubkey: bypass authentication. */
		NOTICE("secure debug: OTP ECC pubkey slot %u not locked, bypass auth\n",
				SD_ECC_PUBKEY_SLOT);
	}

	op_len = message_len - AMB_SD_CHALLENGE_BYTES;
	ret = sd_run_op(message, op_len, result_out);

	sd_invalidate_challenge();

	memset(pubkey, 0, sizeof(pubkey));

	spin_unlock(&sd_lock);
	return ret;
}

