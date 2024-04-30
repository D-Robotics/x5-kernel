//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <te_common.h>
#include "te_worker.h"

/* TEWORKER COMMAND */
#define TEWORKER_CMD_NONE    (0)
#define TEWORKER_CMD_QUIT    (1)

static void te_worker_destroy(te_worker_t *worker)
{
    osal_thread_destroy(worker->wthread);
    osal_completion_destroy(&worker->bell);
    osal_spin_lock_destroy(&worker->lock);
    osal_free(worker);
    return;
}

static osal_err_t te_worker_thread_entry(void *arg)
{
    unsigned long flags = 0;
    te_worker_t *worker = (te_worker_t *)arg;
    te_worker_task_t *task = NULL;
    sqlist_t *list = NULL;

    while (1) {

        osal_spin_lock_irqsave(&worker->lock, &flags);
        worker->state = TEWORKER_ST_RUNNING;

        /* should we quit */
        if (worker->command == TEWORKER_CMD_QUIT) {
            worker->state = TEWORKER_ST_STOPPED;
            osal_spin_unlock_irqrestore(&worker->lock, flags);
            break;
        }

        /* Do we have task to handle ? */
        list = sqlist_dequeue(&worker->tasks);

        osal_spin_unlock_irqrestore(&worker->lock, flags);

        if (list == NULL) {
            osal_spin_lock_irqsave(&worker->lock, &flags);
            worker->state = TEWORKER_ST_SLEEPING;
            osal_spin_unlock_irqrestore(&worker->lock, flags);
            OSAL_COMPLETION_COND_WAIT((!sqlist_is_empty(&worker->tasks) ||
                                       worker->command != TEWORKER_CMD_NONE),
                                       &worker->bell);
            continue;
        }

        task = SQLIST_CONTAINER(list, task, list);
        task->execute(task);
        task = NULL;
    }

    return OSAL_SUCCESS;
}

te_worker_t *te_worker_init(void)
{
    int ret = TE_ERROR_GENERIC;
    te_worker_t *worker = NULL;

    worker = (te_worker_t *)osal_calloc(1, sizeof(te_worker_t));
    if (worker == NULL) {
        return NULL;
    }

    sqlist_init(&worker->tasks);
    worker->command = TEWORKER_CMD_NONE;
    worker->state = TEWORKER_ST_STOPPED;

    ret = osal_spin_lock_init(&worker->lock);
    if (ret != OSAL_SUCCESS) {
        goto err1;
    }

    ret = osal_completion_init(&worker->bell);
    if (ret != OSAL_SUCCESS) {
        goto err2;
    }

    ret = osal_thread_create(&worker->wthread,
                              te_worker_thread_entry,
                              (void *)worker);
    if (ret != OSAL_SUCCESS) {
        goto err3;
    }

    return worker;

err3:
    osal_completion_destroy(&worker->bell);
err2:
    osal_spin_lock_destroy(&worker->lock);
err1:
    osal_free(worker);
    return NULL;
}

static void te_worker_send_command(te_worker_t *worker,
                                                uint32_t command)
{
    unsigned long flags = 0;

    osal_spin_lock_irqsave(&worker->lock, &flags);

    worker->command = command;
    osal_completion_signal(&worker->bell);

    osal_spin_unlock_irqrestore(&worker->lock, flags);

    return;
}

void te_worker_enqueue(te_worker_t *worker, te_worker_task_t *task)
{
    unsigned long flags = 0;

    TE_ASSERT(worker != NULL);
    TE_ASSERT(task != NULL);
    osal_spin_lock_irqsave(&worker->lock, &flags);

    sqlist_enqueue(&worker->tasks, &task->list);
    osal_completion_signal(&worker->bell);

    osal_spin_unlock_irqrestore(&worker->lock, flags);

    return;
}

te_worker_st_t te_worker_state(te_worker_t *worker)
{
    return worker->state;
}

void te_worker_quit(te_worker_t *worker)
{
    TE_ASSERT(worker != NULL);
    te_worker_send_command(worker, TEWORKER_CMD_QUIT);
    osal_wait_thread_done(worker->wthread);
    te_worker_destroy(worker);
    return;
}

