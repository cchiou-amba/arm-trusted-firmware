/*
 * Copyright (c) 2025 Ambarella International LP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AMBARELLA_TRNG_H
#define AMBARELLA_TRNG_H

#include <stddef.h>
#include <stdint.h>

/*
 * Fill buffer with entropy from the hardware TRNG. Registers use TRNG_BASE /
 * TRNG_REG from ambarella_def.h (must match optee ambarella_trng.c for the chip).
 *
 * Returns 0 on success, negative on error.
 */
int ambarella_trng_get_bytes(uint8_t *buf, size_t len);

#endif /* AMBARELLA_TRNG_H */
