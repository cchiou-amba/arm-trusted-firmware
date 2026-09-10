# Copyright (c) 2026 Ambarella International LP
#
# License Identifier: AMBARELLA-2-Clause
#

override ENABLE_ASSERTIONS		:= 1
override CRASH_REPORTING                := 1
override ERROR_DEPRECATED		:= 1

override PROGRAMMABLE_RESET_ADDRESS	:= 0
override COLD_BOOT_SINGLE_CPU		:= 1
override RESET_TO_BL31			:= 1
override RESET_TO_BL2			:= 1
override INIT_UNUSED_NS_EL2		:= 1
override PSCI_EXTENDED_STATE_ID		:= 1
override A53_DISABLE_NON_TEMPORAL_HINT	:= 0
override SEPARATE_CODE_AND_RODATA	:= 1

override ENABLE_PIE			:= 1

override CTX_INCLUDE_AARCH32_REGS	:= 0
override USE_COHERENT_MEM		:= 0

# enable assert() for release/debug builds
PLAT_LOG_LEVEL_ASSERT			:= 40
$(eval $(call add_define,PLAT_LOG_LEVEL_ASSERT))

# enable dynamic memory mapping
PLAT_XLAT_TABLES_DYNAMIC		:= 1
$(eval $(call add_define,PLAT_XLAT_TABLES_DYNAMIC))

# Add Ambarella definition
ifeq (${CHIP}, cv2)
__CPU_ARCH__				:= A53
$(eval $(call add_define,AMBARELLA_CV2))
endif
ifeq (${CHIP}, cv22)
__CPU_ARCH__				:= A53
$(eval $(call add_define,AMBARELLA_CV22))
endif
ifeq (${CHIP}, cv25)
__CPU_ARCH__				:= A53
$(eval $(call add_define,AMBARELLA_CV25))
endif
ifeq (${CHIP}, s6lm)
__CPU_ARCH__				:= A53
$(eval $(call add_define,AMBARELLA_S6LM))
endif
ifeq (${CHIP}, cv28)
__CPU_ARCH__				:= A53
$(eval $(call add_define,AMBARELLA_CV28))
endif
ifeq (${CHIP}, cv5)
__CPU_ARCH__				:= A76
$(eval $(call add_define,AMBARELLA_CV5))
endif
ifeq (${CHIP}, cv52)
__CPU_ARCH__				:= A76
$(eval $(call add_define,AMBARELLA_CV5))
endif
ifeq (${CHIP}, n1)
__CPU_ARCH__				:= A78_AE
$(eval $(call add_define,AMBARELLA_N1))
endif
ifeq (${CHIP}, cv72)
__CPU_ARCH__				:= A76
$(eval $(call add_define,AMBARELLA_CV72))
endif
ifeq (${CHIP}, cv3ad685)
__CPU_ARCH__				:= A78_AE
$(eval $(call add_define,AMBARELLA_CV3AD685))
endif
ifeq (${CHIP}, cv75)
__CPU_ARCH__				:= A76
$(eval $(call add_define,AMBARELLA_CV75))
endif
ifeq (${CHIP}, cv3ad655)
__CPU_ARCH__				:= A78_AE
$(eval $(call add_define,AMBARELLA_CV3AD655))
endif
ifeq (${CHIP}, n1_655)
__CPU_ARCH__				:= A78_AE
$(eval $(call add_define,AMBARELLA_N1_655))
endif
ifeq (${CHIP}, cv7)
__CPU_ARCH__				:= A73
$(eval $(call add_define,AMBARELLA_CV7))
endif
ifeq (${CHIP}, cv8)
__CPU_ARCH__				:= A78_AE
$(eval $(call add_define,AMBARELLA_CV8))
endif

# Errata for A53
ifeq (${__CPU_ARCH__}, A53)
ERRATA_A53_835769			:= 1
ERRATA_A53_836870			:= 1
ERRATA_A53_843419			:= 1
ERRATA_A53_855873			:= 1
HW_ASSISTED_COHERENCY			:= 0
endif

# Errata for A76
ifeq (${__CPU_ARCH__}, A76)
ERRATA_A76_1073348			:= 0
ERRATA_A76_1130799			:= 0
ERRATA_A76_1220197			:= 0
ERRATA_A76_1257314			:= 0
ERRATA_A76_1262606			:= 0
ERRATA_A76_1262888			:= 0
ERRATA_A76_1275112			:= 0
ERRATA_A76_1286807			:= 0
ERRATA_A76_1791580			:= 0
ERRATA_A76_1165522			:= 1
ERRATA_A76_1868343			:= 0
ERRATA_A76_1946160			:= 1
HW_ASSISTED_COHERENCY			:= 1
endif

# Errata for A78_AE
ifeq (${__CPU_ARCH__}, A78_AE)
# A78AE is ARMv8.2-A; needed so CV8 GIC_BASE picks primary CA78 cluster
ARM_ARCH_MAJOR				:= 8
ARM_ARCH_MINOR				:= 2
ERRATA_A78_AE_1941500			:= 1
ERRATA_A78_AE_1951502			:= 1
HW_ASSISTED_COHERENCY			:= 1
endif

ifeq (${__CPU_ARCH__}, A73)
ERRATA_A73_852427			:= 0
ERRATA_A73_855423			:= 0
endif

# Libraries
include lib/xlat_tables_v2/xlat_tables.mk
include lib/libfdt/libfdt.mk
include drivers/arm/gic/v2/gicv2.mk

#
# Curve25519 / Ed25519 source selection (menuconfig: CONFIG_ATF_BUILTIN_CURVE25519):
#   PLAT_CFG_ATF_BUILTIN_CURVE25519=1  -> in-tree lib/curve25519 (no lw_cryptography)
#   default (0 / unset)               -> freestanding lw_cryptography (Ed25519 + HKDF)
#
# TCG DICE / UDS SIPs (menuconfig: CONFIG_TCG_DICE):
#   PLAT_CFG_TCG_DICE=1               -> build UDS/DICE OTP SIP handlers
#   Requires freestanding lw_cryptography (forces BUILTIN_CURVE25519 off).
#
# HAL stub is ATF-internal (not menuconfig):
#   PLAT_CFG_LWC_HAL_STUB=1           -> link lw_cryptography/platform/hal_stub.c
#
PLAT_CFG_ATF_BUILTIN_CURVE25519	?= 0
PLAT_CFG_TCG_DICE		?= 0

# DICE/UDS needs HKDF from freestanding lw_cryptography.
ifeq ($(PLAT_CFG_TCG_DICE),1)
PLAT_CFG_ATF_BUILTIN_CURVE25519	:= 0
endif

ifeq ($(PLAT_CFG_ATF_BUILTIN_CURVE25519),1)
$(eval $(call add_define,PLAT_CFG_ATF_BUILTIN_CURVE25519))
ATF_CURVE25519_SOURCES	:=	lib/curve25519/curve25519_v2.c
PLAT_INCLUDES		:=	-Iinclude/drivers/ambarella/uart/		\
				-Iplat/ambarella/include
else
LW_CRYPTOGRAPHY_PATH	:= $(ENV_TOP_DIR)/security/lw_cryptography
# ATF freestanding always uses the no-op HAL stub (not a menuconfig choice).
PLAT_CFG_LWC_HAL_STUB	?= 1
include $(LW_CRYPTOGRAPHY_PATH)/freestanding.mk
ATF_CURVE25519_SOURCES	:=	$(LW_CRYPTO_FREESTANDING_SOURCES)
PLAT_INCLUDES		:=	-Iinclude/drivers/ambarella/uart/		\
				-Iplat/ambarella/include			\
				$(LW_CRYPTO_FREESTANDING_INCLUDES)
endif

ifeq ($(PLAT_CFG_TCG_DICE),1)
$(eval $(call add_define,PLAT_CFG_TCG_DICE))
endif
ifeq ($(PLAT_CFG_LWC_HAL_STUB),1)
$(eval $(call add_define,PLAT_CFG_LWC_HAL_STUB))
endif

AMBARELLA_DRIVERS	:=	$(wildcard plat/ambarella/driver/*.c)		\
				$(wildcard plat/ambarella/driver/*.S)

ifeq ($(AMBA_SECURITY_ARCH),sec_v3)
AMBARELLA_DRIVERS	:=	$(filter-out plat/ambarella/driver/ambarella_otp.c,$(AMBARELLA_DRIVERS))
else
AMBARELLA_DRIVERS	:=	$(filter-out plat/ambarella/driver/ambarella_otp_flex.c,$(AMBARELLA_DRIVERS))
AMBARELLA_DRIVERS	:=	$(filter-out plat/ambarella/driver/ambarella_otp_flex_layout.c,$(AMBARELLA_DRIVERS))
endif

# common sources for BL2/BL31
PLAT_BL_COMMON_SOURCES	+=	plat/ambarella/ambarella_xlat_setup.c		\
				${XLAT_TABLES_LIB_SRCS}

ifeq (${__CPU_ARCH__}, A53)
BL2_SOURCES		+=	lib/cpus/aarch64/cortex_a53.S
BL31_SOURCES		+=	lib/cpus/aarch64/cortex_a53.S
endif
ifeq (${__CPU_ARCH__}, A76)
BL2_SOURCES		+=	lib/cpus/aarch64/cortex_a76.S
BL31_SOURCES		+=	lib/cpus/aarch64/cortex_a76.S
endif
ifeq (${__CPU_ARCH__}, A78_AE)
BL2_SOURCES		+=	lib/cpus/aarch64/cortex_a78_ae.S
BL31_SOURCES		+=	lib/cpus/aarch64/cortex_a78_ae.S
endif
ifeq (${__CPU_ARCH__}, A73)
BL2_SOURCES		+=	lib/cpus/aarch64/cortex_a73.S
BL31_SOURCES		+=	lib/cpus/aarch64/cortex_a73.S
endif

BL2_SOURCES		+=	drivers/io/io_block.c			\
				drivers/io/io_fip.c			\
				drivers/io/io_storage.c			\
				common/desc_image_load.c		\
				$(AMBARELLA_DRIVERS)			\
				plat/ambarella/ambarella_bl2_setup.c	\
				plat/ambarella/ambarella_boot_cookie.c	\
				plat/ambarella/ambarella_image_desc.c	\
				plat/ambarella/ambarella_io_storage.c	\
				plat/ambarella/ambarella_topology.c	\
				plat/ambarella/aarch64/ambarella_helpers.S

ifeq (${TRUSTED_BOARD_BOOT},1)

include drivers/auth/mbedtls/mbedtls_crypto.mk
include drivers/auth/mbedtls/mbedtls_x509.mk

BL2_SOURCES		+=	drivers/auth/auth_mod.c			\
				drivers/auth/crypto_mod.c		\
				drivers/auth/img_parser_mod.c		\
				drivers/auth/tbbr/tbbr_cot_common.c	\
				drivers/auth/tbbr/tbbr_cot_bl2.c	\
				plat/common/tbbr/plat_tbbr.c		\
				plat/ambarella/ambarella_rotpk.S	\
				plat/ambarella/ambarella_tbbr.c		\
				$(ATF_CURVE25519_SOURCES)

endif	# TRUSTED_BOARD_BOOT

BL31_SOURCES		+=	drivers/delay_timer/delay_timer.c		\
				drivers/delay_timer/generic_delay_timer.c	\
				drivers/gpio/gpio.c				\
				plat/common/plat_gicv2.c			\
				plat/common/plat_psci_common.c			\
				$(AMBARELLA_DRIVERS)				\
				plat/ambarella/aarch64/ambarella_suspend.S	\
				plat/ambarella/aarch64/ambarella_helpers.S	\
				plat/ambarella/aarch64/el2_runtime_exceptions.S	\
				plat/ambarella/ambarella_bl31_cpufreq.c		\
				plat/ambarella/ambarella_bl31_setup.c		\
				plat/ambarella/ambarella_fdt.c			\
				plat/ambarella/ambarella_el2.c			\
				plat/ambarella/ambarella_security.c		\
				plat/ambarella/ambarella_boot_cookie.c		\
				plat/ambarella/ambarella_soc_fixup.c		\
				plat/ambarella/ambarella_gicv2.c		\
				plat/ambarella/ambarella_psci.c			\
				plat/ambarella/ambarella_topology.c		\
				plat/ambarella/ambarella_sip_svc.c		\
				plat/ambarella/ambarella_trng.c			\
				plat/ambarella/ambarella_secure_debug.c		\
				$(ATF_CURVE25519_SOURCES)			\
				plat/ambarella/ambarella_att.c			\
				plat/ambarella/ambarella_cluster.c		\
				${LIBFDT_SRCS}					\
				${GICV2_SOURCES}

ifneq ($(CFG_SOC_FW_CONFIG),)
$(eval $(call add_define,CFG_SOC_FW_CONFIG))
endif

# ATF flexible memory layout
ifneq ($(PLAT_CFG_FLEX_MEM_LAOUT),)
$(eval $(call add_define,PLAT_CFG_FLEX_MEM_LAOUT))

# memory layout related variables, below are mandatory when CONFIG_ATF_FLEX_MEM_LAOUT is defined

ifneq ($(PLAT_CFG_BL31_BASE),)
$(eval $(call add_define,PLAT_CFG_BL31_BASE))
else
$(error "PLAT_CFG_BL31_BASE is not defined")
endif

ifneq ($(PLAT_CFG_BL31_SIZE),)
$(eval $(call add_define,PLAT_CFG_BL31_SIZE))
else
$(error "PLAT_CFG_BL31_SIZE is not defined")
endif

ifneq ($(PLAT_CFG_BL32_BASE),)
$(eval $(call add_define,PLAT_CFG_BL32_BASE))
else
$(error "PLAT_CFG_BL32_BASE is not defined")
endif

ifneq ($(PLAT_CFG_BL32_SIZE),)
$(eval $(call add_define,PLAT_CFG_BL32_SIZE))
else
$(error "PLAT_CFG_BL32_SIZE is not defined")
endif

ifneq ($(PLAT_CFG_SECURE_MEM_PATITIONING),)
$(eval $(call add_define,PLAT_CFG_SECURE_MEM_PATITIONING))
else
$(error "PLAT_CFG_SECURE_MEM_PATITIONING is not defined")
endif

ifneq ($(PLAT_CFG_EL2_RSVD_BASE),)
$(eval $(call add_define,PLAT_CFG_EL2_RSVD_BASE))
else
$(error "PLAT_CFG_EL2_RSVD_BASE is not defined")
endif

ifneq ($(PLAT_CFG_EL2_RSVD_SIZE),)
$(eval $(call add_define,PLAT_CFG_EL2_RSVD_SIZE))
else
$(error "PLAT_CFG_EL2_RSVD_SIZE is not defined")
endif

else

ifneq ($(PLAT_CFG_BL32_SHMEM_SIZE),)
$(eval $(call add_define,PLAT_CFG_BL32_SHMEM_SIZE))
else
$(error "PLAT_CFG_BL32_SHMEM_SIZE is not defined")
endif

endif	# PLAT_CFG_FLEX_MEM_LAOUT

# ROT key index
ifneq ($(PLAT_CFG_ROT_KEY_INDEX),)
$(eval $(call add_define,PLAT_CFG_ROT_KEY_INDEX))
endif

ifneq ($(PLAT_CFG_BL33_BASE),)
$(eval $(call add_define,PLAT_CFG_BL33_BASE))
else
$(error "PLAT_CFG_BL33_BASE is not defined")
endif

ifneq ($(PLAT_CFG_ATF_EMBED_PUB_COT_ROOT),)
$(eval $(call add_define,PLAT_CFG_ATF_EMBED_PUB_COT_ROOT))
ifneq ($(PLAT_CFG_PUB_COT_ROOT_DER),)
$(eval $(call add_define_val,PLAT_CFG_PUB_COT_ROOT_DER,'"$(PLAT_CFG_PUB_COT_ROOT_DER)"'))
else
$(error "PLAT_CFG_PUB_COT_ROOT_DER is not specified")
endif
endif

ifeq ($(PLAT_CFG_ATF_COT_ALGO),rsa)
$(eval $(call add_define,PLAT_CFG_ATF_COT_ALGO_RSA))
else ifeq ($(PLAT_CFG_ATF_COT_ALGO),ecdsa)
$(eval $(call add_define,PLAT_CFG_ATF_COT_ALGO_ECDSA))
else ifeq ($(PLAT_CFG_ATF_COT_ALGO),ed25519)
$(eval $(call add_define,PLAT_CFG_ATF_COT_ALGO_ED25519))
endif

ifneq ($(PLAT_CFG_AMRTOS_ARM_TF),)
$(eval $(call add_define,PLAT_CFG_AMRTOS_ARM_TF))
endif

ifneq ($(PLAT_CFG_FIX_WARM_REBOOT),)
$(eval $(call add_define,PLAT_CFG_FIX_WARM_REBOOT))
endif

ifneq ($(PLAT_CFG_DRAMFREQ_SUPPORT),)
$(eval $(call add_define,PLAT_CFG_DRAMFREQ_SUPPORT))
endif

ifneq ($(PLAT_CFG_FIP_MEDIA_OFFSET),)
$(eval $(call add_define_val,PLAT_CFG_FIP_MEDIA_OFFSET,$(PLAT_CFG_FIP_MEDIA_OFFSET)))
endif

# binutils 2.39 adds the check feature, disable it to fix compile failure:
# aarch64-linux-gnu-ld: warning: .../bl2.elf has a LOAD segment with RWX permissions
ifneq ($(shell $(LD) --help | grep -c '\--no-warn-rwx-segments'),0)
TF_LDFLAGS		+= --no-warn-rwx-segments
endif
