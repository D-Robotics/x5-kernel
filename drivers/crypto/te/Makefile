# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 ARM Technology (China) Co., Ltd.

GCOV_PROFILE := y
PATH_TAG := shared
TOP_DIR  := $(shell echo $(realpath $(srctree)/$(src)/merak_driver) | sed "s@/$(PATH_TAG)/.*@@g")

CUR_SRC_DIR   := $(srctree)/$(src)
TE_SW_TOP     := $(srctree)/$(src)/merak_driver

# use local path for hosal headers
override SQLIST_DIR     := $(CUR_SRC_DIR)/hosal/misc/sqlist
override HOSAL_API_DIR := $(CUR_SRC_DIR)/hosal/apis

include $(TE_SW_TOP)/te_common.mk
-include $(TE_SW_TOP)/hwa/res.mk

TE_OBJECTS     = $(patsubst %.c,%.o, \
                     $(patsubst $(CUR_SRC_DIR)/%,%,$(TE_SOURCES)))

TE_CLEAN_FILES  := $(patsubst $(CUR_SRC_DIR)/%,%,$(TEGENFILES))
TE_CLEAN_DIRS   := $(patsubst $(CUR_SRC_DIR)/%,%,$(TE_AUTO_GEN))

clean-files  := $(TE_CLEAN_FILES)
clean-dirs   := $(TE_CLEAN_DIRS)

ccflags-y  =                                                \
             $(TE_CFLAGS)                                   \
             -DCFG_TE_IRQ_EN                                \
             -DCFG_TE_ASYNC_EN                              \
             -DOSAL_ENV_LINUX_KERNEL                        \
             -DOSAL_LOG_PREFIX_NAME=\"KERNEL\ \"            \
             -DOSAL_MAX_LOG_LEVEL=$(OSAL_MAX_LOG_LEVEL)     \
             -I$(srctree)/$(src)/hosal/apis/osal            \
             -I$(srctree)/$(src)/hosal/src/osal/common      \
             -I$(srctree)/$(src)/hosal/apis/hal

# OSAL log level (0 ~ 4)
OSAL_MAX_LOG_LEVEL  := 2

# TE crypto driver
obj-$(CONFIG_CRYPTO_DEV_TE) += te_crypto.o

HOSAL_OBJS :=  hosal/src/hal/envs/linux_kernel/hal_trng.o          \
               hosal/src/osal/common/osal_log.o                    \
               hosal/src/osal/common/osal_assert.o                 \
               hosal/src/osal/common/osal_mem.o                    \
               hosal/src/osal/common/osal_common.o                 \
               hosal/src/osal/envs/linux_kernel/osal_log.o         \
               hosal/src/osal/envs/linux_kernel/osal_cache.o       \
               hosal/src/osal/envs/linux_kernel/osal_mem.o         \
               hosal/src/osal/envs/linux_kernel/osal_atomic.o      \
               hosal/src/osal/envs/linux_kernel/osal_completion.o  \
               hosal/src/osal/envs/linux_kernel/osal_interrupt.o   \
               hosal/src/osal/envs/linux_kernel/osal_mem_barrier.o \
               hosal/src/osal/envs/linux_kernel/osal_mutex.o       \
               hosal/src/osal/envs/linux_kernel/osal_spin_lock.o   \
               hosal/src/osal/envs/linux_kernel/osal_thread.o      \
               hosal/src/osal/envs/linux_kernel/osal_util.o

te_crypto-y := $(TE_OBJECTS)                                \
               $(HOSAL_OBJS)                                \
               lca_te_aead.o                                \
               lca_te_buf_mgr.o                             \
               lca_te_cipher.o                              \
               lca_te_driver.o                              \
               lca_te_hash.o                                \
               lca_te_otp.o                                 \
               lca_te_trng.o                                \
               lca_te_akcipher.o                            \
               lca_te_kpp.o

