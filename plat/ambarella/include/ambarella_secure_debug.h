/*
 * Copyright (c) 2025 Ambarella International LP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AMBARELLA_SECURE_DEBUG_H
#define AMBARELLA_SECURE_DEBUG_H

#include <stddef.h>
#include <stdint.h>

#define AMB_SD_CHALLENGE_BYTES	32U
#define AMB_SD_SIGNATURE_BYTES	64U

/*
 * Signed message layout: [operation payload][challenge, AMB_SD_CHALLENGE_BYTES]
 * Ed25519 signature from the holder of the secure-debug private key (pubkey in
 * OTP ECC slot 3) over the full message.
 */

#define SD_OP_DDRC_READ_MR		1U

#define SD_OP_WRITE_JTAG_EN		10U
#define SD_OP_READ_JTAG_EN		11U

struct sd_op_read_ddrc_mr {
	uint32_t opcode;
	uint32_t mr_idx;
};

struct sd_op_write_jtag_en {
	uint32_t opcode;
	uint32_t value;
};

struct sd_op_read_jtag_en {
	uint32_t opcode;
};

int ambarella_secure_debug_issue_challenge(uint8_t *challenge_out);
int ambarella_secure_debug_execute(const uint8_t *message, size_t message_len,
		const uint8_t signature[AMB_SD_SIGNATURE_BYTES],
		uint32_t *result_out);

#endif /* AMBARELLA_SECURE_DEBUG_H */

