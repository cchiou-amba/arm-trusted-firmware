# Ambarella ARM Trusted Firmware (BL31 / EL3 Runtime)

This repository packages the ARM Trusted Firmware (TF-A v2.10) port for
Ambarella CV3 and N1-655 SoC platforms. It provides the EL3 Secure Monitor
and Power State Coordination Interface (PSCI v0.2 / v1.1) runtime services
for Edge Virtualization Engine (EVE-OS).

---

## Architectural Role

In the ARMv8-A / ARMv9-A security architecture, ARM Trusted Firmware executes
at Exception Level 3 (EL3 - Secure Monitor):

- **PSCI Implementation**: Implements ARM Power State Coordination Interface
  specification, handling CPU power-down, core boot (`CPU_ON`), CPU idle
  states, and system-level reboot / shutdown (`SYSTEM_RESET`, `SYSTEM_OFF`).
- **Secure Monitor & SMC Dispatch**: Intercepts Secure Monitor Calls (SMC)
  originating from EL1/EL2 non-secure software (Linux, Xen, QNX) and routes
  them to standard or vendor-specific SIP (Silicon Partner) services.
- **Hardware Reset & Power Management**: Directly programs the Ambarella
  Reset & Clock Control (RCT) register block to perform platform-level warm
  reset and core reset sequencing.
- **SMP Core Wake-Up**: Configures core reset vector base address registers
  (RVBAR) and manages the spin-table / PSCI wake-up protocol for secondary
  Cortex-A78AE application processor cores.
- **TrustZone & Hardware Security**: Configures interconnect firewalls,
  peripheral access controls, and secure memory carve-outs (TZDRAM).

---

## Directory Layout

- `plat/ambarella/`: Ambarella SoC family platform implementation:
  - `ambarella_psci.c`: PSCI operations (CPU on/off, suspend, system reset)
  - `ambarella_bl31_setup.c`: BL31 early initialization, console setup, MMU setup
  - `ambarella_sip_svc.c`: Silicon Partner SMC service dispatchers
  - `ambarella_topology.c`: Multi-cluster CPU topology definitions
  - `platform.mk`: Platform-specific build configuration and compiler flags
- `plat/ambarella/include/`: Platform headers:
  - `ambarella_def.h`: RCT, PMU, UART, timer, and memory map definitions
  - `ambarella_smc.h`: SIP SMC function identifiers and calling conventions
  - `platform_def.h`: Memory layout, stack sizes, and translation table parameters
- `drivers/`: Generic ARM architectural drivers (GICv2/v3 interrupt controllers)
- `lib/`: Standard C runtime and EL3 architectural helper libraries

---

## Building the Firmware

To build the BL31 (Bootloader Stage 3-1) runtime binary out-of-tree using
the standard AArch64 cross-compilation toolchain:

### 1. Build for Ambarella N1-655 (Cooper Pro)
```bash
make PLAT=ambarella \
     CHIP=n1_655 \
     CROSS_COMPILE=aarch64-linux-gnu- \
     DEBUG=0 \
     bl31
```

### 2. Build for Ambarella CV3-AD655 (Devkit)
```bash
make PLAT=ambarella \
     CHIP=cv3ad655 \
     CROSS_COMPILE=aarch64-linux-gnu- \
     DEBUG=0 \
     bl31
```

### Build Outputs
- `build/ambarella/release/bl31.bin`: Flat raw binary for packaging into FIP
- `build/ambarella/release/bl31/bl31.elf`: ELF executable with debug symbols

To clean build outputs:
```bash
make PLAT=ambarella distclean
```
