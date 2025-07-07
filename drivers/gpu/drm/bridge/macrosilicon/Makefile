# SPDX-License-Identifier: GPL-2.0-only

ABS_PATH := $(shell pwd)/../../../kernel

obj-$(CONFIG_DRM_MS7210) += macrosilicon-ms7210.o
macrosilicon-ms7210-y := ms-ms7210.o \
				mpi.o \
				ms7210/ms7210.o \
				ms7210/misc/ms7210_mpi.o \
				ms7210/misc/ms7210_drv_misc.o \
				ms7210/hdmi_tx/ms7210_drv_hd_tx.o \
				ms7210/dvin/ms7210_drv_dvin.o

ccflags-y += -I$(ABS_PATH)/$(src)/ms7210/inc \
	-I$(ABS_PATH)/$(src)/ms7210/dvin \
	-I$(ABS_PATH)/$(src)/ms7210/hdmi_tx \
	-I$(ABS_PATH)/$(src)/ms7210/misc

ccflags-y += -Wno-error=parentheses
#ccflags-y += -DDEBUG

