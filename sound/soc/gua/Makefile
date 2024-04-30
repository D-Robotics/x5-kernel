# SPDX-License-Identifier: GPL-2.0-only
# GUA platform support

HBIPC_PATH = ${srctree}/drivers/mailbox/hobot/ipc/
HBMOX_PATH = ${srctree}/drivers/mailbox/hobot/mbox/

ccflags-y += -I${srctree}/include/
ccflags-y += -I${srctree}/drivers/mailbox

ccflags-y += -I${HBIPC_PATH}/common/
ccflags-y += -I${HBIPC_PATH}/os/kernel/
ccflags-y += -I${HBIPC_PATH}/platform/cpusys/

ccflags-y += -I${srctree}/drivers/mailbox
ccflags-y += -I${HBMOX_PATH}/common/
ccflags-y += -I${HBMOX_PATH}/os/kernel/
ccflags-y += -I${HBMOX_PATH}/platform/cpusys/

snd-soc-gua-objs := gua_audio_pipeline.o gua-cpu-dai.o gua-machine.o gua-platform.o
obj-$(CONFIG_SND_SOC_GUA) += ipc_wrapper.o gua_audio_pipeline.o gua-cpu-dai.o gua-machine.o gua-platform.o
obj-$(CONFIG_SND_SOC_GUA) += gua_misc_drv.o
