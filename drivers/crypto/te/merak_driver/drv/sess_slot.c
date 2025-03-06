//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_defines.h>
#include <hwa/te_hwa.h>
#include <driver/te_drv.h>
#include "drv_sess.h"
#include "drv_sess_internal.h"

#define ID_TO_SLOT( _slg, _id)  ((_slg)->slots[(_id)])

static void te_sess_slg_slot_bind_sess( te_sess_slot_gov_t *sltgov,
                                        int32_t id,
                                        te_sess_ctx_t *sctx )
{
    unsigned long flags = 0;
    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, id );
    slot->sctx = sctx;
    te_sess_ctx_get( sctx );

    osal_spin_lock_irqsave(&sctx->lock, &flags);
    sctx->slotid = id;
    osal_spin_unlock_irqrestore(&sctx->lock, flags);
    return;
}

static void te_sess_slg_slot_unbind_sess( te_sess_slot_gov_t *sltgov,
                                        int32_t id )
{
    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, id );
    te_sess_ctx_put( slot->sctx );
    slot->sctx = NULL;
    return;
}

static int te_sess_slg_find_free_slot( te_sess_slot_gov_t *sltgov,
                                       te_sess_slot_cat_t cat )
{
    unsigned long bitmap = 0;
    unsigned long bitsz = 0;
    unsigned long nr = 0;
    int id = -1;

recheck:
    /* Check if we have free slot */
    if (cat == TE_SLOT_CATEGORY_LONG) {
        bitmap = sltgov->lslt_free;
        bitsz = (sizeof(sltgov->lslt_free) * BIT_PER_BYTE);
    } else {
        bitmap = sltgov->sslt_free;
        bitsz = (sizeof(sltgov->sslt_free) * BIT_PER_BYTE);
    }

    nr = te_sess_find_first_bit( &bitmap, bitsz );
    if (nr == bitsz) {
        if (TE_SLOT_CATEGORY_SHORT == cat) {
            /* Try the long slots if out of short slots */
            cat = TE_SLOT_CATEGORY_LONG;
            goto recheck;
        }
        id = -1;
    } else {
        id = (int)nr;
    }

    return id;
}

static bool te_sess_slg_has_free_slot( te_sess_slot_gov_t *sltgov,
                                      te_sess_slot_cat_t cat )
{
    return ( te_sess_slg_find_free_slot(sltgov, cat) >= 0 );
}

static void sess_slot_state_transition( te_sess_slot_gov_t *sltgov,
                                               int32_t id,
                                               te_sess_slot_st_t st )
{
    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, id );
    te_sess_slot_cat_t cat = slot->cat;
    te_sess_slot_st_t cst = slot->stat;
    unsigned long *map = ( cat == TE_SLOT_CATEGORY_LONG ?
                           (&sltgov->lslt_free) :
                           (&sltgov->sslt_free) );

    sqlist_t *list = ( (cat == TE_SLOT_CATEGORY_LONG) ?
                       (&sltgov->lslt_shared) :
                       (&sltgov->sslt_shared) );

    OSAL_LOG_DEBUG( "%s:%d: state %d -> %d\n", __func__, __LINE__, cst, st );

    if ( cst == st ) {
        return;
    }

    if ( cst == TE_SLOT_FREE ) {
        te_sess_clear_bit( id, map );
    }

    if ( cst == TE_SLOT_SHARED ) {
        sqlist_remove( &slot->list );
    }

    slot->stat = st;
    osal_wmb();

    if ( st == TE_SLOT_FREE ) {
        te_sess_set_bit( id, map );
    }

    if ( st == TE_SLOT_SHARED ) {
       sqlist_insert_tail( list, &slot->list );
    }

    return;
}

static uint32_t te_sess_slg_slot_refcnt_inc( te_sess_slot_t *slot )
{
    return osal_atomic_inc( &slot->refcnt );
}

static uint32_t te_sess_slg_slot_refcnt_dec( te_sess_slot_t *slot )
{
    return osal_atomic_dec( &slot->refcnt );
}

static bool te_sess_slg_slot_check_swaping_done( te_sess_slot_t *slot )
{
    unsigned long flags = 0;
    bool result = false;
    te_sess_slot_gov_t *sltgov = slot->governor;

    osal_spin_lock_irqsave( &sltgov->lock, &flags );
    if (slot->stat == TE_SLOT_SWAPING) {
        result = false;
    } else {
        result = true;
    }
    osal_spin_unlock_irqrestore( &sltgov->lock, flags );
    return result;
}

static void slot_wrapin_notify( te_sess_cb_para_t *para, int32_t err )
{
    te_sess_slot_t *slot = (te_sess_slot_t *)para->priv;
    TE_ASSERT( err == (int32_t)TE_SUCCESS );
    /* Guarantee load & store order */
    osal_wmb();
    para->status = err;
    osal_wmb();

    SESS_CMD_WAKE_UP( &slot->wrapin );
    return;
}

static bool check_wrapin_done( te_sess_module_ctx_t *mctx,
                               te_sess_cb_para_t *para )
{
    bool result = false;
    te_sess_event_agent_t *ea __te_unused = mctx->event_agent;
#ifndef CFG_TE_IRQ_EN
    te_sess_ea_dispatch_event( ea );
#endif

    osal_rmb();
    if ( para->status != (int32_t)TE_ERROR_BUSY ) {
        result = true;
    } else {
        result = false;
    }

    return result;
}

static int te_sess_slg_slot_hwctx_wrapin( te_sess_slot_gov_t *sltgov,
                                          int32_t id,
                                          void *hwctx )
{
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_module_ctx_t *mctx = sltgov->mctx;
    te_sess_cmd_agent_t *ca = mctx->cmd_agent;
    te_sess_slot_t *slot = ID_TO_SLOT(sltgov, id);
    te_sess_ca_tsk_t task = { 0 };
    te_sess_ea_item_t it = { 0 };
    uint64_t phyaddr = HTOLE64( (uint64_t)osal_virt_to_phys( hwctx ) );

    uint32_t wrapin[3] = {
        ( (UNWRAPIN_CMD << 24) | (id << 19) | TE_SESS_TINT ),
        ( (uint32_t)(phyaddr & 0xffffffff) ),
        ( (uint32_t)((phyaddr >> 32 ) & 0xffffffff) )
    };

    te_sess_cb_para_t para = {
        .status = (int32_t)TE_ERROR_BUSY,
        .priv = (void *)slot,
    };

    OSAL_LOG_DEBUG( "slotgovernor[%s]: wrapin slot %d\n",
                    sltgov->mctx->ishash ? "hash" : "sca", id );

    te_sess_ca_prepare_task( id, wrapin, sizeof(wrapin), &task );
    te_sess_ea_prepare_event( id, wrapin, slot_wrapin_notify, &para, &it);
    ret = te_sess_ca_submit( ca, &task, &it );
    if ( ret != TE_SUCCESS ) {
        return ret;
    }

    SESS_CMD_WAIT_EVENT(
                ( check_wrapin_done( mctx, &para) ),
                &slot->wrapin );

    return para.status;
}

static void slot_wrapout_notify( te_sess_cb_para_t *para, int32_t err )
{
    unsigned long flags = 0;
    te_sess_slot_t *slot = (te_sess_slot_t *)para->priv;
    te_sess_slot_gov_t *sltgov = slot->governor;
    te_sess_ctx_t *sctx = slot->sctx;
    osal_completion_t *slotavailable = NULL;

    slotavailable = ( (slot->cat == TE_SLOT_CATEGORY_LONG) ?
                      &sltgov->lslt_available :
                      &sltgov->sslt_available );

    if ( err == (int32_t)TE_SUCCESS ) {
        osal_spin_lock_irqsave( &sltgov->lock, &flags );

        sess_slot_state_transition( sltgov, slot->id, TE_SLOT_FREE );
        te_sess_slg_slot_unbind_sess( sltgov, slot->id );
        osal_atomic_store( &slot->refcnt, 0U );
        osal_spin_unlock_irqrestore( &sltgov->lock, flags );

        /* Notify there is a free slot available, since export/import may cause slot free */
        osal_completion_signal( slotavailable );
    } else {
        /*
         * wrapout failed, put the slot into OCCUPIED,
         * and wait its owner submit CLEAR
         */
        osal_spin_lock_irqsave( &sltgov->lock, &flags );
        sess_slot_state_transition( sltgov, slot->id, TE_SLOT_OCCUPIED );
        osal_spin_unlock_irqrestore( &sltgov->lock, flags );

        /* Update session stat to ERROR */
        te_sess_ctx_update_state( sctx, TE_SESS_ERROR );
        OSAL_LOG_ERR("WRAPOUT error, cause other session enter TE_SESS_ERROR stat\n");
    }

    /* Update status */
    para->status = err;
    osal_wmb();
    /* Need broadcast, the slot owner may waiting on wrapout done */
    osal_completion_broadcast( &slot->wrapout );

    return;
}

static bool check_wrapout_done( te_sess_module_ctx_t *mctx,
                                te_sess_cb_para_t *para )
{
    bool result = false;
    te_sess_event_agent_t *ea __te_unused = mctx->event_agent;
#ifndef CFG_TE_IRQ_EN
    te_sess_ea_dispatch_event( ea );
#endif

    osal_rmb();
    if ( para->status != (int32_t)TE_ERROR_BUSY ) {
        result = true;
    } else {
        result = false;
    }

    return result;
}

static int te_sess_slg_slot_hwctx_wrapout( te_sess_slot_gov_t *sltgov,
                                          int32_t id,
                                          void *hwctx)
{
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_module_ctx_t *mctx = sltgov->mctx;
    te_sess_cmd_agent_t *ca = mctx->cmd_agent;
    te_sess_slot_t *slot = ID_TO_SLOT(sltgov, id);
    te_sess_ca_tsk_t task = { 0 };
    te_sess_ea_item_t it = { 0 };
    uint64_t phyaddr = HTOLE64( (uint64_t)osal_virt_to_phys( hwctx ) );

    /* wrapout command */
    uint32_t wrapout[3] = {
        ( (WRAPOUT_CMD << 24) | (id << 19) | TE_SESS_TINT ),
        ( (uint32_t)(phyaddr & 0xffffffff) ),
        ( (uint32_t)((phyaddr >> 32) & 0xffffffff) )
    };

    te_sess_cb_para_t para = {
        .status = (int32_t)TE_ERROR_BUSY,
        .priv = (void *)slot,
    };

    /*
     * Reset completion, due to this completion used by broadcast.
     * slot owner may or may not wait on this completion, reset won't wakeup it.
     */
    osal_completion_reset( &slot->wrapout );

    te_sess_ca_prepare_task( id, wrapout, sizeof(wrapout), &task );
    te_sess_ea_prepare_event( id, wrapout, slot_wrapout_notify, &para, &it);
    ret = te_sess_ca_submit( ca, &task, &it );
    if ( ret != TE_SUCCESS ) {
        return ret;
    }

    /* wait for wrap out done */
    SESS_CMD_WAIT_EVENT(
                (check_wrapout_done( mctx, &para)),
                &slot->wrapout );

    return para.status;
}

static bool te_sess_slg_slot_available_check(
                                        te_sess_slot_gov_t *sltgov,
                                        te_sess_slot_cat_t cat)
{
    unsigned long flags = 0;
    bool result = false;
    sqlist_t *list = ( (cat == TE_SLOT_CATEGORY_LONG) ?
                       &sltgov->lslt_shared :
                       &sltgov->sslt_shared);

    osal_spin_lock_irqsave( &sltgov->lock, &flags );

    if ( !sqlist_is_empty(list) ||
         te_sess_slg_has_free_slot(sltgov, cat) ) {
        result = true;
    } else {
        result = false;
    }

    osal_spin_unlock_irqrestore( &sltgov->lock, flags );

    return result;
}

static int te_sess_slg_swapout_slot( te_sess_slot_gov_t *sltgov,
                                     int32_t id )
{
    int32_t ret = TE_SUCCESS;

    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, id );
    te_sess_ctx_t *sctx = slot->sctx;

    ret = te_sess_slg_slot_hwctx_wrapout( sltgov, id, (void *)sctx->hwctx );
    if ( ret != TE_SUCCESS ) {
        te_sess_ctx_update_state( sctx, TE_SESS_ERROR );
    }
    return ret;
}

static void te_sess_slg_kickoff_swapout(
                                    te_sess_slot_gov_t *sltgov,
                                    te_sess_slot_cat_t cat )
{
    unsigned long flags = 0;
    int32_t id = -1, ret = TE_SUCCESS;
    te_sess_slot_t *slot = NULL;
    osal_completion_t *slotavailable =
                      ( (cat == TE_SLOT_CATEGORY_LONG) ?
                      &sltgov->lslt_available :
                      &sltgov->sslt_available );

    sqlist_t *list = ( (cat == TE_SLOT_CATEGORY_LONG) ?
                       &sltgov->lslt_shared :
                       &sltgov->sslt_shared);


    while (1) {
        /* First wait for any FREE or SHARED slot */
        SESS_CMD_WAIT_EVENT(
                ( te_sess_slg_slot_available_check(sltgov, cat) ),
                  slotavailable );

        osal_spin_lock_irqsave( &sltgov->lock, &flags );

        /* if have FREE slot, we can get out here */
        if ( te_sess_slg_has_free_slot(sltgov, cat) ) {
            osal_spin_unlock_irqrestore(&sltgov->lock, flags);
            goto out;
        }

        slot = SQLIST_PEEK_HEAD_CONTAINER( list, slot, list );

        if ( slot != NULL ) {
            id = slot->id;
            /*
             * Set to 'SWAPING' state
             * indicate this slot owner can't use this slot
             */
            TE_ASSERT( (slot->stat == TE_SLOT_SHARED) );
            TE_ASSERT( (osal_atomic_load(&slot->refcnt) == 0) );
            sess_slot_state_transition( sltgov, id, TE_SLOT_SWAPING );
        }

        osal_spin_unlock_irqrestore( &sltgov->lock, flags );

        /* Got SHARED slot, try to wrapout its context */
        if ( slot != NULL ) {
            OSAL_LOG_DEBUG( "slotgovernor[%s]: swap slot"
                            "(%d) owner(sid:%d)\n",
                            sltgov->mctx->ishash ? "hash" : "sca",
                            id,
                            slot->sctx->sid );

            ret = te_sess_slg_swapout_slot( sltgov, id );

            OSAL_LOG_DEBUG( "slotgovernor[%s]: slot(%d) "
                            "swap done(err:%08x)\n",
                            sltgov->mctx->ishash ? "hash" : "sca",
                            id,
                            ret );

            /* Got one slot and wrapout success */
            if ( ret == TE_SUCCESS ) {
                goto out;
            }
        }

        /* No slot available, continue to wait slot */
    }

out:

    return;
}

static int te_sess_slg_try_reoccupy_slot( te_sess_slot_gov_t *sltgov,
                                          te_sess_ctx_t *sctx )
{
    int id = TE_ERROR_GENERIC;
    unsigned long flags = 0;
    te_sess_slot_t *slot = NULL;
    slot = ID_TO_SLOT( sltgov, sctx->slotid );

    osal_spin_lock_irqsave( &sltgov->lock, &flags );

    if (slot->sctx != sctx) {
        osal_spin_unlock_irqrestore( &sltgov->lock, flags );
        return TE_ERROR_NO_DATA;
    }

    /* TE_SLOT_FREE can't be here, and must be slot->sctx != sctx */
    TE_ASSERT_MSG( ((slot->stat == TE_SLOT_SHARED)
                     || (slot->stat == TE_SLOT_OCCUPIED)
                     || (slot->stat == TE_SLOT_SWAPING)),
                     "Slot in unexpected state %d\n", slot->stat );

    if ( (slot->stat == TE_SLOT_SHARED) || (slot->stat == TE_SLOT_OCCUPIED) ) {

        sess_slot_state_transition( sltgov, slot->id, TE_SLOT_OCCUPIED );

        /* Increase slot reference count */
        te_sess_slg_slot_refcnt_inc(slot);
        id = slot->id;
        osal_spin_unlock_irqrestore( &sltgov->lock, flags );
        goto out;
    }

    /*
     * slot in SWAPING state
     * Wait other swap us done
     * after that we need find another slot
     * we need reset the wrapout completion, it may be woke by broadcast.
     */
    osal_completion_reset( &slot->wrapout );

    osal_spin_unlock_irqrestore( &sltgov->lock, flags );

    OSAL_LOG_DEBUG( "slotgovernor[%s]: sess(%d)'s "
                    "slot(%d) in SWAPING, wait!!\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    sctx->sid,
                    slot->id );

    SESS_CMD_WAIT_EVENT(
                ( te_sess_slg_slot_check_swaping_done(slot) ),
                &slot->wrapout );

    OSAL_LOG_DEBUG( "slotgovernor[%s]: sess(%d) slot swapped"
                    " done by others, continue!\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    sctx->sid,
                    slot->id );

out:
    if ( id >= 0 ) {
        OSAL_LOG_TRACE( "slotgovernor[%s]: reoccupy slot %d\n",
                        sltgov->mctx->ishash ? "hash" : "sca", id );
    }

    return id;
}

static int te_sess_slg_wait_slot( te_sess_slot_gov_t *sltgov,
                                  te_sess_slot_cat_t cat )
{
    unsigned long flags = 0;
    int id = 0;

    do {
        osal_spin_lock_irqsave( &sltgov->lock, &flags );
        /* seeking for free slots */
        id = te_sess_slg_find_free_slot( sltgov, cat );

        /* Got free slot */
        if (id >= 0 ) {
            sess_slot_state_transition( sltgov, id,
                                                TE_SLOT_OCCUPIED );
            osal_spin_unlock_irqrestore( &sltgov->lock, flags );
            break;
        }
        osal_spin_unlock_irqrestore( &sltgov->lock, flags );

        te_sess_slg_kickoff_swapout( sltgov, cat );
    } while ( 1 );

    OSAL_LOG_TRACE( "slotgovernor[%s]: wait and got slot %d\n",
                    sltgov->mctx->ishash ? "hash" : "sca", id );

    return id;
}

static int te_sess_slg_wait_slot_and_bind( te_sess_slot_gov_t *sltgov,
                                            te_sess_ctx_t *sctx)
{
    int id = -1;
    te_sess_slot_t *slot = NULL;

    id = te_sess_slg_wait_slot( sltgov, sctx->cat );
    /* Bind slot with session context */
    te_sess_slg_slot_bind_sess( sltgov, id, sctx );
    /* Increase slot refcnt */
    slot = ID_TO_SLOT( sltgov, id );
    te_sess_slg_slot_refcnt_inc( slot );

    return id;
}

static void clear_done_notify( te_sess_cb_para_t *para, int32_t err )
{
    osal_completion_t *done = (osal_completion_t *)para->priv;
    /* Guarantee load & store order */
    osal_wmb();
    para->status = err;
    osal_wmb();

    SESS_CMD_WAKE_UP( done );
    return;
}

static bool check_clear_done( te_sess_module_ctx_t *mctx,
                              te_sess_cb_para_t *para )
{
    bool result = 0;
    te_sess_event_agent_t *ea __te_unused = mctx->event_agent;
#ifndef CFG_TE_IRQ_EN
    te_sess_ea_dispatch_event( ea );
#endif

    osal_rmb();
    if ( para->status != (int32_t)TE_ERROR_BUSY ) {
        result = true;
    } else {
        result = false;
    }
    return result;
}

static int te_sess_slg_clear_slot( te_sess_slot_gov_t *sltgov,
                                    int id )
{
    int ret = TE_SUCCESS;
    te_sess_module_ctx_t *mctx = sltgov->mctx;
    te_sess_cmd_agent_t *ca = mctx->cmd_agent;
    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, id );
    te_sess_ca_tsk_t task = { 0 };
    te_sess_ea_item_t it = { 0 };
    uint32_t clear[1] = {
        (
            ( (CLEAR_CMD << 24) |
              (id << 19)        |
              TE_SESS_TINT )
        )
    };

    te_sess_cb_para_t para = {
        .status = (int32_t)TE_ERROR_BUSY,
        .priv = (void *)&slot->clear,
    };

    te_sess_ca_prepare_task( id, clear, sizeof(clear), &task );
    te_sess_ea_prepare_event( id, clear, clear_done_notify, &para, &it);
    ret = te_sess_ca_submit( ca, &task, &it );
    if ( ret != TE_SUCCESS ) {
        goto out;
    }

    /* wait for clone done */
    SESS_CMD_WAIT_EVENT(
        ( check_clear_done( mctx, (te_sess_cb_para_t *)&para) ),
        &slot->clear );

    ret = para.status;

out:
    return ret;
}

static int te_sess_slg_free_slot( te_sess_slot_gov_t *sltgov,
                                  int id )
{
    unsigned long flags = 0;
    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, id );

    te_sess_slg_slot_unbind_sess( sltgov, id );
    osal_spin_lock_irqsave( &sltgov->lock, &flags );
    /* reset refcnt */
    osal_atomic_store( &slot->refcnt, 0U);
    /* Put into FREE state */
    sess_slot_state_transition( sltgov, id, TE_SLOT_FREE );
    osal_spin_unlock_irqrestore( &sltgov->lock, flags );
    return TE_SUCCESS;
}

static int te_sess_slg_clear_and_free_slot( te_sess_slot_gov_t *sltgov,
                                            int id )
{
    int ret = TE_SUCCESS;
    ret = te_sess_slg_clear_slot( sltgov, id );
    if ( ret != TE_SUCCESS ) {
        return ret;
    }

    return te_sess_slg_free_slot( sltgov, id );
}

static void clone_done_notify( te_sess_cb_para_t *para, int32_t err )
{
    osal_completion_t *done = (osal_completion_t *)para->priv;
    /* Guarantee load & store order */
    osal_wmb();
    para->status = err;
    osal_wmb();

    SESS_CMD_WAKE_UP( done );
    return;
}

static bool check_clone_done( te_sess_module_ctx_t *mctx,
                              te_sess_cb_para_t *para )
{
    bool result = 0;
    te_sess_event_agent_t *ea __te_unused = mctx->event_agent;
#ifndef CFG_TE_IRQ_EN
    te_sess_ea_dispatch_event( ea );
#endif

    osal_rmb();
    if ( para->status != (int32_t)TE_ERROR_BUSY ) {
        result = true;
    } else {
        result = false;
    }
    return result;
}

static int te_sess_slg_clone_slot( te_sess_slot_gov_t *sltgov,
                                   int32_t destid,
                                   int32_t srcid )
{
    int ret = TE_SUCCESS;
    te_sess_module_ctx_t *mctx = sltgov->mctx;
    te_sess_cmd_agent_t *ca = mctx->cmd_agent;
    te_sess_slot_t *slot = ID_TO_SLOT( sltgov, srcid );
    te_sess_ca_tsk_t task = { 0 };
    te_sess_ea_item_t it = { 0 };

    /* Issue CLONE command */
    uint32_t clone[1] = {
        (
            ( (CLONE_CMD << 24) |
              (srcid << 19)     |
              (destid << 14)    |
              TE_SESS_TINT )
        )
    };

    te_sess_cb_para_t para = {
        .status = (int32_t)TE_ERROR_BUSY,
        .priv = (void *)&slot->clone,
    };

    te_sess_ca_prepare_task( srcid, clone, sizeof(clone), &task );
    te_sess_ea_prepare_event( srcid, clone, clone_done_notify, &para, &it);
    ret = te_sess_ca_submit( ca, &task, &it );
    if ( ret != TE_SUCCESS ) {
        goto out;
    }

    /* wait for clone done */
    SESS_CMD_WAIT_EVENT(
        ( check_clone_done( mctx, &para) ),
        &slot->clone );

    ret = para.status;

out:
    return ret;
}

int te_sess_slg_clone( te_sess_ctx_t *dest, te_sess_ctx_t *src )
{
    int32_t ret = TE_SUCCESS, srcid = -1, destid = -1;
    te_sess_module_ctx_t *mctx = src->mctx;
    te_sess_slot_gov_t *sltgov = mctx->slot_gov;
    te_sess_slot_cat_t cat = src->cat;
    int32_t slotnum = 0;

    slotnum = ( (cat == TE_SLOT_CATEGORY_LONG) ?
                sltgov->lslot_num :
                sltgov->sslot_num );

    /* If total slot number less than 2, fail */
    if ( slotnum < 2 ) {
        return TE_ERROR_OVERFLOW;
    }

    /*
     * Make sure only one thread can execute CLONE operation.
     * Because CLONE need consume two slots, if multiple-thread
     * enter here, it is very easy cause deadlock, due to no slot
     * available.
     */
    osal_mutex_lock( sltgov->clone );
    ret  = te_sess_slg_acquire_slot( sltgov, src, true );
    if ( ret != TE_SUCCESS ) {
        goto out;
    }
    srcid = src->slotid;

    /* Need an empty slot */
    ret = te_sess_slg_acquire_slot( sltgov, dest, false );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }
    destid = dest->slotid;

    ret = te_sess_slg_clone_slot( sltgov, destid, srcid );
    if ( ret != TE_SUCCESS ) {
        /*
         * CLONE failed can cause dest slot enter error state.
         * CLEAR and free this slot.
         */
        ret = te_sess_slg_clear_and_free_slot( sltgov, destid );
        TE_ASSERT_MSG( (ret == TE_SUCCESS), "CLEAR slot failed\n");
        goto err2;
    }

    /* release slot into SHARED state */
    te_sess_slg_release_slot( sltgov, srcid, CLONE_CMD );
    te_sess_slg_release_slot( sltgov, destid, CLONE_CMD );
    goto out;

err2:
    te_sess_slg_release_slot( sltgov, srcid, CLONE_CMD );
out:
    osal_mutex_unlock( sltgov->clone );
    return ret;
}

int te_sess_slg_acquire_slot( te_sess_slot_gov_t *sltgov,
                              te_sess_ctx_t *sctx,
                              bool wrapin )
{
    int id = -1, ret = TE_ERROR_GENERIC;

    id = te_sess_slg_try_reoccupy_slot( sltgov, sctx );
    /* slot hit */
    if ( id >= 0 ) {
        ret = TE_SUCCESS;
        goto out;
    }

    /*
     * Check point
     * Here, our slot may swap out by other process.
     * However, the swap out operation may failure.
     * refer to 'te_sess_slg_swapout_slot'
     */
    ret = te_sess_ctx_check_state( sctx );
    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "session context in error state\n" );
        goto out;
    }

    OSAL_LOG_DEBUG( "slotgovernor[%s]: sess(%d) waiting slot available\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    sctx->sid );

    id = te_sess_slg_wait_slot_and_bind( sltgov, sctx );
    OSAL_LOG_DEBUG( "slotgovernor[%s]: sess(%d) got slot(%d)\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    sctx->sid,
                    id );

    /* Don't need wrapin just return */
    if ( wrapin == false ) {
        goto out;
    }

    /* Need wrapin */
    OSAL_LOG_DEBUG( "slotgovernor[%s]: sess(%d) wrapin slot(%d)\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    sctx->sid,
                    id );

    ret = te_sess_slg_slot_hwctx_wrapin( sltgov, id, sctx->hwctx );

    OSAL_LOG_DEBUG( "slotgovernor[%s]: sess(%d) wrapin"
                    " slot(%d) --- Done(err: %08x)\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    sctx->sid,
                    id,
                    ret );

    if ( ret != TE_SUCCESS ) {
        OSAL_LOG_ERR( "Can't wrapin sess(%d) sctx(0x%lx) hwctx(0x%lx)\n",
                      sctx->sid, sctx, sctx->hwctx );
        OSAL_LOG_ERR( "slot(%d) stuck in OCCUPIED state\n",
                       id );
        /* Mark this session in error state, need CLEAR to recovery */
        te_sess_ctx_update_state( sctx, TE_SESS_ERROR );
        goto out;
    }

out:
    return ret;
}

void te_sess_slg_release_slot( te_sess_slot_gov_t *sltgov,
                                      int32_t id,
                                      uint32_t cmd )
{
    unsigned long flags = 0;
    uint32_t refcnt = 0;
    te_sess_slot_t *slot = NULL;
    osal_completion_t *slotavailable = NULL;
    te_sess_slot_st_t st = { 0 };
    bool need_wake = false;

    slot = ID_TO_SLOT( sltgov, id );
    slotavailable = ( (slot->cat == TE_SLOT_CATEGORY_LONG) ?
                      &sltgov->lslt_available :
                      &sltgov->sslt_available );

    osal_spin_lock_irqsave(&sltgov->lock, &flags);

    TE_ASSERT_MSG( (slot->stat == TE_SLOT_OCCUPIED),
                     "Slot state(%d) is not OCCUPIED\n", slot->stat );

    refcnt = te_sess_slg_slot_refcnt_dec( slot );
    /* if finish command is done, the refcnt of a slot must be '0' */
    if ( cmd == FINISH_CMD ) {
        TE_ASSERT_MSG( (refcnt == 0),
                         "Slot refcnt miss match\n");
    }

    if ( (refcnt == 0) || (cmd == CLEAR_CMD) ) {
        /* CLEAR command done, slot enter FREE */
        if ( cmd == CLEAR_CMD ) {
            OSAL_LOG_DEBUG("sess(%d) CLEAR cmd issued, free slot(%d)\n", slot->sctx->sid, slot->id);
            osal_atomic_store( &slot->refcnt, 0U );
            st = TE_SLOT_FREE;
        } else if ( cmd == FINISH_CMD ) {
            OSAL_LOG_DEBUG("sess(%d) FINISH cmd issued, free slot(%d)\n", slot->sctx->sid, slot->id);
            st = TE_SLOT_FREE;
        } else {
            st = TE_SLOT_SHARED;
        }

        if ( st == TE_SLOT_FREE ) {
            te_sess_slg_slot_unbind_sess( sltgov, id );
        }
        /* Let slot be available in FREE or SHARED status */
        sess_slot_state_transition( sltgov, id, st );

        need_wake = true;
    }

    osal_spin_unlock_irqrestore(&sltgov->lock, flags);

    if (need_wake) {
        /*
         * Notify who is waiting on FREE or SHARED slot.
         * Note it's not allowed to signal while holding a spin in tee.
         */
        osal_completion_signal( slotavailable );
    }

    return;
}

static void sess_slg_slot_destroy( te_sess_slot_t *slot )
{
    osal_completion_destroy( &slot->wrapout );
    osal_completion_destroy( &slot->wrapin );
    osal_completion_destroy( &slot->clear );
    osal_completion_destroy( &slot->clone );
    return;
}

static int sess_slg_slot_init( te_sess_slot_t *slot )
{
    int ret = TE_ERROR_GENERIC;
    sqlist_init( &slot->list );
    ret = osal_completion_init( &slot->wrapout );
    if ( ret != OSAL_SUCCESS ) {
        goto err1;
    }

    ret = osal_completion_init( &slot->wrapin );
    if ( ret != OSAL_SUCCESS ) {
        goto err2;
    }

    ret = osal_completion_init( &slot->clear );
    if ( ret != OSAL_SUCCESS ) {
        goto err3;
    }

    ret = osal_completion_init( &slot->clone );
    if ( ret != OSAL_SUCCESS ) {
        goto err4;
    }

    osal_atomic_store( &slot->refcnt, 0U );
    slot->stat = TE_SLOT_FREE;

    return TE_SUCCESS;
err4:
    osal_completion_destroy( &slot->clear );
err3:
    osal_completion_destroy( &slot->wrapin );
err2:
    osal_completion_destroy( &slot->wrapout );
err1:
    return TE_ERROR_OOM;
}

static void sess_slg_destroy_slot_array( te_sess_slot_gov_t *sltgov )
{
    int i = 0;

    for ( i = 0; i < MAX_SLOT_NUM; i++ ) {
        if ( sltgov->slots[i] == NULL ) {
            continue;
        }
        sess_slg_slot_destroy( sltgov->slots[i] );
        osal_free( sltgov->slots[i] );
        sltgov->slots[i] = NULL;
    }

    /* Clear free bitmap */
    sltgov->sslt_free = 0;
    sltgov->lslt_free = 0;

    return;
}

static int sess_slg_init_slot_array( te_sess_slot_gov_t *sltgov )
{
    int ret = TE_ERROR_GENERIC;
    int i = 0, offs = sltgov->sslot_num;

    /* Initialize SHORT slot */
    for ( i = 0; i < sltgov->sslot_num; i++ ) {
        sltgov->slots[i] = (te_sess_slot_t *)
                           osal_calloc( 1, sizeof(te_sess_slot_t) );
        if (sltgov->slots[i] == NULL ) {
            goto err;
        }
        ret = sess_slg_slot_init( sltgov->slots[i] );
        if ( ret != TE_SUCCESS ) {
            osal_free( sltgov->slots[i] );
            sltgov->slots[i] = NULL;
            goto err;
        }
        sltgov->slots[i]->cat = TE_SLOT_CATEGORY_SHORT;
        sltgov->slots[i]->governor = sltgov;
        sltgov->slots[i]->id = i;
        te_sess_set_bit( i, &sltgov->sslt_free );
    }

    /* Initialize LONG slot */
    for ( i = offs; i < (sltgov->lslot_num + offs); i++ ) {
        sltgov->slots[i] = (te_sess_slot_t *)
                           osal_calloc( 1, sizeof(te_sess_slot_t) );
        if (sltgov->slots[i] == NULL ) {
            goto err;
        }
        ret = sess_slg_slot_init( sltgov->slots[i] );
        if ( ret != TE_SUCCESS ) {
            osal_free( sltgov->slots[i] );
            sltgov->slots[i] = NULL;
            goto err;
        }
        sltgov->slots[i]->cat = TE_SLOT_CATEGORY_LONG;
        sltgov->slots[i]->governor = sltgov;
        sltgov->slots[i]->id = i;
        te_sess_set_bit( i, &sltgov->lslt_free );
    }

    return TE_SUCCESS;

err:
    sess_slg_destroy_slot_array( sltgov );
    return TE_ERROR_OOM;
}

/*
 * wrapout one session initiatively, i.e. on export
 */
int te_sess_slg_wrapout_one( te_sess_slot_gov_t *sltgov,
                             te_sess_ctx_t *sctx )
{
    int ret = TE_SUCCESS;
    int32_t slotid = -1;

    TE_ASSERT( sltgov );
    TE_ASSERT( sctx );

    /*
     * Check if we have HW slot, if have,
     * lock it into OCCUPIED state.
     */
    slotid = te_sess_slg_try_reoccupy_slot( sltgov, sctx );
    /* Session did not occupy any HW slot */
    if ( slotid < 0 ) {
        goto out;
    }

    /*
     * If session own HW slot, swap it out.
     */
    OSAL_LOG_DEBUG( "slotgovernor[%s]: swap slot"
                    "(%d) owner(sid:%d)\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    slotid,
                    sctx->sid );

    ret = te_sess_slg_swapout_slot( sltgov, slotid );

    OSAL_LOG_DEBUG( "slotgovernor[%s]: slot(%d) "
                    "swap done(err:%08x)\n",
                    sltgov->mctx->ishash ? "hash" : "sca",
                    slotid,
                    ret );
out:
    return ret;
}

int te_sess_slg_wrapout_all( te_sess_slot_gov_t *sltgov )
{
    int ret = TE_SUCCESS;
    int i = 0, total = 0;
    unsigned long flags = 0;

    TE_ASSERT( sltgov );
    total = sltgov->sslot_num + sltgov->lslot_num;

    for ( i = 0; i < total; i++ ) {
        osal_spin_lock_irqsave( &sltgov->lock, &flags );
        if ( sltgov->slots[i]->stat == TE_SLOT_FREE ) {
            osal_spin_unlock_irqrestore( &sltgov->lock, flags );
            continue;
        }

        if ( (sltgov->slots[i]->stat == TE_SLOT_OCCUPIED) ||
             (sltgov->slots[i]->stat == TE_SLOT_SWAPING) ) {
            ret = TE_ERROR_BUSY;
            osal_spin_unlock_irqrestore( &sltgov->lock, flags );
            continue;
        }

        sess_slot_state_transition( sltgov, i, TE_SLOT_SWAPING );
        osal_spin_unlock_irqrestore( &sltgov->lock, flags );
        ret = te_sess_slg_swapout_slot( sltgov, i );
        TE_ASSERT( TE_SUCCESS == ret );
    }

    return ret;
}

int te_sess_slg_init( te_sess_module_ctx_t *mctx )
{
    int ret = TE_SUCCESS;
    te_sess_slot_gov_t *sltgov = NULL;
    te_hwa_sca_t *hwa_sca = NULL;
    te_hwa_host_t *hwa_host = NULL;
    te_host_conf_t hwconf = { 0 };

    TE_ASSERT( mctx != NULL );
    TE_ASSERT( mctx->hwa != NULL );

    hwa_sca = (te_hwa_sca_t *)mctx->hwa;
    hwa_host = hwa_crypt_host( &hwa_sca->base );
    ret = te_hwa_host_conf( hwa_host, &hwconf );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't read host configuration!\n" );

    sltgov = (te_sess_slot_gov_t *)
             osal_calloc( 1, sizeof(te_sess_slot_gov_t) );
    if ( sltgov == NULL ) {
        return TE_ERROR_OOM;
    }

    sltgov->sslot_num = ( (mctx->ishash == true) ?
                          hwconf.hash_nctx1 :
                          hwconf.sca_nctx1 );

    sltgov->lslot_num = ( (mctx->ishash == true) ?
                          hwconf.hash_nctx2 :
                          hwconf.sca_nctx2 );

    OSAL_LOG_DEBUG( "slotgovernor[%s]: short slot( %d ~ %d ), total: %d\n",
                    (mctx->ishash ? "hash": "sca"), 0,
                    sltgov->sslot_num,
                    sltgov->sslot_num );

    OSAL_LOG_DEBUG( "slotgovernor[%s]: long slot( %d ~ %d ), total: %d\n",
                    (mctx->ishash ? "hash": "sca"), sltgov->sslot_num,
                    (sltgov->sslot_num + sltgov->lslot_num),
                    sltgov->lslot_num );

    sqlist_init( &sltgov->sslt_shared );
    sqlist_init( &sltgov->lslt_shared );

    ret = osal_spin_lock_init( &sltgov->lock );
    if ( ret != OSAL_SUCCESS ) {
        goto err1;
    }

    ret = osal_completion_init( &sltgov->sslt_available );
    if ( ret != OSAL_SUCCESS ) {
        goto err2;
    }

    ret = osal_completion_init( &sltgov->lslt_available );
    if ( ret != OSAL_SUCCESS ) {
        goto err3;
    }

    ret = sess_slg_init_slot_array( sltgov );
    if ( ret != TE_SUCCESS ) {
        goto err4;
    }

    ret = osal_mutex_create( &sltgov->clone );
    if ( ret != TE_SUCCESS ) {
        goto err5;
    }

    sltgov->mctx = mctx;
    mctx->slot_gov = sltgov;

    return TE_SUCCESS;

err5:
    sess_slg_destroy_slot_array( sltgov );
err4:
    osal_completion_destroy( &sltgov->lslt_available );
err3:
    osal_completion_destroy( &sltgov->sslt_available );
err2:
    osal_spin_lock_destroy( &sltgov->lock );
err1:
    osal_free( sltgov );
    return TE_ERROR_GENERIC;
}

int te_sess_slg_destroy( te_sess_module_ctx_t *mctx )
{
    te_sess_slot_gov_t *sltgov = NULL;

    TE_ASSERT( mctx != NULL );
    TE_ASSERT( mctx->slot_gov != NULL );

    sltgov = mctx->slot_gov;
    osal_mutex_destroy( sltgov->clone );
    sess_slg_destroy_slot_array( sltgov );
    osal_completion_destroy( &sltgov->lslt_available );
    osal_completion_destroy( &sltgov->sslt_available );
    osal_spin_lock_destroy( &sltgov->lock );
    osal_free( sltgov );
    mctx->slot_gov = NULL;

    return TE_SUCCESS;
}

