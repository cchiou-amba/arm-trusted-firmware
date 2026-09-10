/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#ifndef __UART_AMBARELLA_H__
#define __UART_AMBARELLA_H__

#include <drivers/console.h>

#ifndef __ASSEMBLER__

#include <stdint.h>

/*
 * Initialize a new Ambarella console instance and register it with the console
 * framework. The |console| pointer must point to storage that will be valid
 * for the lifetime of the console, such as a global or static local variable.
 * Its contents will be reinitialized from scratch.
 */
int console_ambarella_register(uintptr_t baseaddr, uint32_t clock, uint32_t baud,
			  console_t *console);

#endif /*__ASSEMBLER__*/

#endif	/* __UART_AMBARELLA_H__ */
