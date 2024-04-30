# SPDX-License-Identifier: GPL-2.0
#

osal_test-y :=  osal_linux_test.o   \
					osal_test_alloc.o   \
					osal_test_atomic.o  \
					osal_test_bitops.o  \
					osal_test_cache.o   \
					osal_test_complete.o\
					osal_test_debug.o   \
					osal_test_fifo.o    \
					osal_test_io.o      \
					osal_test_list.o    \
					osal_test_lock.o    \
					osal_test_thread.o  \
					osal_test_time.o	\
					osal_test_sem.o		\
					osal_test_waitqueue.o

obj-m = osal_test.o

ccflags-y += -I${PWD}/../inc

ifeq ($(CONFIG_HOBOT_GCOV_BASE),y)
GCOV_PROFILE := y
endif

OUTPUT_BUILD_DIR=${TARGET_OUT_DIR}/build/

all: FORCE
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) modules

clean:
	make -C ${OUTPUT_BUILD_DIR}/kernel M=$(PWD) clean

FORCE:
