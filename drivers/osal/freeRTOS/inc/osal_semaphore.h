/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_SEMAPHORE_H__
#define __OSAL_SEMAPHORE_H__

#include "FreeRTOS.h"
#include "semphr.h"

typedef SemaphoreHandle_t osal_sem_t;

extern void osal_sema_init(osal_sem_t *sem, int32_t val);
extern void osal_sema_up(osal_sem_t *sem);
int32_t osal_sema_down_interruptible(osal_sem_t *sem);
int32_t osal_sema_down_timeout(osal_sem_t *sem, uint32_t time_ms);

#endif //__OSAL_SEMAPHORE_H__
