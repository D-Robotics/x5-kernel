//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DRV_SESS_H__
#define __TRUSTENGINE_DRV_SESS_H__

#include <te_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#define INVALID_SESS_ID         (-1)

/**
 * Trust engine session index
 */
typedef int32_t te_sess_id_t;

/**
 * Trust engine session module instance
 */
typedef void *  te_sess_inst_t;

/**
 * Trust engine slot category
 */
typedef enum {
    TE_SLOT_CATEGORY_LONG = 0,  /**< long slot */
    TE_SLOT_CATEGORY_SHORT = 1, /**< short slot */
    TE_SLOT_CATEGORY_MAX = 2
} te_sess_slot_cat_t;

/**
 * Trust Engine session asynchronous requset
 */
typedef struct te_sess_ar {
    const uint32_t *cmdptr;         /**< cmd frame ptr */
    uint32_t len;                   /**< byte length of cmd frame */
    void *para;                     /**< parameter user defined */
    int32_t err;                    /**< error code, TE_SUCCESS success */
    void (*cb)(struct te_sess_ar *ar);   /**< callback of this request */
} te_sess_ar_t;

/**
 * \brief       Session module initialize function
 *              \p need be called by SCA and HASH module,
 *              \p with its HWA instance.
 *
 * \param[in]   hwa     SCA or HASH HWA instance pointer.
 * \param[in]   ishash  Indicate HWA is for HASH, otherwise for SCA.
 *
 * \return      Session instance on success.
 * \return      NULL on failure.
 */
te_sess_inst_t *te_sess_module_init( void *hwa, bool ishash );

/**
 * \brief       Session module de-initialize function
 *
 * \param[in]   inst    session module instance.
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_module_deinit( te_sess_inst_t *inst );

/**
 * \brief       Session module suspend function
 *      called from SCA & HASH module driver, when system suspend raised.
 *      freezing trust engine SCA & HASH part, save HW context and be
 *      ready for enter system suspend.
 *
 * \param[in]   inst            session module instance.
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_module_suspend( te_sess_inst_t *inst );

/**
 * \brief       Session module resume function
 *      called from SCA & HASH module driver, when system resume raised.
 *      restore HW context, resume trust enigne SCA & HASH part and be
 *      ready for serve module drivers.
 *
 * \param[in]   inst            session module instance.
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_module_resume( te_sess_inst_t *inst );

/**
 * \brief       Create a trust engine session
 *
 * \param[in]   inst    session module instance.
 * \param[in]   cat     HW slot category, Indicate
 *          \p this session use long or short HW slot.
 *
 * \return      >0      success, the valid session id.
 * \return      other   failed.
 */
te_sess_id_t te_sess_open( te_sess_inst_t *inst,
                           const te_sess_slot_cat_t cat );

/**
 * \brief       Close a trust engine session
 *
 * \param[in]   sid             The id of the session wants to be closed.
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_close( te_sess_id_t sid );

/**
 * \brief       Clone a trust engine session
 *
 * \param[in]   sid     The id of the session wants to be clone.
 *
 * \return      >0      success, the valid session id.
 * \return      other   failed.
 */
te_sess_id_t te_sess_clone( te_sess_id_t sid );

/**
 * \brief       Cancel queued requests of a trust engine session
 *
 * \param[in]   sid             session id to specify which session
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_cancel( te_sess_id_t sid );

/**
 * \brief       Submit trust engine command, synchronously.
 *
 * \param[in]   sid             The session id.
 * \param[in]   cmdptr          ptr point to trust engine command frame.
 * \param[in]   len             size of commmand frame in byte.
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_submit( te_sess_id_t sid, const uint32_t *cmdptr, uint32_t len );

/**
 * \brief       Submit Asynchronous Request.
 *
 * \param[in]   sid             The session id.
 * \param[in]   ar              ptr of asynchronous request.
 *
 * \return      TE_SUCCESS      success.
 * \return      other           failed.
 */
int te_sess_asubmit( te_sess_id_t sid, struct te_sess_ar *ar );

/**
 * \brief           This function exports partial state of the session. This
 *                  function dumps the entire hw state of the specified session
 *                  into a provided block of data so it can be @import 'ed
 *                  back later on. This is useful in case you want to save
 *                  partial result of the calculation after processing certain
 *                  amount of data and reload this partial result multiple
 *                  times later on for multiple re-use.
 *
 * \param[in]  sid  The session id.
 * \param[out] out  Buffer filled with the hwctx data on success.
 * \param[inout] olen Size of \p out buffer on input.
 *                    Length of data filled in the \p out buffer on success.
 *                    Required \p out buffer length on TE_ERROR_SHORT_BUFFER.
 * \return          \c TE_SUCCESS on success.
 *                  \c TE_ERROR_SHORT_BUFFER if *olen is less than required.
 * \return          \c <0 on failure.
 */

int te_sess_export( te_sess_id_t sid,
                    void *out,
                    uint32_t *olen );

/**
 * \brief           This function imports partial state of the calculation.
 *                  This function loads the entire hw state of the specified
 *                  session from a provided block of data so the calculation
 *                  can continue from this point onward.
 *
 * \param[in] sid   The session id.
 * \param[in] in    Buffer filled with the hwctx data exported early.
 * \param[in] ilen  Size of the state data in the \p in buffer.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
int te_sess_import( te_sess_id_t sid,
                    const void *in,
                    uint32_t ilen );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_DRV_SESS_H__ */
