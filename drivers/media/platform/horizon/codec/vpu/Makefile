# Comment/uncomment the	following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O	-g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

ENABLE_PRIORITY = 1

#EXTRA_CFLAGS +=	$(DEBFLAGS) -I$(LDDINCDIR)
ccflags-y += $(DEBFLAGS) -I$(LDDINCDIR) -DCONFIG_PM
ccflags-y += -I$(srctree)/drivers/osal/linux/inc/
ccflags-y += -I$(srctree)/drivers/staging/android/ion/
ccflags-y +=  -I$(srctree)/drivers/smmu/

hobot_vpu-objs := hobot_vpu_dev.o
hobot_vpu-objs += hobot_vpu_ctl.o hobot_vpu_pm.o

ifeq ($(ENABLE_PRIORITY), 1)
	ccflags-y += -DPRIORITY
    hobot_vpu-objs += hobot_vpu_prio.o
endif

obj-$(CONFIG_VIDEO_HOBOTC_VPU)	:= hobot_vpu.o

ccflags-y += -D _LINUX_KERNEL_MODE
ifeq ($(CONFIG_HOBOT_GCOV_AVIO),y)
GCOV_PROFILE := y
endif

all: FORCE
	make -C $(OUT_DIR) M=$(PWD) modules

clean: FORCE
	make -C $(OUT_DIR) M=$(PWD) clean

FORCE:

