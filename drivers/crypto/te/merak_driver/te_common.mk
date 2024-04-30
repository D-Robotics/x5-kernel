################################################################################
# Input variables
################################################################################

# TOP_DIR      - build system top directory
# TE_SW_TOP    - te driver software top directory
# TE_NAME      - te driver name, default to 'merak'
# TE_UNIT_TEST - unit test option
# TE_HWA_DEBUG - hwa debug option
# TE_TEST_ENV  - unit test environment, bm|u-boot|linux|optee
# TE_OTP_PUF   - OTP PUF otpion

include $(dir $(lastword $(MAKEFILE_LIST)))/mk/te_config.mk

################################################################################
# Output variables
################################################################################

# TE_SOURCES   - list of te driver source files
# TE_CFLAGS    - te driver include options and flags

################################################################################
# Global variables
################################################################################

SHARED_DIR     := $(TOP_DIR)/shared
EXTERN_DIR     := $(TOP_DIR)/external
SQLIST_DIR      := $(EXTERN_DIR)/misc/sqlist
HOSAL_API_DIR  := $(SHARED_DIR)/hosal/apis

################################################################################
# TE build variables
################################################################################

TE_NAME        ?= merak
TE_PATH_PREFIX := $(if $(TE_SW_TOP),$(TE_SW_TOP)/)
TE_GEN_TOP     := $(TE_PATH_PREFIX)inc/generated
TE_AUTO_GEN    := $(TE_GEN_TOP)/$(TE_NAME)

TE_SOURCES     := $(wildcard $(TE_PATH_PREFIX)hwa/*.c)                      \
                  $(wildcard $(TE_PATH_PREFIX)drv/*.c)                      \
                  $(wildcard $(TE_PATH_PREFIX)cipher/*.c)                   \
                  $(wildcard $(TE_PATH_PREFIX)hash/*.c)                     \
                  $(wildcard $(TE_PATH_PREFIX)mac/*.c)                      \
                  $(wildcard $(TE_PATH_PREFIX)aead/*.c)                     \
                  $(wildcard $(TE_PATH_PREFIX)pk/*.c)                       \
                  $(wildcard $(TE_PATH_PREFIX)common/*.c)

TE_INCLUDES    := $(TE_PATH_PREFIX)inc

# TE options
ifeq ($(TE_UNIT_TEST),y)
TE_SOURCES     += $(wildcard $(TE_PATH_PREFIX)test/*.c)                     \
                  $(wildcard $(TE_PATH_PREFIX)test/$(TE_TEST_ENV)/*.c)      \
                  $(wildcard $(TE_PATH_PREFIX)test/sw_crypto_ref/src/*.c)
TE_INCLUDES    += $(TE_PATH_PREFIX)test                                     \
                  $(TE_PATH_PREFIX)test/sw_crypto_ref/inc
endif

TE_SOURCES     := $(sort $(TE_SOURCES))

TE_CFLAGS      := $(addprefix -I,$(TE_INCLUDES))                            \
                  -I$(TE_AUTO_GEN)                                          \
                  -I$(SQLIST_DIR)                                           \
                  -I$(HOSAL_API_DIR)/hal                                    \
                  -I$(HOSAL_API_DIR)/osal                                   \
                  -DCFG_TE_NUM_WORKERS=$(TE_NUM_WORKERS)

ifeq ($(TE_HWA_DEBUG),y)
TE_CFLAGS      += -DWITH_BITFIELD_LOG=1                                     \
                  -DBITFIELD_LOG=TE_PRINT
endif

ifeq ($(TE_OTP_PUF),y)
TE_CFLAGS      += -DCFG_OTP_WITH_PUF
endif

ifeq ($(TE_OTP_PUF),y)
TE_CFLAGS      += -DCFG_OTP_PUF_STB_EN
endif

ifeq ($(TE_DYNCLK_CTL),y)
TE_CFLAGS      += -DCFG_TE_DYNCLK_CTL
endif

ifeq ($(ACA_BLINDING_EN),y)
TE_CFLAGS      += -DCFG_ACA_BLINDING_EN
endif

ifeq ($(TE_KASUMI),y)
TE_CFLAGS      += -DTE_KASUMI_C
endif

ifeq ($(TE_SNOW3G),y)
TE_CFLAGS      += -DTE_SNOW3G_C
endif

ifeq ($(TE_ZUC),y)
TE_CFLAGS      += -DTE_ZUC_C
endif
