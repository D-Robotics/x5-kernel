/*
 * Copyright 2022, Horizon Robotics
 */

#ifndef __OSAL_COMMON_FIFO_H__
#define __OSAL_COMMON_FIFO_H__

#ifdef OSAL_POSIX_TEST
#include "../test/osal_common_test.h"
#endif

#ifdef OSAL_FREERTOS
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "osal_debug.h"
#include "osal_alloc.h"
#include "osal_spinlock.h"
#include "FreeRTOS.h"

#ifndef EINVAL
#define EINVAL pdFREERTOS_ERRNO_EINVAL
#endif

#ifndef ENOMEM
#define ENOMEM pdFREERTOS_ERRNO_ENOMEM
#endif

#ifndef EFAULT
#define EFAULT pdFREERTOS_ERRNO_EFAULT
#endif

// return 0 if success
#define copy_from_user(to, from, n) (memcpy(to, from, n), 0)
#define copy_to_user(to, from, n) (memcpy(to, from, n), 0)

// define two empty macro for compatity
#define smp_wmb() __sync_synchronize()
#ifndef __user
#define __user
#endif
#endif

#define osal_cmn_likely(x)       __builtin_expect((x), 1)
#define osal_cmn_unlikely(x)     __builtin_expect((x), 0)

typedef struct osal_fifo {
	uint32_t	in;
	uint32_t	out;
	uint32_t	mask;
	void		*data;
} osal_fifo_t;

#define CHECK_OR_RETURN(cond, ret) \
	if (!(cond)) { \
		osal_pr_err("osal: %s:%d %s check failed\n", __func__, __LINE__, #cond); \
		return ret; \
	}


/**
 * @def OSAL_KMALLOC_KERNEL
 * Mainly used for Linux, tell the allocator to use GFP_KERNEL
 *
 * @def OSAL_KMALLOC_ATOMIC
 * Mainly used for Linux, tell the allocator to use GFP_ATOMIC
 */
#ifndef OSAL_KMALLOC_KERNEL
#define OSAL_KMALLOC_KERNEL 0x1
#define OSAL_KMALLOC_ATOMIC 0x2
#endif

extern int32_t osal_cmn_fifo_init(osal_fifo_t *fifo, void *buffer, uint32_t size);
extern int32_t osal_cmn_fifo_alloc(osal_fifo_t *fifo, uint32_t size, uint32_t flags);
extern void osal_cmn_fifo_free(osal_fifo_t *fifo);
extern uint32_t osal_cmn_fifo_in(osal_fifo_t *fifo, void *buf, uint32_t len);
extern uint32_t osal_cmn_fifo_out(osal_fifo_t *fifo, void *buf, uint32_t len);
extern int32_t osal_cmn_fifo_copy_from_user(struct osal_fifo *fifo, const void __user *from, uint32_t len, uint32_t *copied);
extern int32_t osal_cmn_fifo_copy_to_user(struct osal_fifo *fifo, void __user *to, uint32_t len, uint32_t *copied);
extern bool osal_cmn_fifo_initialized(osal_fifo_t *fifo);
extern uint32_t osal_cmn_fifo_size(osal_fifo_t *fifo);
extern void osal_cmn_fifo_reset(osal_fifo_t *fifo);
extern uint32_t osal_cmn_fifo_len(osal_fifo_t *fifo);
extern bool osal_cmn_fifo_is_empty(osal_fifo_t *fifo);
extern bool osal_cmn_fifo_is_empty_spinlocked(osal_fifo_t *fifo, osal_spinlock_t *lock);
extern bool osal_cmn_fifo_is_full(osal_fifo_t *fifo);
extern uint32_t osal_cmn_fifo_in_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t n, osal_spinlock_t *lock);
extern uint32_t osal_cmn_fifo_out_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t n, osal_spinlock_t *lock);


#endif
