# Comment/uncomment the	following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O	-g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

#EXTRA_CFLAGS +=	$(DEBFLAGS) -I$(LDDINCDIR)
ccflags-y +=	$(DEBFLAGS) -I$(LDDINCDIR) -DCONFIG_PM
ccflags-y += -I$(srctree)/drivers/media/platform/horizon/codec/vpu/
ccflags-y += -I$(srctree)/drivers/osal/linux/inc/
ccflags-y += -I$(srctree)/drivers/staging/android/ion/
ccflags-y +=  -I$(srctree)/drivers/smmu/

hobot_jpu-objs := hobot_jpu_dev.o
hobot_jpu-objs += hobot_jpu_ctl.o hobot_jpu_pm.o

obj-$(CONFIG_VIDEO_HOBOTC_JPU)	:= hobot_jpu.o

ccflags-y += -D _LINUX_KERNEL_MODE
ifeq ($(CONFIG_HOBOT_GCOV_AVIO),y)
GCOV_PROFILE := y
endif

all: FORCE
	make -C $(OUT_DIR) M=$(PWD) modules

clean: FORCE
	make -C $(OUT_DIR) M=$(PWD) clean

FORCE:
