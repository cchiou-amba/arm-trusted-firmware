/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <ambarella_def.h>
#include <platform_def.h>

	.global ambarella_rotpk
	.global ambarella_rotpk_raw
	.global ambarella_rotpk_end
	.global ambarella_embeded_rotpk_der
	.global ambarella_embeded_rotpk_der_end

	.section .data.ambarella_rotpk
#if defined(PLAT_CFG_ATF_COT_ALGO_RSA)
ambarella_rotpk:
	/* DER header */
	.byte 0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09
	.byte 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01
	.byte 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00
	.byte 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00
ambarella_rotpk_raw:
	/* Raw Public Key, filled with N(Public Key) stord in in OTP */
.rept 256
	.byte 0x00
.endr
	.byte 0x02, 0x03, 0x01, 0x00, 0x01
ambarella_rotpk_end:
#elif defined(PLAT_CFG_ATF_COT_ALGO_ED25519)
ambarella_rotpk:
	/* DER header */
	.byte 0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65
	.byte 0x70, 0x03, 0x21, 0x00
ambarella_rotpk_raw:
	/* Raw Public Key, filled with N(Public Key) stord in in OTP */
.rept 32
	.byte 0x00
.endr
ambarella_rotpk_end:
#elif defined(PLAT_CFG_ATF_COT_ALGO_ECDSA)
	/* ECDSA need use embed cot root key */
#else
#error not supported cot algo
#endif

.align 3
ambarella_embeded_rotpk_der:
#if defined(PLAT_CFG_ATF_EMBED_PUB_COT_ROOT)
#if defined(PLAT_CFG_PUB_COT_ROOT_DER)
	.incbin PLAT_CFG_PUB_COT_ROOT_DER
#else
	.int	0xdeadbeef, 0xdeadbeef
#endif
#endif
ambarella_embeded_rotpk_der_end:
