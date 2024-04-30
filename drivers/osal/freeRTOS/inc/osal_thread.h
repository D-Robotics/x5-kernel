/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_THREAD_H__
#define __OSAL_THREAD_H__

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#define MAX_THREAD_NAME_LEN 20

typedef int osal_thread_entry_t(void* data);

typedef enum {
    OSAL_ATTR_SCHED_POLICY,
    OSAL_ATTR_SCHED_PRIORITY,
    OSAL_ATTR_AFFINITY,
    OSAL_ATTR_STACKSIZE,
} osal_thread_attr_type_t;

typedef struct {
    uint8_t sched_policy;
    uint8_t priority;
    uint8_t affinity;
    uint32_t stack_size;
    char name[MAX_THREAD_NAME_LEN];
}  osal_thread_attr_t;

typedef struct {
    TaskHandle_t handle;
    osal_thread_attr_t attr;
} osal_thread_t;

/**
 * @brief Start a thread, allocating any resources required.
 *
 * @param[in] res simple to access initial caps, vka to allocate objects, vspace to manage vspaces
 * @param[in] attr config thread, including sched attributes, cpu affinity, stack size or others
 * @param[in] entryFunc the address that the thread will start at
 * @param[in] param a pointer to the arguments struct for this thread.
 * @param[out] res thread instance, including thread data structure
 *
 * @return 0 on success, an error code on failure.
 */
extern int osal_thread_init(osal_thread_t *res, osal_thread_attr_t *attr,
                        osal_thread_entry_t entryFunc, const void *param);
/**
 * @brief Release any resources used by this thread. The thread instance will not be usable
 *
 * @param[in] p  thread instance, including thread data structure
 *
 * @return 0 on success, an error code on failure.
 */
extern int osal_thread_destory(osal_thread_t *p);

/**
 * @brief get the unique thread identification
 * 
 * @return identification of this component.
 */
extern uint64_t osal_thread_self(void);
/**
 * @brief start the thread
 *
 * @param[in] p  thread instance
 *
 * @return 0 on success, an error code on failure.
 */
extern int osal_thread_start(osal_thread_t *p);
/**
 * @brief Configure a thread
 *
 * @param[in] p thread instance, including thread data structure
 * @param[in] type attributes choices including sched attributes, cpu affinity, stack size or others
 * @param[in] value attribute value to config
 *
 * @return 0 on success, an error code on failure.
 */
extern int osal_thread_config_set(osal_thread_t *p, const osal_thread_attr_type_t type, const osal_thread_attr_t *pattr);

/**
 * @brief get the thread management struct
 *
 * @return osal thread management struct, NULL on failure.
 */
extern osal_thread_t* os_get_current(void);
#endif //__OSAL_THREAD_H__
