/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_SPINLOCK_H__
#define __OSAL_SPINLOCK_H__

#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

typedef struct {
    SemaphoreHandle_t semaphore;
    UBaseType_t saved_priority;
} osal_spinlock_t;

extern void osal_spin_init(osal_spinlock_t *lock);
extern void osal_spin_lock(osal_spinlock_t *lock);
extern void osal_spin_unlock(osal_spinlock_t *lock);

// Linux 里irqsave的flag是宏传递的，会修改，这里不能定义成宏，就得传地址了
extern void osal_spin_lock_irqsave(osal_spinlock_t *lock, uint64_t *flag);
extern void osal_spin_unlock_irqrestore(osal_spinlock_t *lock, uint64_t *flag);

#endif //__OSAL_SPINLOCK_H__
