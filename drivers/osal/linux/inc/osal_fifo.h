/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_FIFO_H__
#define __OSAL_FIFO_H__

#include <linux/kfifo.h>
#include <osal_spinlock.h>

typedef struct kfifo osal_fifo_t;

/**
 * @brief initialize a fifo instance with pre-allocated buffer
 *
 * @param[in]   fifo: fifo instance to initialize
 * @param[in] buffer: buffer used to store fifo data
 * @param[in]   size: buffer size to use, will be down-aligned to 2^n size
 *
 * @retval =0: success
 * @retval <0: failure
 */
static inline int32_t osal_fifo_init(osal_fifo_t *fifo, void *buffer, uint32_t size)
{
	return kfifo_init((struct kfifo *)fifo, buffer, size);
}

/**
 * @brief initialize a fifo instance with new allocated memory
 *
 * @param[in]   fifo: fifo instance to initialize
 * @param[in]   size: size to allocated, will be up-aligned to 2^n size
 * @param[in]  flags: memory allocation flags
 *
 * @retval =0: success
 * @retval <0: failure
 */
static inline int32_t osal_fifo_alloc(osal_fifo_t *fifo, uint32_t size, uint32_t flags)
{
	if (flags & OSAL_KMALLOC_KERNEL)
		return kfifo_alloc((struct kfifo *)fifo, size, GFP_KERNEL);

	if (flags & OSAL_KMALLOC_ATOMIC)
		return kfifo_alloc((struct kfifo *)fifo, size, GFP_ATOMIC);

	return kfifo_alloc((struct kfifo *)fifo, size, GFP_KERNEL);
}

/**
 * @brief free memory of fifo
 *
 * @param[in]  fifo: fifo instance to operate
 */
static inline void osal_fifo_free(osal_fifo_t *fifo)
{
	kfifo_free((struct kfifo *)fifo);
}

/**
 * @brief add memory to fifo
 *
 * @param[in]   fifo: fifo instance to operate
 * @param[in]    buf: memory pointer to add to fifo
 * @param[in]    len: length of memory to add
 *
 * @retval [0-len] sizeof memory added
 */
static inline uint32_t osal_fifo_in(osal_fifo_t *fifo, void *buf, uint32_t len)
{
	return kfifo_in((struct kfifo *)fifo, buf, len);
}

/**
 * @brief add memory to fifo
 *
 * @param[in]   fifo: fifo instance to operate
 * @param[in]    buf: memory pointer to add to fifo
 * @param[in]    len: length of memory to add
 * @param[in]  slock: spinlock pointer to use when operating the fifo
 *
 * @retval [0-len] sizeof memory added
 */
static inline uint32_t osal_fifo_in_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t len, osal_spinlock_t *slock)
{
	return kfifo_in_spinlocked((struct kfifo *)fifo, buf, len, (spinlock_t *)slock);
}

/**
 * @brief remove memory from fifo
 *
 * @param[in]   fifo: fifo instance to operate
 * @param[in]    buf: memory pointer to remove from fifo
 * @param[in]    len: length of memory to remove
 *
 * @retval [0-len] sizeof memory removed
 */
static inline uint32_t osal_fifo_out(osal_fifo_t *fifo, void *buf, uint32_t len)
{
	return kfifo_out((struct kfifo *)fifo, buf, len);
}

/**
 * @brief remove memory from fifo
 *
 * @param[in]   fifo: fifo instance to operate
 * @param[in]    buf: memory pointer to remove from fifo
 * @param[in]    len: length of memory to remove
 * @param[in]  slock: spinlock pointer to use when operating the fifo
 *
 * @retval [0-len] sizeof memory removed
 */
static inline uint32_t osal_fifo_out_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t len, osal_spinlock_t *slock)
{
	return kfifo_out_spinlocked((struct kfifo *)fifo, buf, len, (spinlock_t *)slock);
}

/**
 * @brief reset the fifo instance
 *
 * @param[in]   fifo: fifo instance to operate
 */
static inline void osal_fifo_reset(osal_fifo_t *fifo)
{
	kfifo_reset((struct kfifo *)fifo);
}

/**
 * @brief check if the fifo is initialized
 *
 * @param[in]   fifo: fifo instance to operate
 *
 * @return true of initialized or false if not intializaed
 */
static inline bool osal_fifo_initialized(osal_fifo_t *fifo)
{
	return !!kfifo_initialized((struct kfifo *)fifo);
}

/**
 * @brief check if the fifo is full
 *
 * @param[in]   fifo: fifo instance to operate
 *
 * @return true if full or false if not full
 */
static inline bool osal_fifo_is_full(osal_fifo_t *fifo)
{
	return kfifo_is_full((struct kfifo *)fifo);
}

/**
 * @brief check if the fifo is empty
 *
 * @param[in]   fifo: fifo instance to operate
 *
 * @return true of empty or false if not empty
 */
static inline bool osal_fifo_is_empty(osal_fifo_t *fifo)
{
	return kfifo_is_empty((struct kfifo *)fifo);
}

/**
 * @brief check if the fifo is empty with spinlock
 *
 * @param[in]   fifo: fifo instance to operate
 * @param[in]  slock: spinlock pointer to use when operating the fifo
 *
 * @return true of empty or false if not empty
 */
static inline bool osal_fifo_is_empty_spinlocked(osal_fifo_t *fifo, osal_spinlock_t *slock)
{
	return kfifo_is_empty_spinlocked((struct kfifo *)fifo, (spinlock_t *)slock);
}

/**
 * @brief get the full size of the fifo
 *
 * @param[in]   fifo: fifo instance to operate
 *
 * @return full size of the fifo
 */
static inline uint32_t osal_fifo_size(osal_fifo_t *fifo)
{
	return kfifo_size((struct kfifo *)fifo);
}

/**
 * @brief get the data length in the fifo
 *
 * @param[in]   fifo: fifo instance to operate
 *
 * @return data length in the fifo
 */
static inline uint32_t osal_fifo_len(osal_fifo_t *fifo)
{
	return kfifo_len((struct kfifo *)fifo);
}

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
	return kfifo_from_user((struct kfifo *)fifo, from, len, copied);
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
	return kfifo_to_user((struct kfifo *)fifo, to, len, copied);
}

#endif

