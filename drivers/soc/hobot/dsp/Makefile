# SPDX-License-Identifier: GPL-2.0
#
# Hobot VDSP Acore Drivers
#
ccflags-y += -I${srctree}/drivers/remoteproc
ccflags-y += -I${srctree}/drivers/staging/android/ion
ccflags-y += -I${srctree}/drivers/iommu

HBIPC_PATH = ${srctree}/drivers/mailbox/hobot/ipc/
HBMOX_PATH = ${srctree}/drivers/mailbox/hobot/mbox/

ccflags-y += -I${srctree}/include/
ccflags-y += -I${srctree}/drivers/mailbox

ccflags-y += -I${HBIPC_PATH}/common/
ccflags-y += -I${HBIPC_PATH}/os/kernel/
ccflags-y += -I${HBIPC_PATH}/platform/cpusys/
ccflags-y += -I${HBMBOX_PATH}/platform/cpusys/
ccflags-y += -I${HBMOX_PATH}/common/
ccflags-y += -I${HBMOX_PATH}/os/kernel/
ccflags-y += -I${HBMOX_PATH}/platform/cpusys/

ccflags-y += -I${srctree}/sound/soc/gua/

obj-$(CONFIG_HOBOT_DSP) += hobot_dsp.o
hobot_dsp-objs += hobot_dsp_dev.o

all: FORCE
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) modules

clean:
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) clean

FORCE:
