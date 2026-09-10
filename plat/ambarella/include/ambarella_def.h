/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */

#ifndef __AMBARELLA_DEF_H__
#define __AMBARELLA_DEF_H__

#include <lib/utils_def.h>

/*******************************************************************************
 * Ambarella SoC Bus Base
 ******************************************************************************/
#define DRAM_BASE			0x0000000000

#define DDRC_DEBUG_SUPPORT		(0)

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define DEVICE_BASE			0xE0000000
#define DEVICE_SIZE			0x1FF00000
#elif defined(AMBARELLA_CV5)
#define DEVICE_BASE			0x1000000000
#define DEVICE_SIZE			0x2000000000	/* SIZE: 128GB, including DRAMC_BASE */
#elif defined(AMBARELLA_CV72)
#define DEVICE_BASE			0xFE00000000
#define DEVICE_SIZE			0x0200000000	/* SIZE: 8GB */
#elif defined(AMBARELLA_N1) || defined(AMBARELLA_CV75)
#define DEVICE_BASE			0xFF00000000
#define DEVICE_SIZE			0x0100000000	/* SIZE: 4GB */
#elif defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV8)
#define DEVICE_BASE			0xFC00000000
#define DEVICE_SIZE			0x0400000000	/* SIZE: 16GB */
#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define DEVICE_BASE			0xFD00000000
#define DEVICE_SIZE			0x0300000000	/* SIZE: 12GB */
#elif defined(AMBARELLA_CV7)
#define DEVICE_BASE			0x2000000000
#define DEVICE_SIZE			0x2000000000	/* SIZE: 64GB */
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define DRAMC_BASE			0xDFFE0000
#define DRAMC_SIZE			0x00020000
#elif defined(AMBARELLA_CV5)
#define DRAMC_BASE			0x1000000000
#define DRAMC_SIZE			0x0000200000
#elif defined(AMBARELLA_CV7)
#define DRAMC_BASE			0x3000000000
#define DRAMC_SIZE			0x0000200000	/* expand to 2MB, easy map */
#elif defined(AMBARELLA_CV8)
#define DRAMC_MAIN_BASE			0xFF10000000	/* main: all except ddrc */
#define DRAMC_BASE			0xFC08000000	/* mini: ddrc is here */
#define DRAMC_SIZE			0x0001000000
#else
#define DRAMC_BASE			0xFF08000000
#define DRAMC_SIZE			0x0001000000
#endif

#if defined(AMBARELLA_CV8)
#define DRAMC_REG(x)			(DRAMC_MAIN_BASE + (x))
#else
#define DRAMC_REG(x)			(DRAMC_BASE + (x))
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define AXI_BASE			0xF2000000
#define AXI_SIZE			0x00001000
#elif defined(AMBARELLA_CV5) || defined(AMBARELLA_CV7)
#define AXI_BASE			0x20F2000000
#define AXI_SIZE			0x00001000
#else
#define AXI_BASE			0xFFF3000000
#define AXI_SIZE			0x00001000
#endif
#define AXI_REG(x)			(AXI_BASE + (x))

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define GIC_BASE			0xF3000000
#elif defined(AMBARELLA_CV5) || defined(AMBARELLA_CV7)
#define GIC_BASE			0x20F3000000
#elif defined(AMBARELLA_CV8)
#if ARM_ARCH_AT_LEAST(8, 2)  //CA78 Primary Cluster
#define GIC_BASE			0xFF09100000
#else  //CA53 Secondary Cluster
#define GIC_BASE			0xFCF0100000
#endif  //ARM_ARCH_AT_LEAST(8, 2)
#else
#define GIC_BASE			0xFFF0100000
#endif
#define GICD_BASE			(GIC_BASE + 0x1000)
#define GICC_BASE			(GIC_BASE + 0x2000)
#define GICH_BASE			(GIC_BASE + 0x4000)
#define GICV_BASE			(GIC_BASE + 0x6000)

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define SRAM_BASE			0xE8020000
#define SRAM_SIZE			0x00001000
#elif defined(AMBARELLA_CV5)
#define SRAM_BASE			0x20E0030000
#define SRAM_SIZE			0x00004000
#elif defined(AMBARELLA_N1)
#define SRAM_BASE			0xFFE0030000
#define SRAM_SIZE			0x00004000
#elif defined(AMBARELLA_CV7)
#define SRAM_BASE			0x20F4010000
#define SRAM_SIZE			0x00004000
#elif defined(AMBARELLA_CV8)
#define SRAM_BASE			0xFFF3010000
#define SRAM_SIZE			0x00008000
#else
#define SRAM_BASE			0xFFE0030000
#define SRAM_SIZE			0x00008000
#endif

#if defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75) || defined(AMBARELLA_CV8)
#define BOOT_COOKIE_REG		(SRAM_BASE + 0x7F40)
#elif defined(AMBARELLA_CV7)
#define BOOT_COOKIE_REG		(0x20F4020000 + 0x3F40)
#else
#define BOOT_COOKIE_REG		0x0
#endif

/*******************************************************************************
 * Ambarella SoC Module Reg Base
 ******************************************************************************/

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define PERI_BASE			0x0000000000
#elif defined(AMBARELLA_CV5) || defined(AMBARELLA_CV7)
#define PERI_BASE			0x2000000000
#else
#define PERI_BASE			0xFF00000000
#endif

#define NIC_GPV_BASE			(PERI_BASE + 0xF1000000)
#define NIC_GPV_REG(x)			(NIC_GPV_BASE + (x))

#if defined(AMBARELLA_CV8)
#define NIC_GPV_PWI_BASES		0xFCF0000000
#define NIC_GPV_PWI_REG(x)		(NIC_GPV_PWI_BASES + (x))
#endif

#if defined(AMBARELLA_CV8)
#define PWI_BASE			0xFC00000000
#define RCT_BASE			(PWI_BASE + 0xED000000)
#else
#define RCT_BASE			(PERI_BASE + 0xED080000)
#endif
#define RCT_REG(x)			(RCT_BASE + (x))

#define FIO_BASE			(PERI_BASE + 0xE0002000)
#define FIO_REG(x)			(FIO_BASE + (x))

#if defined(AMBARELLA_CV5) || defined(AMBARELLA_N1) || defined(AMBARELLA_CV28)
#define FIO_4K_BASE			(PERI_BASE + 0xE000C000)
#elif defined(AMBARELLA_CV8)
#define FIO_4K_BASE			(PERI_BASE + 0xE0030000)
#else
#define FIO_4K_BASE			(PERI_BASE + 0xE0003000)
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define EMMC0_BASE			(PERI_BASE + 0xE0004000)
#elif defined(AMBARELLA_CV5)
#define EMMC0_BASE			(PERI_BASE + 0xE0003000)
#elif defined(AMBARELLA_CV7)
#define EMMC0_BASE			(PERI_BASE + 0x70000000)
#else
#define EMMC0_BASE			(PERI_BASE + 0xF2000000)
#endif
#define EMMC0_REG(x)			(EMMC0_BASE + (x))

#define SPINOR_BASE			(PERI_BASE + 0xE0001000)
#define SPINOR_REG(x)			(SPINOR_BASE + (x))

#if defined(AMBARELLA_CV2)
#define DMA0_BASE			(PERI_BASE + 0xE000A000)
#else
#define DMA0_BASE			(PERI_BASE + 0xE0020000)
#endif
#define DMA0_REG(x)			(DMA0_BASE + (x))

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define GPIO0_BASE			(PERI_BASE + 0xEC003000)
#define GPIO1_BASE			(PERI_BASE + 0xEC004000)
#define GPIO2_BASE			(PERI_BASE + 0xEC005000)
#define GPIO3_BASE			(PERI_BASE + 0xEC006000)
#define GPIO4_BASE			(PERI_BASE + 0xEC007000)
#define GPIO5_BASE			(PERI_BASE + 0xEC008000)
#define GPIO6_BASE			(PERI_BASE + 0xEC009000)
#define IOMUX_BASE			(PERI_BASE + 0xEC000000)
#else
#define GPIO0_BASE			(PERI_BASE + 0xE4013000)
#define GPIO1_BASE			(PERI_BASE + 0xE4014000)
#define GPIO2_BASE			(PERI_BASE + 0xE4015000)
#define GPIO3_BASE			(PERI_BASE + 0xE4016000)
#define GPIO4_BASE			(PERI_BASE + 0xE4017000)
#define GPIO5_BASE			(PERI_BASE + 0xE4018000)
#define GPIO6_BASE			(PERI_BASE + 0xE4019000)
#define IOMUX_BASE			(PERI_BASE + 0xE4010000)
#endif

#define GPIO_BANK			7
#define NRGPIO_PER_BANK			32

#define UART0_BASE			(PERI_BASE + 0xE4000000)

#if defined(AMBARELLA_CV5) || defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7)
#define PWC_BASE			(PERI_BASE + 0xE002F000)
#elif defined(AMBARELLA_N1) || defined(AMBARELLA_CV72) || defined(AMBARELLA_CV3AD685)
#define PWC_BASE			(PERI_BASE + 0xE002E000)
#elif defined(AMBARELLA_CV75)
#define PWC_BASE			(PERI_BASE + 0xED0D0000)
#elif defined(AMBARELLA_CV2)
#define PWC_BASE			(PERI_BASE + 0xEC001000)
#elif defined(AMBARELLA_CV8)		/* NO PWC/RTC  */
#define PWC_BASE			(PERI_BASE + 0xFFFFFFFF)
#else
#define PWC_BASE			(PERI_BASE + 0xE8001000)
#endif
#define PWC_REG(x)			(PWC_BASE + (x))

#if defined(AMBARELLA_CV5) || defined(AMBARELLA_N1) || defined(AMBARELLA_CV7)
#define SCRATCHPAD_BASE			(PERI_BASE + 0xE0024000)
#elif defined(AMBARELLA_CV72)
#define SCRATCHPAD_BASE			(PERI_BASE + 0xE003E000)
#elif defined(AMBARELLA_CV2)
#define SCRATCHPAD_BASE			(PERI_BASE + 0xE8001000)
#elif defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define SCRATCHPAD_BASE			(PERI_BASE + 0xE003F000)
#elif defined(AMBARELLA_CV75) || defined(AMBARELLA_CV8)
#define SCRATCHPAD_BASE			(PERI_BASE + 0xE004E000)
#define PWI_SCRATCHPAD_BASE		0xFCE0000000
#else
#define SCRATCHPAD_BASE			(PERI_BASE + 0xE0022000)
#endif
#define SCRATCHPAD_REG(x)		(SCRATCHPAD_BASE + (x))

#if defined(AMBARELLA_CV8)
#define PWI_SCRATCHPAD_REG(x)		(PWI_SCRATCHPAD_BASE + (x))
#endif

#if defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV8)
#define	OTP_CONFIG_BASE			(PERI_BASE + 0xE002A000)
#define OTP_CONFIG_REG(x)		(OTP_CONFIG_BASE + (x))
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define SECURE_SCRATCHPAD_BASE		(PERI_BASE + 0xE8001000)
#elif defined(AMBARELLA_CV5) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7) || defined(AMBARELLA_CV8)
#define SECURE_SCRATCHPAD_BASE		(PERI_BASE + 0xE002F000)
#else
#define SECURE_SCRATCHPAD_BASE		(PERI_BASE + 0xE002E000)
#endif
#define SECURE_SCRATCHPAD_REG(x)	(SECURE_SCRATCHPAD_BASE + (x))

#if defined(AMBARELLA_CV7) || defined(AMBARELLA_CV5) || \
	defined(AMBARELLA_N1) || defined(AMBARELLA_CV72) || \
	defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV75) || \
	defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655) || \
	defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || \
	defined(AMBARELLA_CV25) || defined(AMBARELLA_S6LM) || \
	defined(AMBARELLA_CV28) || defined(AMBARELLA_CV8)
#define TRNG_BASE			(SECURE_SCRATCHPAD_BASE)
#define TRNG_REG(x)			(TRNG_BASE + (x))
#else
#define TRNG_BASE			(0UL)
#define TRNG_REG(x)			(0UL)
#endif

#if defined(AMBARELLA_N1) \
	|| defined(AMBARELLA_CV72) \
	|| defined(AMBARELLA_CV3AD685) \
	|| defined(AMBARELLA_CV75) \
	|| defined(AMBARELLA_CV3AD655) \
	|| defined(AMBARELLA_CV8)
#define TIMER_OFFSET		0xE4004000
#else
#define TIMER_OFFSET		0xE4005000
#endif

#define TIMER_BASE		(PERI_BASE + TIMER_OFFSET)
#define TIMER_REG(x)		(TIMER_BASE + x)

/*******************************************************************************
 * Ambarella platform constants
 ******************************************************************************/

/*
 * Security Definition
 */
#if defined(AMBARELLA_CV2)
#define AMBARELLA_SUPPORT_SEURITY_CTRL	0
#else
#define AMBARELLA_SUPPORT_SEURITY_CTRL	1
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5)
#define AMBARELLA_SUPPORT_AST		0
#else
#define AMBARELLA_SUPPORT_AST		1
#endif

#if (AMBARELLA_SUPPORT_AST == 0)
#if defined(AMBARELLA_CV5)
#define DRAM_ACCESS_VIRTUAL_OFFSET	0x400
#define DRAM_SECMEM_CTRL_OFFSET		0x304
#define DRAM_SECMEM_BASE_OFFSET		0x31C
#define DRAM_SECMEM_LIMIT_OFFSET	0x320
#define DRAM_VPN_BASE_OFFSET		0x40C
#define DRAM_VPN_BOUND_OFFSET		0x48C
#define DRAM_ATT_OFFSET			0x10000
#else
#define DRAM_ACCESS_VIRTUAL_OFFSET	0x214
#define DRAM_SECMEM_CTRL_OFFSET		0x218
#define DRAM_SECMEM_BASE_OFFSET		0x248
#define DRAM_SECMEM_LIMIT_OFFSET	0x24c
#define DRAM_VPN_BASE_OFFSET		0x300
#define DRAM_VPN_BOUND_OFFSET		0x380
#define DRAM_ATT_OFFSET			0x8000
#endif
#else
#define AST_SEG_LIMIT_OFFSET		0x5600
#if defined(AMBARELLA_N1) || defined(AMBARELLA_CV72)
#define AST_SEG_RELOC_OFFSET		0x5740
#define AST_SEG_RIGHT_OFFSET		0x5880
#elif defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV75) || \
	defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655) || \
	defined(AMBARELLA_CV7) || defined(AMBARELLA_CV8)
#define AST_SEG_RELOC_OFFSET		0x5780
#define AST_SEG_RIGHT_OFFSET		0x5900
#else
#error "Wrong AST define!!!"
#endif
#endif

#if defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) ||\
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV8)
#define AMBARELLA_NEED_SHMEM_INIT
#endif

/*
 * PLL Register Layout Version
 * 1: dividers in CTRL2 (clkpll-v0/v1)
 * 2: dividers in CTRL  (clkpll-v2/v3, e.g. CV7/CV8)
 */
#if defined(AMBARELLA_CV7) || defined(AMBARELLA_CV8)
#define PLL_VERSION			2
#else
#define PLL_VERSION			1
#endif

/*
 * DDRC Register Definition
 */

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define DDRC_VERSION			1
#define DDRC_HOST_FIRST(x)		0
#define DDRC_HOST_NUM(x)		(((x) & 0x1) ? 2 : 1)
#define DDRC_DIE_NUM(x)			1	/* unsupport dual die */
#define DDRC_REG(x, y)			(DRAMC_BASE + 0x00800 + (x) * 0x0200 + (y))
#define DDRCB				0x0600
#define DDRC0				0x0800
#define DDRC1				0x0A00
#define DDRC_SRS_CTRL_STATUS		0x0020
#elif defined(AMBARELLA_CV5)
#define DDRC_VERSION			2
#define DDRC_HOST_FIRST(x)		((((x) & 0x2) == 0x2) ? 0 : 1)
#define DDRC_HOST_NUM(x)		((((x) & 0x6) == 0x6) ? 2 : 1)
#define DDRC_DIE_NUM(x)			(((x) & BIT(26)) ? 2 : 1)
#define DDRC_REG(x, y)			(DRAMC_BASE + 0x05000 + (x) * 0x1000 + (y))
#define DDRCB				0x4000
#define DDRC0				0x5000
#define DDRC1				0x6000
#define DDRC_SRS_CTRL_STATUS		0x03a4
#define DDRC_IO_CTRL			0xd0
#define DDRC_TYPE			0x4
#else
#define DDRC_VERSION			3	/* inc version different from CV5 */
#define DDRC_HOST_FIRST(x)		(((x) & 0xf) / 2)		/* on dramc */
#define DDRC_HOST_NUM(x)		(((((x) >> 4) & 0xf) + 1) / 2)	/* on dramc */
#define DDRC_DIE_NUM(x)			(((x) & BIT(26)) ? 2 : 1)	/* on ddrc */
#define DDRC_REG(x, y)			(DRAMC_BASE + 0x14000 + (x) * 0xc000 + (y))
#define DDRCB				0x08000
#define DDRC0				0x14000
#if defined(AMBARELLA_N1) || defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || \
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7) || defined(AMBARELLA_CV8)
#define DDRC1				(0x14000 + 0xc000)
#define DDRC2				(DDRC1 + 0xC000)
#define DDRC3				(DDRC2 + 0xC000)
#else
#define DDRC1				0x14000
#endif
#define DRAM_DTTE_CONFIG3		0x0194
#define DDRC_SRS_CTRL_STATUS		0x0608
#define DDRC_IO_CTRL			0xd0	/* on ddrc */
#define DDRC_TYPE			0x4
#endif

/*
 * AXI Register Definition
 */
#define CORTEX_RESET_OFFSET		0x28
#define CORTEX_RESET_REG		AXI_REG(CORTEX_RESET_OFFSET)
#if defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || \
	defined(AMBARELLA_CV25) || defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5)
#define CORTEX_RVBARADDR0_OFFSET	0x64
#define CORTEX_RVBARADDR1_OFFSET	0x68
#define CORTEX_RVBARADDR2_OFFSET	0x6c
#define CORTEX_RVBARADDR3_OFFSET	0x70
#elif defined(AMBARELLA_N1)
#define CORTEX_RVBARADDR0_OFFSET	0x48
#define CORTEX_RVBARADDR1_OFFSET	0x4c
#define CORTEX_RVBARADDR2_OFFSET	0x50
#define CORTEX_RVBARADDR3_OFFSET	0x54
#else
#define CORTEX_RVBARADDR0_OFFSET	0x48	/* 64-bit */
#define CORTEX_RVBARADDR1_OFFSET	0x50	/* 64-bit */
#define CORTEX_RVBARADDR2_OFFSET	0x58	/* 64-bit */
#define CORTEX_RVBARADDR3_OFFSET	0x60	/* 64-bit */
#endif

#if defined(AMBARELLA_CV5) || defined(AMBARELLA_N1)
#define CORTEX_RVBAR_ADDR(x)		((x) >> 8)
#else
#define CORTEX_RVBAR_ADDR(x)		((x) >> 0)
#endif

#if defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || \
	defined(AMBARELLA_CV25) || defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5)
#define CORTEX_CORE0_RESET_MASK		(1 << 16)
#define CORTEX_CORE1_RESET_MASK		(1 << 17)
#define CORTEX_CORE2_RESET_MASK		(1 << 18)
#define CORTEX_CORE3_RESET_MASK		(1 << 19)
#elif defined(AMBARELLA_N1)
#define CORTEX_CORE0_RESET_MASK		(1 << 3)
#define CORTEX_CORE1_RESET_MASK		(1 << 4)
#define CORTEX_CORE2_RESET_MASK		(1 << 5)
#define CORTEX_CORE3_RESET_MASK		(1 << 6)
#else
#define CORTEX_CORE0_RESET_MASK		(1 << 2)
#define CORTEX_CORE1_RESET_MASK		(1 << 3)
#define CORTEX_CORE2_RESET_MASK		(1 << 4)
#define CORTEX_CORE3_RESET_MASK		(1 << 5)
#endif

#if defined(AMBARELLA_N1)
#define CORTEX_RESET_MASK(id)		((0xf << ((id) * 7 + 3)))
#else
#define CORTEX_RESET_MASK(id)		((0x1f << ((id) * 7 + 2)))
#endif

#define CORTEX_RVBARADDR0_REG		AXI_REG(CORTEX_RVBARADDR0_OFFSET)
#define CORTEX_RVBARADDR1_REG		AXI_REG(CORTEX_RVBARADDR1_OFFSET)
#define CORTEX_RVBARADDR2_REG		AXI_REG(CORTEX_RVBARADDR2_OFFSET)
#define CORTEX_RVBARADDR3_REG		AXI_REG(CORTEX_RVBARADDR3_OFFSET)

#if defined(AMBARELLA_N1)
#define NX_RVBARADDR_IS_64BIT		0
#define NX_RVBARADDR(H,L)		((H) << 8)
#define NX_RVBARADDR0_REG(n)		(CORTEX_RVBARADDR0_REG + (n << 4))
#define NX_RVBARADDR1_REG(n)		(CORTEX_RVBARADDR1_REG + (n << 4))
#define NX_RVBARADDR2_REG(n)		(CORTEX_RVBARADDR2_REG + (n << 4))
#define NX_RVBARADDR3_REG(n)		(CORTEX_RVBARADDR3_REG + (n << 4))
#else
#define NX_RVBARADDR_IS_64BIT		1
#define NX_RVBARADDR(H,L)		(((H) << 32) | (L))
#define NX_RVBARADDR0_REG(n)		(CORTEX_RVBARADDR0_REG + (n << 5))
#define NX_RVBARADDR1_REG(n)		(CORTEX_RVBARADDR1_REG + (n << 5))
#define NX_RVBARADDR2_REG(n)		(CORTEX_RVBARADDR2_REG + (n << 5))
#define NX_RVBARADDR3_REG(n)		(CORTEX_RVBARADDR3_REG + (n << 5))
#endif

#if defined(AMBARELLA_N1)
#define CORTEX_CLUSTER_NUM		4
#elif defined(AMBARELLA_CV3AD685)
#define CORTEX_CLUSTER_NUM		3
#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define CORTEX_CLUSTER_NUM		2
#else
#define CORTEX_CLUSTER_NUM		1
#endif

#define CLUSTER_CORE_NUM		4

#if defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) ||\
	defined(AMBARELLA_N1_655) || defined(AMBARELLA_CV7) || defined(AMBARELLA_CV8)
#define AXI_SEC0_OFFSET		0xd0
#define AXI_SEC1_OFFSET		0xd4
#define AXI_SEC2_OFFSET		0xd8
#define AXI_SEC3_OFFSET		0xdc
#define AXI_SEC4_OFFSET		0xe0
#else
#define AXI_SEC0_OFFSET		0x90
#define AXI_SEC1_OFFSET		0x94
#define AXI_SEC2_OFFSET		0x98
#define AXI_SEC3_OFFSET		0xa0
#define AXI_SEC4_OFFSET		0x9c	/* invalid */
#endif
#define AXI_SEC_CTRL_NUM	5
#define AXI_CFG_SEC0_REG	AXI_REG(AXI_SEC0_OFFSET)
#define AXI_CFG_SEC1_REG	AXI_REG(AXI_SEC1_OFFSET)
#define AXI_CFG_SEC2_REG	AXI_REG(AXI_SEC2_OFFSET)
#define AXI_CFG_SEC3_REG	AXI_REG(AXI_SEC3_OFFSET)
#define AXI_CFG_SEC4_REG	AXI_REG(AXI_SEC4_OFFSET)

#if defined(AMBARELLA_CV5)
#define CORTEX_BOOT_SECONDARY_WORKAROUND	1
#endif

#if defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75)
#define CORTEX_RESET_SECONDARY_WORKAROUND	1
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28)
#define NIC_GPV_MASTER_PORT		4
#define NIC_GPV_SLAVE_PORT_AXI		0x10
#else
#define NIC_GPV_MASTER_PORT		63 /* Just simply program all slave ports" */
#define NIC_GPV_SLAVE_PORT_AXI		0xc
#endif

#if defined(AMBARELLA_CV8)
#define NIC_GPV_PWI_PORTS		6
#else
#define NIC_GPV_PWI_PORTS		0
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25)
#define AXI_SYS_TIMER_DIVISOR		16
#define AXI_SYS_TIMER_INDEPENDENT	0
#elif defined(AMBARELLA_CV8)
#define AXI_SYS_TIMER_DIVISOR		8
#define AXI_SYS_TIMER_INDEPENDENT	1
#else
#define AXI_SYS_TIMER_DIVISOR		12
#define AXI_SYS_TIMER_INDEPENDENT	1
#endif

/*
 * RCT Register Definition
 */
#define PLL_CORE_CTRL_OFFSET		0x00
#define PLL_CORE_CTRL2_OFFSET		0x100
#define SYS_CONFIG_OFFSET		0x34
#define CG_UART_OFFSET			0x38
#define ANA_PWR_OFFSET			0x50
#define SOFT_OR_DLL_RESET_OFFSET	0x68
#if defined(AMBARELLA_CV8)
#define SOFT_OR_DLL_RESET_VALUE		0x3
#else
#define SOFT_OR_DLL_RESET_VALUE		0x1
#endif
#define CLUSTER_SOFT_RESET_OFFSET	0x228
#define RCT_TIMER_OFFSET		0x254
#define RCT_TIMER_CTRL_OFFSET		0x258
#define PLL_CORTEX_CTRL_OFFSET		0x264
#define PLL_CORTEX_CTRL2_OFFSET 	0x26C
#define PLL_ENET_CTRL_OFFSET		0x520
#define PLL_ENET_CTRL2_OFFSET		0x528
#if defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || \
	defined(AMBARELLA_CV25) || defined(AMBARELLA_CV28)
#define PLL_DDR_CTRL_OFFSET		0xDC	/* ddrc-freq */
#define PLL_DDR_CTRL2_OFFSET		0x110
#else
#define PLL_DDR_CTRL_OFFSET 		0x000
#define PLL_DDR_CTRL2_OFFSET		0x008
#endif

/*
 * DDRH Register Definition
 */
#if defined(AMBARELLA_CV5)
#define DDRH0_REG_BASE  		(PERI_BASE + 0xED180000)
#define DDRH1_REG_BASE  		(PERI_BASE + 0xED190000)
#elif defined(AMBARELLA_CV8)
#define DDRH0_REG_BASE  		(PWI_BASE + 0xED190000)
#define DDRH1_REG_BASE  		(PWI_BASE + 0xED1a0000)

#define DDRH_STA_IN_OFFSET		0x060
#define DDRH_UPDATE_EN_OFFSET		0x064
#define DDRH_STA_OUT_OFFSET		0x090
#define DDRH_KEY_IN0_REG_OFFSET	0x040
#define DDRH_KEY_IN1_REG_OFFSET	0x044
#define DDRH_KEY_IN2_REG_OFFSET	0x048
#define DDRH_KEY_IN3_REG_OFFSET	0x04c
#define DDRH_KEY_IN4_REG_OFFSET	0x050
#define DDRH_KEY_IN5_REG_OFFSET	0x054
#define DDRH_KEY_IN6_REG_OFFSET	0x058
#define DDRH_KEY_IN7_REG_OFFSET	0x05c
#define DDRH_KEY_OUT0_REG_OFFSET	0x070
#define DDRH_KEY_OUT1_REG_OFFSET	0x074
#define DDRH_KEY_OUT2_REG_OFFSET	0x078
#define DDRH_KEY_OUT3_REG_OFFSET	0x07c
#define DDRH_KEY_OUT4_REG_OFFSET	0x080
#define DDRH_KEY_OUT5_REG_OFFSET	0x084
#define DDRH_KEY_OUT6_REG_OFFSET	0x088
#define DDRH_KEY_OUT7_REG_OFFSET	0x08c

#define DDRH0_SRS_REG			DDRH0_REG(DDRH_STA_OUT_OFFSET)
#define DDRH1_SRS_REG			DDRH1_REG(DDRH_STA_OUT_OFFSET)
#else
#define DDRH0_REG_BASE  		(PERI_BASE + 0xED190000)
#define DDRH1_REG_BASE  		(PERI_BASE + 0xED1a0000)
#endif
#define DDRH0_REG(x) 			(DDRH0_REG_BASE + (x))
#define DDRH1_REG(x) 			(DDRH1_REG_BASE + (x))

#if defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || \
	defined(AMBARELLA_CV25) || defined(AMBARELLA_CV28)
#define POC_PERIPHERAL_CLK_MODE		0x00000200
#else
#define POC_PERIPHERAL_CLK_MODE		0x04000000
#endif

/* Boot Media */
#define POC_BOOT_FROM_SPINOR		0x00000000
#define POC_BOOT_FROM_NAND		0x00000010
#define POC_BOOT_FROM_EMMC		0x00000020
#define POC_BOOT_FROM_PCIE		0x00000030
#define POC_BOOT_FROM_MASK		0x00000030

#define SYS_CONFIG_SECURE_BOOT		0x00000040
#define SYS_CONFIG_USB_BOOT		0x00000400

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25)
#define SYS_CONFIG_NAND_SPINAND		0x00400000
#define SYS_CONFIG_NAND_SCKMODE		0x00080000
#define SYS_CONFIG_NAND_4K_FIFO		0xffffffff /* not used */
#define SYS_CONFIG_NAND_8K_FIFO		0x00000000 /* not used */
#define SYS_CONFIG_NAND_32K_FIFO	0x00000000 /* not used */
#define SYS_CONFIG_NAND_PAGE_SIZE	0x00040000
#define SYS_CONFIG_NAND_READ_CONFIRM	0xffffffff /* not used */
#define SYS_CONFIG_NAND_ECC_BCH_EN	0x00010000
#define SYS_CONFIG_NAND_ECC_SPARE_2X	0x00008000
#elif defined(AMBARELLA_S6LM)
#define SYS_CONFIG_NAND_SPINAND		0xffffffff /* not used, spinand only */
#define SYS_CONFIG_NAND_SCKMODE		0x00080000
#define SYS_CONFIG_NAND_4K_FIFO		0xffffffff /* not used */
#define SYS_CONFIG_NAND_8K_FIFO		0x00000000 /* not used */
#define SYS_CONFIG_NAND_32K_FIFO	0x00000000 /* not used */
#define SYS_CONFIG_NAND_PAGE_SIZE	0x00040000
#define SYS_CONFIG_NAND_READ_CONFIRM	0xffffffff /* not used */
#define SYS_CONFIG_NAND_ECC_BCH_EN	0x00010000
#define SYS_CONFIG_NAND_ECC_SPARE_2X	0x00008000
#elif defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5) || defined(AMBARELLA_N1)
#define SYS_CONFIG_NAND_SPINAND		0xffffffff /* not used, spinand only */
#define SYS_CONFIG_NAND_SCKMODE		0x00040000
#define SYS_CONFIG_NAND_4K_FIFO		0xffffffff /* not used */
#define SYS_CONFIG_NAND_8K_FIFO		0x00100000
#define SYS_CONFIG_NAND_32K_FIFO	0x00000000 /* not used */
#define SYS_CONFIG_NAND_PAGE_SIZE	0x00020000
#define SYS_CONFIG_NAND_READ_CONFIRM	0xffffffff /* not used */
#define SYS_CONFIG_NAND_ECC_BCH_EN	0x00008000
#define SYS_CONFIG_NAND_ECC_SPARE_2X	0x00004000
#elif defined(AMBARELLA_CV8)
#define SYS_CONFIG_NAND_SPINAND		0xffffffff /* not used, spinand only */
#define SYS_CONFIG_NAND_SCKMODE		0x00040000
#define SYS_CONFIG_NAND_4K_FIFO		0xffffffff /* not used */
#define SYS_CONFIG_NAND_8K_FIFO		0x00000000
#define SYS_CONFIG_NAND_32K_FIFO	0x00080000
#define SYS_CONFIG_NAND_PAGE_SIZE	0x00020000 /* set 17:2K;else 4K */
#define SYS_CONFIG_NAND_READ_CONFIRM	0xffffffff /* not used */
#define SYS_CONFIG_NAND_ECC_BCH_EN	0x00008000
#define SYS_CONFIG_NAND_ECC_SPARE_2X	0x00004000
#else
#define SYS_CONFIG_NAND_SPINAND		0xffffffff /* not used, spinand only */
#define SYS_CONFIG_NAND_SCKMODE		0x00040000
#define SYS_CONFIG_NAND_4K_FIFO		0xffffffff /* not used */
#define SYS_CONFIG_NAND_8K_FIFO		0x00080000
#define SYS_CONFIG_NAND_32K_FIFO	0x00100000
#define SYS_CONFIG_NAND_PAGE_SIZE	0x00020000
#define SYS_CONFIG_NAND_READ_CONFIRM	0xffffffff /* not used */
#define SYS_CONFIG_NAND_ECC_BCH_EN	0x00008000
#define SYS_CONFIG_NAND_ECC_SPARE_2X	0x00004000
#endif

#if defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || defined(AMBARELLA_S6LM)
#define SYS_CONFIG_MMC_HS		0x00004000
#define SYS_CONFIG_MMC_4BIT		0x00010000
#define SYS_CONFIG_MMC_8BIT		0x00008000
#else
#define SYS_CONFIG_MMC_HS		0x00002000	/* not used on CV7/CV8 */
#define SYS_CONFIG_MMC_4BIT		0x00008000
#define SYS_CONFIG_MMC_8BIT		0x00004000
#endif

#if defined(AMBARELLA_CV2)
#define CLUSTER_SOFT_RESET_VP		0x00000780
#elif defined(AMBARELLA_CV5)
#define CLUSTER_SOFT_RESET_VP		0x00000300
#elif defined(AMBARELLA_N1)
#define CLUSTER_SOFT_RESET_VP		0x01fe0000
#elif defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75)
#define CLUSTER_SOFT_RESET_VP		0x00000400
#elif defined(AMBARELLA_CV3AD685)
#define CLUSTER_SOFT_RESET_VP		0x0007C000
#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define CLUSTER_SOFT_RESET_VP		0x200
#elif defined(AMBARELLA_CV7) || defined(AMBARELLA_CV8)
#define CLUSTER_SOFT_RESET_VP		0x2000
#else
#define CLUSTER_SOFT_RESET_VP		0x00000700
#endif

/* ==========================================================================*/

/*
 * ScratchPad Register Definition
 */
#if defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV22) || defined(AMBARELLA_CV25) || \
	defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5) || defined(AMBARELLA_CV7)
#define AHBSP_DMA0_SEL0_OFFSET		0x50
#elif defined(AMBARELLA_CV2)
#define AHBSP_DMA0_SEL0_OFFSET		0x30
#else
#define AHBSP_DMA0_SEL0_OFFSET		0x2C
#endif

#if defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV2) || defined(AMBARELLA_CV22) || \
	defined(AMBARELLA_CV25) || defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5) || defined(AMBARELLA_CV7)
#define AHBSP_DMA0_SEL0_REG		SCRATCHPAD_REG(AHBSP_DMA0_SEL0_OFFSET)
#else
#define AHBSP_DMA0_SEL0_REG		SECURE_SCRATCHPAD_REG(AHBSP_DMA0_SEL0_OFFSET)
#endif

#if defined(AMBARELLA_N1) || defined(AMBARELLA_CV72) || defined(AMBARELLA_CV3AD685) || \
	defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define AHBSP_PWC_STROBE_OFFSET		0x98
#define SECSP_BOOT_STS_OFFSET		0x9C
#elif defined(AMBARELLA_CV75)
#define AHBSP_PWC_STROBE_OFFSET		0x1ac
#define SECSP_BOOT_STS_OFFSET		0x9C
#elif defined(AMBARELLA_CV8)
#define AHBSP_PWC_STROBE_OFFSET		-1	/* on pwc on cv8 */
#define SECSP_BOOT_STS_OFFSET		0x9C
#else
#define AHBSP_PWC_STROBE_OFFSET		0x90
#define SECSP_BOOT_STS_OFFSET		0x94
#endif

#if defined(AMBARELLA_CV2)
#define AHBSP_JTAG_EN_OFFSET		0x68
#elif defined(AMBARELLA_CV8)
#define AHBSP_JTAG_EN_OFFSET		0x70
#else
#define AHBSP_JTAG_EN_OFFSET		0x18
#endif

#if defined(AMBARELLA_CV8)
#define AHBSP_JTAG_EN_REG		PWI_SCRATCHPAD_REG(AHBSP_JTAG_EN_OFFSET)
#else
#define AHBSP_JTAG_EN_REG		SECURE_SCRATCHPAD_REG(AHBSP_JTAG_EN_OFFSET)
#endif

#if defined(AMBARELLA_CV2)
#define AHBSP_DATA0_OFFSET		0x18
#define AHBSP_DATA1_OFFSET		0x1C
#define AHBSP_DATA2_OFFSET		0x20
#define AHBSP_DATA3_OFFSET		0x24

#elif defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75) || defined(AMBARELLA_CV8)
#define AHBSP_DATA0_OFFSET		0x106C
#define AHBSP_DATA1_OFFSET		0x1070
#define AHBSP_DATA2_OFFSET		0x1074
#define AHBSP_DATA3_OFFSET		0x1078
#else
#define AHBSP_DATA0_OFFSET		0x6C
#define AHBSP_DATA1_OFFSET		0x70
#define AHBSP_DATA2_OFFSET		0x74
#define AHBSP_DATA3_OFFSET		0x78
#endif

/*
 * UART PINs and Register Definition
 */

#define UART_CLOCK			24000000
#define UART_BAUDRATE			115200

#if defined(AMBARELLA_CV22) || defined(AMBARELLA_CV2)
#define AMBARELLA_UART_PIN_RX		39
#define AMBARELLA_UART_PIN_TX		40
#elif defined(AMBARELLA_CV25)
#define AMBARELLA_UART_PIN_RX		17
#define AMBARELLA_UART_PIN_TX		18
#elif defined(AMBARELLA_S6LM) || defined(AMBARELLA_CV28) || defined(AMBARELLA_CV5)
#define AMBARELLA_UART_PIN_RX		10
#define AMBARELLA_UART_PIN_TX		11
#elif defined(AMBARELLA_N1)
#define AMBARELLA_UART_PIN_RX		45
#define AMBARELLA_UART_PIN_TX		46
#elif defined(AMBARELLA_CV72)
#define AMBARELLA_UART_PIN_RX		31
#define AMBARELLA_UART_PIN_TX		32
#elif defined(AMBARELLA_CV3AD685)
#define AMBARELLA_UART_PIN_RX		75
#define AMBARELLA_UART_PIN_TX		76
#elif defined(AMBARELLA_CV75) || defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define AMBARELLA_UART_PIN_RX		44
#define AMBARELLA_UART_PIN_TX		45
#elif defined(AMBARELLA_CV7)
#define AMBARELLA_UART_PIN_RX		30
#define AMBARELLA_UART_PIN_TX		31
#elif defined(AMBARELLA_CV8)
#define AMBARELLA_UART_PIN_RX		40
#define AMBARELLA_UART_PIN_TX		41
#else
#error "Unknown chip for UART pins"
#endif

#define UART_RB_OFFSET			0x00
#define UART_TH_OFFSET			0x00
#define UART_DLL_OFFSET			0x00
#define UART_IE_OFFSET			0x04
#define UART_DLH_OFFSET			0x04
#define UART_II_OFFSET			0x08
#define UART_FC_OFFSET			0x08
#define UART_LC_OFFSET			0x0c
#define UART_MC_OFFSET			0x10
#define UART_LS_OFFSET			0x14
#define UART_MS_OFFSET			0x18
#define UART_SC_OFFSET			0x1c	/* Byte */
#define UART_DMAE_OFFSET		0x28
#define UART_DMAF_OFFSET		0x40	/* DMA fifo */
#define UART_US_OFFSET			0x7c
#define UART_TFL_OFFSET			0x80
#define UART_RFL_OFFSET			0x84
#define UART_SRR_OFFSET			0x88


/*
 * GPIO Register Definition
 */
#define GPIO_DATA_OFFSET		0x00
#define GPIO_DIR_OFFSET			0x04
#define GPIO_IS_OFFSET			0x08
#define GPIO_IBE_OFFSET			0x0c
#define GPIO_IEV_OFFSET			0x10
#define GPIO_IE_OFFSET			0x14
#define GPIO_AFSEL_OFFSET		0x18
#define GPIO_RIS_OFFSET			0x1c
#define GPIO_MIS_OFFSET			0x20
#define GPIO_IC_OFFSET			0x24
#define GPIO_MASK_OFFSET		0x28
#define GPIO_ENABLE_OFFSET		0x2c

#define IOMUX_CTRL_SET_OFFSET		0xf0
#define IOMUX_REG_OFFSET(b, n)		(((b) * 0xc) + ((n) * 4))


/*
 * PWC Register Definition
 */
#if defined(AMBARELLA_CV75)
#define PWC_SET_RTC_OFFSET		0x190
#define PWC_CUR_RTC_OFFSET		0x194
#else
#define PWC_SET_RTC_OFFSET		0x30
#define PWC_CUR_RTC_OFFSET		0x34
#endif
#if defined(AMBARELLA_CV2)
#define PWC_RESET_OFFSET		0x40
#define PWC_CUR_STA_OFFSET		0xB4
#define	PWC_SET_STA_OFFSET		0xC0
#elif defined(AMBARELLA_N1) || defined(AMBARELLA_CV72) || defined(AMBARELLA_CV3AD685) ||\
	defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define PWC_RESET_OFFSET		0x38
#define PWC_CUR_STA_OFFSET		0x44
#define PWC_SET_STA_OFFSET		0x48
#elif defined(AMBARELLA_CV75)
#define PWC_RESET_OFFSET		0x198
#define PWC_CUR_STA_OFFSET		0x1A4
#define PWC_SET_STA_OFFSET		0x1A8
#elif defined(AMBARELLA_CV8)		/* NO PWC */
#define PWC_RESET_OFFSET		-1
#define PWC_CUR_STA_OFFSET		-1
#define	PWC_SET_STA_OFFSET		-1
#else
#define PWC_RESET_OFFSET		0x40
#define PWC_CUR_STA_OFFSET		0x54
#define	PWC_SET_STA_OFFSET		0x58
#endif
#define PWC_RTC_SRS_REG			PWC_REG(PWC_CUR_STA_OFFSET)

/* Definition for Recovery, please see amboot/src/bst/inlcude/bst_devfw.h */
#define DEVFW_MAGIC			(0x33219fbd)
#define DEVFW_FLAG_OFFSET		(0x44)

/* CPU IPI trigger CM3 from CV72 */
#if defined(AMBARELLA_CV72) || defined(AMBARELLA_CV75)
#define CPU_IPI2CM3_BASE		0xFFF0300000
#elif defined(AMBARELLA_CV3AD685) || defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
#define CPU_IPI2CM3_BASE		0xFFF0200000
#endif
#define IPI_0_BIT_MSK			(1 << 0)
#define IPI_1_BIT_MSK			(1 << 1)
#define IPI2CM3_OFFSET			0x8
#define CPU_IPI2CM3_REG			(CPU_IPI2CM3_BASE + (IPI2CM3_OFFSET))

#if defined(AMBARELLA_CV75)
#define PLATFORM_SUPPORT_CMVI		1
/* SCRATCHPAD_REG[3] pass SRS confirm data when suspend
 * must be consistent with cm3 code
*/
#define CMVI_SRS_MARK			0x5253
#define CMVI_SRS_NOTIFY_REG		SCRATCHPAD_REG(AHBSP_DATA3_OFFSET)
#endif

#if defined(AMBARELLA_CV72) || defined(AMBARELLA_CV8)
#define AMBARELLA_PLAT_NO_RTC		1
#endif

/************************************************************************/
#if defined(AMBARELLA_CV8)
#define PWC_DBG_REG(x)			0xFFFFFFFFFF
#else
#define PWC_DBG_BASE			(PERI_BASE + 0xED0D0000)
#define PWC_DBG_REG(x)			(PWC_DBG_BASE + (x))

#define PWC_KEY_IN0_REG_OFFSET		0x000
#define PWC_KEY_IN1_REG_OFFSET		0x004
#define PWC_KEY_IN2_REG_OFFSET		0x008
#define PWC_KEY_IN3_REG_OFFSET		0x00c
#define PWC_KEY_IN4_REG_OFFSET		0x010
#define PWC_KEY_IN5_REG_OFFSET		0x014
#define PWC_KEY_IN6_REG_OFFSET		0x018
#define PWC_KEY_IN7_REG_OFFSET		0x01c
#define PWC_KEY_OUT0_REG_OFFSET		0x020
#define PWC_KEY_OUT1_REG_OFFSET		0x024
#define PWC_KEY_OUT2_REG_OFFSET		0x028
#define PWC_KEY_OUT3_REG_OFFSET		0x02c
#define PWC_KEY_OUT4_REG_OFFSET		0x030
#define PWC_KEY_OUT5_REG_OFFSET		0x034
#define PWC_KEY_OUT6_REG_OFFSET		0x038
#define PWC_KEY_OUT7_REG_OFFSET		0x03c
#endif

#define AMBARELLA_IMAGE_HEADER		1
#define AMBARELLA_IMAGE_SIZE_ALIGN	1

#endif
