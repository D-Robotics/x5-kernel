//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_defines.h>
#include <hwa/te_hwa.h>
#include <driver/te_drv.h>
#include "drv_sess.h"
#include "drv_sess_internal.h"

#define     CSQ_EMPTY   0x00U
#define     INIT        0x80U
#define     PROC        0x40U
#define     FINISH      0x20U
#define     CLONE       0x08U
#define     WRAPOUT     0x04U
#define     UNWRAPIN    0x02U
#define     CLEAR       0xffU

#ifdef CFG_TE_IRQ_EN
static int te_sess_irqhandler( const uint32_t type, void *uparam )
{
    int ret = TE_SUCCESS;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_event_agent_t *ea = NULL;
    te_irq_type_t irqtype = { 0 };

    TE_ASSERT( uparam != NULL );
    mctx = (te_sess_module_ctx_t *)uparam;
    ea = mctx->event_agent;
    irqtype = ( (mctx->ishash) ? TE_IRQ_TYPE_HASH : TE_IRQ_TYPE_SCA );


    OSAL_LOG_TRACE( "Event Agent[%s]: Got Interrupt\n",
                    (mctx->ishash ? "hash" : "sca"));

    TE_ASSERT_MSG( (irqtype == type),
                     "Wrong IRQ type(%d) expected(%d)\n",
                     type,
                     irqtype );

    te_sess_ea_dispatch_event( ea );

    return ret;
}
#endif

void te_sess_ea_prepare_event( int32_t slotid,
                               uint32_t *cmdptr,
                               te_sess_notify_t notify,
                               te_sess_cb_para_t *para,
                               te_sess_ea_item_t *it )
{
    TE_ASSERT( it != NULL );
    TE_ASSERT( cmdptr != NULL );
    TE_ASSERT( notify != NULL );
    TE_ASSERT( (slotid >= 0) && (slotid < MAX_SLOT_NUM) );

    it->slotid = slotid;
    it->cmdptr = cmdptr;
    it->notify = notify;
    it->para = para;
    return;
}

int te_sess_ea_book_event( te_sess_event_agent_t *ea,
                           te_sess_ea_item_t *it )
{
    unsigned long flags = 0;
    sqlist_t *list = NULL;

    TE_ASSERT( ea != NULL );
    TE_ASSERT( it != NULL );
    TE_ASSERT( it->notify != NULL );
    TE_ASSERT( (it->slotid >= 0) && (it->slotid < MAX_SLOT_NUM) );

    list = &ea->queues[it->slotid];

    osal_spin_lock_irqsave( &ea->lock, &flags );
    sqlist_enqueue( list, &it->list );
    osal_spin_unlock_irqrestore( &ea->lock, flags );

    return TE_SUCCESS;
}

int te_sess_ea_cancel( te_sess_event_agent_t *ea,
                       te_sess_ea_item_t *it )
{
    int32_t ret = TE_ERROR_NO_DATA;
    unsigned long flags = 0;
    sqlist_t *list = NULL;
    te_sess_ea_item_t *item = NULL, *next = NULL;
    bool got = false;
    te_sess_module_ctx_t *mctx = NULL;

    TE_ASSERT( ea != NULL );
    TE_ASSERT( it != NULL );
    TE_ASSERT( (it->slotid >= 0) && (it->slotid < MAX_SLOT_NUM) );
    mctx = ea->mctx;
    list = &ea->queues[it->slotid];

    osal_spin_lock_irqsave( &ea->lock, &flags );
    SQLIST_FOR_EACH_CONTAINER_SAFE( list, item, next, list ) {
        if ( item == it ) {
            got = true;
            /* Dequeue this item */
            sqlist_remove( &item->list );
            ret = TE_SUCCESS;
            break;
        }
    }
    osal_spin_unlock_irqrestore( &ea->lock, flags );

    if ( got ) {
        /* Put module clock */
        te_sess_module_clk_put( mctx );
        item->notify( item->para, (int32_t)TE_ERROR_CANCEL );
    }

    return ret;
}

static void te_sess_ea_event_flush( te_sess_event_agent_t *ea, int32_t slotid )
{
    unsigned long flags = 0;
    te_sess_ea_item_t *item = NULL;
    sqlist_t *list = NULL;
    te_sess_module_ctx_t *mctx = ea->mctx;

    while (1) {
        osal_spin_lock_irqsave( &ea->lock, &flags );
        /* Remove from list */
        list = sqlist_dequeue( &ea->queues[slotid] );
        osal_spin_unlock_irqrestore( &ea->lock, flags );

        if ( list == NULL ) {
            break;
        }

        item = SQLIST_CONTAINER( list, item, list );
        /* Put module clock */
        te_sess_module_clk_put( mctx );
        item->notify( item->para, (int32_t)TE_ERROR_GENERIC );
        item = NULL;
    }

    return;
}

void te_sess_ea_dispatch_event( te_sess_event_agent_t *ea )
{
    unsigned long flags = 0;
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_cmd_agent_t *ca = NULL;
    te_hwa_sca_t *hwa = NULL;
    te_sca_stat_t stat = { 0 };
    te_sca_int_t intr = { 0 };
    te_sca_csq_entry_t entry = { 0 };
    te_sess_ea_item_t *item = NULL;
    sqlist_t *list = NULL;
    int32_t csqcnt = 0, slotid = 0;
    int32_t err = TE_ERROR_GENERIC;
    static const uint8_t code_to_cmd[] = {
                         CSQ_EMPTY, INIT, PROC, FINISH,
                         CLONE, WRAPOUT, UNWRAPIN, CLEAR };

    TE_ASSERT( ea );
    mctx = ea->mctx;
    ca = mctx->cmd_agent;
    hwa = (te_hwa_sca_t *)mctx->hwa;

    te_sess_module_clk_get( mctx );

    ret = hwa->int_state( hwa, &intr );
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't get interrupt stat\n");
    ret = hwa->eoi( hwa, &intr );
    TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't ack interrupt\n");

    TE_ASSERT_MSG( (intr.stat.axi_to_err == 0), "Fatal error, AXI acess error\n");
    /* Should something wrong, driver never overcommit CQ & CSQ */
    TE_ASSERT_MSG( (intr.stat.cq_wr_err == 0), "Fatal error, CQ write error\n");
    TE_ASSERT_MSG( (intr.stat.csq_rd_err == 0), "Fatal error, CSQ read error\n");

#ifdef CFG_TE_IRQ_EN
    /* Mask cq_wm interrupt */
    ret = hwa->cqwm_ctrl( hwa, false );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                   "Fatal error, can't disable %s cqwm intr!\n",
                   (mctx->ishash ? "hash" : "sca") );
#endif /* CFG_TE_IRQ_EN */

    /* Handle CSQ first, CSQ full will block whole host */
    /* Fetch all of CSQ item */
    while ( 1 ) {
#ifndef CFG_TE_IRQ_EN
        osal_spin_lock_irqsave( &ea->lock, &flags );
#endif
        /* Get CSQ event available info */
        ret = hwa->state( hwa, &stat );
        TE_ASSERT_MSG( (ret == TE_SUCCESS), "Fatal error, Can't get host stat\n" );
        csqcnt = stat.csq_ocpd_slots;
        if ( csqcnt == 0 ) {
#ifndef CFG_TE_IRQ_EN
            osal_spin_unlock_irqrestore( &ea->lock, flags );
#endif
            break;
        }

        ret = hwa->csq_read( hwa, &entry );
        slotid = entry.slot;
#ifdef CFG_TE_IRQ_EN
        osal_spin_lock_irqsave( &ea->lock, &flags );
#endif
        /* Dequeue first item from queue */
        list = sqlist_dequeue( &ea->queues[slotid] );
        item = SQLIST_CONTAINER( list, item, list );
        osal_spin_unlock_irqrestore( &ea->lock, flags );

        TE_ASSERT_MSG( (item != NULL),
                         "Got CSQ event(slot:0x%02x, code:0x%02x, stat:0x%02x)"
                         " but no corresponding ea item\n",
                         entry.slot, entry.code, entry.stat );

        TE_ASSERT_MSG( (CMDID(item->cmdptr) == code_to_cmd[entry.code]),
                         "OP code is not match(slot:%x, item:%x, entry:%x)!\n",
                         slotid,
                         CMDID(item->cmdptr),
                         code_to_cmd[entry.code] );


        OSAL_LOG_DEBUG( "Event Agent[%s]: Got CSQ event"
                        "(slot:0x%02x, code:0x%02x, stat:0x%02x)\n",
                        (mctx->ishash ? "hash" : "sca"),
                        entry.slot, entry.code, entry.stat );

        if (entry.stat != 0) {
            OSAL_LOG_ERR( "Event Agent[%s]: Got CSQ event"
                          "(slot:0x%02x, code:0x%02x, stat:0x%02x)\n",
                          (mctx->ishash ? "hash" : "sca"),
                          entry.slot, entry.code, entry.stat );
        }

        if ( entry.stat == 0 ) {
            err = TE_SUCCESS;
        } else if ( entry.stat == 1 ) {
            /* hardware AXI bus error */
            err = TE_ERROR_ACCESS_DENIED;
        } else {
            /* Other hardware exceptions */
            err = TE_ERROR_GENERIC;
        }

        te_sess_module_clk_put( mctx );
        item->notify( item->para, err );
        item = NULL;

        /* Flush all of booked event, if on error */
        if ( err != (int32_t)TE_SUCCESS ) {
            te_sess_ea_event_flush( ea, slotid );
        }
    }

    /* Fill command Q */
    te_sess_ca_fill( ca );
    te_sess_module_clk_put( mctx );
    return;
}

int te_sess_ea_init( te_sess_module_ctx_t *mctx )
{
    int ret = TE_SUCCESS;
    int i = 0;
    te_sess_event_agent_t *ea = NULL;
#ifdef CFG_TE_IRQ_EN
    te_hwa_sca_t *hwa = NULL;
    te_irq_type_t irqtype = { 0 };
#endif

    TE_ASSERT( mctx );
#ifdef CFG_TE_IRQ_EN
    hwa = (te_hwa_sca_t *)mctx->hwa;
    irqtype = ( (mctx->ishash) ? TE_IRQ_TYPE_HASH : TE_IRQ_TYPE_SCA );
#endif

    ea = (te_sess_event_agent_t *)
            osal_calloc( 1, sizeof(te_sess_event_agent_t) );
    if (ea == NULL) {
        ret = TE_ERROR_OOM;
        goto err;
    }

    ret = osal_spin_lock_init( &ea->lock );
    if ( ret != OSAL_SUCCESS ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    for ( i = 0; i < MAX_SLOT_NUM; i++ ) {
        sqlist_init( &ea->queues[i] );
    }

    ea->mctx = mctx;
    mctx->event_agent = ea;
#ifdef CFG_TE_IRQ_EN
    ret = te_hwa_register_notifier( hwa->base.host,
                                    irqtype,
                                    te_sess_irqhandler,
                                    (void *)mctx,
                                    &ea->nb );
    if ( ret != TE_SUCCESS ) {
        osal_spin_lock_destroy( &ea->lock );
        goto err1;
    }

#endif
    return ret;
err1:
    osal_free( ea );
err:
    return ret;
}

int te_sess_ea_destroy( te_sess_module_ctx_t *mctx )
{
    int ret = TE_SUCCESS;
    te_sess_event_agent_t *ea = NULL;
#ifdef CFG_TE_IRQ_EN
    te_hwa_sca_t *hwa = NULL;
#endif

    TE_ASSERT( mctx && mctx->event_agent );
    ea = mctx->event_agent;

#ifdef CFG_TE_IRQ_EN
    hwa = (te_hwa_sca_t *)mctx->hwa;
    ret = te_hwa_unregister_notifier( hwa->base.host, ea->nb );
#endif

    osal_spin_lock_destroy( &ea->lock );
    osal_free( ea );
    mctx->event_agent = NULL;

    return ret;
}

