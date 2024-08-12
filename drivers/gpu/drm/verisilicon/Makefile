# SPDX-License-Identifier: GPL-2.0

VS_PATH = $(srctree)/$(src)
GC_PATH = $(srctree)/drivers/misc/verisilicon/gc820/
# GC_USER_PATH = $(srctree)/$(src)/../../../media/platform/horizon/hbn/n2d/nano2D
CAMSYS_PATH = $(srctree)/drivers/media/platform/horizon/camsys
OSAL_PATH = $(srctree)/drivers/osal
IOMMU_PATH = $(srctree)/drivers/iommu/
ccflags-y += \
		-I$(VS_PATH)/dc/dc_hw \
		-I$(VS_PATH)/dc/dc_proc \
		-I$(VS_PATH)/dc/vs_sif \
		-I$(VS_PATH)/dc \
		-I$(VS_PATH) \

ccflags-$(CONFIG_VERISILICON_DC8000_NANO) += \
		-I$(VS_PATH)/dc/dc_hw/dc8000nano

ccflags-$(CONFIG_VERISILICON_GEM_ION) += -I$(srctree)/drivers/staging/android/ion/

ccflags-$(CONFIG_VERISILICON_BT1120) += \
		-I$(VS_PATH)/dc_bt1120 \

ccflags-$(CONFIG_VERISILICON_GC_PROC_SUPPORT) += \
		-I$(GC_PATH)/nano2Dkernel \
		-I$(GC_PATH)/nano2Dkernel/linux \
		-I$(GC_PATH)/nano2D \
		-I$(GC_PATH)/nano2D/linux \
		-I$(GC_PATH)/inc \
		-I$(GC_PATH)/inc/shared \
		-I$(CAMSYS_PATH)/vpf \
		-I$(OSAL_PATH)/linux/inc \
		-I$(IOMMU_PATH) \
		-DUSE_LINUX_PCIE=0 \

vs_drm-objs := \
		dc/dc_hw/dc_hw.o \
		dc/dc_proc/dc_proc.o \
		dc/dc_proc/dc_hw_proc.o \
		dc/vs_dc.o \
		vs_crtc.o \
		vs_drv.o \
		vs_fb.o \
		vs_gem.o \
		vs_plane.o

vs_drm-$(CONFIG_VERISILICON_GC_PROC_SUPPORT) += dc/dc_proc/gc_proc.o

vs_drm-$(CONFIG_VERISILICON_DC8000_NANO) += dc/dc_info/dc_8000_nano_info.o \
					    dc/dc_hw/dc8000nano/dc_8000_nano.o

vs_drm-$(CONFIG_VERISILICON_BT1120) += dc_bt1120/vs_bt1120.o \
				       dc_bt1120/bt1120_bridge.o

vs_drm-$(CONFIG_VERISILICON_SIMPLE_ENCODER) += vs_simple_enc.o
vs_drm-$(CONFIG_VERISILICON_WRITEBACK) += vs_writeback.o
vs_drm-$(CONFIG_VERISILICON_WRITEBACK_SIF) += dc/dc_proc/sif_proc.o \
					      dc/vs_sif/vs_sif.o

vs_drm-$(CONFIG_VERISILICON_DW_MIPI_DSI) += dw_mipi_dsi.o

obj-$(CONFIG_DRM_VERISILICON) += vs_drm.o

GCOV_PROFILE := $(CONFIG_GCOV_KERNEL)
