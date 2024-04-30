# SPDX-License-Identifier: GPL-2.0
#
AQROOT := $(srctree)/$(src)
HOBOT_DRIVER_PATH := $(srctree)/drivers/
PLATFORM := x5
USE_ARCH64 := 1
OUTDIR := .
src_dir := nano2Dkernel
user_dir := nano2D

OBJECTS += ${src_dir}/nano2D_kernel.o
OBJECTS += ${src_dir}/nano2D_kernel_hardware.o
OBJECTS += ${src_dir}/nano2D_kernel_mmu.o
OBJECTS += ${src_dir}/linux/nano2D_kernel_os.o
OBJECTS += ${src_dir}/linux/nano2D_kernel_driver.o
OBJECTS += ${src_dir}/linux/platform/${PLATFORM}/gc_hal_kernel_platform_${PLATFORM}.o
OBJECTS += ${src_dir}/linux/nano2D_kernel_debugfs.o
OBJECTS += ${src_dir}/nano2D_kernel_vidmem.o
OBJECTS += ${src_dir}/nano2D_kernel_common.o
OBJECTS += ${src_dir}/nano2D_kernel_db.o
OBJECTS += ${src_dir}/nano2D_kernel_event.o

OBJECTS += $(user_dir)/nano2D.o \
           $(user_dir)/nano2D_user_hardware.o \
           $(user_dir)/nano2D_debug.o \
           $(user_dir)/nano2D_user_hardware_dec.o \
           $(user_dir)/nano2D_user_buffer.o \
           $(user_dir)/nano2D_user_query.o \
           $(user_dir)/nano2D_user.o \
           $(user_dir)/nano2D_user_hardware_source.o \
           $(user_dir)/nano2D_user_hardware_target.o \
           $(user_dir)/common.o \
           $(user_dir)/series_common.o \
           $(user_dir)/chip.o \
           $(user_dir)/linux/nano2D_user_os.o

ifeq ($(KERNELRELEASE),)

ARCH ?= arm64

PWD := $(shell pwd)

all:$(OUTDIR)
	@mkdir -p out
	@$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) V=$(V) modules
	@cp vio_n2d.ko out

clean:
	@rm -rf $(OBJECTS) *.o vio_n2d.ko vio_n2d.mod.c .tmp_versions out $(OUTDIR)/vio_n2d.ko
	@rm -rf modules.order Module.symvers
	@find $(AQROOT) -name ".*.cmd" | xargs rm -f

install:all
	@cp out/vio_n2d.ko $(OUTDIR)

$(OUTDIR):
	@mkdir -p $(OUTDIR)

.PHONY: modules clean install

else

obj-$(CONFIG_HOBOT_VIO_N2D) := vio_n2d.o
vio_n2d-objs := $(OBJECTS)

ccflags-y += -I$(AQROOT)/inc
ccflags-y += -I$(AQROOT)/inc/shared
ccflags-y += -I$(AQROOT)/nano2D
ccflags-y += -I$(AQROOT)/nano2D/linux
ccflags-y += -I$(AQROOT)/nano2Dkernel
ccflags-y += -I$(AQROOT)/nano2Dkernel/linux
ccflags-y += -I$(HOBOT_DRIVER_PATH)/media/platform/horizon/camsys/vpf
ccflags-y += -I$(HOBOT_DRIVER_PATH)/osal/linux/inc
ccflags-y += -I$(HOBOT_DRIVER_PATH)/iommu
ccflags-y += -I$(HOBOT_DRIVER_PATH)/ion

ifneq ($(wildcard $(AQROOT)/nano2D/nano2D_option.config),)
    -include $(AQROOT)/nano2D/nano2D_option.config
EXTRA_CFLAGS += ${PLATFORM_FLAGS}
endif

ifeq ($(USE_LINUX_PCIE), 1)
ccflags-y += -DUSE_LINUX_PCIE=1
else
ccflags-y += -DUSE_LINUX_PCIE=0
endif

ifeq ($(USE_ARM_VDK), 1)
ccflags-y += -DUSE_ARM_VDK=1
else
ccflags-y += -DUSE_ARM_VDK=0
endif

ifeq ($(USE_ARCH64), 1)
ccflags-y += -DUSE_ARCH64=1
else
ccflags-y += -DUSE_ARCH64=0
endif

#ccflags-y += -DNANO2D_MMU_ENABLE=1

GCOV_PROFILE := $(CONFIG_GCOV_KERNEL)

endif
