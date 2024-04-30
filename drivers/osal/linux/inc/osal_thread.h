/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_THREAD_H__
#define __OSAL_THREAD_H__

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <uapi/linux/sched/types.h>

#define OSAL_THREAD_DEFAULT_NAME   "osal_thread"
#define OSAL_THREAD_NAME_MAX_LEN   16

#define OSAL_THREAD_PRI_LOWEST     1
#define OSAL_THREAD_PRI_IRQ        50
#define OSAL_THREAD_PRI_HIGHEST    100

#define OSAL_THREAD_AFFINITY_NONE  0xff

typedef int32_t osal_threadfn_t(void* data);

#define OSAL_SCHED_NORMAL SCHED_NORMAL
#define OSAL_SCHED_FIFO   SCHED_FIFO
#define OSAL_SCHED_RR     SCHED_RR


typedef struct {
    uint32_t stack_size;
    uint8_t  sched_policy;
    uint8_t  priority;
    uint8_t  affinity;
    char     name[OSAL_THREAD_NAME_MAX_LEN];
}  osal_thread_attr_t;

typedef struct {
    struct task_struct *ptask;
} osal_thread_t;

/**
 * @brief create a thread and set attritubes
 *
 * @param[in]    thread: thread instance, can't be NULL
 * @param[in]      attr: attribute pointer, can be NULL
 * @param[in]  threadfn: thread function, can't be NULL
 * @param[in]      data: thread function parameter, can be NULL
 *
 * @retval =0: success
 * @retval <0: failure
 */
static inline int32_t osal_thread_init(osal_thread_t *thread,
                osal_thread_attr_t *attr, osal_threadfn_t threadfn, void *data)
{
    struct sched_attr lnx_attr;
    char *thread_name;

    memset(&lnx_attr, 0, sizeof(lnx_attr));

    if (NULL == attr) {
        lnx_attr.sched_policy = SCHED_NORMAL;
        lnx_attr.sched_nice = 0;
        thread_name = OSAL_THREAD_DEFAULT_NAME;
    } else {
        if (attr->sched_policy > SCHED_RR) {
            pr_err("sched policy only support NORMAL,FIFO,RR\n");
            return -EINVAL;
        }

        lnx_attr.sched_policy = attr->sched_policy;
        lnx_attr.sched_priority = attr->priority;

        thread_name = attr->name;
    }

    thread->ptask = kthread_create(threadfn, data, thread_name);

    if (IS_ERR(thread->ptask)) {
        pr_err("kthread create failed\n");
        return PTR_ERR(thread->ptask);
    }

    sched_setattr_nocheck(thread->ptask, &lnx_attr);

    return 0;
}

/**
 * @brief start to run the thread
 *
 * @param[in]  p: thread instance, can't be NULL
 *
 * @retval =0: success
 * @retval <0: failure
 */
static inline int32_t osal_thread_start(osal_thread_t *p)
{
    if (IS_ERR(p->ptask)) {
        pr_err("kthread create failed\n");
        return PTR_ERR(p->ptask);
    }
    wake_up_process(p->ptask);

    return 0;
}

/**
 * @brief check if the current thread should stop,
 *        use it as a condition of thread loop.
 *
 * @param[in]  p: thread instance, can't be NULL
 *
 * @retval 1: should stop
 * @retval 0: shouldn't stop
 */
static inline int32_t osal_thread_should_stop(void)
{
    return kthread_should_stop();
}

/**
 * @brief set thread should_stop status and wait the thread to exit
 *
 * @param[in]  p: thread instance, can't be NULL
 *
 * @retval : return the return value of thread function
 */
static inline int32_t osal_thread_destory(osal_thread_t *p)
{
    int32_t ret;

    if (IS_ERR(p->ptask)) {
        pr_err("kthread create failed\n");
        return PTR_ERR(p->ptask);
    }

    ret = kthread_stop(p->ptask);
    memset(p, 0, sizeof(*p));

    return ret;
}

#endif
