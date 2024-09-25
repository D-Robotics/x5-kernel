# SPDX-License-Identifier: GPL-2.0
#
# Hobot VDSP Acore Drivers
#
ccflags-y += -I$(srctree)/../hobot-drivers/remoteproc
subdir-ccflags-y += -I$(srctree)/../hobot-drivers/ion/
subdir-ccflags-y += -I$(srctree)/../hobot-drivers/smmu/
subdir-ccflags-y += -I$(srctree)/../hobot-drivers/rpmsg

ccflags-y += -I${srctree}/drivers/remoteproc
ccflags-y += -I${srctree}/drivers/staging/android/ion
ccflags-y += -I${srctree}/drivers/iommu
subdir-ccflags-y += -I$(srctree)/drivers/rpmsg

obj-$(CONFIG_HOBOT_VDSP) += hobot_vdsp.o
hobot_vdsp-objs += hobot_vdsp_dev.o
hobot_vdsp-objs += hobot_vdsp_sys.o

all: FORCE
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) modules

clean:
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) clean

FORCE:
