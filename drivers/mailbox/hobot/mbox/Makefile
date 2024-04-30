# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Hobot mailbox drivers
#

CURRENT_PATH = ${srctree}/drivers/mailbox/hobot/mbox/

ccflags-y += -I${srctree}/include/ -I${srctree}/drivers/mailbox
ccflags-y += -I${CURRENT_PATH}/common/
ccflags-y += -I${CURRENT_PATH}/os/kernel/
ccflags-y += -I${CURRENT_PATH}/platform/cpusys/
# ccflags-y += -DDEBUG

hobot_mbox-objs := common/mbox_common.o os/kernel/mbox_os.o
obj-$(CONFIG_HB_MBOX_PL320)	+= hobot_mbox.o
obj-$(CONFIG_HB_MBOX_PL320)     += mbox_test.o
