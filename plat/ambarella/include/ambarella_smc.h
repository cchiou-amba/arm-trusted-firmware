/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#ifndef __AMBARELLA_SMC__
#define __AMBARELLA_SMC__

/* cpufreq svc ID 0x1 */
#define AMBA_SCM_SVC_FREQ			0x1
#define AMBA_SCM_CNTFRQ_SETUP_CMD		0x1

/* SYSTEM power manager 0x2 */
#define AMBA_SCM_SVC_PM				0x2	/* Deprecated */
#define AMBA_SCM_PM_GPIO_SETUP			0x1	/* Deprecated */

/* Register Access 0x3*/
#define AMBA_SIP_ACCESS_REG			0x3
#define AMBA_SIP_ACCESS_REG_READ		0x1
#define AMBA_SIP_ACCESS_REG_WRITE		0x2
#define AMBA_SIP_ACCESS_REG_SETBIT		0x3
#define AMBA_SIP_ACCESS_REG_CLRBIT		0x4
#define AMBA_SIP_ACCESS_REG_READ8		0x5
#define AMBA_SIP_ACCESS_REG_READ16		0x6
#define AMBA_SIP_ACCESS_REG_READ32		0x7
#define AMBA_SIP_ACCESS_REG_READ64		0x8
#define AMBA_SIP_ACCESS_REG_WRITE8		0x9
#define AMBA_SIP_ACCESS_REG_WRITE16		0xa
#define AMBA_SIP_ACCESS_REG_WRITE32		0xb
#define AMBA_SIP_ACCESS_REG_WRITE64		0xc

/* Switch to AARCH32 0x4 */
#define AMBA_SIP_SWITCH_TO_AARCH32		0x4
#define AMBA_SIP_AARCH32_KERNEL			0x1

/* OTP operation 0x5 */
#define AMBA_SIP_ACCESS_OTP			0x5
#define AMBA_SIP_GET_AMBA_UNIQUE_ID		0x1
#define AMBA_SIP_GET_PUKEY			0x3
#define AMBA_SIP_SET_PUKEY			0x4
#define AMBA_SIP_GET_CUSTOMER_ID		0x5
#define AMBA_SIP_SET_CUSTOMER_ID		0x6
#define AMBA_SIP_GET_COUNTER			0x7
#define AMBA_SIP_ADD_COUNTER			0x8
#define AMBA_SIP_PERMANENTLY_ENABLE_SECURE_BOOT	0x9

/* Reserve 0x0a ~ 0x1f*/
#define AMBA_SIP_GET_HUK			0x20
#define AMBA_SIP_SET_HUK_NONCE			0x21
#define AMBA_SIP_GET_HUK_NONCE			0x22
#define AMBA_SIP_GET_AES_KEY			0x23
#define AMBA_SIP_SET_AES_KEY			0x24
#define AMBA_SIP_GET_ECC_KEY			0x25
#define AMBA_SIP_SET_ECC_KEY			0x26
#define AMBA_SIP_GET_USR_SLOT_G0		0x27
#define AMBA_SIP_SET_USR_SLOT_G0		0x28
#define AMBA_SIP_GET_USR_SLOT_G1		0x29
#define AMBA_SIP_SET_USR_SLOT_G1		0x2a
#define AMBA_SIP_REVOKE_KEY			0x2b
#define AMBA_SIP_GET_CHIP_REPAIR_INFO		0x2c
#define AMBA_SIP_GET_TEST_REGION		0x2d
#define AMBA_SIP_SET_TEST_REGION		0x2e
#define AMBA_SIP_EN_ANTI_ROLLBACK		0x2f

//unsigned int lock bits
//unsigned int invalid bits
//unsigned int sysconfig
//unsigned int reserved
#define AMBA_SIP_QUERY_OTP_SETTING		0x30
#define AMBA_SIP_SET_JTAG_EFUSE			0x31
#define AMBA_SIP_LOCK_ZONA_A			0x32
#define AMBA_SIP_LOCK_PUKEY			0x33
#define AMBA_SIP_GET_SYSCONFIG			0x38
#define AMBA_SIP_SET_SYSCONFIG			0x39
#define AMBA_SIP_GET_CST_SEED			0x3a
#define AMBA_SIP_GET_CST_CUK			0x3b
#define AMBA_SIP_SET_CST_SEED_CUK		0x3c
#define AMBA_SIP_GET_USR_CUK			0x3d
#define AMBA_SIP_SET_USR_CUK			0x3e
#define AMBA_SIP_DIS_SECURE_USB_BOOT		0x3f
#define AMBA_SIP_GET_BST_VER			0x40
#define AMBA_SIP_INCREASE_BST_VER		0x41
#define AMBA_SIP_GET_MISC_CONFIG		0x42
#define AMBA_SIP_SET_MISC_CONFIG		0x43

#define AMBA_SIP_SET_AND_LOCK_AMBA_RESERVED	0x44
#define AMBA_SIP_LOCK_AMBA_RESERVED		0x45

#define AMBA_SIP_GET_DIAGNOSIS_REPORT		0x46

// debug only
#define AMBA_SIP_GENERIC_OTP_READ		0x47
#define AMBA_SIP_GENERIC_OTP_WRITE		0x48

#define AMBA_SIP_DIS_MONO_CNTS			0x49

#define AMBA_SIP_GET_TEMP_SENSOR_PARAMS		0x4a

// this api is used for normal world query readable OTP content.
#define AMBA_SIP_NW_QUERY_OTP			0x60

/* OTP write auth (challenge + Ed25519 on anchoring_key_1), sec_v3 */
#define AMBA_SIP_OTP_GET_CHALLENGE		0x61
#define AMBA_SIP_OTP_EXECUTE			0x62

/* UDS / DICE (sec_v3) */
#define AMBA_SIP_GEN_UDS_DEBUG			0x63
#define AMBA_SIP_GEN_UDS			0x64
#define AMBA_SIP_READ_UDS			0x65
#define AMBA_SIP_ENABLE_DICE			0x66

/* Authentication next stage of bootloader 0x6 */
#define AMBA_SIP_SVC_AUTH			0x6	/* deprecated */
#define AMBA_SIP_AUTH_INIT			0x1
#define AMBA_SIP_AUTH_PK			0x2
#define AMBA_SIP_AUTH_SIG			0x3
#define AMBA_SIP_AUTH_DATA			0x4
#define AMBA_SIP_AUTH_VERIFY			0x5
#define AMBA_SIP_AUTH_EXIT			0x6

/* Stage2 translation SIP 0x7 */
#define AMBA_SIP_SVC_EL2_FAULT			0x7
#define AMBA_SIP_EL2_DATA_ABORT			0x1

/* Memory monitor 0x8 */
#define AMBA_SIP_MEMORY_MONITOR			0x8
#define AMBA_SIP_MONITOR_CONFIG			0x1
#define AMBA_SIP_MONITOR_ENABLE			0x2
#define AMBA_SIP_MONITOR_DISABLE		0x3

/* VP Software reset 0x9 */
#define AMBA_SIP_VP_CONFIG			0x9
#define AMBA_SIP_VP_CONFIG_RESET		0x1

/* security-cpufreq, adjust freq of cpu(cortex) or core under security boot */
#define AMBA_SIP_SECURITY_CPUFREQ		0xA

#define AMBA_SIP_LP5_ADJUST			0xB
#define AMBA_SIP_LP5_ADJUST_ISLP5		0x1
#define AMBA_SIP_LP5_ADJUST_INIT		0x2
#define AMBA_SIP_LP5_ADJUST_RUN 		0x3
#define AMBA_SIP_LP5_ADJUST_SET_PVAL 		0x4
#define AMBA_SIP_LP5_ADJUST_GET_PVAL 		0x5
#define AMBA_SIP_LP5_ADJUST_SET_NVAL 		0x6
#define AMBA_SIP_LP5_ADJUST_GET_NVAL 		0x7
#define AMBA_SIP_LP5_ADJUST_SHOW_SWITCH 	0x8
#define AMBA_SIP_LP5_ADJUST_SET_WCK2DQI_TIMER	0x9

/* HSM smc call */
#define AMBA_SIP_HSM_CALL                       0xC
#define AMBA_SIP_HSM_INIT_QUEUE                 0x1
#define AMBA_SIP_HSM_BOOT_DECRYPTAUTH           0x2
#define AMBA_SIP_HSM_BOOT_DOMAIN		0x3
#define AMBA_SIP_HSM_DMA_CHAN_ID		0x4
#define AMBA_SIP_HSM_DOMALLOC			0x5

#define AMBA_SIP_SVC_CLUSTER			0xD
#define AMBA_SIP_FUNC_CPU_ON			0x1
#define AMBA_SIP_FUNC_CPU_OFF			0x2

/* Secure debug (challenge + Ed25519 on OTP slot 3 pubkey) 0xE */
#define AMBA_SIP_SECURE_DEBUG			0xE
#define AMBA_SIP_SD_GET_CHALLENGE		0x1
#define AMBA_SIP_SD_EXECUTE			0x2

/* XXX svc ID 0xff */
#define AMBA_SCM_SVC_QUERY			0xff
#define AMBA_SCM_QUERY_COUNT			0x00
#define AMBA_SCM_QUERY_UID			0x01
#define AMBA_SCM_QUERY_VERSION			0x03
#define AMBA_SCM_QUERY_SECURE_BOOT		0x04

#define SVC_OF_SMC(c)				(((c) >> 8) & 0xff)
#define FNID_OF_SMC(c)				((c) & 0xff)
#endif
