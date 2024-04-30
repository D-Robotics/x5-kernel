subdir-ccflags-y += -I$(srctree)/drivers/staging/android/ion/
subdir-ccflags-y += -I$(srctree)/drivers/iommu/
ccflags-y +=  -I$(srctree)/drivers/iommu/
ccflags-y +=  -I$(srctree)/drivers/devfreq/

obj-$(CONFIG_BPU)		+= bpu_framework.o
bpu_framework-objs		:=  bpu_drv.o bpu_ctrl.o \
							bpu_debug.o bpu_prio.o \
							bpu_sched.o bpu_stat.o
obj-$(CONFIG_BPU_ONDEMAND_GOVERNOR) += bpu_governor.o
bpu_cores-y := bpu_core.o 
obj-$(CONFIG_BPU_CORE)	+= bpu_cores.o

ifeq ($(CONFIG_HOBOT_GCOV_AVIO),y)
GCOV_PROFILE := y
endif
