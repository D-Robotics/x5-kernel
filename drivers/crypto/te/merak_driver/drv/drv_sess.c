//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_defines.h>
#include <hwa/te_hwa.h>
#include <driver/te_drv.h>
#include "drv_sess.h"
#include "drv_sess_internal.h"

/* Session context used flag */
#define SESS_CTX_FLAG_USED     1U

/* Session wait step in ms */
#define SESS_WAIT_STEP_MS      10U

/**
 *  Global session governor
 *  shared with SCA & HASH
 */
static te_sess_gov_t *g_sess_gov = NULL;

static void te_sess_ctx_destroy( te_sess_ctx_t *sctx );
void te_sess_ctx_update_state( te_sess_ctx_t *sctx,
                               te_sess_st_t st )
{
    unsigned long flags = 0;

    osal_spin_lock_irqsave( &sctx->lock, &flags );
    sctx->stat = st;
    osal_spin_unlock_irqrestore( &sctx->lock, flags );

    return;
}

int te_sess_ctx_check_state( te_sess_ctx_t *sctx )
{
    unsigned long flags = 0;
    int ret = TE_SUCCESS;

    osal_spin_lock_irqsave( &sctx->lock, &flags );
    if ( sctx->stat == TE_SESS_ERROR ) {
        ret = TE_ERROR_GENERIC;
    }
    osal_spin_unlock_irqrestore( &sctx->lock, flags );

    return ret;
}

void te_sess_ctx_put( te_sess_ctx_t *sctx )
{
    if ( sctx == NULL ) {
        return;
    }

    if ( osal_atomic_dec( &sctx->refcnt ) == 0 ) {
        te_sess_ctx_destroy( sctx );
    }

    return;
}

te_sess_ctx_t *te_sess_ctx_get( te_sess_ctx_t *sctx )
{
    if ( sctx == NULL ) {
        return NULL;
    }

    osal_atomic_inc( &sctx->refcnt );
    return sctx;
}

static int te_sess_acquire_slot( te_sess_ctx_t *sctx, te_sess_srb_t *req )
{
    te_sess_module_ctx_t *mctx = sctx->mctx;
    te_sess_slot_gov_t *sltgov = mctx->slot_gov;
    bool wrapin = ( (CMDID(req->cmdptr) == INIT_CMD) ?
                    false : true );


    return te_sess_slg_acquire_slot( sltgov, sctx, wrapin );
}

static void te_sess_release_slot( te_sess_ctx_t *sctx, te_sess_srb_t *req )
{
    int32_t id = sctx->slotid;
    te_sess_module_ctx_t *mctx = sctx->mctx;
    te_sess_slot_gov_t *sltgov = mctx->slot_gov;
    uint32_t cmd = CMDID( req->cmdptr );

    te_sess_slg_release_slot( sltgov, id, cmd);
    return;
}

static int te_sess_gov_init(void)
{
    int ret = TE_ERROR_GENERIC;

    /* Session governor already exist */
    if ( g_sess_gov != NULL ) {
        return TE_SUCCESS;
    }

    g_sess_gov = (te_sess_gov_t *)osal_calloc( 1, sizeof(te_sess_gov_t) );

    if ( g_sess_gov == NULL ) {
        return TE_ERROR_OOM;
    }

    /* Initialize mutex */
    ret = osal_mutex_create( &g_sess_gov->mutex );
    UTILS_CHECK_CONDITION( ret == OSAL_SUCCESS, TE_ERROR_OOM,
                          "Can't allocate mutex\n" );

    /* Initialize refcnt */
    osal_atomic_store( &g_sess_gov->refcnt, 0U );

    /* Empty session context array */
    g_sess_gov->ctxs = (te_sess_ctx_t **)
                       osal_calloc( 1, (sizeof(uintptr_t) *
                       sizeof(unsigned long) * BIT_PER_BYTE) );

    if (g_sess_gov->ctxs == NULL) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    /* session id Bitmap buffer pointer to NULL */
    g_sess_gov->free_sids = (unsigned long *)
                            osal_malloc(sizeof(unsigned long));
    if (g_sess_gov->free_sids == NULL) {
        ret = TE_ERROR_OOM;
        goto err2;
    }

    /* bitmap buffer len set to '0' byte */
    g_sess_gov->bitmap_len = sizeof(unsigned long);

    /* Set all as free */
    memset( (void *)g_sess_gov->free_sids, 0xff, g_sess_gov->bitmap_len);

    /* initialize assign count */
    g_sess_gov->assigned = 0;

    return TE_SUCCESS;

err2:
    osal_free( g_sess_gov->ctxs );
err1:
    osal_mutex_destroy( g_sess_gov->mutex );
finish:
    osal_free( g_sess_gov );
    g_sess_gov = NULL;

    return ret;
}

static int te_sess_gov_destroy(void)
{
    if ( g_sess_gov == NULL ) {
        return TE_SUCCESS;
    }

    TE_ASSERT( g_sess_gov->assigned == 0 );

    /* Release session context array */
    if ( g_sess_gov->ctxs != NULL ) {
        osal_free( g_sess_gov->ctxs );
        g_sess_gov->ctxs = NULL;
    }

    /* Release session context array */
    if ( g_sess_gov->free_sids != NULL ) {
        osal_free( (void *)g_sess_gov->free_sids );
        g_sess_gov->free_sids = NULL;
    }

    g_sess_gov->bitmap_len = 0;

    osal_mutex_destroy( g_sess_gov->mutex );

    osal_free( g_sess_gov );
    g_sess_gov = NULL;

    return TE_SUCCESS;
}

static int te_sess_gov_bitmap_may_expansion(void)
{
    unsigned long bitsz = ( g_sess_gov->bitmap_len * BIT_PER_BYTE );
    void *oldctxs = NULL, *oldmaps = NULL;
    uint32_t bmpsz = 0, arraysz = 0;
    unsigned long *bmptr = NULL;
    te_sess_ctx_t **ctxs = NULL;

    if ( bitsz > g_sess_gov->assigned ) {
        return TE_SUCCESS;
    }

    /* reach the maximun value of session id  */
    if ( bitsz == MAX_SESSION_ID ) {
        return TE_ERROR_OVERFLOW;
    }

    /* Need expansion */
    bmpsz = g_sess_gov->bitmap_len + sizeof(unsigned long);
    arraysz = ( bmpsz * BIT_PER_BYTE * sizeof(uintptr_t) );

    /* Alloc bitmap */
    bmptr = (unsigned long *)osal_malloc( bmpsz );
    if ( bmptr == NULL ) {
        return TE_ERROR_OOM;
    }

    ctxs = (te_sess_ctx_t **)osal_calloc( 1, arraysz );
    if ( ctxs == NULL ) {
        osal_free( bmptr );
        return TE_ERROR_OOM;
    }

    memset( (void *)bmptr, 0xff, bmpsz );

    memcpy( (void *)bmptr,
            (void *)g_sess_gov->free_sids,
            g_sess_gov->bitmap_len );

    memcpy( (void *)ctxs,
            (void *)g_sess_gov->ctxs,
            (g_sess_gov->bitmap_len * BIT_PER_BYTE * sizeof(uintptr_t)) );

    oldmaps = (void *)g_sess_gov->free_sids;
    oldctxs = (void *)g_sess_gov->ctxs;
    g_sess_gov->free_sids = (unsigned long *)bmptr;
    g_sess_gov->bitmap_len = bmpsz;
    g_sess_gov->ctxs = ctxs;

    osal_free( oldmaps );
    osal_free( oldctxs );

    return TE_SUCCESS;
}

static int te_sess_gov_assign_sid( te_sess_ctx_t *sctx )
{

    int32_t ret = TE_SUCCESS;
    unsigned long nr = 0, bitsz = 0;

    /* avoid other process acess concurrently */
    osal_mutex_lock( g_sess_gov->mutex );

    /* expansion bitmap on demand */
    ret = te_sess_gov_bitmap_may_expansion();
    if ( ret != TE_SUCCESS ) {
        goto err1;
    }

    bitsz = (g_sess_gov->bitmap_len * BIT_PER_BYTE);

    /*Find first free sid */
    nr = te_sess_find_first_bit( g_sess_gov->free_sids, bitsz );
    sctx->sid = (int32_t)nr;

    te_sess_clear_bit( sctx->sid, g_sess_gov->free_sids);
    g_sess_gov->ctxs[sctx->sid] = sctx;
    g_sess_gov->assigned++;

err1:
    osal_mutex_unlock( g_sess_gov->mutex );
    return ret;
}

static int te_sess_gov_retrive_sid( te_sess_ctx_t *sctx )
{
    osal_mutex_lock( g_sess_gov->mutex );

    te_sess_set_bit( sctx->sid, g_sess_gov->free_sids );
    g_sess_gov->ctxs[sctx->sid] = NULL;
    g_sess_gov->assigned--;

    osal_mutex_unlock( g_sess_gov->mutex );

    return TE_SUCCESS;
}

static te_sess_gov_t *te_sess_gov_get( te_sess_gov_t *gov )
{
    if ( gov == NULL ) {
        return NULL;
    }

    osal_atomic_inc( &gov->refcnt );
    return gov;
}

static void te_sess_gov_put( te_sess_gov_t *gov )
{
    if ( gov == NULL ) {
        return;
    }

    if ( osal_atomic_dec( &gov->refcnt ) == 0 ) {
        te_sess_gov_destroy();
    }

    return;
}

static int te_sess_gov_sid_is_valid( te_sess_id_t sid )
{

    if ( (sid < (g_sess_gov->bitmap_len * BIT_PER_BYTE)) &&
         (sid >= 0) && (!te_sess_test_bit(sid, g_sess_gov->free_sids)) ) {

        return TE_SUCCESS;
    }

    return TE_ERROR_GENERIC;
}

static te_sess_module_ctx_t *te_sess_module_get( te_sess_module_ctx_t *mctx )
{
    if ( mctx == NULL ) {
        return NULL;
    }

    osal_atomic_inc( &mctx->refcnt );
    return mctx;
}

static void te_sess_module_put( te_sess_module_ctx_t *mctx )
{
    if ( mctx == NULL ) {
        return;
    }
    osal_atomic_dec( &mctx->refcnt );
    return;
}

static te_sess_srb_t *te_sess_srb_alloc( void )
{
    int ret = TE_SUCCESS;
    te_sess_srb_t *srb = (te_sess_srb_t *)
                         osal_calloc( 1, sizeof(te_sess_srb_t) );

    if ( srb == NULL ) {
        return NULL;
    }

    sqlist_init( &srb->list );
    srb->stat = TE_SUCCESS;

    ret = osal_completion_init( &srb->done );
    if ( ret != OSAL_SUCCESS ) {
        osal_free( srb );
        srb = NULL;
        return NULL;
    }
    return srb;
}

static void te_sess_srb_free( te_sess_srb_t *srb )
{
    if ( srb == NULL ) {
        return;
    }

    osal_completion_destroy( &srb->done );
    osal_free( srb );
    return;
}

static te_sess_ctx_t *te_sess_sid_to_ctx( int32_t sid )
{
    te_sess_ctx_t *sctx = NULL;

    /*
     * Avoid race condtion with other process
     * who is now extending the bitmap and context Array
     */
    osal_mutex_lock( g_sess_gov->mutex );
    sctx = g_sess_gov->ctxs[sid];
    osal_mutex_unlock( g_sess_gov->mutex );

    return sctx;
}

static te_sess_ctx_t *te_sess_ctx_get_by_id( int32_t sid )
{
    te_sess_ctx_t *sctx = NULL;
    sctx = te_sess_sid_to_ctx( sid );
    return te_sess_ctx_get( sctx );
}

static void te_sess_ctx_srbs_destroy( te_sess_ctx_t *sctx )
{
    sqlist_t *node = NULL, *tmp = NULL;
    te_sess_srb_t *srb = NULL;

    SQLIST_FOR_EACH_NODE_SAFE( &sctx->srbs, node, tmp ) {
        sqlist_remove( node );
        srb = SQLIST_CONTAINER( node, srb, list );
        te_sess_srb_free(srb);
    }

    return;
}

static int te_sess_ctx_srbs_create( te_sess_ctx_t *sctx )
{
    int i = 0;
    te_sess_srb_t *srb = NULL;

    for (i = 0; i < sctx->enqueue_threshold; i++) {
        srb = te_sess_srb_alloc();
        if ( srb == NULL ) {
            goto rollback;
        }
        sqlist_insert_tail( &sctx->srbs, &srb->list );
    }

    return TE_SUCCESS;

rollback:
    te_sess_ctx_srbs_destroy( sctx );
    return TE_ERROR_OOM;
}

static bool sess_ctx_srb_ready( te_sess_ctx_t *sctx )
{
    unsigned long flags = 0;
    bool ready = false;

    osal_spin_lock_irqsave( &sctx->lock, &flags );
    if ( sqlist_is_empty(&sctx->srbs) ) {
        ready = false;
    } else {
        ready = true;
    }
    osal_spin_unlock_irqrestore( &sctx->lock, flags );

    return ready;
}

static te_sess_srb_t *te_sess_ctx_get_srb( te_sess_ctx_t *sctx )
{
    unsigned long flags = 0;
    te_sess_srb_t *srb = NULL;
    sqlist_t *list = NULL;

    while (1) {
        osal_spin_lock_irqsave( &sctx->lock, &flags );
        list = sqlist_get( &sctx->srbs );
        osal_spin_unlock_irqrestore( &sctx->lock, flags );

        if ( list != NULL ) {
            break;
        }

        SESS_CMD_WAIT_EVENT( sess_ctx_srb_ready(sctx), &sctx->srbs_available );
    }

    srb = SQLIST_CONTAINER( list, srb, list );

    sqlist_init( &srb->list );
    srb->stat = TE_SUCCESS;
    srb->cmdptr = NULL;
    srb->cmdlen = 0;
    osal_completion_reset( &srb->done );
    srb->ar = NULL;
    srb->sctx = te_sess_ctx_get( sctx );
    memset( (void *)&srb->task, 0, sizeof(te_sess_ca_tsk_t) );
    memset( (void *)&srb->it, 0, sizeof(te_sess_ea_item_t) );
    memset( (void *)&srb->para, 0, sizeof(te_sess_cb_para_t) );

    return srb;
}

static void te_sess_ctx_put_srb( te_sess_ctx_t *sctx, te_sess_srb_t *srb )
{
    unsigned long flags = 0;
    /* Decrease session context refcnt */
    te_sess_ctx_put( srb->sctx );
    osal_spin_lock_irqsave( &sctx->lock, &flags );
    sqlist_insert_tail( &sctx->srbs, &srb->list );
    osal_spin_unlock_irqrestore( &sctx->lock, flags );

    osal_completion_signal( &sctx->srbs_available );

    return;
}

static te_sess_ctx_t *te_sess_ctx_init( te_sess_module_ctx_t *mctx,
                                        te_sess_slot_cat_t cat )
{
    int ret = TE_SUCCESS, hwctx_sz = mctx->hwctx_sz;
    te_sess_ctx_t *sctx = NULL;

    sctx = (te_sess_ctx_t *)osal_calloc( 1, sizeof(te_sess_ctx_t) );
    if ( sctx == NULL ) {
        return NULL;
    }

    ret = osal_mutex_create( &sctx->mutex );
    if ( ret != OSAL_SUCCESS ) {
        goto err1;
    }

    ret = osal_spin_lock_init( &sctx->lock );
    if ( ret != OSAL_SUCCESS ) {
        goto err2;
    }

    ret = osal_completion_init( &sctx->can_enqueue );
    if ( ret != OSAL_SUCCESS ) {
        goto err3;
    }

    ret = osal_completion_init( &sctx->srbs_available );
    if ( ret != OSAL_SUCCESS ) {
        goto err4;
    }

    ret = te_sess_gov_assign_sid( sctx );
    if ( ret != TE_SUCCESS ) {
        goto err5;
    }

    sctx->hwctx = osal_malloc_aligned( hwctx_sz, TE_DMA_ALIGNED );
    if ( sctx->hwctx == NULL ) {
        goto err6;
    }

    memset( sctx->hwctx, 0, hwctx_sz );
    /* clean & invalidate */
    osal_cache_flush( (uint8_t *)sctx->hwctx, hwctx_sz );

    sctx->stat = TE_SESS_NORMAL;
    osal_atomic_store( &sctx->refcnt, 0U );
    sctx->enqueue_threshold = TE_SESS_ENQ_THRSHLD;
    sctx->enqueue_cnt = 0;
    sqlist_init( &sctx->enqueued );
    sqlist_init( &sctx->srbs );
    ret = te_sess_ctx_srbs_create( sctx );
    if ( ret != TE_SUCCESS ) {
        goto err7;
    }

    /* reference to session module */
    sctx->mctx = te_sess_module_get( mctx );
    sctx->cat = cat;
    sctx->slotid = 0;

    return sctx;

err7:
    osal_free( sctx->hwctx );
err6:
    te_sess_gov_retrive_sid( sctx );
err5:
    osal_completion_destroy( &sctx->srbs_available );
err4:
    osal_completion_destroy( &sctx->can_enqueue );
err3:
    osal_spin_lock_destroy( &sctx->lock );
err2:
    osal_mutex_destroy( sctx->mutex );
err1:
    osal_free( sctx );
    sctx = NULL;
    return NULL;
}

static void te_sess_ctx_destroy( te_sess_ctx_t *sctx )
{
#ifdef CFG_TE_IRQ_EN
    uint32_t last = osal_read_timestamp_ms();

    /**
     * Wait until the sctx gets released if applicable.
     * The intent is to sync with the following callbacks that is called in the
     * interrupt service routine when the CFG_TE_IRQ_EN is enabled.
     * - te_sess_sync_srb_done()
     * - te_sess_async_srb_done()
     *
     * Just cannot destroy the sctx when it is still in use!
     * Note the sctx pointer is checked in te_sess_ctx_put(). So it is ok to use
     * the sctx pointer directly here.
     */
    while ( (sctx->flags & SESS_CTX_FLAG_USED) != 0U ) {
        osal_sleep_ms( SESS_WAIT_STEP_MS );
        if ( (osal_read_timestamp_ms() - last) > 1000U ) {
            OSAL_LOG_WARN( "Wait for sess(%d) done ...\n", sctx->sid );
            last = osal_read_timestamp_ms();
        }
    }
    osal_rmb(); /* read barrier#3 */
#endif /* CFG_TE_IRQ_EN */

    osal_mutex_destroy( sctx->mutex );
    osal_spin_lock_destroy( &sctx->lock );
    osal_completion_destroy( &sctx->can_enqueue );
    osal_completion_destroy( &sctx->srbs_available );
    te_sess_ctx_srbs_destroy( sctx );

    te_sess_module_put( sctx->mctx );
    te_sess_gov_retrive_sid( sctx );
    osal_cache_invalidate( (uint8_t *)sctx->hwctx, sctx->mctx->hwctx_sz );
    osal_free( sctx->hwctx );
    osal_free( sctx );
    return;
}

static void te_sess_sync_srb_done( te_sess_cb_para_t *para, int32_t err )
{
    te_sess_srb_t *srb = (te_sess_srb_t *)para->priv;
    struct te_sess_ctx *sctx = srb->sctx;

    osal_wmb(); /* write barrier#1 */
    sctx->flags |= SESS_CTX_FLAG_USED;  /* Set the used flag */
    srb->stat = err;
    osal_wmb(); /* write barrier#2 */
    para->status = err;
    osal_wmb(); /* write barrier#3 */
    SESS_CMD_WAKE_UP( &srb->done );
    sctx->flags &= ~SESS_CTX_FLAG_USED; /* Clear the used flag */

    return;
}

static bool te_sess_wait_sync_srb_done( te_sess_module_ctx_t *mctx,
                                        te_sess_cb_para_t *para )
{
    bool result = 0;
#ifndef CFG_TE_IRQ_EN
    te_sess_event_agent_t *ea = mctx->event_agent;
    te_sess_ea_dispatch_event( ea );
#endif

    osal_rmb(); /* read barrier#1 */
    if ( para->status != (int32_t)TE_ERROR_BUSY ) {
        result = true;
    } else {
        result = false;
    }
    osal_rmb(); /* read barrier#2 */

    return result;
}

int te_sess_submit( te_sess_id_t sid, const uint32_t *cmdptr, uint32_t len )
{
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_ctx_t *sctx = NULL;
    te_sess_srb_t *srb = NULL;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_cmd_agent_t *ca = NULL;

    ret = te_sess_gov_sid_is_valid( sid );
    UTILS_CHECK_CONDITION( (cmdptr != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "cmdptr is NULL\n" );

    UTILS_CHECK_CONDITION( (len != 0),
                          TE_ERROR_BAD_PARAMS,
                          "cmd buffer length is 0\n" );

    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_ctx_get_by_id( sid );

    srb = te_sess_ctx_get_srb( sctx );
    TE_ASSERT( srb != NULL );

    srb->cmdptr = (uint32_t *)cmdptr;
    srb->cmdlen = len;

    OSAL_LOG_DEBUG( "SESS_DRV[%s]: sess(%d) : "
                   "submit(cmd: %08x, id:%02x, len:%d)\n",
                   sctx->mctx->ishash ? "hash" : "sca",
                   (int)sid, cmdptr[0], CMDID(cmdptr), len);

    /* Lock this session, avoid other process enter, concurrently */
    osal_mutex_lock( sctx->mutex );
    mctx = sctx->mctx;
    ca = mctx->cmd_agent;

    /*
     * If session already in error status,
     * reject any CMD, except 'CLEAR'
     */
    if ( sctx->stat == TE_SESS_CLOSE ||
         ((sctx->stat == TE_SESS_ERROR) &&
          (CMDID(cmdptr) != CLEAR_CMD)) ) {

        ret = TE_ERROR_BAD_STATE;
        osal_mutex_unlock( sctx->mutex );
        goto out;
    }

    /* acquire slot */
    ret = te_sess_acquire_slot( sctx, srb );
    if ( ret != TE_SUCCESS ) {
        osal_mutex_unlock( sctx->mutex );
        goto out;
    }

    srb->para.status = (int32_t)TE_ERROR_BUSY;
    srb->para.priv = (void *)srb;

    te_sess_ca_prepare_task( sctx->slotid, srb->cmdptr, srb->cmdlen, &srb->task );
    te_sess_ea_prepare_event( sctx->slotid, srb->cmdptr,
                              te_sess_sync_srb_done,
                              &srb->para, &srb->it );
    ret = te_sess_ca_submit( ca, &srb->task, &srb->it );

    /* release this session lock */
    osal_mutex_unlock( sctx->mutex );
    if ( ret != TE_SUCCESS ) {
        goto out;
    }

    /* wait command done */
    SESS_CMD_WAIT_EVENT(
            ( te_sess_wait_sync_srb_done(sctx->mctx, &srb->para) ),
            &srb->done );

    /*
     * On error, just update session status does not release slot.
     * Let slot keep in OCCUPIED, wait user issue CLEAR command
     */
    if ( srb->stat != (int32_t)TE_SUCCESS &&
         srb->stat != (int32_t)TE_ERROR_CANCEL ) {
        te_sess_ctx_update_state( sctx, TE_SESS_ERROR );
    } else {
        if ( CMDID(srb->cmdptr) == CLEAR_CMD &&
             sctx->stat == TE_SESS_ERROR ) {
            te_sess_ctx_update_state( sctx, TE_SESS_NORMAL );
        }
        /* release slot */
        te_sess_release_slot( sctx, srb );
    }

    ret = srb->stat;
out:
    te_sess_ctx_put_srb( sctx, srb );
    te_sess_ctx_put( sctx );
finish:
    return ret;
}

static void te_sess_async_srb_done( te_sess_cb_para_t *para, int32_t err )
{
    unsigned long flags = 0;
    te_sess_srb_t *srb = (te_sess_srb_t *)para->priv;
    te_sess_ctx_t *sctx = srb->sctx;
    te_sess_srb_t *record = NULL;
    te_sess_ar_t *ar = NULL;

    srb->stat = err;
    sctx->flags |= SESS_CTX_FLAG_USED; /* Set the used flag */
    osal_wmb();
    switch (srb->stat) {
    case TE_SUCCESS:
        if ( CMDID(srb->cmdptr) == CLEAR_CMD &&
             sctx->stat == TE_SESS_ERROR ) {
            te_sess_ctx_update_state( sctx, TE_SESS_NORMAL );
        }
        break;
    case TE_ERROR_CANCEL:
        break;
    default:
        te_sess_ctx_update_state( sctx, TE_SESS_ERROR );
        break;
    }

    /* Sanity check and update enqueue counter */
    osal_spin_lock_irqsave( &sctx->lock, &flags );
    record = SQLIST_PEEK_HEAD_CONTAINER( &sctx->enqueued, record, list);

    /* Cancel case, can't keep the order */
    if ( srb->stat != (int)TE_ERROR_CANCEL ) {
        TE_ASSERT_MSG( (record == srb),
                         "SRB done miss match with SCTX record\n" );
    }

    /* Remove from enqueued list */
    sqlist_remove( &srb->list );
    if ( sqlist_is_empty(&sctx->enqueued) ) {
        sctx->enqueue_cnt = 0;

        /* It's not allowed to signal while holding a spin in tee */
        osal_spin_unlock_irqrestore( &sctx->lock, flags );

        /* Wakeup all waiting processes */
        osal_completion_signal( &sctx->can_enqueue );
    } else {
        osal_spin_unlock_irqrestore( &sctx->lock, flags );
    }

    /* release slot reference count */
    if ( srb->stat == (int)TE_SUCCESS ||
         srb->stat == (int)TE_ERROR_CANCEL ) {
        te_sess_release_slot( sctx, srb );
    }

    ar = srb->ar;
    ar->err = srb->stat;
    te_sess_ctx_put_srb( sctx, srb );
    sctx->flags &= ~SESS_CTX_FLAG_USED; /* Clear the used flag */

    osal_wmb();
    ar->cb( ar );
    return;
}

static bool te_sess_asubmit_wait_can_enqueue( te_sess_ctx_t *sctx )
{
    unsigned long flags = 0;
    bool result = false;

    osal_spin_lock_irqsave( &sctx->lock, &flags );

    if ( sctx->enqueue_cnt < sctx->enqueue_threshold ) {
        result = true;
    }

    osal_spin_unlock_irqrestore( &sctx->lock, flags );

    return result;
}

int te_sess_asubmit( te_sess_id_t sid, struct te_sess_ar *ar )
{
    int32_t ret = TE_ERROR_GENERIC;
    unsigned long flags = 0;
    te_sess_ctx_t *sctx = NULL;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_cmd_agent_t *ca = NULL;
    te_sess_srb_t *srb = NULL;

    ret = te_sess_gov_sid_is_valid( sid );
    UTILS_CHECK_CONDITION( (ar != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "ar is NULL\n" );

    UTILS_CHECK_CONDITION( (ar->cmdptr != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "ar->cmdptr is NULL\n" );

    UTILS_CHECK_CONDITION( (ar->len != 0),
                          TE_ERROR_BAD_PARAMS,
                          "cmd buffer length is 0\n" );

    UTILS_CHECK_CONDITION( (ar->cb != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "ar->cb is NULL\n" );

    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_sid_to_ctx( sid );

    srb = te_sess_ctx_get_srb( sctx );
    TE_ASSERT( srb != NULL );

    srb->cmdptr = (uint32_t *)ar->cmdptr;
    srb->cmdlen = ar->len;
    srb->ar = ar;

    OSAL_LOG_DEBUG( "SESS_DRV[%s]: sess(%d) : "
                   "ASYNC submit(cmd: %08x, id:%02x, len:%d)\n",
                   sctx->mctx->ishash ? "hash" : "sca",
                   (int)sid, ar->cmdptr[0], CMDID(ar->cmdptr), ar->len);
    /* Lock this session, avoid other process enter, concurrently */
    osal_mutex_lock( sctx->mutex );
    mctx = sctx->mctx;
    ca = mctx->cmd_agent;

    /*
     * If session already in error status,
     * reject any CMD, except 'CLEAR'
     */
    if ( (sctx->stat == TE_SESS_CLOSE) ||
         ((sctx->stat == TE_SESS_ERROR) &&
          (CMDID(srb->cmdptr) != CLEAR_CMD)) ) {

        ret = TE_ERROR_BAD_STATE;
        osal_mutex_unlock( sctx->mutex );
        goto out;
    }


    /* wait can enqueue */
    SESS_CMD_WAIT_EVENT(
                ( te_sess_asubmit_wait_can_enqueue( sctx ) ),
                &sctx->can_enqueue );

    /* acquire slot */
    ret = te_sess_acquire_slot( sctx, srb );
    if ( ret != TE_SUCCESS ) {
        osal_mutex_unlock( sctx->mutex );
        goto out;
    }

    osal_spin_lock_irqsave( &sctx->lock, &flags );
    sqlist_insert_tail( &sctx->enqueued, &srb->list );
    sctx->enqueue_cnt++;
    osal_spin_unlock_irqrestore( &sctx->lock, flags );

    srb->para.status = (int32_t)TE_ERROR_BUSY;
    srb->para.priv = (void *)srb;

    te_sess_ca_prepare_task( sctx->slotid, srb->cmdptr, srb->cmdlen, &srb->task );
    te_sess_ea_prepare_event( sctx->slotid, srb->cmdptr,
                              te_sess_async_srb_done,
                              &srb->para, &srb->it );
    ret = te_sess_ca_submit( ca, &srb->task, &srb->it );

    if ( ret != TE_SUCCESS ) {
        /* Revert the operation */
        osal_spin_lock_irqsave( &sctx->lock, &flags );
        sqlist_remove( &srb->list );
        sctx->enqueue_cnt--;
        osal_spin_unlock_irqrestore( &sctx->lock, flags );
        osal_mutex_unlock( sctx->mutex );
        goto out;
    }

    /* release this session lock */
    osal_mutex_unlock( sctx->mutex );
    return ret;

out:
    te_sess_ctx_put_srb( sctx, srb );
finish:
    return ret;

}

te_sess_id_t te_sess_open( te_sess_inst_t *inst,
                           const te_sess_slot_cat_t cat )
{
    int32_t ret = TE_SUCCESS;
    te_sess_ctx_t *sctx = NULL;
    te_sess_module_ctx_t *mctx = (te_sess_module_ctx_t *)inst;

    UTILS_CHECK_CONDITION( (mctx != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "inst is NULL\n" );

    UTILS_CHECK_CONDITION( ((cat == TE_SLOT_CATEGORY_LONG) ||
                          (cat == TE_SLOT_CATEGORY_SHORT)),
                          TE_ERROR_BAD_PARAMS,
                          "Bad slot category\n" );

    sctx = te_sess_ctx_init( mctx, cat );

    if ( sctx == NULL ) {
        return TE_ERROR_OOM;
    }

    /* Increase refcnt */
    sctx = te_sess_ctx_get( sctx );

    return sctx->sid;

finish:
    return ret;
}

int te_sess_close( te_sess_id_t sid )
{
    int32_t ret = TE_SUCCESS;
    te_sess_ctx_t *sctx = NULL;

    ret = te_sess_gov_sid_is_valid( sid );
    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_sid_to_ctx( sid );

    osal_mutex_lock( sctx->mutex );
    TE_ASSERT( sctx->stat != TE_SESS_CLOSE );

    /*
     * If session already in error status,
     * must issue CLEAR first
     */
    if ( sctx->stat == TE_SESS_ERROR ) {
        ret = TE_ERROR_BAD_STATE;
        osal_mutex_unlock( sctx->mutex );
        goto finish;
    }

    /*
     * BUG condtion, before close, must submit FINISH or CLEAR.
     * The refcnt must be 1 which means no pending or processing request.
     */
    TE_ASSERT( osal_atomic_load(&sctx->refcnt) == 1 );

    te_sess_ctx_update_state( sctx, TE_SESS_CLOSE );
    osal_mutex_unlock( sctx->mutex );
    te_sess_ctx_put( sctx );

finish:
    return ret;
}

te_sess_id_t te_sess_clone( te_sess_id_t sid )
{
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_ctx_t *sctx = NULL, *nctx = NULL;

    ret = te_sess_gov_sid_is_valid( sid );
    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_ctx_get_by_id( sid );

    /* Lock this session, avoid other process enter, concurrently */
    osal_mutex_lock( sctx->mutex );

    /*
     * If session already in error status,
     */
    if ( (sctx->stat == TE_SESS_ERROR) ||
         (sctx->stat == TE_SESS_CLOSE) ) {
        ret = TE_ERROR_BAD_STATE;
        goto err1;
    }

    nctx = te_sess_ctx_init( sctx->mctx, sctx->cat );
    if ( nctx == NULL ) {
        ret = TE_ERROR_OOM;
        goto err1;
    }

    nctx = te_sess_ctx_get( nctx );

    ret = te_sess_slg_clone( nctx, sctx );
    if ( ret != TE_SUCCESS ) {
        goto err2;
    }

    osal_mutex_unlock( sctx->mutex );
    te_sess_ctx_put( sctx );

    return nctx->sid;

err2:
    te_sess_ctx_put( nctx );
err1:
    osal_mutex_unlock( sctx->mutex );
    te_sess_ctx_put( sctx );
finish:
    return INVALID_SESS_ID;
}

int te_sess_cancel( te_sess_id_t sid )
{
    int32_t ret = TE_ERROR_GENERIC;
    unsigned long flags = 0;
    te_sess_ctx_t *sctx = NULL;
    te_sess_srb_t *srb = NULL;
    te_sess_cmd_agent_t *ca = NULL;
    te_sess_event_agent_t *ea = NULL;
    ret = te_sess_gov_sid_is_valid( sid );

    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_ctx_get_by_id( sid );
    ca = sctx->mctx->cmd_agent;
    ea = sctx->mctx->event_agent;
    osal_mutex_lock( sctx->mutex );

    while ( 1 ) {
        /* Need hold the spinlock avoid async callback free the SRB */
        osal_spin_lock_irqsave( &sctx->lock, &flags );

        /* cancel for the tail of the queue */
        srb = SQLIST_CONTAINER( sqlist_peek_tail(&sctx->enqueued),
                                srb,
                                list );
        osal_spin_unlock_irqrestore( &sctx->lock, flags );

        /* if no pending request */
        if ( srb == NULL ) {
            break;
        }

        /*
         * Cancel may return error, which means this srb already be written into CQ
         * if not in CA queue, we can't cancel this command in EA, because
         * the command finish event can be issued at any time.
         */
        ret = te_sess_ca_cancel( ca, &srb->task );
        if ( ret != TE_SUCCESS && sctx->stat == TE_SESS_NORMAL ) {
            continue;
        }

        /* srb callback may be called right now, but can't get sctx spinlock */
        ret = te_sess_ea_cancel( ea, &srb->it );
    };

    osal_mutex_unlock( sctx->mutex );
    ret = TE_SUCCESS;
    te_sess_ctx_put( sctx );

finish:
    return ret;
}

/*
 * The idea to export the hwctx of the slot of the calling session outside:
 * 1. lock out the session, to stop accepting new commands.
 * 2. issue a WRAPOUT command and queue it in the CA task list.
 * 3. wait until the engine done the WRAPOUT command or detect any error.
 * 4. copy the hwctx out if WRAPOUT well.
 * 5. unlock the session.
 */
int te_sess_export( te_sess_id_t sid,
                    void *out,
                    uint32_t *olen )
{
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_ctx_t *sctx = NULL;

    UTILS_CHECK_CONDITION( (olen != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "null olen\n" );

    ret = te_sess_gov_sid_is_valid( sid );
    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_ctx_get_by_id( sid );
    mctx = sctx->mctx;
    if ( *olen < mctx->hwctx_sz ) {
        *olen = mctx->hwctx_sz;
        ret = TE_ERROR_SHORT_BUFFER;
        goto err1;
    }

    if ( !out ) {
        OSAL_LOG_ERR( "null out!\n" );
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    /* Lock this session, to avoid racing with other threads */
    osal_mutex_lock( sctx->mutex );

    /*
     * If session already in error status,
     */
    if ( (sctx->stat == TE_SESS_ERROR) ||
         (sctx->stat == TE_SESS_CLOSE) ) {
        ret = TE_ERROR_BAD_STATE;
        goto err2;
    }

    /*
     * Initiatively wrapout the slot of the calling sess to obtain the hwctx.
     */
    ret = te_sess_slg_wrapout_one( mctx->slot_gov, sctx );
    if ( ret == TE_SUCCESS ) {
        /*
         * Copy the hwctx out while leaving the slot in wrapout state.
         * The slot will be unwrapin by the CA on arrival of further commands
         * of this session.
         */
        osal_cache_invalidate( (uint8_t *)sctx->hwctx, mctx->hwctx_sz );
        osal_memcpy( out, sctx->hwctx, mctx->hwctx_sz );
        *olen = mctx->hwctx_sz;
    }

err2:
    osal_mutex_unlock( sctx->mutex );
err1:
    te_sess_ctx_put( sctx );
finish:
    return ret;
}

/*
 * The idea to import the hwctx of the slot for the calling session:
 * 1. lock out the session, to stop accepting new commands.
 * 2. update the hwctx using the imported one if the session is not bound yet.
 * 3. issue a WRAPOUT command and queue it in the CA task list.
 * 4. wait until the engine done the WRAPOUT command or detect any error.
 * 5. update the hwctx using the imported one if WRAPOUT well.
 * 6. unlock the session.
 */
int te_sess_import( te_sess_id_t sid,
                    const void *in,
                    uint32_t ilen )
{
    int32_t ret = TE_ERROR_GENERIC;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_ctx_t *sctx = NULL;

    UTILS_CHECK_CONDITION( (in != NULL),
                          TE_ERROR_BAD_PARAMS,
                          "null in\n" );

    ret = te_sess_gov_sid_is_valid( sid );
    UTILS_CHECK_CONDITION( (ret == TE_SUCCESS),
                          TE_ERROR_BAD_PARAMS,
                          "invalid session id\n" );

    sctx = te_sess_ctx_get_by_id( sid );
    mctx = sctx->mctx;

    if ( ilen != mctx->hwctx_sz ) {
        OSAL_LOG_ERR( "bad ilen %d\n", ilen );
        ret = TE_ERROR_BAD_PARAMS;
        goto err1;
    }

    /* Lock this session, avoid racing with other threads */
    osal_mutex_lock( sctx->mutex );

    /*
     * If session already in error status,
     */
    if ( (sctx->stat == TE_SESS_ERROR) ||
         (sctx->stat == TE_SESS_CLOSE) ) {
        ret = TE_ERROR_BAD_STATE;
        goto err2;
    }

    ret = te_sess_slg_wrapout_one( mctx->slot_gov, sctx );
    if ( ret == TE_SUCCESS ) {
        /*
         * Update the hwctx and leave the slot in wrapout state.
         * The renewed hwctx will take effect during wrapin next time.
         */
        osal_memcpy( sctx->hwctx, in, mctx->hwctx_sz );
        /*
         * clean cache lines
         */
        osal_cache_clean( (uint8_t *)sctx->hwctx, mctx->hwctx_sz );
    }

err2:
    osal_mutex_unlock( sctx->mutex );
err1:
    te_sess_ctx_put( sctx );
finish:
    return ret;
}

void te_sess_module_clk_get( te_sess_module_ctx_t *mctx )
{
#ifdef CFG_TE_DYNCLK_CTL
    int ret = TE_ERROR_GENERIC;
    te_hwa_sca_t *hwa = (te_hwa_sca_t *)mctx->hwa;
    te_sca_ctl_t ctl = { 0 };
    unsigned long flags = 0;
    osal_spin_lock_irqsave( &mctx->lock, &flags );
    if ( mctx->clk_refcnt == 0 ) {
        ret = hwa->get_ctrl( hwa, &ctl );
        TE_ASSERT_MSG( ret == TE_SUCCESS,
                       "Fatal error, can't get ctrl register!\n" );
        ctl.clk_en = 1;
        ret = hwa->set_ctrl( hwa, &ctl );
        TE_ASSERT_MSG( ret == TE_SUCCESS,
                       "Fatal error, can't set ctrl register !\n" );
    }
    mctx->clk_refcnt++;
    osal_spin_unlock_irqrestore( &mctx->lock, flags );
#else
    (void)mctx;
#endif
    return;
}

void te_sess_module_clk_put( te_sess_module_ctx_t *mctx )
{
#ifdef CFG_TE_DYNCLK_CTL
    int ret = TE_ERROR_GENERIC;
    te_hwa_sca_t *hwa = (te_hwa_sca_t *)mctx->hwa;
    te_sca_ctl_t ctl = { 0 };
    unsigned long flags = 0;
    osal_spin_lock_irqsave( &mctx->lock, &flags );

    TE_ASSERT( mctx->clk_refcnt != 0 );

    mctx->clk_refcnt--;

    if ( mctx->clk_refcnt == 0 ) {
        ret = hwa->get_ctrl( hwa, &ctl );
        TE_ASSERT_MSG( ret == TE_SUCCESS,
                       "Fatal error, can't get ctrl register!\n" );
        ctl.clk_en = 0;
        ret = hwa->set_ctrl( hwa, &ctl );
        TE_ASSERT_MSG( ret == TE_SUCCESS,
                       "Fatal error, can't set ctrl register !\n" );
    }
    osal_spin_unlock_irqrestore( &mctx->lock, flags );
#else
    (void)mctx;
#endif
    return;
}

int te_sess_module_suspend( te_sess_inst_t *inst )
{
    int ret = TE_ERROR_GENERIC;
    te_sess_module_ctx_t *mctx = NULL;
    te_sess_slot_gov_t *sltgov = NULL;
    te_sess_event_agent_t *ea = NULL;
    if ( !inst ) {
        return TE_ERROR_BAD_PARAMS;
    }

    mctx = (te_sess_module_ctx_t *)inst;
    sltgov = mctx->slot_gov;
    ea = mctx->event_agent;

    while (1) {
        ret = te_sess_slg_wrapout_all( sltgov );
        if ( ret == TE_SUCCESS ) {
            break;
        }

        if ( ret == (int)TE_ERROR_BUSY ) {
            te_sess_ea_dispatch_event( ea );
        }
    }
    return ret;
}

int te_sess_module_resume( te_sess_inst_t *inst )
{
    if ( !inst ) {
        return TE_ERROR_BAD_PARAMS;
    }

    return TE_SUCCESS;
}

te_sess_inst_t *te_sess_module_init( void *hwa, bool ishash )
{
    te_sess_module_ctx_t *mctx = NULL;
    int ret = TE_ERROR_GENERIC;
    te_hwa_stat_t *hwa_stat = NULL;
    te_hwa_sca_t *hwa_sca = NULL;
    te_hwa_host_t *hwa_host = NULL;
    te_rtl_conf_t conf = { 0 };
    int hwctx_sz = 0;

    TE_ASSERT_MSG( hwa, "parameter hwa is NULL!\n" );

    ret = te_sess_gov_init();
    UTILS_CHECK_RET( "Session governor instance initialize error\n" );

    mctx = (te_sess_module_ctx_t *)
                osal_calloc( 1, sizeof(te_sess_module_ctx_t) );
    UTILS_CHECK_CONDITION( mctx != NULL, TE_ERROR_OOM,
            "Can't allocate session module instance(OOM)\n" );

    hwa_sca = (te_hwa_sca_t *)hwa;
    hwa_host = hwa_crypt_host( &hwa_sca->base );
    hwa_stat = &hwa_host->stat;

    ret = hwa_stat->conf( hwa_stat, &conf );
    TE_ASSERT_MSG( ret == TE_SUCCESS,
                     "Fatal error, can't read host top conf!\n" );
    hwctx_sz = conf.sram.wrap_max_sz;
    hwctx_sz = UTILS_ROUND_UP( hwctx_sz, TE_DMA_ALIGNED );
    mctx->hwctx_sz = hwctx_sz;

    osal_atomic_store( &mctx->refcnt, 0U);
    mctx = te_sess_module_get( mctx );
    mctx->hwa = hwa;
    mctx->ishash = ishash;
    /* reference of session governor */
    mctx->sess_gov = te_sess_gov_get( g_sess_gov );

    ret = te_sess_slg_init( mctx );
    if ( ret ) {
        goto err1;
    }

    ret = te_sess_ca_init( mctx, ishash ? conf.sram.hash_cq_depth :
                                          conf.sram.sca_cq_depth );
    if ( ret ) {
        goto err2;
    }
    ret = te_sess_ea_init( mctx );
    if ( ret ) {
        goto err3;
    }

    ret = osal_spin_lock_init( &mctx->lock );
    if ( ret != OSAL_SUCCESS ) {
        goto err4;
    }

    mctx->clk_refcnt = 0;

    return (te_sess_inst_t *)mctx;

err4:
    te_sess_ea_destroy( mctx );
err3:
    te_sess_ca_destroy( mctx );
err2:
    te_sess_slg_destroy( mctx );
err1:
    te_sess_gov_put( g_sess_gov );
    osal_free( mctx );
    mctx = NULL;
finish:
    return NULL;
}

int te_sess_module_deinit( te_sess_inst_t *inst )
{
    int ret = TE_ERROR_GENERIC;
    int err = TE_SUCCESS;
    te_sess_module_ctx_t *mctx = NULL;

    TE_ASSERT_MSG( (inst != NULL),  "inst is NULL\n" );

    mctx = (te_sess_module_ctx_t *)inst;

    if ( osal_atomic_load( &mctx->refcnt ) != 1 ) {
        return TE_ERROR_BUSY;
    }

    ret = te_sess_slg_destroy( mctx );
    if ( ret ) {
        OSAL_LOG_ERR( "slot governor destroy failed\n");
        err = TE_ERROR_GENERIC;
    }

    ret = te_sess_ca_destroy( mctx );
    if ( ret ) {
        OSAL_LOG_ERR( "command agent destroy failed\n");
        err = TE_ERROR_GENERIC;
    }

    ret = te_sess_ea_destroy( mctx );
    if ( ret ) {
        OSAL_LOG_ERR( "event agent destroy failed\n");
        err = TE_ERROR_GENERIC;
    }

    osal_spin_lock_destroy( &mctx->lock );
    mctx->clk_refcnt = 0;

    /* Dereference of session governor */
    te_sess_gov_put( mctx->sess_gov );

    osal_free( mctx );
    mctx = NULL;

    return err;
}

