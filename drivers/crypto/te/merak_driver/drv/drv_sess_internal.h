//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#ifndef __TRUSTENGINE_DRV_SESS_INTERNAL_H__
#define __TRUSTENGINE_DRV_SESS_INTERNAL_H__

#include "drv_sess.h"
#include <hwa/te_hwa.h>
#include <sqlist.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define INIT_CMD        (0x80U)
#define PROC_CMD        (0x40U)
#define FINISH_CMD      (0x20U)
#define CLONE_CMD       (0x08U)
#define WRAPOUT_CMD     (0x04U)
#define UNWRAPIN_CMD    (0x02U)
#define CLEAR_CMD       (0xFFU)

#define TE_SESS_TINT    (0x01U)


#define CMDID(__ptr) ({                                     \
    GET_BITS((*((uint32_t *)(__ptr))), 24, 8 );             \
})

#define SLOTID(__ptr) ({                                    \
    GET_BITS((*((uint32_t *)(__ptr))), 19, 5 );             \
})

#define SET_SLOTID(__ptr, __v)      ({                      \
        uint32_t __t = (*((uint32_t *)(__ptr)));            \
        SET_BITS(__t, 19, 5, __v);                          \
        (*((uint32_t *)(__ptr))) = (__t);                   \
})

#define MAX_SESSION_ID  (0x7fffffffU)
#define TE_SESS_ENQ_THRSHLD     (2)
#define MAX_SLOT_NUM    (32)

#define BIT_PER_BYTE (8)

#ifdef CFG_TE_IRQ_EN
#define SESS_CMD_WAIT_EVENT(condition, comp)                \
    OSAL_COMPLETION_COND_WAIT(condition, comp)

#define SESS_CMD_WAKE_UP(comp) osal_completion_signal(comp)

#else  /* CFG_TE_IRQ_EN */
#define SESS_CMD_WAIT_EVENT(condition, comp) do {           \
    (void)(comp);                                           \
    if (condition) {                                        \
        break;                                              \
    }                                                       \
} while(1)

#define SESS_CMD_WAKE_UP(comp) do{                          \
    (void)(comp);                                           \
}while(0)

#endif /* !CFG_TE_IRQ_EN */

struct te_sess_ctx;
struct te_sess_slot_gov;
struct te_sess_module_ctx;

/**
 * The session state
 */
typedef enum te_sess_st {
    TE_SESS_NORMAL = 0,
    TE_SESS_ERROR = 1,
    TE_SESS_CLOSE = 2
} te_sess_st_t;

/**
 * states of slot
 */
typedef enum {
    TE_SLOT_FREE = 0,       /**< Slot is free, no SSF bind */
    TE_SLOT_OCCUPIED,       /**< Slot is occupied, can't be swapped out */
    TE_SLOT_SHARED,         /**< Slot is shared, can be swapped out */
    TE_SLOT_SWAPING,        /**< Slot is in swapping */
    TE_SLOT_INVALID
} te_sess_slot_st_t;

/**
 * Trust engine session callback parameter
 */
typedef struct te_sess_cb_para {
    volatile int32_t status;        /**< status of command execution */
    void *priv;                     /**< caller private data */
} te_sess_cb_para_t;

/**
 * command agent task item
 */
typedef struct te_sess_ca_tsk {
    sqlist_t list;
    int32_t slotid;         /**< slot id */
    uint32_t *cmdptr;       /**< command frame */
    uint32_t cmdlen;        /**< frame length */
    uint32_t offs;          /**< access offset */
} te_sess_ca_tsk_t;

/**
 * Event agent callback function
 */
typedef void (*te_sess_notify_t)( te_sess_cb_para_t *para, int32_t err );

/**
 * event agent callback item
 */
typedef struct te_sess_ea_item {
    sqlist_t list;
    int32_t slotid;             /**< slot id */
    uint32_t *cmdptr;           /**< command frame */
    te_sess_notify_t notify;    /**< callback */
    te_sess_cb_para_t *para;    /**< callback parameter */
} te_sess_ea_item_t;

/**
 * slot structure
 */
typedef struct te_sess_slot {
    sqlist_t list;
    osal_completion_t wrapout;          /**< wrapout completion */
    osal_completion_t wrapin;           /**< wrapin completion */
    osal_completion_t clear;            /**< clear completion */
    osal_completion_t clone;            /**< clone completion */
    int32_t id;                         /**< slot id */
    osal_atomic_t refcnt;               /**< reference count */
    te_sess_slot_st_t stat;             /**< slot state */
    te_sess_slot_cat_t cat;             /**< slot category */
    struct te_sess_ctx *sctx;           /**< session context reference */
    struct te_sess_slot_gov *governor;  /**< session governor reference */
} te_sess_slot_t;

/**
 * Slot governor context structure
 */
typedef struct te_sess_slot_gov {
    osal_mutex_t clone;                       /**< mutex of clone op */
    osal_spin_lock_t lock;                    /**< spin lock */
    int32_t sslot_num;                        /**< short slot number */
    osal_completion_t sslt_available;         /**< short slot available */
    int32_t lslot_num;                        /**< long slot number */
    osal_completion_t lslt_available;         /**< long slot available */
    struct te_sess_slot *slots[MAX_SLOT_NUM]; /**< slot array */
    unsigned long sslt_free;                  /**< bitmap of free short slot */
    unsigned long lslt_free;                  /**< bitmap of free long slot */
    sqlist_t sslt_shared;                     /**< list of shared short slot */
    sqlist_t lslt_shared;                     /**< list of shared long slot */
    struct te_sess_module_ctx *mctx;          /**< session module context */
} te_sess_slot_gov_t;

/**
 * session request block
 */
typedef struct te_sess_srb {
    sqlist_t list;
    int32_t stat;                   /**< stat of srb */
    uint32_t *cmdptr;               /**< command frame buffer */
    uint32_t cmdlen;                /**< command frame buffer len */
    osal_completion_t done;         /**< srb done completion */
    struct te_sess_ar *ar;          /**< Asynchronous requset structure */
    te_sess_ca_tsk_t task;          /**< Command agent task */
    te_sess_ea_item_t it;           /**< Event agent event item */
    te_sess_cb_para_t para;         /**< callback para */
    struct te_sess_ctx *sctx;       /**< reference to session context */
} te_sess_srb_t;

/**
 *  session context
 */
typedef struct te_sess_ctx {
    osal_mutex_t mutex;                 /**< mutex */
    osal_spin_lock_t lock;              /**< spin lock */
    te_sess_st_t stat;                  /**< stat of session */
    osal_atomic_t refcnt;               /**< reference count */
    int32_t sid;                        /**< session id */
    int32_t enqueue_threshold;          /**< threshold of continuous req */
    int32_t enqueue_cnt;                /**< reqs count, that enqueued */
    osal_completion_t can_enqueue;      /**< completion indicate can enqueue */
    sqlist_t srbs;                      /**< SRBs available to use */
    osal_completion_t srbs_available;   /**< indicate srbs available to get */
    sqlist_t enqueued;                  /**< reqs that be submitted */
    struct te_sess_module_ctx *mctx;    /**< session module context */
    te_sess_slot_cat_t cat;             /**< session slot category */
    int32_t slotid;                     /**< slot id of this session */
    void *hwctx;                        /**< HW context */
} te_sess_ctx_t;

/**
 * Session governor context structure
 */
typedef struct te_sess_gov {
    osal_mutex_t mutex;
    osal_atomic_t refcnt;
    te_sess_ctx_t **ctxs;               /**< session context array */
    unsigned long *free_sids;           /**< bitmap of free session id */
    int32_t bitmap_len;                 /**< bitmap buffer length */
    uint32_t assigned;                  /**< assigned session id count */
} te_sess_gov_t;

/**
 * command agent context
 */
typedef struct te_sess_cmd_agent {
    osal_spin_lock_t lock;
    struct te_sess_module_ctx *mctx;    /**< session module context */
    sqlist_t *cur;                      /**< current work task */
    int qidx;                           /**< queue index */
    sqlist_t queues[MAX_SLOT_NUM];      /**< task queues */
} te_sess_cmd_agent_t;

/**
 * event agent context
 */
typedef struct te_sess_event_agent {
    osal_spin_lock_t lock;
    struct te_sess_module_ctx *mctx;    /**< session module context */
    sqlist_t queues[MAX_SLOT_NUM];      /**< event book queues */
    te_irq_nb_handle nb;                /**< hwa irq instance */
} te_sess_event_agent_t;

/**
 * session module context
 */
typedef struct te_sess_module_ctx {
    osal_spin_lock_t lock;
    uint32_t clk_refcnt;                    /**< Clock reference count */
    osal_atomic_t refcnt;
    uint32_t hwctx_sz;                      /**< h/w context size in byte */
    void *hwa;                              /**< hwa pointer */
    bool ishash;                            /**< true for HASH, other SCA */
    te_sess_gov_t *sess_gov;                /**< session governor instance */
    te_sess_slot_gov_t *slot_gov;           /**< slot governor instance */
    te_sess_cmd_agent_t *cmd_agent;         /**< command agent instance */
    te_sess_event_agent_t *event_agent;     /**< event agent instance */
} te_sess_module_ctx_t;

/* bit operation */
unsigned long te_sess_find_first_bit(unsigned long *addr, unsigned long size);
void te_sess_set_bit(int nr, unsigned long *addr);
void te_sess_clear_bit(int nr, unsigned long *addr);
int te_sess_test_bit(int nr, unsigned long *addr);

/* session context */
void te_sess_ctx_update_state( te_sess_ctx_t *sctx,
                               te_sess_st_t st );
int te_sess_ctx_check_state( te_sess_ctx_t *sctx );
void te_sess_ctx_put( te_sess_ctx_t *sctx );
te_sess_ctx_t *te_sess_ctx_get( te_sess_ctx_t *sctx );


/* slot governor */
int te_sess_slg_destroy( te_sess_module_ctx_t *mctx );
int te_sess_slg_init( te_sess_module_ctx_t *mctx );
int te_sess_slg_acquire_slot( te_sess_slot_gov_t *sltgov,
                              te_sess_ctx_t *sctx,
                              bool wrapin );
void te_sess_slg_release_slot( te_sess_slot_gov_t *sltgov,
                                      int32_t id,
                                      uint32_t cmd );
int te_sess_slg_clone( te_sess_ctx_t *dest, te_sess_ctx_t *src );
int te_sess_slg_wrapout_one( te_sess_slot_gov_t *sltgov,
                             te_sess_ctx_t *sctx );
int te_sess_slg_wrapout_all( te_sess_slot_gov_t *sltgov );

/* command agent */
int te_sess_ca_init( te_sess_module_ctx_t *mctx);
int te_sess_ca_destroy( te_sess_module_ctx_t *mctx );
void te_sess_ca_prepare_task( int32_t slotid,
                              uint32_t *cmdptr,
                              uint32_t cmdlen,
                              te_sess_ca_tsk_t *tsk );
int te_sess_ca_submit( te_sess_cmd_agent_t *ca,
                       te_sess_ca_tsk_t *tsk,
                       te_sess_ea_item_t *it );
int te_sess_ca_cancel( te_sess_cmd_agent_t *ca,
                       te_sess_ca_tsk_t *tsk );
void te_sess_ca_fill( te_sess_cmd_agent_t *ca );


/* event agent */
int te_sess_ea_init( te_sess_module_ctx_t *mctx );
int te_sess_ea_destroy( te_sess_module_ctx_t *mctx );
void te_sess_ea_prepare_event( int32_t slotid,
                               uint32_t *cmdptr,
                               te_sess_notify_t notify,
                               te_sess_cb_para_t *para,
                               te_sess_ea_item_t *it );
int te_sess_ea_book_event( te_sess_event_agent_t *ea,
                           te_sess_ea_item_t *it );
int te_sess_ea_cancel( te_sess_event_agent_t *ea,
                       te_sess_ea_item_t *it );
void te_sess_ea_dispatch_event( te_sess_event_agent_t *ea );

/* session module clock control */
void te_sess_module_clk_get( te_sess_module_ctx_t *mctx );
void te_sess_module_clk_put( te_sess_module_ctx_t *mctx );
#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif
#endif /* __TRUSTENGINE_DRV_SESS_INTERNAL_H__ */
