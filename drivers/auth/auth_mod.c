/*
 * Copyright (c) 2015-2023, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <platform_def.h>

#include <common/debug.h>
#include <common/tbbr/cot_def.h>
#include <drivers/auth/auth_common.h>
#include <drivers/auth/auth_mod.h>
#include <drivers/auth/crypto_mod.h>
#include <drivers/auth/img_parser_mod.h>
#include <drivers/fwu/fwu.h>
#include <lib/fconf/fconf_tbbr_getter.h>
#include <plat/common/platform.h>

#include <tools_share/zero_oid.h>

/* ASN.1 tags */
#define ASN1_INTEGER                 0x02

#pragma weak plat_set_nv_ctr2

static int cmp_auth_param_type_desc(const auth_param_type_desc_t *a,
		const auth_param_type_desc_t *b)
{
	if ((a->type == b->type) && (a->cookie == b->cookie)) {
		return 0;
	}
	return 1;
}

/*
 * This function obtains the requested authentication parameter data from the
 * information extracted from the parent image after its authentication.
 */
static int auth_get_param(const auth_param_type_desc_t *param_type_desc,
			  const auth_img_desc_t *img_desc,
			  void **param, unsigned int *len)
{
	int i;

	if (img_desc->authenticated_data == NULL)
		return 1;

	for (i = 0 ; i < COT_MAX_VERIFIED_PARAMS ; i++) {
		if (0 == cmp_auth_param_type_desc(param_type_desc,
				img_desc->authenticated_data[i].type_desc)) {
			*param = img_desc->authenticated_data[i].data.ptr;
			*len = img_desc->authenticated_data[i].data.len;
			return 0;
		}
	}

	return 1;
}

/*
 * Authenticate an image by matching the data hash
 *
 * This function implements 'AUTH_METHOD_HASH'. To authenticate an image using
 * this method, the image must contain:
 *
 *   - The data to calculate the hash from
 *
 * The parent image must contain:
 *
 *   - The hash to be matched with (including hash algorithm)
 *
 * For a successful authentication, both hashes must match. The function calls
 * the crypto-module to check this matching.
 *
 * Parameters:
 *   param: parameters to perform the hash authentication
 *   img_desc: pointer to image descriptor so we can know the image type
 *             and parent image
 *   img: pointer to image in memory
 *   img_len: length of image (in bytes)
 *
 * Return:
 *   0 = success, Otherwise = error
 */
static int auth_hash(const auth_method_param_hash_t *param,
		     const auth_img_desc_t *img_desc,
		     void *img, unsigned int img_len)
{
	void *data_ptr, *hash_der_ptr;
	unsigned int data_len, hash_der_len;
	int rc;

	VERBOSE("[CoT] Starting hash authentication for image ID %u\n", img_desc->img_id);

	/* Get the hash from the parent image. This hash will be DER encoded
	 * and contain the hash algorithm */
	VERBOSE("[CoT] Getting hash from parent image (ID: %u)\n",
		img_desc->parent ? img_desc->parent->img_id : 0);
	rc = auth_get_param(param->hash, img_desc->parent,
			&hash_der_ptr, &hash_der_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Retrieved hash from parent: len=%u\n", hash_der_len);

	/* Get the data to be hashed from the current image */
	VERBOSE("[CoT] Extracting data to hash from image (type=%u, len=%u)\n",
		img_desc->img_type, img_len);
	rc = img_parser_get_auth_param(img_desc->img_type, param->data,
			img, img_len, &data_ptr, &data_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Extracted data to hash: len=%u\n", data_len);

	/* Ask the crypto module to verify this hash */
	VERBOSE("[CoT] Verifying hash match (data_len=%u, hash_len=%u)\n",
		data_len, hash_der_len);
	rc = crypto_mod_verify_hash(data_ptr, data_len,
				    hash_der_ptr, hash_der_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}

	VERBOSE("[CoT] Hash authentication successful for image ID %u\n", img_desc->img_id);
	return 0;
}

/*
 * Authenticate by digital signature
 *
 * This function implements 'AUTH_METHOD_SIG'. To authenticate an image using
 * this method, the image must contain:
 *
 *   - Data to be signed
 *   - Signature
 *   - Signature algorithm
 *
 * We rely on the image parser module to extract this data from the image.
 * The parent image must contain:
 *
 *   - Public key (or a hash of it)
 *
 * If the parent image contains only a hash of the key, we will try to obtain
 * the public key from the image itself (i.e. self-signed certificates). In that
 * case, the signature verification is considered just an integrity check and
 * the authentication is established by calculating the hash of the key and
 * comparing it with the hash obtained from the parent.
 *
 * If the image has no parent (NULL), it means it has to be authenticated using
 * the ROTPK stored in the platform. Again, this ROTPK could be the key itself
 * or a hash of it.
 *
 * Return: 0 = success, Otherwise = error
 */
static int auth_signature(const auth_method_param_sig_t *param,
			  const auth_img_desc_t *img_desc,
			  void *img, unsigned int img_len)
{
	void *data_ptr, *pk_ptr, *cnv_pk_ptr, *pk_plat_ptr, *sig_ptr, *sig_alg_ptr, *pk_oid;
	unsigned int data_len, pk_len, cnv_pk_len, pk_plat_len, sig_len, sig_alg_len;
	unsigned int flags = 0;
	int rc;

	VERBOSE("[CoT] Starting signature authentication for image ID %u\n", img_desc->img_id);

	/* Get the data to be signed from current image */
	VERBOSE("[CoT] Extracting signed data from image\n");
	rc = img_parser_get_auth_param(img_desc->img_type, param->data,
			img, img_len, &data_ptr, &data_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Extracted signed data: len=%u\n", data_len);

	/* Get the signature from current image */
	VERBOSE("[CoT] Extracting signature from image\n");
	rc = img_parser_get_auth_param(img_desc->img_type, param->sig,
			img, img_len, &sig_ptr, &sig_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Extracted signature: len=%u\n", sig_len);

	/* Get the signature algorithm from current image */
	VERBOSE("[CoT] Extracting signature algorithm from image\n");
	rc = img_parser_get_auth_param(img_desc->img_type, param->alg,
			img, img_len, &sig_alg_ptr, &sig_alg_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Extracted signature algorithm: len=%u\n", sig_alg_len);

	/* Get the public key from the parent. If there is no parent (NULL),
	 * the certificate has been signed with the ROTPK, so we have to get
	 * the PK from the platform */
	if (img_desc->parent != NULL) {
		VERBOSE("[CoT] Getting public key from parent image (ID: %u)\n",
			img_desc->parent->img_id);
		rc = auth_get_param(param->pk, img_desc->parent,
				&pk_ptr, &pk_len);
		if (rc != 0) {
			VERBOSE("[TBB] %s():%d failed with error code %d.\n",
				__func__, __LINE__, rc);
			return rc;
		}
		VERBOSE("[CoT] Retrieved public key from parent: len=%u\n", pk_len);
	} else {
		VERBOSE("[CoT] Root certificate detected - using ROTPK from platform\n");
		/*
		 * Root certificates are signed with the ROTPK, so we have to
		 * get it from the platform.
		 */
		VERBOSE("[CoT] Retrieving ROTPK from platform\n");
		rc = plat_get_rotpk_info(param->pk->cookie, &pk_plat_ptr,
					 &pk_plat_len, &flags);
		if (rc != 0) {
			VERBOSE("[TBB] %s():%d failed with error code %d.\n",
				__func__, __LINE__, rc);
			return rc;
		}
		VERBOSE("[CoT] Retrieved ROTPK from platform: len=%u, flags=0x%x\n",
			pk_plat_len, flags);

		assert(is_rotpk_flags_valid(flags));

		/* Also retrieve the key from the image. */
		VERBOSE("[CoT] Extracting public key from certificate\n");
		rc = img_parser_get_auth_param(img_desc->img_type,
					       param->pk, img, img_len,
					       &pk_ptr, &pk_len);
		if (rc != 0) {
			VERBOSE("[TBB] %s():%d failed with error code %d.\n",
				__func__, __LINE__, rc);
			return rc;
		}
		VERBOSE("[CoT] Extracted public key from certificate: len=%u\n", pk_len);

		/*
		 * Validate the certificate's key against the platform ROTPK.
		 *
		 * Platform may store key in one of the following way -
		 * 1. Hash of ROTPK
		 * 2. Hash if prefixed, suffixed or modified ROTPK
		 * 3. Full ROTPK
		 */
		if ((flags & ROTPK_NOT_DEPLOYED) != 0U) {
			NOTICE("ROTPK is not deployed on platform. "
				"Skipping ROTPK verification.\n");
			VERBOSE("[CoT] ROTPK not deployed - skipping verification\n");
		} else if ((flags & ROTPK_IS_HASH) != 0U) {
			VERBOSE("[CoT] ROTPK stored as hash - converting and verifying\n");
			/*
			 * platform may store the hash of a prefixed,
			 * suffixed or modified pk
			 */
			rc = crypto_mod_convert_pk(pk_ptr, pk_len, &cnv_pk_ptr, &cnv_pk_len);
			if (rc != 0) {
				VERBOSE("[TBB] %s():%d failed with error code %d.\n",
					__func__, __LINE__, rc);
				return rc;
			}
			VERBOSE("[CoT] Converted PK: len=%u\n", cnv_pk_len);

			/*
			 * The hash of the certificate's public key must match
			 * the hash of the ROTPK.
			 */
			VERBOSE("[CoT] Verifying certificate PK hash against ROTPK hash\n");
			rc = crypto_mod_verify_hash(cnv_pk_ptr, cnv_pk_len,
						    pk_plat_ptr, pk_plat_len);
			if (rc != 0) {
				VERBOSE("[TBB] %s():%d failed with error code %d.\n",
					__func__, __LINE__, rc);
				return rc;
			}
			VERBOSE("[CoT] Certificate PK hash matches ROTPK hash\n");
		} else {
			VERBOSE("[CoT] ROTPK stored as full key - comparing directly\n");
			/* Platform supports full ROTPK */
			if (pk_len != pk_plat_len) {
				ERROR("plat and cert ROTPK len mismatch\n");
				return -1;
			}
			{
				int diff = 0;
				unsigned int i;

				for (i = 0; i < pk_len; i++) {
					diff |= ((const unsigned char *)pk_plat_ptr)[i] ^
						((const unsigned char *)pk_ptr)[i];
				}
				if (diff != 0) {
					ERROR("plat and cert ROTPK mismatch\n");
					return -1;
				}
			}
			VERBOSE("[CoT] Certificate PK matches platform ROTPK\n");
		}

		/*
		 * Set Zero-OID for ROTPK(subject key) as a the certificate
		 * does not hold Key-OID information for ROTPK.
		 */
		if (param->pk->cookie != NULL) {
			pk_oid = param->pk->cookie;
		} else {
			pk_oid = ZERO_OID;
		}

		/*
		 * Public key is verified at this stage, notify platform
		 * to measure and publish it.
		 */
		VERBOSE("[CoT] Measuring and publishing verified ROTPK\n");
		rc = plat_mboot_measure_key(pk_oid, pk_ptr, pk_len);
		if (rc != 0) {
			VERBOSE("[TBB] %s():%d failed with error code %d.\n",
				__func__, __LINE__, rc);
			return rc;
		}
	}

	/* Ask the crypto module to verify the signature */
	VERBOSE("[CoT] Verifying signature (data_len=%u, sig_len=%u, pk_len=%u)\n",
		data_len, sig_len, pk_len);
	rc = crypto_mod_verify_signature(data_ptr, data_len,
					 sig_ptr, sig_len,
					 sig_alg_ptr, sig_alg_len,
					 pk_ptr, pk_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}

	VERBOSE("[CoT] Signature authentication successful for image ID %u\n", img_desc->img_id);
	return 0;
}

/*
 * Authenticate by Non-Volatile counter
 *
 * To protect the system against rollback, the platform includes a non-volatile
 * counter whose value can only be increased. All certificates include a counter
 * value that should not be lower than the value stored in the platform. If the
 * value is larger, the counter in the platform must be updated to the new value
 * (provided it has been authenticated).
 *
 * Return: 0 = success, Otherwise = error
 * Returns additionally,
 * cert_nv_ctr -> NV counter value present in the certificate
 * need_nv_ctr_upgrade = 0 -> platform NV counter upgrade is not needed
 * need_nv_ctr_upgrade = 1 -> platform NV counter upgrade is needed
 */
static int auth_nvctr(const auth_method_param_nv_ctr_t *param,
		      const auth_img_desc_t *img_desc,
		      void *img, unsigned int img_len,
		      unsigned int *cert_nv_ctr,
		      bool *need_nv_ctr_upgrade)
{
	unsigned char *p;
	void *data_ptr = NULL;
	unsigned int data_len, len, i;
	unsigned int plat_nv_ctr;
	int rc;
	bool is_trial_run = false;

	VERBOSE("[CoT] Starting NV counter authentication for image ID %u\n", img_desc->img_id);

	/* Get the counter value from current image. The AM expects the IPM
	 * to return the counter value as a DER encoded integer */
	VERBOSE("[CoT] Extracting NV counter from certificate\n");
	rc = img_parser_get_auth_param(img_desc->img_type, param->cert_nv_ctr,
				       img, img_len, &data_ptr, &data_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Extracted NV counter DER data: len=%u\n", data_len);

	/* Parse the DER encoded integer */
	assert(data_ptr);
	p = (unsigned char *)data_ptr;

	/*
	 * Integers must be at least 3 bytes: 1 for tag, 1 for length, and 1
	 * for value.  The first byte (tag) must be ASN1_INTEGER.
	 */
	if ((data_len < 3) || (*p != ASN1_INTEGER)) {
		/* Invalid ASN.1 integer */
		VERBOSE("[CoT] Invalid ASN.1 integer format\n");
		return 1;
	}
	p++;

	/*
	 * NV-counters are unsigned integers up to 31 bits.  Trailing
	 * padding is not allowed.
	 */
	len = (unsigned int)*p;
	if ((len > 4) || (data_len - 2 != len)) {
		VERBOSE("[CoT] Invalid NV counter length: %u\n", len);
		return 1;
	}
	p++;

	/* Check the number is not negative */
	if (*p & 0x80) {
		VERBOSE("[CoT] NV counter appears to be negative\n");
		return 1;
	}

	/* Convert to unsigned int. This code is for a little-endian CPU */
	*cert_nv_ctr = 0;
	for (i = 0; i < len; i++) {
		*cert_nv_ctr = (*cert_nv_ctr << 8) | *p++;
	}
	VERBOSE("[CoT] Parsed certificate NV counter: %u\n", *cert_nv_ctr);

	/* Get the counter from the platform */
	VERBOSE("[CoT] Retrieving platform NV counter\n");
	rc = plat_get_nv_ctr(param->plat_nv_ctr->cookie, &plat_nv_ctr);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Platform NV counter: %u\n", plat_nv_ctr);

	if (*cert_nv_ctr < plat_nv_ctr) {
		/* Invalid NV-counter */
		VERBOSE("[CoT] Certificate NV counter (%u) < platform NV counter (%u) - rollback detected!\n",
			*cert_nv_ctr, plat_nv_ctr);
		return 1;
	} else if (*cert_nv_ctr > plat_nv_ctr) {
		VERBOSE("[CoT] Certificate NV counter (%u) > platform NV counter (%u) - upgrade needed\n",
			*cert_nv_ctr, plat_nv_ctr);
#if PSA_FWU_SUPPORT && IMAGE_BL2
		is_trial_run = fwu_is_trial_run_state();
#endif /* PSA_FWU_SUPPORT && IMAGE_BL2 */
		*need_nv_ctr_upgrade = !is_trial_run;
		if (*need_nv_ctr_upgrade) {
			VERBOSE("[CoT] NV counter upgrade will be performed after authentication\n");
		} else {
			VERBOSE("[CoT] NV counter upgrade skipped (trial run state)\n");
		}
	} else {
		VERBOSE("[CoT] Certificate NV counter matches platform NV counter: %u\n",
			*cert_nv_ctr);
	}

	VERBOSE("[CoT] NV counter authentication successful for image ID %u\n", img_desc->img_id);
	return 0;
}

int plat_set_nv_ctr2(void *cookie, const auth_img_desc_t *img_desc __unused,
		unsigned int nv_ctr)
{
	return plat_set_nv_ctr(cookie, nv_ctr);
}

/*
 * Return the parent id in the output parameter '*parent_id'
 *
 * Return value:
 *   0 = Image has parent, 1 = Image has no parent or parent is authenticated
 */
int auth_mod_get_parent_id(unsigned int img_id, unsigned int *parent_id)
{
	const auth_img_desc_t *img_desc = NULL;

	assert(parent_id != NULL);
	/* Get the image descriptor */
	img_desc = FCONF_GET_PROPERTY(tbbr, cot, img_id);

	/* Check if the image has no parent (ROT) */
	if (img_desc->parent == NULL) {
		*parent_id = 0;
		return 1;
	}

	/* Check if the parent has already been authenticated */
	if (auth_img_flags[img_desc->parent->img_id] & IMG_FLAG_AUTHENTICATED) {
		*parent_id = 0;
		return 1;
	}

	*parent_id = img_desc->parent->img_id;
	return 0;
}

/*
 * Initialize the different modules in the authentication framework
 */
void auth_mod_init(void)
{
	/* Check we have a valid CoT registered */
	assert(cot_desc_ptr != NULL);

	/* Image parser module */
	img_parser_init();
}

/*
 * Authenticate a certificate/image
 *
 * Return: 0 = success, Otherwise = error
 */
int auth_mod_verify_img(unsigned int img_id,
			void *img_ptr,
			unsigned int img_len)
{
	const auth_img_desc_t *img_desc = NULL;
	const auth_param_type_desc_t *type_desc = NULL;
	const auth_method_desc_t *auth_method = NULL;
	void *param_ptr;
	unsigned int param_len;
	int rc, i;
	unsigned int cert_nv_ctr = 0;
	bool need_nv_ctr_upgrade = false;
	bool sig_auth_done = false;
	const auth_method_param_nv_ctr_t *nv_ctr_param = NULL;

	VERBOSE("[CoT] ===== Starting authentication for image ID %u (len=%u) =====\n",
		img_id, img_len);

	/* Get the image descriptor from the chain of trust */
	img_desc = FCONF_GET_PROPERTY(tbbr, cot, img_id);
	if (img_desc == NULL) {
		VERBOSE("[CoT] ERROR: Image descriptor not found for ID %u\n", img_id);
		return 1;
	}
	VERBOSE("[CoT] Image type: %u, Parent ID: %u\n",
		img_desc->img_type,
		img_desc->parent ? img_desc->parent->img_id : 0);

	/* Ask the parser to check the image integrity */
	VERBOSE("[CoT] Checking image integrity\n");
	rc = img_parser_check_integrity(img_desc->img_type, img_ptr, img_len);
	if (rc != 0) {
		VERBOSE("[TBB] %s():%d failed with error code %d.\n",
			__func__, __LINE__, rc);
		return rc;
	}
	VERBOSE("[CoT] Image integrity check passed\n");

	/* Authenticate the image using the methods indicated in the image
	 * descriptor. */
	if (img_desc->img_auth_methods == NULL) {
		VERBOSE("[CoT] ERROR: No authentication methods defined\n");
		return 1;
	}
	VERBOSE("[CoT] Processing authentication methods\n");
	for (i = 0 ; i < AUTH_METHOD_NUM ; i++) {
		auth_method = &img_desc->img_auth_methods[i];
		switch (auth_method->type) {
		case AUTH_METHOD_NONE:
			VERBOSE("[CoT] Authentication method %d: NONE\n", i);
			rc = 0;
			break;
		case AUTH_METHOD_HASH:
			VERBOSE("[CoT] Authentication method %d: HASH\n", i);
			rc = auth_hash(&auth_method->param.hash,
					img_desc, img_ptr, img_len);
			break;
		case AUTH_METHOD_SIG:
			VERBOSE("[CoT] Authentication method %d: SIGNATURE\n", i);
			rc = auth_signature(&auth_method->param.sig,
					img_desc, img_ptr, img_len);
			sig_auth_done = true;
			break;
		case AUTH_METHOD_NV_CTR:
			VERBOSE("[CoT] Authentication method %d: NV_COUNTER\n", i);
			nv_ctr_param = &auth_method->param.nv_ctr;
			rc = auth_nvctr(nv_ctr_param,
					img_desc, img_ptr, img_len,
					&cert_nv_ctr, &need_nv_ctr_upgrade);
			break;
		default:
			/* Unknown authentication method */
			VERBOSE("[CoT] ERROR: Unknown authentication method %d\n",
				auth_method->type);
			rc = 1;
			break;
		}
		if (rc != 0) {
			VERBOSE("[TBB] %s():%d failed with error code %d.\n",
				__func__, __LINE__, rc);
			VERBOSE("[CoT] ===== Authentication FAILED for image ID %u =====\n", img_id);
			return rc;
		}
	}
	VERBOSE("[CoT] All authentication methods passed\n");

	/*
	 * Do platform NV counter upgrade only if the certificate gets
	 * authenticated, and platform NV-counter upgrade is needed.
	 */
	if (need_nv_ctr_upgrade && sig_auth_done) {
		VERBOSE("[CoT] Upgrading platform NV counter to %u\n", cert_nv_ctr);
		rc = plat_set_nv_ctr2(nv_ctr_param->plat_nv_ctr->cookie,
				      img_desc, cert_nv_ctr);
		if (rc != 0) {
			VERBOSE("[TBB] %s():%d failed with error code %d.\n",
				__func__, __LINE__, rc);
			return rc;
		}
		VERBOSE("[CoT] Platform NV counter upgraded successfully to %u\n", cert_nv_ctr);
	}

	/* Extract the parameters indicated in the image descriptor to
	 * authenticate the children images. */
	if (img_desc->authenticated_data != NULL) {
		VERBOSE("[CoT] Extracting authenticated parameters for child images\n");
		for (i = 0 ; i < COT_MAX_VERIFIED_PARAMS ; i++) {
			if (img_desc->authenticated_data[i].type_desc == NULL) {
				continue;
			}

			type_desc = img_desc->authenticated_data[i].type_desc;
			VERBOSE("[CoT] Extracting parameter %d: type=%u\n", i, type_desc->type);

			/* Get the parameter from the image parser module */
			rc = img_parser_get_auth_param(img_desc->img_type,
					img_desc->authenticated_data[i].type_desc,
					img_ptr, img_len, &param_ptr, &param_len);
			if (rc != 0) {
				VERBOSE("[TBB] %s():%d failed with error code %d.\n",
					__func__, __LINE__, rc);
				return rc;
			}
			VERBOSE("[CoT] Extracted parameter %d: len=%u\n", i, param_len);

			/* Check parameter size */
			if (param_len > img_desc->authenticated_data[i].data.len) {
				VERBOSE("[CoT] ERROR: Parameter %d size (%u) exceeds buffer size (%u)\n",
					i, param_len, img_desc->authenticated_data[i].data.len);
				return 1;
			}

			/* Copy the parameter for later use */
			memcpy((void *)img_desc->authenticated_data[i].data.ptr,
					(void *)param_ptr, param_len);
			VERBOSE("[CoT] Copied parameter %d to authenticated data buffer\n", i);

			/*
			 * If this is a public key then measure and publicise
			 * it.
			 */
			if (type_desc->type == AUTH_PARAM_PUB_KEY) {
				VERBOSE("[CoT] Measuring and publishing public key (parameter %d)\n", i);
				rc = plat_mboot_measure_key(type_desc->cookie,
							    param_ptr,
							    param_len);
				if (rc != 0) {
					VERBOSE("[TBB] %s():%d failed with error code %d.\n",
						__func__, __LINE__, rc);
					return rc;
				}
				VERBOSE("[CoT] Public key measured and published successfully\n");
			}
		}
		VERBOSE("[CoT] Finished extracting authenticated parameters\n");
	} else {
		VERBOSE("[CoT] No authenticated data to extract\n");
	}

	/* Mark image as authenticated */
	auth_img_flags[img_desc->img_id] |= IMG_FLAG_AUTHENTICATED;
	VERBOSE("[CoT] Image ID %u marked as authenticated\n", img_desc->img_id);
	VERBOSE("[CoT] ===== Authentication SUCCESSFUL for image ID %u =====\n", img_id);

	return 0;
}
