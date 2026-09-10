/*
 * ambarella_otp_flex.h - CV8 / sec_v3 OTP layout-driven driver
 *
 * Copyright (c) 2026 Ambarella International LP
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AMBARELLA_OTP_FLEX_H__
#define __AMBARELLA_OTP_FLEX_H__

#include <stddef.h>
#include <stdint.h>

#ifndef BIT
#define BIT(n)				(1U << (n))
#endif

#define OTP_LAYOUT_VERSION		3

/* OTP_CTRL1_REG bit define (CV8 / CV3AD655 class) */
#define READ_FSM_ENABLE			BIT(23)
#define READ_ENABLE			BIT(22)
#define DBG_READ_MODE			BIT(21)
#define FSM_WRITE_MODE			BIT(19)
#define PROG_ENABLE			BIT(18)
#define PROG_FSM_ENABLE			BIT(17)

/* OTP_OBSV_REG bit define */
#define WRITE_PROG_DONE			BIT(4)
#define WRITE_PROG_FAIL			BIT(3)
#define WRITE_PROG_RDY			BIT(2)
#define READ_OBSV_RDY			BIT(1)
#define READ_OBSV_DONE			BIT(0)

#define OTP_CTRL1_OFFSET		0x00
#define OTP_OBSV_OFFSET			0x04
#define OTP_READ_DOUT_OFFSET		0x08
#define OTP_CTRL2_OFFSET		0x0C
#define OTP_CTRL1_INVERT_OFFSET		0x10
#define OTP_CTRL1_REG			OTP_CONFIG_REG(OTP_CTRL1_OFFSET)
#define OTP_OBSV_REG			OTP_CONFIG_REG(OTP_OBSV_OFFSET)
#define OTP_READ_DOUT_REG		OTP_CONFIG_REG(OTP_READ_DOUT_OFFSET)
#define OTP_CTRL2_REG			OTP_CONFIG_REG(OTP_CTRL2_OFFSET)
#define OTP_CTRL1_INVERT_REG		OTP_CONFIG_REG(OTP_CTRL1_INVERT_OFFSET)

#define OTP_BIT_SIZE			0x10000
#define OTP_BIT_MASK			(OTP_BIT_SIZE - 1)

/* CTRL2 default: VPP setup/hold = 0xf0 (silicon reset default) */
#define OTP_CTRL2_DEFAULT		0x00f000f0U

#define CHIP_REPAIR_INFO_ADDR		0x0
#define CHIP_REPAIR_INFO_BITS		16000

#define SYS_CONFIG_BIT_ADDR		0x40
#define SYS_CONFIG_BITS			64
#define SECURE_BOOT_BIT			6

#define UNIQUE_ID_ADDR			0x100
#define UNIQUE_ID_BITS			128

#define WRITE_LOCK_BIT_ADDR		0xC0
#define WRITE_LOCK_BITS			32

#define CST_PLANTED_SEED_ADDR		0xE0
#define CST_PLANTED_SEED_BITS		32

#define CST_PLANTED_CUK_ADDR		0x180
#define CST_PLANTED_CUK_BITS		128

#define FUNCTION_DISABLE_ADDR		0x200
#define FUNCTION_DISABLE_BITS		32
#define JTAG_DIS_BIT			2
#define DBG_TDR_DIS_BIT			7

#define WRITE_LOCK_2_BIT_ADDR		0x220
#define WRITE_LOCK_2_BITS		32

#define DATA_INVALID_BIT_ADDR		0xBE80
#define DATA_INVALID_BITS		32

#define EN_ANTI_ROLLBACK_BIT		0xBEC0
#define DIS_SECURE_USB_BOOT_BIT		0xBEC1
#define OTP_ENABLE_DICE_BIT		0xBEC4
#define ENFORCE_ACCESS_CONTROL_BITMASK_BIT	0xBEC7
#define ENFORCE_ACCESS_AUTH_BIT		0xBEC8

#define MISC_CONFIG_ADDR		0xBEC0
#define MISC_CONFIG_BITS		32

#define UDS_ADDR			0xAC80
#define UDS_BITS			512
#define UDS_BYTES			(UDS_BITS / 8U)
#define UDS_PRIVKEY_BYTES		32U
#define UDS_PUBKEY_BYTES		32U

#define CUSTOMER_ID_ADDR		0xBF00
#define CUSTOMER_ID_BITS		128

#define TEST_REGION_ADDR		0xBF80
#define TEST_REGION_BITS		128

#define HUK_ADDR			0xC000
#define HUK_BITS			256

#define USR_PLANTED_CUK_ADDR		0xC100
#define USR_PLANTED_CUK_BITS		256

#define AES_KEY_BASE_ADDR		0xC200
#define AES_KEY_NUM			4
#define AES_KEY_BITS			256

#define ECC_KEY_BASE_ADDR		0xC600
#define ECC_KEY_NUM			4
#define ECC_KEY_BITS			256

#define USR_SLOT_G0_ADDR		0xCA00
#define USR_SLOT_G0_NUM			6
#define USR_SLOT_G0_BITS		256

#define ROT_PUBKEY_ADDR			0xD000
#define ROT_KEY_NUM			20
#define ROT_PUBKEY_BITS			512
#define NON_REVOKABLE_KEY_INDEX		0

#define MONO_CNT_0_ADDR			0xF800
#define MONO_CNT_0_BITS			256
#define MONO_CNT_1_ADDR			0xF900
#define MONO_CNT_1_BITS			256
#define MONO_CNT_2_ADDR			0xFA00
#define MONO_CNT_2_BITS			512
#define NUM_OF_MONO_CNT			3

#define BST_VER_ADDR			0xFC00
#define BST_VER_BITS			256

#define ANCHORING_KEY_0_ADDR		0xFD00
#define ANCHORING_KEY_0_BITS		256
#define ANCHORING_KEY_1_ADDR		0xFE00
#define ANCHORING_KEY_1_BITS		256

#define ACCESS_CONTROL_BITMASK_ADDR	0xFF00
#define ACCESS_CONTROL_BITMASK_BITS	256

#define HAS_ON_DIE_TEMPERATURE_SENSOR
#define ON_DIE_TEMP_SENSOR_PARAMS_ADDR	0x240
#define ON_DIE_TEMP_SENSOR_PARAMS_BITS	0x40

#define HAS_BST_ANTI_ROLLBACK
#define HAS_FUNCTION_DISABLE

/* write_lock bit indices (CV8) */
#define LOCK_BIT_A			0
#define LOCK_BIT_UNIQUE_ID		1
#define LOCK_BIT_AMBA_CHIP_ID		2
#define LOCK_BIT_SYS_CONFIG		3
#define LOCK_BIT_CST_PLANTED_SEED_CUK	4
#define LOCK_BIT_CUSTOMER_ID		5
#define LOCK_BIT_TEST_REGION		6
#define LOCK_BIT_UDS			7
#define LOCK_BIT_HUK_NONCE		11
#define LOCK_BIT_USR_PLANTED_CUK	12
#define LOCK_BIT_AES_KEY_BASE		13
#define LOCK_BIT_ECC_KEY_BASE		17
#define LOCK_BIT_USR_SLOT_G0_BASE	21
#define LOCK_BIT_BST_VERSION_VALID	28
#define LOCK_BIT_ANCHORING_KEY_0	28
#define LOCK_BIT_ANCHORING_KEY_1	29
#define LOCK_BIT_ACCESS_CONTROL_BITMASK	30
#define LOCK_BIT_AMBA_RESERVED		31
#define LOCK_BIT_ROT_BASE		0

/* embedded flag (keep consistent with invoker) */
#define D_FLAG_LOCKED			0x01
#define D_FLAG_PARAM_ERROR		0x02
#define D_FLAG_DATA_MISMATCH		0x04
#define D_FLAG_HW_ERROR			0x08
#define D_FLAG_REVOKED			0x10
#define D_FLAG_PROG_INDICATED		0x20

#define D_IN_FLAG_WRITE_CONTENT		0x01
#define D_IN_FLAG_WRITE_LOCK		0x02
#define D_IN_FLAG_FAST_WRITE		0x04
#define D_IN_FLAG_SIMULATE_WRITE	0x08

#define D_LOCKED_RETCODE		(-80)
#define D_NOT_AVAILABLE_RETCODE		(-81)
#define D_AUTH_REQUIRED_RETCODE		(-82)

#define OTP_ACL_NONE			0xFFU

#define OTP_CAP_NW_RD			BIT(0)
#define OTP_CAP_NW_WR			BIT(1)
#define OTP_CAP_SW_RD			BIT(2)
#define OTP_CAP_SW_WR			BIT(3)

#define OTP_LOCK_BANK_WRITE_LOCK	0U
#define OTP_LOCK_BANK_WRITE_LOCK_2	1U

#define OTP_CLASS_PLAIN			0U
#define OTP_CLASS_MONO_CNT		1U
#define OTP_CLASS_ROT_DIGEST		2U
#define OTP_CLASS_META			3U

typedef enum {
	OTP_IDX_SYS_CONFIG = 0,
	OTP_IDX_UUID,
	OTP_IDX_WRITE_LOCK,
	OTP_IDX_CST_SEED,
	OTP_IDX_CST_CUK,
	OTP_IDX_FUNCTION_DISABLE,
	OTP_IDX_WRITE_LOCK_2,
	OTP_IDX_SERIAL_NUMBER,
	OTP_IDX_TEST_REGION,
	OTP_IDX_HUK,
	OTP_IDX_USER_CUK,
	OTP_IDX_AES_KEY_0,
	OTP_IDX_AES_KEY_1,
	OTP_IDX_AES_KEY_2,
	OTP_IDX_AES_KEY_3,
	OTP_IDX_ECC_KEY_0,
	OTP_IDX_ECC_KEY_1,
	OTP_IDX_ECC_KEY_2,
	OTP_IDX_ECC_KEY_3,
	OTP_IDX_USER_SLOT_0,
	OTP_IDX_USER_SLOT_1,
	OTP_IDX_USER_SLOT_2,
	OTP_IDX_USER_SLOT_3,
	OTP_IDX_USER_SLOT_4,
	OTP_IDX_USER_SLOT_5,
	OTP_IDX_ROT_KEY_0,
	OTP_IDX_ROT_KEY_1,
	OTP_IDX_ROT_KEY_2,
	OTP_IDX_ROT_KEY_3,
	OTP_IDX_ROT_KEY_4,
	OTP_IDX_ROT_KEY_5,
	OTP_IDX_ROT_KEY_6,
	OTP_IDX_ROT_KEY_7,
	OTP_IDX_ROT_KEY_8,
	OTP_IDX_ROT_KEY_9,
	OTP_IDX_ROT_KEY_10,
	OTP_IDX_ROT_KEY_11,
	OTP_IDX_ROT_KEY_12,
	OTP_IDX_ROT_KEY_13,
	OTP_IDX_ROT_KEY_14,
	OTP_IDX_ROT_KEY_15,
	OTP_IDX_ROT_KEY_16,
	OTP_IDX_ROT_KEY_17,
	OTP_IDX_ROT_KEY_18,
	OTP_IDX_ROT_KEY_19,
	OTP_IDX_MONO_CNT_0,
	OTP_IDX_MONO_CNT_1,
	OTP_IDX_MONO_CNT_2,
	OTP_IDX_BST_VERSION_VALID,
	OTP_IDX_ANCHORING_KEY_0,
	OTP_IDX_ANCHORING_KEY_1,
	OTP_IDX_ACCESS_CONTROL_BITMASK,
	OTP_IDX_DATA_INVALID,
	OTP_IDX_UDS,
	OTP_IDX_REGION_COUNT
} otp_region_idx_t;

typedef enum {
	OTP_CALLER_SW = 0,
	OTP_CALLER_NW = 1,
} otp_caller_t;

typedef struct {
	const char *name;
	otp_region_idx_t index;
	uint32_t bit_addr;
	uint32_t bit_size;
	uint8_t has_lock;
	uint8_t lock_bank;
	uint8_t lock_bit;
	uint8_t has_invalid_bit;
	uint8_t invalid_bit;
	uint8_t acl_bit;
	uint16_t caps;
	uint8_t region_class;
} otp_region_desc_t;

extern const otp_region_desc_t g_otp_regions[OTP_IDX_REGION_COUNT];

const otp_region_desc_t *otp_region_get(otp_region_idx_t idx);

/* OTP access auth (ED25519 challenge-response for writes) */
#define AMB_OTP_CHALLENGE_BYTES		32U
#define AMB_OTP_SIGNATURE_BYTES		64U

#define OTP_AUTH_OP_WRITE		1U
#define OTP_AUTH_OP_LOCK		2U
#define OTP_AUTH_OP_INC_MONO		3U
#define OTP_AUTH_OP_WRITE_MISC		4U
#define OTP_AUTH_OP_REVOKE_KEY		5U
#define OTP_AUTH_OP_WRITE_BIT		6U

struct otp_auth_op_hdr {
	uint32_t opcode;
	uint32_t region_idx;
	uint32_t sub_index;
	uint32_t flags;
	uint32_t data_len;
};

int ambarella_otp_auth_issue_challenge(uint8_t *challenge_out);
int ambarella_otp_auth_execute(const uint8_t *message, size_t message_len,
		const uint8_t signature[AMB_OTP_SIGNATURE_BYTES],
		uint32_t *result_out);

#endif /* __AMBARELLA_OTP_FLEX_H__ */
