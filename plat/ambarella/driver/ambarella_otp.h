/*
 * ambarella_otp.h
 *
 * History:
 *	2018/05/24 - [Cao Rongrong] created file
 *
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#ifndef __AMBARELLA_OTP_H__
#define __AMBARELLA_OTP_H__

/* ==========================================================================*/

/* OTP_CTRL1_REG bit define */
#if defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7)
#define READ_FSM_ENABLE			BIT(23)
#define READ_ENABLE			BIT(22)
#define DBG_READ_MODE			BIT(21)
#define FSM_WRITE_MODE			BIT(19)
#define PROG_ENABLE			BIT(18)
#define PROG_FSM_ENABLE			BIT(17)
#else
#define READ_FSM_ENABLE			BIT(22)
#define READ_ENABLE			BIT(21)
#define DBG_READ_MODE			BIT(20)
#define FSM_WRITE_MODE			BIT(18)
#define PROG_ENABLE			BIT(17)
#define PROG_FSM_ENABLE			BIT(16)
#endif

/* OTP_OBSV_REG bit define */
#define WRITE_PROG_DONE			BIT(4)
#define WRITE_PROG_FAIL			BIT(3)
#define WRITE_PROG_RDY			BIT(2)
#define READ_OBSV_RDY			BIT(1)
#define READ_OBSV_DONE			BIT(0)

#if defined(AMBARELLA_CV2)
#define OTP_CTRL1_OFFSET		0x760
#define OTP_CTRL2_OFFSET		0x764
#define OTP_OBSV_OFFSET			0x768
#define OTP_READ_DOUT_OFFSET		0x76C
#define OTP_CTRL1_REG			RCT_REG(OTP_CTRL1_OFFSET)
#define OTP_OBSV_REG			RCT_REG(OTP_OBSV_OFFSET)
#define OTP_READ_DOUT_REG		RCT_REG(OTP_READ_DOUT_OFFSET)
#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define OTP_CTRL1_OFFSET		0x00
#define OTP_OBSV_OFFSET			0x04
#define OTP_READ_DOUT_OFFSET		0x08
#define OTP_CTRL2_OFFSET		0x0C
#define OTP_CTRL1_REG			OTP_CONFIG_REG(OTP_CTRL1_OFFSET)
#define OTP_OBSV_REG			OTP_CONFIG_REG(OTP_OBSV_OFFSET)
#define OTP_READ_DOUT_REG		OTP_CONFIG_REG(OTP_READ_DOUT_OFFSET)
#define OTP_CTRL2_REG			OTP_CONFIG_REG(OTP_CTRL2_OFFSET)
#else
#define OTP_CTRL1_OFFSET		0xA0
#define OTP_OBSV_OFFSET			0xA4
#define OTP_READ_DOUT_OFFSET		0xA8
#define OTP_CTRL1_REG			SECURE_SCRATCHPAD_REG(OTP_CTRL1_OFFSET)
#define OTP_OBSV_REG			SECURE_SCRATCHPAD_REG(OTP_OBSV_OFFSET)
#define OTP_READ_DOUT_REG		SECURE_SCRATCHPAD_REG(OTP_READ_DOUT_OFFSET)
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define OTP_BIT_SIZE			0x08000
#else
#define OTP_BIT_SIZE			0x10000
#endif

#if defined (AMBARELLA_N1) || defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75) || \
	defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7)
#define HAS_FUNCTION_DISABLE
#endif

#if defined (AMBARELLA_N1) || defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75) || \
	defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7)
#define HAS_OTP_INVERT_REG
#endif

#if defined (AMBARELLA_N1) || defined(AMBARELLA_CV3AD685)
#define OTP_CTRL1_INVERT_REG        SECURE_SCRATCHPAD_REG(0x110)
#elif defined (AMBARELLA_CV72) || defined(AMBARELLA_CV75)
#define OTP_CTRL1_INVERT_REG        SECURE_SCRATCHPAD_REG(0xB0)
#elif defined(AMBARELLA_CV7)
#define OTP_CTRL1_INVERT_REG        SECURE_SCRATCHPAD_REG(0xB8)
#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define OTP_CTRL1_INVERT_REG	OTP_CONFIG_REG(0x10)
#endif

/* ==========================================================================*/

/*
 *  OTP EXTERNAL POWER supply
 */
#if defined(AMBARELLA_S6LM)
#define GPIO_OTP_PWR_SW			35
#elif defined(AMBARELLA_CV28)
#define GPIO_OTP_PWR_SW			85
#endif

/*
 * OTP Layout
 */
#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_CV28) || defined(AMBARELLA_S6LM)
#define OTP_LAYOUT_VERSION		1
#else
#define OTP_LAYOUT_VERSION		2

/*
 * has BST anti rollback
 */

#if defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75) || defined(AMBARELLA_N1) || \
	defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7)
#define HAS_BST_ANTI_ROLLBACK
#endif

/*
 * has on-die temperature sensor
 */

#if defined(AMBARELLA_CV75) || defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_CV3AD635) || defined(AMBARELLA_N1_655) || defined(AMBARELLA_N1_635) || defined(AMBARELLA_CV7)
#define HAS_ON_DIE_TEMPERATURE_SENSOR

#if defined(AMBARELLA_CV75)

#define ON_DIE_TEMP_SENSOR_PARAMS_ADDR		(0x240)	/* bit addr */
#define ON_DIE_TEMP_SENSOR_PARAMS_BITS		(0x40)

#elif defined(AMBARELLA_CV3AD685)

#define ON_DIE_TEMP_SENSOR_PARAMS_ADDR		(0x420)	/* bit addr */
#define ON_DIE_TEMP_SENSOR_PARAMS_BITS		(0xE0)

#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV3AD635) || defined(AMBARELLA_N1_635)

#define ON_DIE_TEMP_SENSOR_PARAMS_ADDR		(0x440)	/* bit addr */
#define ON_DIE_TEMP_SENSOR_PARAMS_BITS		(0x100)

#elif defined(AMBARELLA_CV7)
#define ON_DIE_TEMP_SENSOR_PARAMS_ADDR		(0x220)	/* bit addr */
#define ON_DIE_TEMP_SENSOR_PARAMS_BITS		(0x80)	/* 128 bits */
#endif

#endif

/*
 * reserve upper 128 bits
 */

#if defined(AMBARELLA_CV75) || defined(AMBARELLA_CV7) || defined(AMBARELLA_CV3AD635) || defined(AMBARELLA_N1_635)
#define NOT_NEED_RESV_UPPER_128BITS
#else
#define NEED_RESV_UPPER_128BITS
#endif

/*
 * has indication bits
 */

#if defined(AMBARELLA_CV7) || defined(AMBARELLA_CV3AD635) || defined(AMBARELLA_N1_635)
#define HAS_INDICATION_BITS
#endif

#endif


#define CHIP_REPAIR_INFO_ADDR		(0x0)		/* bit addr */
#define CHIP_REPAIR_INFO_BITS		16000

/*
 *  OTP write lock
 */
#define WRITE_LOCK_BIT_ADDR		(0xC0)		/* bit addr */

/*
 *  OTP efuse bit and write lock bit
 */

#define SECURE_BOOT_BIT				6

#ifndef HAS_FUNCTION_DISABLE
#define JTAG_EFUSE_BIT				63
#endif

#if (OTP_LAYOUT_VERSION == 1)
#define LOCK_BIT_A				0
#define LOCK_BIT_UNIQUE_ID			0
#define LOCK_BIT_CUSTOMER_ID			2
#define LOCK_BIT_TEST_REGION			3
#define LOCK_BIT_ROT_BASE			5
#define LOCK_BIT_HUK_NONCE			8
#define LOCK_BIT_AES_KEY_BASE			10
#define LOCK_BIT_ECC_KEY_BASE			14
#define LOCK_BIT_USR_SLOT_G0_BASE		18
#if defined(AMBARELLA_CV28)
#define LOCK_BIT_SYS_CONFIG			24
#else
#define LOCK_BIT_SYS_CONFIG			0
#endif

#elif (OTP_LAYOUT_VERSION == 2)
#define EN_ANTI_ROLLBACK_BIT			0xBE90
#define DIS_SECURE_USB_BOOT_BIT			0xBE91

#define LOCK_BIT_A				0
#define LOCK_BIT_UNIQUE_ID			1
#define LOCK_BIT_SYS_CONFIG			3
#define LOCK_BIT_CST_PLANTED_SEED_CUK		4
#define LOCK_BIT_CUSTOMER_ID			5
#define LOCK_BIT_TEST_REGION			6
#define DISABLE_MONO_CNTS_INC_BIT	7
#define LOCK_BIT_ROT_BASE			8

#define MONO_CNTS_KEEP_ORI_INC_BIT	9
#define USER_DATA_KEEP_ORI_INC_BIT	10

#define LOCK_BIT_HUK_NONCE			11
#define LOCK_BIT_USR_PLANTED_CUK		12
#define LOCK_BIT_AES_KEY_BASE			13
#define LOCK_BIT_ECC_KEY_BASE			17
#define LOCK_BIT_USR_SLOT_G0_BASE		21
#define LOCK_BIT_USR_SLOT_G1_BASE		27

#define LOCK_BIT_AMBA_RESERVED		31
#endif

/*
 *  OTP sysconfig
 */
#define SYS_CONFIG_BITS			64

/*
 *  OTP Function Disable
 */
#if (OTP_LAYOUT_VERSION == 2)

#ifdef HAS_FUNCTION_DISABLE
#define FUNCTION_DISABLE_ADDR		(0x200)
#define FUNCTION_DISABLE_BITS		32

#if defined(AMBARELLA_CV72)

#define LOCK_BIT_FUNCTION_DISABLE	2

#elif defined(AMBARELLA_CV75) || defined(AMBARELLA_CV7)

#define LOCK_BIT_FUNCTION_DISABLE	3

#elif defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_N1) || defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV3AD635) || defined(AMBARELLA_N1_635)

#define LOCK_BIT_FUNCTION_DISABLE	4

#endif

#define JTAG_DIS_BIT	2
#define GPU_DIS_BIT		3
#define USBPHY_DIS_BIT	4
#define DBG_TDR_DIS_BIT	7

#endif

#endif

/*
 *  OTP ROT public key
 */
#if (OTP_LAYOUT_VERSION == 1)
#define ROT_KEY_NUM			3
#define ROT_PUBKEY_BITS			4096
#define NON_REVOKABLE_KEY_INDEX		2
#else
#define ROT_KEY_NUM			16
#define ROT_PUBKEY_BITS			256
#define NON_REVOKABLE_KEY_INDEX		0
#endif

/*
 *  OTP HUK
 */
#define HUK_BITS		128
#define HW_NONCE_BITS		128

/*
 *  OTP customer defined AES and ECC keys
 */
#define AES_KEY_NUM		4
#define ECC_KEY_NUM		4
#define AES_KEY_BITS		256
#define ECC_KEY_BITS		256

/*
 *  OTP Unique ID
 */
#define UNIQUE_ID_BITS		128

/*
 *  OTP Customer ID (Customer serial number)
 */
#define CUSTOMER_ID_BITS	128

/*
 *  OTP Monotonic counter
 */
#if (OTP_LAYOUT_VERSION == 1)
#define NUM_OF_MONO_CNT		1
#define MONO_CNT_0_BITS		256
#else
#define NUM_OF_MONO_CNT		3

#define MONO_CNT_0_BITS		256
#define MONO_CNT_1_BITS		256
#define MONO_CNT_2_BITS		512

#define MONO_CNT_NEW_0_BITS		512
#define MONO_CNT_NEW_1_BITS		512
#define MONO_CNT_NEW_2_BITS		1024

#endif

/*
 *  OTP User Slot Group 0 (lockable)
 */
#define USR_SLOT_G0_NUM		6
#define USR_SLOT_G0_BITS	256

/*
 *  OTP User Slot Group 1 (lockable)
 */
#if (OTP_LAYOUT_VERSION == 2)
#define USR_SLOT_G1_NUM		4
#define USR_SLOT_G1_BITS	1024
#endif

/*
 *  User Data Group 0 (not lockable)
 */
#if (OTP_LAYOUT_VERSION == 2)

#define USER_DATA_BITS		1024

#define USER_DATA_START_INX	0

#ifdef HAS_INDICATION_BITS
#define USER_DATA_NUM		2
#else
#define USER_DATA_NUM		3
#endif

#ifdef NEED_RESV_UPPER_128BITS
#define USER_DATA_START_NEW_INX	1
#define USER_DATA_NEW_NUM		2
#endif

#endif

/*
 *  OTP Test Region
 */
#define TEST_REGION_BITS		128


/*
 *  OTP Misc Config
 */

#if (OTP_LAYOUT_VERSION == 1)
#define MISC_CONFIG_ADDR		(0x3E83)
#define MISC_CONFIG_BITS		29
#else
#define MISC_CONFIG_ADDR		(0xBE92)
#define MISC_CONFIG_BITS		14
#endif

/*
 *  other OTP fields
 */
#if (OTP_LAYOUT_VERSION == 1)

#define SYS_CONFIG_BIT_ADDR		(0x00)		/* bit addr of sysconfig */

#define UNIQUE_ID_ADDR_ORICV25		(0x19C0)	/* bit addr */
#define UNIQUE_ID_ADDR			(0x3E00)	/* bit addr */

#define DATA_INVALID_BIT_ADDR		(0x3E80)	/* bit addr of revoke bits */
#define CUSTOMER_ID_ADDR		(0x3F00)	/* bit addr of customer id*/
#define TEST_REGION_ADDR		(0x3F80)	/* bit addr of test region */
#define HUK_ADDR			(0x4000)	/* bit addr */
#define HW_NONCE_ADDR			(0x4080)	/* bit addr */
#define MONO_CNT_0_ADDR			(0x4100)	/* bit addr */
#define AES_KEY_BASE_ADDR		(0x4200)	/* bit addr */
#define ECC_KEY_BASE_ADDR		(0x4600)	/* bit addr */
#define USR_SLOT_G0_ADDR		(0x4A00)	/* bit addr of usr slot group 0*/
#define ROT_PUBKEY_ADDR			(0x5000)	/* bit addr of ROT pubkey base */

#else

#define SYS_CONFIG_BIT_ADDR		(0x40)		/* bit addr of sysconfig */

#define CST_PLANTED_SEED_ADDR		(0xE0)	/* bit addr */

#define UNIQUE_ID_ADDR			(0x100)	/* bit addr */

#define CST_PLANTED_CUK_ADDR		(0x180)	/* bit addr */

#if defined(AMBARELLA_N1)
#define DATA_INVALID_BIT_ADDR		(0x3E80)	/* bit addr of revoke bits */
#define BST_VER_ADDR			(0x3EA0)	/* bit addr */
#define CUSTOMER_ID_ADDR		(0x3F00)	/* bit addr */
#define TEST_REGION_ADDR		(0x3F80)	/* bit addr of test region */
#define HUK_ADDR			(0x4000)	/* bit addr */
#define HW_NONCE_ADDR			(0x4080)	/* bit addr */
#define USR_PLANTED_CUK_ADDR		(0x4100)	/* bit addr */
#define AES_KEY_BASE_ADDR		(0x4200)	/* bit addr */
#define ECC_KEY_BASE_ADDR		(0x4600)	/* bit addr */
#define USR_SLOT_G0_ADDR		(0x4A00)	/* bit addr of usr slot group 0*/

#define ROT_PUBKEY_ADDR			(0x5000)	/* bit addr of ROT pubkey base */

#define MONO_CNT_0_ADDR			(0x6000)	/* bit addr */
#define MONO_CNT_1_ADDR			(0x6100)	/* bit addr */
#define MONO_CNT_2_ADDR			(0x6200)	/* bit addr */

#define USER_DATA_BASE_ADDR		(0x6400)	/* bit addr of user data 0*/

#define MONO_CNT_NEW_0_ADDR			(0x6000)	/* bit addr */
#define MONO_CNT_NEW_1_ADDR			(0x6200)	/* bit addr */
#define MONO_CNT_NEW_2_ADDR			(0x6400)	/* bit addr */
#define USER_DATA_BASE_NEW_ADDR		(0x6800)	/* bit addr of user data 1*/

#define USR_SLOT_G1_ADDR		(0x7000)	/* bit addr of usr slot group 1*/

#else
#define DATA_INVALID_BIT_ADDR		(0xBE80)	/* bit addr of revoke bits */
#define BST_VER_ADDR			(0xBEA0)	/* bit addr */
#define CUSTOMER_ID_ADDR		(0xBF00)	/* bit addr */
#define TEST_REGION_ADDR		(0xBF80)	/* bit addr of test region */
#define HUK_ADDR			(0xC000)	/* bit addr */
#define HW_NONCE_ADDR			(0xC080)	/* bit addr */
#define USR_PLANTED_CUK_ADDR		(0xC100)	/* bit addr */
#define AES_KEY_BASE_ADDR		(0xC200)	/* bit addr */
#define ECC_KEY_BASE_ADDR		(0xC600)	/* bit addr */
#define USR_SLOT_G0_ADDR		(0xCA00)	/* bit addr of usr slot group 0*/
#define ROT_PUBKEY_ADDR			(0xD000)	/* bit addr of ROT pubkey base */

#define MONO_CNT_0_ADDR			(0xE000)	/* bit addr */
#define MONO_CNT_1_ADDR			(0xE100)	/* bit addr */
#define MONO_CNT_2_ADDR			(0xE200)	/* bit addr */

#define USER_DATA_BASE_ADDR		(0xE400)	/* bit addr of user data 0*/

#ifdef HAS_INDICATION_BITS
#define INDICATION_BITS_ADDR		(0xEC00)	/* bit addr of indication bits*/
#endif

#define MONO_CNT_NEW_0_ADDR			(0xE000)	/* bit addr */
#define MONO_CNT_NEW_1_ADDR			(0xE200)	/* bit addr */
#define MONO_CNT_NEW_2_ADDR			(0xE400)	/* bit addr */
#define USER_DATA_BASE_NEW_ADDR		(0xE800)	/* bit addr of user data 1*/

#define USR_SLOT_G1_ADDR		(0xF000)	/* bit addr of usr slot group 1*/

#endif

#define CST_PLANTED_SEED_BITS		32
#define CST_PLANTED_CUK_BITS		128
#define BST_VER_BITS			96
#define USR_PLANTED_CUK_BITS		256

#endif

/* keep consistent with invoker */
// output flag
#define D_FLAG_LOCKED 0x01
#define D_FLAG_PARAM_ERROR 0x02
#define D_FLAG_HW_ERROR 0x08
#define D_FLAG_DATA_MISMATCH 0x04
#define D_FLAG_REVOKED 0x10
#define D_FLAG_PROG_INDICATED 0x20

// input flag
#define D_IN_FLAG_WRITE_CONTENT 0x01
#define D_IN_FLAG_WRITE_LOCK 0x02
#define D_IN_FLAG_FAST_WRITE 0x04  //pullup GPIO, write all, pull down GPIO

#define D_IN_FLAG_SIMULATE_WRITE 0x08

/* ==========================================================================*/

#endif
