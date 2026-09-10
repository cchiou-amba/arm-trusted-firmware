/*******************************************************************************
 * curve25519.h
 *
 * History:
 *  2020/06/19 - [Bo-xi Chen] create file
 *
 * Copyright [2020] Ambarella International LP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

#ifndef __CURVE25519_H__
#define __CURVE25519_H__

#include <stdint.h>

#define C25519_KEY_LEN_BYTES 32
#define C25519_SIGNATURE_LEN_BYTES 64

#define C25519_KEY_EXCHANGE_LENGTH 32

#define SHA512_DIGEST_LENGTH 64

#define UL64(x) x##ULL

typedef struct {
    uint64_t H[8];
    uint64_t tot_num_0;
    uint64_t tot_num_1;
    uint8_t cache_buf[128];
} sha512_ctx_t;

/* read big endian */
#define DR_BE64(n, b, i)					\
{								\
	(n) = ((uint64_t) (b)[(i)    ] << 56)			\
	    | ((uint64_t) (b)[(i) + 1] << 48)			\
	    | ((uint64_t) (b)[(i) + 2] << 40)			\
	    | ((uint64_t) (b)[(i) + 3] << 32)			\
	    | ((uint64_t) (b)[(i) + 4] << 24)			\
	    | ((uint64_t) (b)[(i) + 5] << 16)			\
	    | ((uint64_t) (b)[(i) + 6] << 8)			\
	    | ((uint64_t) (b)[(i) + 7]       );			\
}

/* write big endian */
#define DW_BE64(n, b, i)					\
{								\
	(b)[(i)    ] = (uint8_t) ((n) >> 56);			\
	(b)[(i) + 1] = (uint8_t) ((n) >> 48);			\
	(b)[(i) + 2] = (uint8_t) ((n) >> 40);			\
	(b)[(i) + 3] = (uint8_t) ((n) >> 32);			\
	(b)[(i) + 4] = (uint8_t) ((n) >> 24);			\
	(b)[(i) + 5] = (uint8_t) ((n) >> 16);			\
	(b)[(i) + 6] = (uint8_t) ((n) >> 8);			\
	(b)[(i) + 7] = (uint8_t) ((n)       );			\
}

int X25519_memcmp(const void *dst, const void *src, uint32_t n);

int ed25519_sha512_sign(uint8_t *out_sig,
    const uint8_t *message, uint32_t message_len,
    const uint8_t public_key[32], const uint8_t private_key[32]);

int ed25519_sha512_verify(const uint8_t *message, uint32_t message_len,
    const uint8_t *signature, uint32_t signature_len,
    const uint8_t *public_key);

int x25519_gen_public_key(uint8_t public[32], const uint8_t secret[32]);

int x25519_gen_shared_secret(uint8_t out_shared_secret[32],
    const uint8_t private_key[32],
    const uint8_t public_key[32]);

void ed25519_public_from_private(uint8_t out_public_key[32],
    const uint8_t private_key[32]);

#endif
