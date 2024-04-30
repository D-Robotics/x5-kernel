# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_ION) += ion.o ion_heap.o
obj-$(CONFIG_ION_SYSTEM_HEAP) += ion_system_heap.o ion_page_pool.o
obj-$(CONFIG_ION_CARVEOUT_HEAP) += ion_carveout_heap.o
obj-$(CONFIG_ION_CHUNK_HEAP) += ion_chunk_heap.o
obj-$(CONFIG_ION_CMA_HEAP) += ion_cma_heap.o
obj-$(CONFIG_ION_HOBOT) += hobot_ion.o
#obj-$(CONFIG_ION_HOBOT) += hobot_ion_device.o
#hobot_ion_device-objs-y += hobot_ion.o
#hobot_ion_device-objs-y += ion.o
#hobot_ion_device-objs-$(CONFIG_ION_CMA_HEAP) += ion_cma_heap.o
#hobot_ion_device-objs-$(CONFIG_ION_CARVEOUT_HEAP) += ion_carveout_heap.o
#hobot_ion_device-objs-$(CONFIG_ION) += ion_heap.o
#hobot_ion_device-objs := $(hobot_ion_device-objs-y)

ifeq ($(CONFIG_HOBOT_GCOV_BASE),y)
GCOV_PROFILE := y
endif

all: FORCE
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) modules

clean:
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) clean

FORCE:
