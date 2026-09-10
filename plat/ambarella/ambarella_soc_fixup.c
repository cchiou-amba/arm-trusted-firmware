/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <lib/mmio.h>
#include <plat_private.h>

static void ambarella_set_arbiter(void)
{
#if defined(AMBARELLA_CV2)
	mmio_write_32(DRAMC_REG(0x008), 0x200); /* DRAM_THROTTLE_DLN */
	mmio_write_32(DRAMC_REG(0x080), 0x002a0036); /* AXI0/Cortex */
	mmio_write_32(DRAMC_REG(0x084), 0x002a0006); /* ARM_DMA0/AHB */
	mmio_write_32(DRAMC_REG(0x088), 0x002a0006); /* ARM_DMA1/AHB */
	mmio_write_32(DRAMC_REG(0x08c), 0x102a0017); /* ENET*/
	mmio_write_32(DRAMC_REG(0x090), 0x102a0017); /* Flash DMA (FDMA) */
	mmio_write_32(DRAMC_REG(0x094), 0x102a0006); /* CANC0 */
	mmio_write_32(DRAMC_REG(0x098), 0x102a0006); /* CANC1 */
	mmio_write_32(DRAMC_REG(0x09c), 0x102a0000); /* GDMA */
	mmio_write_32(DRAMC_REG(0x0a0), 0x102a0017); /* SDXC0 */
	mmio_write_32(DRAMC_REG(0x0a4), 0x102a0017); /* SDXC1 */
	mmio_write_32(DRAMC_REG(0x0a8), 0x102a0006); /* USB20 (device) */
	mmio_write_32(DRAMC_REG(0x0ac), 0x102a0006); /* USB20 (host) */
	mmio_write_32(DRAMC_REG(0x0b0), 0x102a0008); /* OrcMe */
	mmio_write_32(DRAMC_REG(0x0b4), 0x102a0008); /* OrcCode */
	mmio_write_32(DRAMC_REG(0x0b8), 0x102a0004); /* OrcVp */
	mmio_write_32(DRAMC_REG(0x0bc), 0x102a0004); /* OrcL2 Cache */
	mmio_write_32(DRAMC_REG(0x0c0), 0x103b0005); /* SMEM */
	mmio_write_32(DRAMC_REG(0x0c4), 0x102a0004); /* VMEM0 */
	mmio_write_32(DRAMC_REG(0x0c8), 0x102a0002); /* FEX */
	mmio_write_32(DRAMC_REG(0x0cc), 0x102a0002); /* BMEM */
	mmio_write_32(DRAMC_REG(0x0fc), 0x003b0009); /* SMEM hi_priority */
	mmio_write_32(DRAMC_REG(0x10c), 0x20703); /* DRAM_SMEM_USAGE_TARGET ENET (r:1.17%, w:1.17%) */
	mmio_write_32(DRAMC_REG(0x110), 0x103); /* DRAM_SMEM_USAGE_TARGET FDMA (0%) */
	mmio_write_32(DRAMC_REG(0x120), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC0 (1.17%) */
	mmio_write_32(DRAMC_REG(0x124), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC1 (1.17%) */
	mmio_write_32(DRAMC_REG(0x140), 0x1e0); /* DRAM_SMEM_USAGE_TARGET SMEM (87.5%) */
	mmio_write_32(DRAMC_REG(0x190), 0xC00); /* rw throttle mandatory = 48 */
	mmio_write_32(DRAMC_REG(0x194), 0x20); /* bank throttle mandatory = 32 */

#elif defined(AMBARELLA_CV22)
	mmio_write_32(DRAMC_REG(0x008), 0x200); /* DRAM_THROTTLE_DLN */
	mmio_write_32(DRAMC_REG(0x080), 0x002a0036); /* AXI0/Cortex */
	mmio_write_32(DRAMC_REG(0x084), 0x002a0006); /* ARM_DMA0/AHB */
	mmio_write_32(DRAMC_REG(0x088), 0x002a0006); /* ARM_DMA1/AHB */
	mmio_write_32(DRAMC_REG(0x08c), 0x102a0017); /* ENET*/
	mmio_write_32(DRAMC_REG(0x090), 0x102a0017); /* Flash DMA (FDMA) */
	mmio_write_32(DRAMC_REG(0x094), 0x102a0006); /* CANC0 */
	mmio_write_32(DRAMC_REG(0x098), 0x102a0000); /* GDMA */
	mmio_write_32(DRAMC_REG(0x09c), 0x102a0017); /* SDXC0 */
	mmio_write_32(DRAMC_REG(0x0a0), 0x102a0017); /* SDXC1 */
	mmio_write_32(DRAMC_REG(0x0a4), 0x102a0006); /* USB20 (device)  */
	mmio_write_32(DRAMC_REG(0x0a8), 0x102a0006); /* USB20 (host) */
	mmio_write_32(DRAMC_REG(0x0ac), 0x102a0008); /* OrcMe */
	mmio_write_32(DRAMC_REG(0x0b0), 0x102a0008); /* OrcCode */
	mmio_write_32(DRAMC_REG(0x0b4), 0x102a0004); /* OrcVp */
	mmio_write_32(DRAMC_REG(0x0b8), 0x102a0004); /* OrcL2 Cache */
	mmio_write_32(DRAMC_REG(0x0bc), 0x103b0005); /* SMEM */
	mmio_write_32(DRAMC_REG(0x0c0), 0x102a0004); /* VMEM0 */
	mmio_write_32(DRAMC_REG(0x0fc), 0x003b0009); /* SMEM hi_priority */
	mmio_write_32(DRAMC_REG(0x10c), 0x20703); /* DRAM_SMEM_USAGE_TARGET ENET (r:1.17%, w:1.17%) */
	mmio_write_32(DRAMC_REG(0x110), 0x103); /* DRAM_SMEM_USAGE_TARGET FDMA (1.17%) */
	mmio_write_32(DRAMC_REG(0x11c), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC0 (1.17%) */
	mmio_write_32(DRAMC_REG(0x120), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC1 (1.17%) */
	mmio_write_32(DRAMC_REG(0x13C), 0x1e0); /* DRAM_SMEM_USAGE_TARGET SMEM (87.5%) */
	mmio_write_32(DRAMC_REG(0x190), 0xC00); /* rw throttle mandatory = 48 */
	mmio_write_32(DRAMC_REG(0x194), 0x20); /* bank throttle mandatory = 32 */

#elif defined(AMBARELLA_CV25) || defined(AMBARELLA_CV28)
	mmio_write_32(DRAMC_REG(0x008), 0x200); /* DRAM_THROTTLE_DLN */
	mmio_write_32(DRAMC_REG(0x080), 0x002a0036); /* AXI0/Cortex */
	mmio_write_32(DRAMC_REG(0x084), 0x002a0006); /* ARM_DMA0/AHB */
	mmio_write_32(DRAMC_REG(0x088), 0x002a0006); /* ARM_DMA1/AHB */
	mmio_write_32(DRAMC_REG(0x08c), 0x102a0017); /* ENET*/
	mmio_write_32(DRAMC_REG(0x090), 0x102a0017); /* Flash DMA (FDMA) */
	mmio_write_32(DRAMC_REG(0x094), 0x102a0006); /* CANC0 */
	mmio_write_32(DRAMC_REG(0x098), 0x102a0000); /* GDMA */
	mmio_write_32(DRAMC_REG(0x09c), 0x102a0017); /* SDXC0 */
	mmio_write_32(DRAMC_REG(0x0a0), 0x102a0017); /* SDXC1 */
	mmio_write_32(DRAMC_REG(0x0a4), 0x102a0017); /* SDXC2 */
	mmio_write_32(DRAMC_REG(0x0a8), 0x102a0006); /* USB20 (device)  */
	mmio_write_32(DRAMC_REG(0x0ac), 0x102a0006); /* USB20 (host) */
	mmio_write_32(DRAMC_REG(0x0b0), 0x102a0008); /* OrcMe */
	mmio_write_32(DRAMC_REG(0x0b4), 0x102a0008); /* OrcCode */
	mmio_write_32(DRAMC_REG(0x0b8), 0x102a0004); /* OrcVp */
	mmio_write_32(DRAMC_REG(0x0bc), 0x102a0004); /* OrcL2 Cache */
	mmio_write_32(DRAMC_REG(0x0c0), 0x103b0005); /* SMEM */
	mmio_write_32(DRAMC_REG(0x0c4), 0x102a0004); /* VMEM0 */
	mmio_write_32(DRAMC_REG(0x0fc), 0x003b0009); /* SMEM hi_priority */
	mmio_write_32(DRAMC_REG(0x10c), 0x20703); /* DRAM_SMEM_USAGE_TARGET ENET (r:1.17%, w:1.17%) */
	mmio_write_32(DRAMC_REG(0x110), 0x103); /* DRAM_SMEM_USAGE_TARGET FDMA (1.17%) */
	mmio_write_32(DRAMC_REG(0x11c), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC0 (1.17%) */
	mmio_write_32(DRAMC_REG(0x120), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC1 (1.17%) */
	mmio_write_32(DRAMC_REG(0x124), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC2 (1.17%) */
	mmio_write_32(DRAMC_REG(0x140), 0x1e0); /* DRAM_SMEM_USAGE_TARGET SMEM (87.5%) */
	mmio_write_32(DRAMC_REG(0x190), 0xC00); /* rw throttle mandatory = 48 */
	mmio_write_32(DRAMC_REG(0x194), 0x20); /* bank throttle mandatory = 32 */

#elif defined(AMBARELLA_S6LM)
	mmio_write_32(DRAMC_REG(0x008), 0x200); /* DRAM_THROTTLE_DLN */
	mmio_write_32(DRAMC_REG(0x080), 0x00280035); /* AXI0/Cortex */
	mmio_write_32(DRAMC_REG(0x084), 0x00280005); /* ARM_DMA0/AHB */
	mmio_write_32(DRAMC_REG(0x088), 0x00280005); /* ARM_DMA1/AHB */
	mmio_write_32(DRAMC_REG(0x08c), 0x10280016); /* ENET*/
	mmio_write_32(DRAMC_REG(0x090), 0x10280016); /* Flash DMA (FDMA) */
	mmio_write_32(DRAMC_REG(0x094), 0x10280000); /* GDMA */
	mmio_write_32(DRAMC_REG(0x098), 0x10280016); /* SDXC0 */
	mmio_write_32(DRAMC_REG(0x09c), 0x10280016); /* SDXC1 */
	mmio_write_32(DRAMC_REG(0x0a0), 0x10280016); /* SDXC2 */
	mmio_write_32(DRAMC_REG(0x0a4), 0x10280005); /* USB20 (device)  */
	mmio_write_32(DRAMC_REG(0x0a8), 0x10280005); /* USB20 (host) */
	mmio_write_32(DRAMC_REG(0x0ac), 0x10290006); /* OrcMe */
	mmio_write_32(DRAMC_REG(0x0b0), 0x10290006); /* OrcCode */
	mmio_write_32(DRAMC_REG(0x0b4), 0x10390004); /* SMEM */
	mmio_write_32(DRAMC_REG(0x0fc), 0x003a0007); /* SMEM hi_priority */
	mmio_write_32(DRAMC_REG(0x10c), 0x20703); /* DRAM_SMEM_USAGE_TARGET ENET (r:1.17%, w:1.17%) */
	mmio_write_32(DRAMC_REG(0x110), 0x103); /* DRAM_SMEM_USAGE_TARGET FDMA (1.17%) */
	mmio_write_32(DRAMC_REG(0x118), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC0 (1.17%) */
	mmio_write_32(DRAMC_REG(0x11c), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC1 (1.17%) */
	mmio_write_32(DRAMC_REG(0x120), 0x103); /* DRAM_SMEM_USAGE_TARGET SDXC2 (1.17%) */
	mmio_write_32(DRAMC_REG(0x134), 0x1e0); /* DRAM_SMEM_USAGE_TARGET SMEM (87.5%) */
	mmio_write_32(DRAMC_REG(0x190), 0xC00); /* rw throttle mandatory = 48 */
	mmio_write_32(DRAMC_REG(0x194), 0x20); /* bank throttle mandatory = 32 */

#elif defined(AMBARELLA_CV5)
	mmio_write_32(DRAMC_REG(0x110), 0x002d0069);   // 0: AXI0/Cortex
	mmio_write_32(DRAMC_REG(0x114), 0x002d0069);   // 1: AXI1/Cortex
	mmio_write_32(DRAMC_REG(0x118), 0x002d0007);   // 2: OrcL2 Cache
	mmio_write_32(DRAMC_REG(0x11c), 0x002d0049);   // 3: USB3
	mmio_write_32(DRAMC_REG(0x120), 0x002d0049);   // 4: PCIE
	mmio_write_32(DRAMC_REG(0x124), 0x002d004a);   // 5: ENET0
	mmio_write_32(DRAMC_REG(0x128), 0x002d004a);   // 6: ENET1
	mmio_write_32(DRAMC_REG(0x12c), 0x002d004a);   // 7: Flash DMA (FDMA)
	mmio_write_32(DRAMC_REG(0x130), 0x002d004a);   // 8: SDAXI0
	mmio_write_32(DRAMC_REG(0x134), 0x002d004a);   // 9: SDAXI1
	mmio_write_32(DRAMC_REG(0x138), 0x002d004a);   // 10: SDAHB0
	mmio_write_32(DRAMC_REG(0x13c), 0x002d0009);   // 11: USB2_device
	mmio_write_32(DRAMC_REG(0x140), 0x002d0009);   // 12: ARM_DMA0/AHB
	mmio_write_32(DRAMC_REG(0x144), 0x002d0009);   // 13: ARM_DMA1/AHB
	mmio_write_32(DRAMC_REG(0x148), 0x002d0009);   // 14: CANC0
	mmio_write_32(DRAMC_REG(0x14c), 0x002d0003);   // 15: GDMA
	mmio_write_32(DRAMC_REG(0x150), 0x002d000b);   // 16: OrcMe0
	mmio_write_32(DRAMC_REG(0x154), 0x002d000b);   // 17: OrcCode0
	mmio_write_32(DRAMC_REG(0x158), 0x002d000b);   // 18: OrcMe0
	mmio_write_32(DRAMC_REG(0x15c), 0x002d000b);   // 19: OrcCode1
	mmio_write_32(DRAMC_REG(0x160), 0x002d0007);   // 20: OrcVp
	mmio_write_32(DRAMC_REG(0x164), 0x103e1108);   // 21: SMEM_WR: Granularity = 2KB
	mmio_write_32(DRAMC_REG(0x168), 0x103e1108);   // 22: SMEM_RD: Granularity = 2KB
	mmio_write_32(DRAMC_REG(0x16c), 0x002d0057);   // 23: VMEM0
	mmio_write_32(DRAMC_REG(0x170), 0x002d0005);   // 24: DBSE
	mmio_write_32(DRAMC_REG(0x190), 0x003fff0f);   // SMEM_WR hi_priority: Granularity = 2KB
	mmio_write_32(DRAMC_REG(0x194), 0x003fff0f);   // SMEM_RD hi_priority: Granularity = 2KB
	mmio_write_32(DRAMC_REG(0x198), 0x800);        // DRAM_THROTTLE_DLN (2048 cycles)
	mmio_write_32(DRAMC_REG(0x1a8), 0x103);        // DRAM_USAGE_TARGET USB3 (1.17%)
	mmio_write_32(DRAMC_REG(0x1ac), 0x108);        // DRAM_USAGE_TARGET PCIE (3.12%)
	mmio_write_32(DRAMC_REG(0x1b0), 0x20703);      // DRAM_USAGE_TARGET ENET0 (r:1.17%, w:1.17%)
	mmio_write_32(DRAMC_REG(0x1b4), 0x20703);      // DRAM_USAGE_TARGET ENET1 (r:1.17%, w:1.17%)
	mmio_write_32(DRAMC_REG(0x1b8), 0x103);        // DRAM_USAGE_TARGET FDMA (1.17%)
	mmio_write_32(DRAMC_REG(0x1bC), 0x103);        // DRAM_USAGE_TARGET SDAXI0 (1.17%)
	mmio_write_32(DRAMC_REG(0x1c0), 0x103);        // DRAM_USAGE_TARGET SDAXI1 (1.17%)
	mmio_write_32(DRAMC_REG(0x1c4), 0x103);        // DRAM_USAGE_TARGET SDAHB0 (1.17%)
	mmio_write_32(DRAMC_REG(0x1f0), 0x170);        // DRAM_USAGE_TARGET SMEM_WR (43.75%)
	mmio_write_32(DRAMC_REG(0x1f4), 0x170);        // DRAM_USAGE_TARGET SMEM_RD (43.75%)
	mmio_write_32(DRAMC_REG(0x1f8), 0x130);        // DRAM_USAGE_TARGET VMEM0 (18.5%)
	mmio_write_32(DRAMC_REG(0x21c), 0x01400C20);   // rw throttle mandatory = 32
	mmio_write_32(DRAMC_REG(0x220), 0x01401030);   // bank throttle mandatory = 48
	mmio_write_32(DRAMC_REG(0x224), 0x88);         // DIE_BG_THROTTLE: different_die/bg_disable_cycles = 8

#elif defined(AMBARELLA_N1)
	mmio_write_32(0xff08004000, 0x00000509);    // 0x0000 - cortex0
	mmio_write_32(0xff08004004, 0x00000509);    // 0x0004 - cortex1
	mmio_write_32(0xff08004008, 0x00000509);    // 0x0008 - cortex2
	mmio_write_32(0xff0800400c, 0x00000509);    // 0x000c - cortex3
	mmio_write_32(0xff08004010, 0x0000041d);    // 0x0010 - usb3h0
	mmio_write_32(0xff08004014, 0x0000041d);    // 0x0014 - pcie0
	mmio_write_32(0xff08004018, 0x0000041d);    // 0x0018 - pcie1
	mmio_write_32(0xff0800401c, 0x00000408);    // 0x001c - gpu
	mmio_write_32(0xff08004020, 0x0000041d);    // 0x0020 - enet
	mmio_write_32(0xff08004024, 0x0000041d);    // 0x0024 - periphls0
	mmio_write_32(0xff08004028, 0x0000041d);    // 0x0028 - periphls1
	mmio_write_32(0xff0800402c, 0x00000000);    // 0x002c - gdma
	mmio_write_32(0xff08004030, 0x00000f0f);    // 0x0030 - orcme0
	mmio_write_32(0xff08004034, 0x00000f0f);    // 0x0034 - orcme1
	mmio_write_32(0xff08004038, 0x00000f0f);    // 0x0038 - orccode
	mmio_write_32(0xff0800403c, 0x0000060b);    // 0x003c - nvp0maxi
	mmio_write_32(0xff08004040, 0x00000428);    // 0x0040 - nvp0vmem
	mmio_write_32(0xff08004044, 0x0000060b);    // 0x0044 - nvp1maxi
	mmio_write_32(0xff08004048, 0x00000428);    // 0x0048 - nvp1vmem
	mmio_write_32(0xff0800404c, 0x0000060b);    // 0x004c - nvp2maxi
	mmio_write_32(0xff08004050, 0x00000428);    // 0x0050 - nvp2vmem
	mmio_write_32(0xff08004054, 0x0000060b);    // 0x0054 - nvp3maxi
	mmio_write_32(0xff08004058, 0x00000428);    // 0x0058 - nvp3vmem
	mmio_write_32(0xff0800405c, 0x0000060b);    // 0x005c - nvp4maxi
	mmio_write_32(0xff08004060, 0x00000428);    // 0x0060 - nvp4vmem
	mmio_write_32(0xff08004064, 0x0000060b);    // 0x0064 - nvp5maxi
	mmio_write_32(0xff08004068, 0x00000428);    // 0x0068 - nvp5vmem
	mmio_write_32(0xff0800406c, 0x0000060b);    // 0x006c - gvp0maxi
	mmio_write_32(0xff08004070, 0x00000428);    // 0x0070 - gvp0vmem
	mmio_write_32(0xff08004074, 0x0000060b);    // 0x0074 - gvp1maxi
	mmio_write_32(0xff08004078, 0x00000428);    // 0x0078 - gvp1vmem
	mmio_write_32(0xff0800407c, 0x0000060b);    // 0x007c - fexmaxi
	mmio_write_32(0xff08004080, 0x00000428);    // 0x0080 - fexdma
	mmio_write_32(0xff08004084, 0x0000060b);    // 0x0084 - fmamaxi
	mmio_write_32(0xff08004088, 0x00000428);    // 0x0088 - fmabmem
	mmio_write_32(0xff0800408c, 0x0000070c);    // 0x008c - swmaxi
	mmio_write_32(0xff08004090, 0x0000070c);    // 0x0090 - hsm
	mmio_write_32(0xff08004094, 0x0001053a);    // 0x0094 - smemwr: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004098, 0x0001053a);    // 0x0098 - smemrd: allow_rw_switch_disable_cyc
	mmio_write_32(0xff0800409c, 0x0001070e);    // 0x009c - smemwrh: allow_rw_switch_disable_cyc
	mmio_write_32(0xff080040a0, 0x0001070e);    // 0x00a0 - smemrdh: allow_rw_switch_disable_cyc

	mmio_write_32(0xff08004208, 0x800);        // DRAM_THROTTLE_DLN (2048 cycles)

	mmio_write_32(0xff0800421c, 0x102);        // DRAM_USAGE_TARGET USB3 (0.78%)
	mmio_write_32(0xff08004220, 0x108);        // DRAM_USAGE_TARGET PCIE0 (3.125%)
	mmio_write_32(0xff08004224, 0x108);        // DRAM_USAGE_TARGET PCIE0 (3.125)
	mmio_write_32(0xff0800422c, 0x20502);      // DRAM_USAGE_TARGET ENET0 (r:0.78%, w:0.78%)
	mmio_write_32(0xff08004230, 0x102);        // DRAM_USAGE_TARGET PERIPHLS0 (0.78%)
	mmio_write_32(0xff08004234, 0x102);        // DRAM_USAGE_TARGET PERIPHLS1 (0.78%)
	mmio_write_32(0xff0800424c, 0x110);        // DRAM_USAGE_TARGET NVP0_VMEM (6.25%)
	mmio_write_32(0xff08004254, 0x110);        // DRAM_USAGE_TARGET NVP1_VMEM (6.25%)
	mmio_write_32(0xff0800425c, 0x110);        // DRAM_USAGE_TARGET NVP2_VMEM (6.25%)
	mmio_write_32(0xff08004264, 0x110);        // DRAM_USAGE_TARGET NVP3_VMEM (6.25%)
	mmio_write_32(0xff0800426c, 0x110);        // DRAM_USAGE_TARGET NVP4_VMEM (6.25%)
	mmio_write_32(0xff08004274, 0x110);        // DRAM_USAGE_TARGET NVP5_VMEM (6.25%)
	mmio_write_32(0xff0800427c, 0x104);        // DRAM_USAGE_TARGET GVP0_VMEM (1.56%)
	mmio_write_32(0xff08004284, 0x104);        // DRAM_USAGE_TARGET GVP1_VMEM (1.56%)
	mmio_write_32(0xff08004288, 0x102);        // DRAM_USAGE_TARGET FEXDMA (0.78%)
	mmio_write_32(0xff08004290, 0x102);        // DRAM_USAGE_TARGET FMA_BMEM (0.78%)
	mmio_write_32(0xff080042a0, 0x120);        // DRAM_USAGE_TARGET SMEM_WR (12.5%)
	mmio_write_32(0xff080042a4, 0x120);        // DRAM_USAGE_TARGET SMEM_RD (12.5%)

	mmio_write_32(0xff08004800, 0x140283f);   // RW_SWITCHING_CNT
					   // post_rw_switch_opp_type_disable_cycles = 63
					   // post_rw_switch_opp_type_disable_cycles_if_allowed = 160
					   // post_rw_switch_opp_type_throttle_cycles = 320
					   // post_grant_opp_req_type_throttle_cycles = 0

#elif defined(AMBARELLA_CV72)
	mmio_write_32(0xff08004000, 0x00000509);    // 0x0000 - cortex0wr
	mmio_write_32(0xff08004004, 0x00000509);    // 0x0004 - cortex0rd
	mmio_write_32(0xff08004008, 0x0000041d);    // 0x0010 - usb3h0
	mmio_write_32(0xff0800400c, 0x0000041d);    // 0x0014 - pcie0
	mmio_write_32(0xff08004010, 0x0000041d);    // 0x0020 - enet
	mmio_write_32(0xff08004014, 0x0000041d);    // 0x0024 - periphls0
	mmio_write_32(0xff08004018, 0x0000041d);    // 0x0028 - periphls1
	mmio_write_32(0xff0800401c, 0x0000070c);    // 0x008c - swmaxi
	mmio_write_32(0xff08004020, 0x0000070c);    // 0x0090 - hsm
	mmio_write_32(0xff08004024, 0x0000060b);    // 0x003c - nvp0maxi
	mmio_write_32(0xff08004028, 0x00000000);    // 0x002c - gdma
	if (boot_cookie_ptr()->chip == 0xc0 || boot_cookie_ptr()->chip == 0xd0)
		mmio_write_32(0xff0800402c, 0x0000042b);    // 0x002c - nvp0vmem
	else
		mmio_write_32(0xff0800402c, 0x00000428);    // 0x0040 - nvp0vmem
	mmio_write_32(0xff08004030, 0x0001953a);    // 0x0094 - smemwr: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004034, 0x0001953a);    // 0x0098 - smemrd: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004038, 0x00000f0f);    // 0x0030 - orcme0
	mmio_write_32(0xff0800403c, 0x00000f0f);    // 0x0038 - orccode
	mmio_write_32(0xff08004040, 0x0001970e);    // 0x009c - smemwrh: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004044, 0x0001970e);    // 0x00a0 - smemrdh: allow_rw_switch_disable_cyc

	mmio_write_32(0xff08004208, 0x800);        // DRAM_THROTTLE_DLN (2048 cycles)

	mmio_write_32(0xff08004214, 0x102);        // DRAM_USAGE_TARGET USB3 (0.78%)
	mmio_write_32(0xff08004218, 0x108);        // DRAM_USAGE_TARGET PCIE0 (3.125%)
	mmio_write_32(0xff0800421c, 0x20502);      // DRAM_USAGE_TARGET ENET0 (r:0.78%, w:0.78%)
	mmio_write_32(0xff08004220, 0x102);        // DRAM_USAGE_TARGET PERIPHLS0 (0.78%)
	mmio_write_32(0xff08004224, 0x102);        // DRAM_USAGE_TARGET PERIPHLS1 (0.78%)
	mmio_write_32(0xff08004230, 0x140);        // DRAM_USAGE_TARGET NVP0_VMEM (25.0%)
	mmio_write_32(0xff0800423c, 0x150);        // DRAM_USAGE_TARGET SMEM_WR (31.25%)
	mmio_write_32(0xff08004240, 0x150);        // DRAM_USAGE_TARGET SMEM_RD (31.25%)

	mmio_write_32(0xff08004800, 0x140283f);    // RW_SWITCHING_CNT
						   // post_rw_switch_opp_type_disable_cycles = 63
						   // post_rw_switch_opp_type_disable_cycles_if_allowed = 160
						   // post_rw_switch_opp_type_throttle_cycles = 320
						   // post_grant_opp_req_type_throttle_cycles = 0
	/* limit cortex/gdma bandwidth */
	mmio_write_32(0xff08000040, 0x7f7f1f1f);    // set cortex0wr/cortex0rd request credit to 0x1f
	mmio_write_32(0xff08000048, 0x7f007f7f);    // set gdma request credit to 0

#elif defined(AMBARELLA_CV75)
	mmio_write_32(0xff08004000, 0x00000509);    // 0x0000 - cortex0wr
	mmio_write_32(0xff08004004, 0x00000509);    // 0x0004 - cortex0rd
	mmio_write_32(0xff08004008, 0x0000041d);    // 0x0008 - usb3h0
	mmio_write_32(0xff0800400c, 0x0000041d);    // 0x000c - enet
	mmio_write_32(0xff08004010, 0x0000041d);    // 0x0010 - periphls0
	mmio_write_32(0xff08004014, 0x0000041d);    // 0x0014 - periphls1
	mmio_write_32(0xff08004018, 0x0000070c);    // 0x0018 - swmaxi
	mmio_write_32(0xff0800401c, 0x0000060b);    // 0x001c - nvp0maxi
	mmio_write_32(0xff08004020, 0x00000000);    // 0x0020 - gdma
	mmio_write_32(0xff08004024, 0x00000428);    // 0x0024 - nvp0vmem
	mmio_write_32(0xff08004028, 0x0001953a);    // 0x0028 - smemwr: allow_rw_switch_disable_cyc
	mmio_write_32(0xff0800402c, 0x0001953a);    // 0x002c - smemrd: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004030, 0x00000f0f);    // 0x0030 - orcme0
	mmio_write_32(0xff08004034, 0x00000f0f);    // 0x0034 - orccode
	mmio_write_32(0xff08004038, 0x00000e0e);    // 0x0038 - smemwrh: allow_rw_switch_disable_cyc
	mmio_write_32(0xff0800403c, 0x0001970e);    // 0x003c - smemrdh: allow_rw_switch_disable_cyc

	mmio_write_32(0xff08004208, 0x800);         // DRAM_THROTTLE_DLN (2048 cycles)

	mmio_write_32(0xff08004214, 0x102);         // DRAM_USAGE_TARGET USB3 (0.78%)
	mmio_write_32(0xff08004218, 0x20502);       // DRAM_USAGE_TARGET ENET0 (r:0.78%, w:0.78%)
	mmio_write_32(0xff0800421c, 0x102);         // DRAM_USAGE_TARGET PERIPHLS0 (0.78%)
	mmio_write_32(0xff08004220, 0x102);         // DRAM_USAGE_TARGET PERIPHLS1 (0.78%)
	mmio_write_32(0xff08004230, 0x160);         // DRAM_USAGE_TARGET NVP0_VMEM (37.5%)
	mmio_write_32(0xff08004234, 0x140);         // DRAM_USAGE_TARGET SMEM_WR (25%)
	mmio_write_32(0xff08004238, 0x140);         // DRAM_USAGE_TARGET SMEM_RD (25%)

	mmio_write_32(0xff08004800, 0x140283f);     // RW_SWITCHING_CNT
						    // post_rw_switch_opp_type_disable_cycles = 63
						    // post_rw_switch_opp_type_disable_cycles_if_allowed = 160
						    // post_rw_switch_opp_type_throttle_cycles = 320
						    // post_grant_opp_req_type_throttle_cycles = 0
	/* limit cortex/gdma bandwidth */
	mmio_write_32(0xff08000040, 0x7f7f1f1f);    // set cortex0wr/cortex0rd request credit to 0x1f
	mmio_write_32(0xff08000048, 0x7f7f7f00);    // set gdma request credit to 0

#elif defined(AMBARELLA_CV3AD685)
	mmio_write_32(0xff08004000, 0x00000509);    // 0x0000 - cortex0wr
	mmio_write_32(0xff08004004, 0x00000509);    // 0x0004 - cortex0rd
	mmio_write_32(0xff08004008, 0x00000509);    // 0x0008 - cortex1wr
	mmio_write_32(0xff0800400c, 0x00000509);    // 0x000c - cortex1rd
	mmio_write_32(0xff08004010, 0x00000509);    // 0x0010 - cortex2wr
	mmio_write_32(0xff08004014, 0x00000509);    // 0x0014 - cortex2rd
	mmio_write_32(0xff08004018, 0x00000509);    // 0x0018 - cortexr52wr
	mmio_write_32(0xff0800401c, 0x00000509);    // 0x001c - cortexr52rd
	mmio_write_32(0xff08004020, 0x0000041d);    // 0x0020 - usb3h0
	mmio_write_32(0xff08004024, 0x0000041d);    // 0x0024 - pcie0wr
	mmio_write_32(0xff08004028, 0x0000041d);    // 0x0028 - pcie1rd
	mmio_write_32(0xff0800402c, 0x00000408);    // 0x002c - gpu
	mmio_write_32(0xff08004030, 0x0000041d);    // 0x0030 - enet
	mmio_write_32(0xff08004034, 0x0000041d);    // 0x0034 - periphls0
	mmio_write_32(0xff08004038, 0x0000041d);    // 0x0038 - periphls1
	mmio_write_32(0xff0800403c, 0x0000070c);    // 0x003c - swmaxi
	mmio_write_32(0xff08004040, 0x0000070c);    // 0x0040 - hsm
	mmio_write_32(0xff08004044, 0x0000060b);    // 0x0044 - nvp0maxi
	mmio_write_32(0xff08004048, 0x0000060b);    // 0x0048 - nvp1maxi
	mmio_write_32(0xff0800404c, 0x0000060b);    // 0x004c - nvp2maxi
	mmio_write_32(0xff08004050, 0x0000060b);    // 0x0050 - gvp0maxi
	mmio_write_32(0xff08004054, 0x0000060b);    // 0x0054 - gvp1maxi
	mmio_write_32(0xff08004058, 0x0000060b);    // 0x0058 - fexmaxi
	mmio_write_32(0xff0800405c, 0x0000060b);    // 0x005c - fmamaxi
	mmio_write_32(0xff08004060, 0x00000000);    // 0x0060 - gdma
	mmio_write_32(0xff08004064, 0x00000428);    // 0x0064 - nvp0vmem
	mmio_write_32(0xff08004068, 0x00000428);    // 0x0068 - nvp1vmem
	mmio_write_32(0xff0800406c, 0x00000428);    // 0x006c - nvp2vmem
	mmio_write_32(0xff08004070, 0x00000428);    // 0x0070 - gvp0vmem
	mmio_write_32(0xff08004074, 0x00000428);    // 0x0074 - gvp1vmem
	mmio_write_32(0xff08004078, 0x00000428);    // 0x0078 - fexdma
	mmio_write_32(0xff0800407c, 0x00000428);    // 0x007c - fmabmem
	mmio_write_32(0xff08004080, 0x0001053a);    // 0x0080 - smemwr: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004084, 0x0001053a);    // 0x0084 - smemrd: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004088, 0x00000f0f);    // 0x0088 - orcme0
	mmio_write_32(0xff0800408c, 0x00000f0f);    // 0x008c - orcme1
	mmio_write_32(0xff08004090, 0x00000f0f);    // 0x0090 - orccode
	mmio_write_32(0xff08004094, 0x0000070e);    // 0x0094 - ecchmpw
	mmio_write_32(0xff08004098, 0x0000070e);    // 0x0098 - ecchevc
	mmio_write_32(0xff0800409c, 0x0000070e);    // 0x009c - ecchrmf
	mmio_write_32(0xff080040a0, 0x00000e0e);    // 0x00a0 - smemwrhi: disable allow_rw_switch_disable_cyc
	mmio_write_32(0xff080040a4, 0x0001070e);    // 0x00a4 - smemrdhi: allow_rw_switch_disable_cyc
	mmio_write_32(0xff080040a8, 0x00000f0f);    // 0x00a8 - ecchmpwhi
	mmio_write_32(0xff080040ac, 0x00000f0f);    // 0x00ac - ecchevchi
	mmio_write_32(0xff080040b0, 0x00000f0f);    // 0x00b0 - ecchrmfhi

	mmio_write_32(0xff08004214, 0x800);         // DRAM_THROTTLE_DLN (2048 cycles)

	mmio_write_32(0xff08004238, 0x102);         // DRAM_USAGE_TARGET USB3 (0.78%)
	mmio_write_32(0xff0800423c, 0x108);         // DRAM_USAGE_TARGET PCIEWR (3.125%)
	mmio_write_32(0xff08004240, 0x108);         // DRAM_USAGE_TARGET PCIERD (3.125)
	mmio_write_32(0xff08004248, 0x20502);       // DRAM_USAGE_TARGET ENET0 (r:0.78%, w:0.78%)
	mmio_write_32(0xff0800424c, 0x102);         // DRAM_USAGE_TARGET PERIPHLS0 (0.78%)
	mmio_write_32(0xff08004250, 0x102);         // DRAM_USAGE_TARGET PERIPHLS1 (0.78%)
	mmio_write_32(0xff0800427c, 0x120);         // DRAM_USAGE_TARGET NVP0_VMEM (12.5%)
	mmio_write_32(0xff08004280, 0x120);         // DRAM_USAGE_TARGET NVP1_VMEM (12.5%)
	mmio_write_32(0xff08004284, 0x120);         // DRAM_USAGE_TARGET NVP2_VMEM (12.5%)
	mmio_write_32(0xff08004288, 0x108);         // DRAM_USAGE_TARGET GVP0_VMEM (3.125%)
	mmio_write_32(0xff0800428c, 0x108);         // DRAM_USAGE_TARGET GVP1_VMEM (3.125%)
	mmio_write_32(0xff08004290, 0x104);         // DRAM_USAGE_TARGET FEXDMA (1.56%)
	mmio_write_32(0xff08004294, 0x104);         // DRAM_USAGE_TARGET FMA_BMEM (1.56%)
	mmio_write_32(0xff08004298, 0x130);         // DRAM_USAGE_TARGET SMEM_WR (18.75%)
	mmio_write_32(0xff0800429c, 0x130);         // DRAM_USAGE_TARGET SMEM_RD (18.75%)

	mmio_write_32(0xff08004800, 0x140283f);     // RW_SWITCHING_CNT
						    // post_rw_switch_opp_type_disable_cycles = 63
						    // post_rw_switch_opp_type_disable_cycles_if_allowed = 160
						    // post_rw_switch_opp_type_throttle_cycles = 320
						    // post_grant_opp_req_type_throttle_cycles = 0
#elif defined(AMBARELLA_CV3AD655) || defined(AMBARELLA_N1_655)
	mmio_write_32(0xff08004000, 0x00000509);    // 0x0000 - cortex0wr
	mmio_write_32(0xff08004004, 0x00000509);    // 0x0004 - cortex0rd
	mmio_write_32(0xff08004008, 0x00000509);    // 0x0008 - cortex1wr
	mmio_write_32(0xff0800400c, 0x00000509);    // 0x000c - cortex1rd
	mmio_write_32(0xff08004010, 0x00000509);    // 0x0010 - cortexr52wr
	mmio_write_32(0xff08004014, 0x00000509);    // 0x0014 - cortexr52rd
	mmio_write_32(0xff08004018, 0x0000041d);    // 0x0018 - usb3h0
	mmio_write_32(0xff0800401c, 0x0000041d);    // 0x001c - pcie0wr
	mmio_write_32(0xff08004020, 0x0000041d);    // 0x0020 - pcie1rd
	mmio_write_32(0xff08004024, 0x00000408);    // 0x0024 - gpu
	mmio_write_32(0xff08004028, 0x0000041d);    // 0x0028 - enet
	mmio_write_32(0xff0800402c, 0x0000041d);    // 0x002c - periphls0
	mmio_write_32(0xff08004030, 0x0000041d);    // 0x0030 - periphls1
	mmio_write_32(0xff08004034, 0x0000070c);    // 0x0034 - swmaxi
	mmio_write_32(0xff08004038, 0x0000070c);    // 0x0038 - hsm
	mmio_write_32(0xff0800403c, 0x0000060b);    // 0x003c - nvp0maxi
	mmio_write_32(0xff08004040, 0x0000060b);    // 0x0040 - fexmaxi
	mmio_write_32(0xff08004044, 0x00000000);    // 0x0044 - gdma
	mmio_write_32(0xff08004048, 0x00000428);    // 0x0048 - nvp0vmem
	mmio_write_32(0xff0800404c, 0x00000428);    // 0x004c - fexdma
	mmio_write_32(0xff08004050, 0x0001053a);    // 0x0050 - smemwr: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004054, 0x0001053a);    // 0x0054 - smemrd: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004058, 0x00000f0f);    // 0x0058 - orcme0
	mmio_write_32(0xff0800405c, 0x00000f0f);    // 0x005c - orccode
	mmio_write_32(0xff08004060, 0x0000070e);    // 0x0060 - ecchmpw
	mmio_write_32(0xff08004064, 0x0000070e);    // 0x0064 - ecchevc
	mmio_write_32(0xff08004068, 0x0000070e);    // 0x0068 - ecchrmf
	mmio_write_32(0xff0800406c, 0x00000e0e);    // 0x006c - smemwrhi: disable allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004070, 0x0001070e);    // 0x0070 - smemrdhi: allow_rw_switch_disable_cyc
	mmio_write_32(0xff08004074, 0x00000f0f);    // 0x0074 - ecchmpwhi
	mmio_write_32(0xff08004078, 0x00000f0f);    // 0x0078 - ecchevchi
	mmio_write_32(0xff0800407c, 0x00000f0f);    // 0x007c - ecchrmfhi

	mmio_write_32(0xff08004214, 0x800);        // DRAM_THROTTLE_DLN (2048 cycles)

	mmio_write_32(0xff08004230, 0x103);        // DRAM_USAGE_TARGET USB3 (1.17%)
	mmio_write_32(0xff08004234, 0x108);        // DRAM_USAGE_TARGET PCIEWR (3.125%)
	mmio_write_32(0xff08004238, 0x108);        // DRAM_USAGE_TARGET PCIERD (3.125)
	mmio_write_32(0xff08004240, 0x20502);      // DRAM_USAGE_TARGET ENET0 (r:0.78%, w:0.78%)
	mmio_write_32(0xff08004244, 0x103);        // DRAM_USAGE_TARGET PERIPHLS0 (1.17%)
	mmio_write_32(0xff08004248, 0x103);        // DRAM_USAGE_TARGET PERIPHLS1 (1.17%)
	mmio_write_32(0xff08004260, 0x160);        // DRAM_USAGE_TARGET NVP0_VMEM (37.5%)
	mmio_write_32(0xff08004264, 0x104);        // DRAM_USAGE_TARGET FEXDMA (1.56%)
	mmio_write_32(0xff08004268, 0x150);        // DRAM_USAGE_TARGET SMEM_WR (31.25%)
	mmio_write_32(0xff0800426c, 0x150);        // DRAM_USAGE_TARGET SMEM_RD (31.25%)

	mmio_write_32(0xff08004800, 0x140283f);    // RW_SWITCHING_CNT
					    // post_rw_switch_opp_type_disable_cycles = 63
					    // post_rw_switch_opp_type_disable_cycles_if_allowed = 160
					    // post_rw_switch_opp_type_throttle_cycles = 320
					    // post_grant_opp_req_type_throttle_cycles = 0
#elif defined(AMBARELLA_CV7)
	/* T.B.D */
#else
	/* nothting to do */
#endif
}

void ambarella_soc_fixup(void)
{
	uint32_t core_freq = get_core_bus_freq_hz();

	if (!ambarella_is_primary_cluster())
		return;

	ambarella_set_arbiter();

	if (core_freq < 466000000)
		mmio_setbits_32(RCT_REG(SYS_CONFIG_OFFSET), POC_PERIPHERAL_CLK_MODE);
	else
		mmio_clrbits_32(RCT_REG(SYS_CONFIG_OFFSET), POC_PERIPHERAL_CLK_MODE);
}
