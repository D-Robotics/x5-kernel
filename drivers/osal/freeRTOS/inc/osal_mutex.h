/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_MUTEX_H__
#define __OSAL_MUTEX_H__

#include "FreeRTOS.h"
#include "semphr.h"

typedef SemaphoreHandle_t osal_mutex_t;

extern void osal_mutex_init(osal_mutex_t *lock);
extern void osal_mutex_lock(osal_mutex_t *lock);
extern void osal_mutex_unlock(osal_mutex_t *lock);
extern int32_t osal_mutex_trylock(osal_mutex_t *lock);

#endif //__OSAL_MUTEX_H__
