# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Hobot ipc base share memory drivers
#

MODULE_NAME := hobot_ipc
UIO_MODULE_NAME := hobot_ipc_uio
HAL_MODULE_NAME := hobot_ipc_hal

CURRENT_PATH = ${srctree}/drivers/mailbox/hobot/ipc/

ccflags-y += -I${srctree}/include/
ccflags-y += -I${CURRENT_PATH}/common/
ccflags-y += -I${CURRENT_PATH}/os/kernel/
ccflags-y += -I${CURRENT_PATH}/platform/cpusys/
ccflags-y += -I${CURRENT_PATH}/../mbox/platform/cpusys/
# ccflags-y += -DDEBUG

$(MODULE_NAME)-objs := common/ipc_shm.o common/ipc_queue.o common/hb_ipc.o
$(MODULE_NAME)-objs += os/kernel/ipc_os.o
$(UIO_MODULE_NAME)-y := os/kernel/ipc_uio.o
$(HAL_MODULE_NAME)-y := os/kernel/ipc_hal.o

obj-$(CONFIG_HB_IPC_SHM) += $(MODULE_NAME).o
obj-$(CONFIG_HB_IPC_SHM) += ipc_test.o
#obj-$(CONFIG_HB_IPC_SHM) += $(UIO_MODULE_NAME).o
obj-$(CONFIG_HB_IPC_SHM) += $(HAL_MODULE_NAME).o
