//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_defines.h>
#include <hwa/te_hwa.h>
#include <driver/te_drv.h>
#include "drv_sess.h"
#include "drv_sess_internal.h"

#define FILL_CQ_THRESH  5U

void te_sess_ca_prepare_task( int32_t slotid,
                              uint32_t *cmdptr,
                              uint32_t cmdlen,
                              te_sess_ca_tsk_t *tsk )
{
    TE_ASSERT( tsk != NULL );
    TE_ASSERT( (cmdptr != NULL) && (cmdlen != 0) );
    TE_ASSERT( (slotid >= 0) && (slotid < MAX_SLOT_NUM) );

    tsk->slotid = slotid;
    tsk->cmdptr = cmdptr;
    tsk->cmdlen = cmdlen;
    tsk->offs = 0;

    /* Fixup slotid */
    SET_SLOTID( cmdptr, slotid );

    return;
}

int te_sess_ca_submit( te_sess_cmd_agent_t *ca,
                       te_sess_ca_tsk_t *tsk,
                       te_sess_ea_item_t *it )
{
    uint32_t ret = TE_SUCCESS;
    unsigned long flags = 0;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_event_agent_t *ea = NULL;
    sqlist_t *list = NULL;

    TE_ASSERT( ca != NULL );
    TE_ASSERT( tsk != NULL );
    TE_ASSERT( it != NULL );
    TE_ASSERT( (tsk->cmdptr != NULL) && (tsk->cmdlen != 0) );
    TE_ASSERT( (tsk->slotid >= 0) && (tsk->slotid < MAX_SLOT_NUM) );
    TE_ASSERT( tsk->offs == 0 );

    mctx = ca->mctx;
    ea = mctx->event_agent;
    list = &ca->queues[tsk->slotid];

    OSAL_LOG_DEBUG( "Command Agent[%s]: submit cmd(%02x) to slot(%d) \n",
                    (mctx->ishash ? "hash" : "sca"), CMDID(tsk->cmdptr), tsk->slotid );

    ret = te_sess_ea_book_event( ea, it );
    if ( ret != TE_SUCCESS ) {
        goto out;
    }

    te_sess_module_clk_get( mctx );

    osal_spin_lock_irqsave( &ca->lock, &flags );
    sqlist_enqueue( list, &tsk->list );
    osal_spin_unlock_irqrestore( &ca->lock, flags );

    /* Kick off CQ fill */
    te_sess_ca_fill( ca );

#ifndef CFG_TE_IRQ_EN
    /* Kick off ea dispatcher */
    te_sess_ea_dispatch_event( ea );
#endif
out:
    return ret;
}

int te_sess_ca_cancel( te_sess_cmd_agent_t *ca,
                       te_sess_ca_tsk_t *tsk )
{
    int32_t ret = TE_ERROR_NO_DATA;
    unsigned long flags = 0;
    sqlist_t *list = NULL;
    te_sess_ca_tsk_t *task = NULL, *next = NULL;

    TE_ASSERT( ca != NULL );
    TE_ASSERT( tsk != NULL );
    TE_ASSERT( (tsk->slotid >= 0) && (tsk->slotid < MAX_SLOT_NUM) );

    list = &ca->queues[tsk->slotid];

    osal_spin_lock_irqsave( &ca->lock, &flags );
    SQLIST_FOR_EACH_CONTAINER_SAFE(list, task, next, list) {
        if ( task == tsk ) {
            /* Dequeue this task */
            sqlist_remove( &task->list );
            ret = TE_SUCCESS;
            break;
        }
    }
    osal_spin_unlock_irqrestore( &ca->lock, flags );

    return ret;
}

static void sess_ca_wm_en( te_sess_cmd_agent_t *ca )
{
#ifdef CFG_TE_IRQ_EN
    int ret = TE_SUCCESS;
    te_sess_module_ctx_t *mctx = NULL;
    te_hwa_sca_t *hwa = NULL;

    mctx = ca->mctx;
    hwa = (te_hwa_sca_t *)mctx->hwa;
    ret = hwa->cqwm_ctrl( hwa, true );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                   "Fatal error, can't enable %s cqwm intr!\n",
                   (mctx->ishash ? "hash" : "sca") );
#else
    (void)ca;
#endif
}

void te_sess_ca_fill( te_sess_cmd_agent_t *ca )
{
    unsigned long flags = 0;
    int ret = TE_SUCCESS;
    te_sess_module_ctx_t *mctx = NULL;
    te_hwa_sca_t *hwa = NULL;
    te_sca_stat_t stat = { 0 };
    uint32_t roomsz = 0, fillsz = 0, cmd = 0;
    int32_t qidx = 0;
    uint32_t *p = NULL;
    te_sess_ca_tsk_t *tsk = NULL;

    TE_ASSERT( ca != NULL );
    mctx = ca->mctx;
    hwa = (te_hwa_sca_t *)mctx->hwa;

    osal_spin_lock_irqsave( &ca->lock, &flags );

    /*Get CQ room info */
    ret = hwa->state(hwa, &stat);
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't get host stat\n");
    if ( stat.cq_avail_slots < ca->cq_thresh ) {
        goto out; /* Wait */
    }

    roomsz = ( stat.cq_avail_slots * 4 );
    OSAL_LOG_TRACE( "Command Agent[%s]: CQ room: %d \n",
                    (mctx->ishash ? "hash" : "sca"), roomsz );
    /*
     * if cur task not done, fill CQ, directly.
     * if cur task done, shift to next Q, and fetch first task
     * if no room in CQ, end.
     */
    qidx = ca->qidx;

    while (roomsz) {
        if ( ca->cur == NULL ) {
            ca->qidx = ((ca->qidx + 1) % MAX_SLOT_NUM);
            ca->cur = sqlist_dequeue( &ca->queues[ca->qidx] );
            /* if we already go through all of Q, just break, no task pending */
            if ( (qidx == ca->qidx) && (ca->cur == NULL) ) {
                break;
            }

            if (ca->cur != NULL) {
                tsk = SQLIST_CONTAINER( ca->cur, tsk, list );
                OSAL_LOG_TRACE( "CA: submit cmd(%02x), cmdptr(%p) to slot(%d) \n",
                                CMDID(tsk->cmdptr), tsk->cmdptr, SLOTID(tsk->cmdptr) );
            }
            continue;
        }

        tsk = SQLIST_CONTAINER( ca->cur, tsk, list );
        fillsz = ( (roomsz > (tsk->cmdlen - tsk->offs)) ?
                        (tsk->cmdlen - tsk->offs) : roomsz );
        p = (uint32_t *)( (uintptr_t)tsk->cmdptr + tsk->offs );

        /* fill CQ */
        if ( 0 == tsk->offs ) {
            /* fill function part */
            uint32_t func_len = sizeof(*p); /* one word */
            ret = hwa->cq_write_func( hwa, *p );
            TE_ASSERT_MSG( (ret == TE_SUCCESS),
                             "Fatal error, CQ_FUNC can't write\n" );

            tsk->offs += func_len;
            roomsz -= func_len;
            fillsz -= func_len;
            p += 1;
        }
        if (fillsz) {
            /* fill parameters part */
            ret = hwa->cq_write_para( hwa, p, fillsz );
            TE_ASSERT_MSG( (ret == TE_SUCCESS),
                             "Fatal error, CQ_PARA can't write\n" );

            tsk->offs += fillsz;
            roomsz -= fillsz;
        }

        /* Current task item has not been finished, continue */
        if ( (tsk->cmdlen - tsk->offs) != 0 ) {
            /* Enable water mark interrupt trigger next fill */
            if ( cmd == 0 ) {
                sess_ca_wm_en( ca );
            }
            break;
        }

        /* Current task has been done */
        tsk = NULL;
        ca->cur = NULL;
        /* Indicate how many commands we filled in this fill call*/
        cmd++;
    }

out:
    osal_spin_unlock_irqrestore( &ca->lock, flags );
    return;
}

int te_sess_ca_init( te_sess_module_ctx_t *mctx, uint32_t cq_depth )
{
    int ret = TE_SUCCESS;
    int i = 0;
    te_sess_cmd_agent_t *ca = NULL;

    TE_ASSERT( mctx != NULL );

    ca = (te_sess_cmd_agent_t *)osal_calloc( 1, sizeof(te_sess_cmd_agent_t) );
    if (ca == NULL) {
        ret = TE_ERROR_OOM;
        goto err;
    }

    ret = osal_spin_lock_init( &ca->lock );
    if ( ret != OSAL_SUCCESS ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    for ( i = 0; i < MAX_SLOT_NUM; i++ ) {
        sqlist_init( &ca->queues[i] );
    }

    ca->cq_thresh = UTILS_MIN(FILL_CQ_THRESH, (cq_depth - TE_CQ_WM + 1U));
    ca->mctx = mctx;
    mctx->cmd_agent = ca;

    return ret;
err1:
    osal_free( ca );
err:
    return ret;
}

int te_sess_ca_destroy( te_sess_module_ctx_t *mctx )
{
    int ret = TE_SUCCESS;
    te_sess_cmd_agent_t *ca = NULL;

    TE_ASSERT( mctx && mctx->cmd_agent );
    ca = mctx->cmd_agent;
    osal_spin_lock_destroy( &ca->lock );
    osal_free( ca );
    mctx->cmd_agent = NULL;

    return ret;
}

