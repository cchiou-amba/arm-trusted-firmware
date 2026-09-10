/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <plat/common/platform.h>
#include <common/debug.h>
#include <lib/mmio.h>
#include <plat_private.h>
#include <ambarella_def.h>
#include <platform_def.h>
#if defined(AMBA_SECURITY_ARCH_sec_v3)
#include "driver/ambarella_otp_flex.h"
#else
#include "driver/ambarella_otp.h"
#endif

extern char ambarella_rotpk[], ambarella_rotpk_raw[], ambarella_rotpk_end[];
extern char ambarella_embeded_rotpk_der[], ambarella_embeded_rotpk_der_end[];

#if !defined(PLAT_CFG_ATF_EMBED_PUB_COT_ROOT) && defined(PLAT_CFG_ATF_COT_ALGO_RSA)
/* Only apply to RSA key */
static int plat_rsa_get_otp_rotpk_info(void **key_ptr, unsigned int *key_len,
				       unsigned int *flags)
{
	uint32_t i, lock_bit, invalid_bit, pk_n_addr, pk_n;

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bit) < 0)
		return -1;

	if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32, &invalid_bit) < 0)
		return -1;

	/* The 3rd Public Key cannot be invalid. */
	invalid_bit &= ~(1 << (ROT_KEY_NUM - 1));

	for (i = 0; i < ROT_KEY_NUM; i++) {
		if (!(lock_bit & (1 << (i + LOCK_BIT_ROT_BASE)))) {
			*key_len = 0;
			*flags = ROTPK_NOT_DEPLOYED;
			NOTICE("The Public Key (RSA) %d is NOT Locked!\n", i);
			return 0;
		}

		if (!(invalid_bit & (1 << i)))
			break;
	}

	INFO("Using the Public Key %d\n", i);

	/* RSA N plus RSA RN */
	pk_n_addr = ROT_PUBKEY_ADDR + i * ROT_PUBKEY_BITS;

	for (i = 0; i < ROT_PUBKEY_BITS / 2; i += 32) {
		if (ambarella_otp_read(pk_n_addr + (ROT_PUBKEY_BITS / 2) - 32 - i, 32, &pk_n) < 0)
			return -1;
		ambarella_rotpk_raw[i / 8 + 0] = (pk_n >> 24) & 0xff;
		ambarella_rotpk_raw[i / 8 + 1] = (pk_n >> 16) & 0xff;
		ambarella_rotpk_raw[i / 8 + 2] = (pk_n >> 8) & 0xff;
		ambarella_rotpk_raw[i / 8 + 3] = (pk_n >> 0) & 0xff;
	}

	*key_ptr = ambarella_rotpk;
	*key_len = (unsigned long) ambarella_rotpk_end
		- (unsigned long) ambarella_rotpk;
	*flags = 0;

	return 0;
}
#endif

#if !defined(PLAT_CFG_ATF_EMBED_PUB_COT_ROOT) && defined(PLAT_CFG_ATF_COT_ALGO_ED25519)
static int plat_ed25519_get_otp_rotpk_info(void **key_ptr, unsigned int *key_len,
					   unsigned int *flags)
{
	uint32_t i, lock_bit, pk_addr, pk;
	uint32_t key_index;
	uint32_t bst_key_index;

	boot_cookie_t *boot_cookie;
	boot_cookie = boot_cookie_ptr();

	bst_key_index = boot_cookie->bst_key_index;
	key_index = (bst_key_index & 0xffff0000) >> 16;

	INFO("Using the Public Key %d\n", key_index);

#if defined(AMBA_SECURITY_ARCH_sec_v3)
	if (key_index >= ROT_KEY_NUM) {
		return -1;
	}
	if (ambarella_otp_read(WRITE_LOCK_2_BIT_ADDR + key_index, 1, &lock_bit) < 0) {
		return -1;
	}
#else
	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR + LOCK_BIT_ROT_BASE, 1,
			&lock_bit) < 0) {
		return -1;
	}
#endif

	if (!lock_bit) {
		*key_len = 0;
		*flags = ROTPK_NOT_DEPLOYED;
		NOTICE("The Public Key (ED25519) %d is NOT Locked!\n", key_index);
		return 0;
	}

	pk_addr = ROT_PUBKEY_ADDR + key_index * ROT_PUBKEY_BITS;

#if defined(AMBA_SECURITY_ARCH_sec_v3)
	/* 512-bit digest slot; Ed25519 pubkey is the first 256 bits */
	for (i = 0; i < 256U; i += 32U) {
#else
	for (i = 0; i < ROT_PUBKEY_BITS; i += 32) {
#endif
		if (ambarella_otp_read(pk_addr + i, 32, &pk) < 0)
			return -1;
		ambarella_rotpk_raw[i / 8 + 3] = (pk >> 24) & 0xff;
		ambarella_rotpk_raw[i / 8 + 2] = (pk >> 16) & 0xff;
		ambarella_rotpk_raw[i / 8 + 1] = (pk >> 8) & 0xff;
		ambarella_rotpk_raw[i / 8 + 0] = (pk >> 0) & 0xff;
	}

	*flags = ROTPK_NOT_DEPLOYED;

	*key_ptr = ambarella_rotpk;
	*key_len = (unsigned long) ambarella_rotpk_end
		- (unsigned long) ambarella_rotpk;
	*flags = 0;

	return 0;
}
#endif

#if defined(PLAT_CFG_ATF_EMBED_PUB_COT_ROOT)
static int plat_get_embedded_rotpk_info(
			void **key_ptr, unsigned int *key_len,
			unsigned int *flags)
{
	*key_ptr = ambarella_embeded_rotpk_der;
	*key_len = (unsigned long) ambarella_embeded_rotpk_der_end
		- (unsigned long) ambarella_embeded_rotpk_der;
	*flags = 0;
	return 0;
}
#else
static int plat_get_otp_rotpk_info(void **key_ptr, unsigned int *key_len,
				    unsigned int *flags)
{
	int ret = 0;

#if defined(PLAT_CFG_ATF_COT_ALGO_ED25519)
	ret = plat_ed25519_get_otp_rotpk_info(key_ptr, key_len, flags);
#elif defined(PLAT_CFG_ATF_COT_ALGO_RSA)
	ret = plat_rsa_get_otp_rotpk_info(key_ptr, key_len, flags);
#else
	ret = -1;
	ERROR("Not supported COT algo!\n");
#endif
	return ret;
}
#endif

int plat_get_rotpk_info(void *cookie, void **key_ptr, unsigned int *key_len,
			unsigned int *flags)
{
	const char *algo = NULL;

	if (!ambarella_is_secure_boot()) {
		*key_len = 0;
		*flags = ROTPK_NOT_DEPLOYED;
		NOTICE("Secure Boot is NOT turned on\n");
		return 0;
	}

#if defined(PLAT_CFG_ATF_COT_ALGO_ED25519)
	algo = "ed25519";
#elif defined(PLAT_CFG_ATF_COT_ALGO_RSA)
	algo = "rsa";
#elif defined(PLAT_CFG_ATF_COT_ALGO_ECDSA)
	algo = "ecdsa";
#else
	*flags = ROTPK_NOT_DEPLOYED;
	ERROR("PLAT_CFG_ATF_COT_ALGO is not selected\n");
	return 0;
#endif

#if defined(PLAT_CFG_ATF_EMBED_PUB_COT_ROOT)
	NOTICE("Using the embedded Public Key, algo is %s\n", algo);
	return plat_get_embedded_rotpk_info(key_ptr, key_len, flags);
#else
	NOTICE("Using the Public Key in OTP, algo is %s\n", algo);
	return plat_get_otp_rotpk_info(key_ptr, key_len, flags);
#endif
}

int plat_get_nv_ctr(void *cookie, unsigned int *nv_ctr)
{
	/*
	 * No support for non-volatile counter.  Update the ROT key to protect
	 * the system against rollback.
	 */
	*nv_ctr = 0;

	return 0;
}

int plat_set_nv_ctr(void *cookie, unsigned int nv_ctr)
{
	return 0;
}

int plat_get_mbedtls_heap(void **heap_addr, size_t *heap_size)
{
	return get_mbedtls_heap_helper(heap_addr, heap_size);
}

