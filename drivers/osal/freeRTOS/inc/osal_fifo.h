/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_FIFO_H__
#define __OSAL_FIFO_H__

#include "osal_common_fifo.h"

extern int32_t osal_fifo_init(osal_fifo_t *fifo, void *buffer, uint32_t size);
extern int32_t osal_fifo_alloc(osal_fifo_t *fifo, uint32_t size);
extern void osal_fifo_free(osal_fifo_t *fifo);
extern uint32_t osal_fifo_in(osal_fifo_t *fifo, void *buf, uint32_t len);
extern uint32_t osal_fifo_in_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t len, osal_spinlock_t *slock);
extern uint32_t osal_fifo_out(osal_fifo_t *fifo, void *buf, uint32_t len);
extern uint32_t osal_fifo_out_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t len, osal_spinlock_t *slock);
extern void osal_fifo_reset(osal_fifo_t *fifo);
extern bool osal_fifo_initialized(osal_fifo_t *fifo);
extern bool osal_fifo_is_full(osal_fifo_t *fifo);
extern uint32_t osal_fifo_len(osal_fifo_t *fifo);
extern bool osal_fifo_is_empty(osal_fifo_t *fifo);
extern bool osal_fifo_is_empty_spinlocked(osal_fifo_t *fifo, osal_spinlock_t *slock);

/**
 * @brief copy data from user memory to fifo
 *
 * @param[in]    fifo: the fifo to operate
 * @param[in]    from: the user memory pointer
 * @param[in]     len: the length of data to copy
 * @param[out] copied: data length copied
 *
 * @retval =0: success
 * @retval <0: failure
 */
static inline int32_t osal_fifo_copy_from_user(osal_fifo_t *fifo, const void __user *from, uint32_t len, uint32_t *copied)
{
    return osal_cmn_fifo_copy_from_user(fifo, from, len, copied);
}

/**
 * @brief copy data to user memory from fifo
 *
 * @param[in]    fifo: the fifo to operate
 * @param[in]    from: the user memory pointer
 * @param[in]     len: the length of data to copy
 * @param[out] copied: data length copied
 *
 * @retval =0: success
 * @retval <0: failure
 */
static inline int32_t osal_fifo_copy_to_user(osal_fifo_t *fifo, void __user *to, uint32_t len, uint32_t *copied)
{
    return osal_cmn_fifo_copy_to_user(fifo, to, len, copied);
}

#endif
