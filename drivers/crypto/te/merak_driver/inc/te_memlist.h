//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_MEMLIST_H__
#define __TRUSTENGINE_MEMLIST_H__

#include <te_common.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * memory entry structure
 */
typedef struct te_mement {
    void *buf;
    size_t len;
} te_mement_t;

/**
 * memory list structure
 */
typedef struct te_memlist {
    te_mement_t *ents;
    uint32_t nent;               /**< number of entries */
} te_memlist_t;

/**
 * memory list break point structure
 */
typedef struct te_ml_bp{
    size_t nent;   /**< original list's number of entries */
    int ind;       /**< node's index of the break point */
    size_t len;    /**< length of break point's node */
    size_t offset; /**< offset of break point's node */
} te_ml_bp_t;

/**
 * \brief           This function gets the total length of a memory list.
 * \param[in] list  The memory list.
 * \return          The totoal length of the memory list.
 */
size_t te_memlist_get_total_len( te_memlist_t *list );

/**
 * \brief                This function truancates a memory list from tail.
 * \param[in] list       The memory list to be truncated,
 *                       mandatory if b_copy is true.
 * \param[out] buf       Buf to hold truncated data if b_copy is true,
 *                       mandatory if b_copy is true.
 * \param[in] remainder  Size of data to be truncated.
 * \param[in] b_copy     Save truncated data or not, \p true save the data to buf,
 *                       \p false skip saving.
 * \param[out] info      Object to hold break point's info.
 * \return               none
 */
void te_memlist_truncate_from_tail( te_memlist_t *list,
                                    uint8_t *buf,
                                    size_t remainder,
                                    bool b_copy,
                                    te_ml_bp_t *info );

/**
 * \brief            This function truancates a memory list from head.
 * \param[in] list   The memory list to be truncated.
 * \param[in] len    Size of data to be truncated.
 * \param[out] info  Object to hold break point's info.
 * \return           none.
 */
void te_memlist_truncate_from_head( te_memlist_t *list,
                                    size_t len,
                                    te_ml_bp_t *info );

/**
 * \brief                This function copies data from a memory list from tail.
 * \param[in] list       The memory list to be copied.
 * \param[out] buf       Buf to hold the data.
 * \param[in] size       Size of data to be copied.
 * \return               \c TE_SUCCESS on success, others failed.
 */
int te_memlist_copy_from_tail( te_memlist_t *list,
                               uint8_t *buf,
                               size_t size );

/**
 * \brief                This function clones a slice from src list to dst list.
 * \param[out] dst       The dst list to hold the slice.
 * \param[in]  src       The src list to clone from.
 * \param[in]  offset    offset from head.
 * \param[in]  len       size to clone.
 * \return               \c TE_SUCCESS on success, others failed.
 */
int te_memlist_clone( te_memlist_t *dst,
                      te_memlist_t *src,
                      size_t offset,
                      size_t len );

/**
 * \brief              This function duplicates a slice from \p src list to
 *                     \p dst list.
 *                     Note: Caller should make sure no overflow of \p src.
 * \param[out] dst     The dst list to hold the slice.
 * \param[in]  src     The src list to duplicate from.
 * \param[in]  offset  offset of \p src from head.
 * \param[in]  len     size of \p src to clone.
 * \return             \c TE_SUCCESS on success, others failed.
 */
int te_memlist_dup( te_memlist_t *dst,
                    const te_memlist_t *src,
                    size_t offset,
                    size_t len );

/**
 * \brief                This function applys a XOR op on a memory list and a
 *                       buffer and store the result into another memory list.
 *                       Note: Caller should make sure no overflow of \p src
 *                             or \p dst.
 * \param[out] dst       The dst list to hold the result.
 * \param[in]  in        The in list for XOR operation.
 * \param[in]  ks        The in buffer for XOR operation.
 * \param[in]  len       size of \p ks.
 * \return               \c TE_SUCCESS on success, others failed.
 */
int te_memlist_xor( te_memlist_t *dst,
                    const te_memlist_t *in,
                    const uint8_t *ks, size_t len );

/**
 * \brief                This function fills data \p src into a memory list
 *                       \p dst with offset specified by \p ofs.
 *                       Note: Caller should make sure no overflow of \p dst.
 * \param[out] dst       The dst list for filling the data \p src.
 * \param[in]  ofs       The offset of \p dst to start with.
 * \param[in]  src       The in buffer to fill the \p dst.
 * \param[in]  len       size of \p src.
 * \return               \c TE_SUCCESS on success, others failed.
 */
int te_memlist_fill( te_memlist_t *dst,
                     size_t ofs,
                     const uint8_t *src,
                     size_t len );

/**
 * \brief                This function copies data from a memory list \p src
 *                       to a buffer \p dst with spedifed offset \p ofs and
 *                       len \p len.
 *                       Note: Caller should make sure no overflow of \p src.
 * \param[out] dst       The dst buffer to copy to.
 * \param[in]  src       The source memory list to copy from.
 * \param[in]  ofs       The offset of \p src to start with.
 * \param[in]  len       size of data \p src to copy.
 * \return               \c TE_SUCCESS on success, others failed.
 */
int te_memlist_copy( uint8_t *dst,
                     const te_memlist_t *src,
                     size_t ofs,
                     size_t len );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_MEMLIST_H__ */
