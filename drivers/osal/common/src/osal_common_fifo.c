/*
 * Copyright 2022, Horizon Robotics
 */

#include "osal_common_fifo.h"
//#include "osal_kmalloc.h"

static inline __attribute__((const)) bool __osal_is_pow_of_two(uint64_t n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

#define osal_fifo_min(x,y) ((x)<(y)?(x):(y))

static inline uint32_t __osal_roundup_pow_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static inline uint32_t __osal_rounddown_pow_of_two(uint32_t n)
{
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return (n+1) >> 1;
}

/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline uint32_t fifo_unused(osal_fifo_t *fifo)
{
	return (fifo->mask + 1) - (fifo->in - fifo->out);
}

/**
 * @brief initialize a fifo object with new allocated memory
 *
 * @param[in]  fifo: fifo object to initialize
 * @param[in]  size: size to allocated, will be up-aligned to 2^n size
 *
 * @retval =0: success
 * @retval <0: failure
 */
int32_t osal_cmn_fifo_alloc(osal_fifo_t *fifo, uint32_t size, uint32_t flags)
{
	CHECK_OR_RETURN(fifo != NULL, -EINVAL);

	/*
	 * round up to the next power of 2, since our 'let the indices
	 * wrap' technique works only in this case.
	 */
	size = __osal_roundup_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;

	if (size < 2) {
		fifo->data = NULL;
		fifo->mask = 0;
		return -EINVAL;
	}

	fifo->data = osal_kmalloc(size, flags);

	if (!fifo->data) {
		fifo->mask = 0;
		return -ENOMEM;
	}
	fifo->mask = size - 1;

	return 0;
}

/**
 * @brief free memory of fifo
 *
 * @param[in]  fifo: fifo object to operate
 */
void osal_cmn_fifo_free(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, );

	osal_kfree(fifo->data);
	fifo->in = 0;
	fifo->out = 0;
	fifo->data = NULL;
	fifo->mask = 0;
}

/**
 * @brief initialize a fifo object with pre-allocated buffer
 *
 * @param[in]   fifo: fifo object to initialize
 * @param[in] buffer: buffer used to store fifo data
 * @param[in]   size: buffer size to use, will be down-aligned to 2^n size
 *
 * @retval =0: success
 * @retval <0: failure
 */
int32_t osal_cmn_fifo_init(osal_fifo_t *fifo, void *buffer, uint32_t size)
{
	CHECK_OR_RETURN(fifo != NULL, -EINVAL);
	CHECK_OR_RETURN(buffer != NULL, -EINVAL);

	if (!__osal_is_pow_of_two(size))
		size = __osal_rounddown_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->data = buffer;

	if (size < 2) {
		fifo->mask = 0;
		return -EINVAL;
	}
	fifo->mask = size - 1;

	return 0;
}

static void fifo_copy_in(osal_fifo_t *fifo, const void *src,
			uint32_t len, uint32_t off)
{
	uint32_t size = fifo->mask + 1;
	uint32_t l;

	off &= fifo->mask;

	l = osal_fifo_min(len, size - off);

	memcpy(fifo->data + off, src, l);
	memcpy(fifo->data, src + l, len - l);
	/*
	 * make sure that the data in the fifo is up to date before
	 * incrementing the fifo->in index counter
	 */
	smp_wmb();
}


/**
 * @brief add memory to fifo
 *
 * @param[in]   fifo: fifo object to operate
 * @param[in]    buf: memory pointer to add to fifo
 * @param[in]    len: length of memory to add
 *
 * @retval [0-len] sizeof memory added
 */
uint32_t osal_cmn_fifo_in(osal_fifo_t *fifo, void *buf, uint32_t len)
{
	uint32_t l;

	CHECK_OR_RETURN(fifo != NULL, 0);
	CHECK_OR_RETURN(buf != NULL, 0);

	l = fifo_unused(fifo);
	if (len > l)
		len = l;

	fifo_copy_in(fifo, buf, len, fifo->in);
	fifo->in += len;
	return len;
}

static void fifo_copy_out(osal_fifo_t *fifo, void *dst,
		uint32_t len, uint32_t off)
{
	uint32_t size = fifo->mask + 1;
	uint32_t l;

	off &= fifo->mask;

	l = osal_fifo_min(len, size - off);

	memcpy(dst, fifo->data + off, l);
	memcpy(dst + l, fifo->data, len - l);
	/*
	 * make sure that the data is copied before
	 * incrementing the fifo->out index counter
	 */
	smp_wmb();
}

static uint32_t osal_fifo_out_peek(osal_fifo_t *fifo,
		void *buf, uint32_t len)
{
	uint32_t l;

	CHECK_OR_RETURN(fifo != NULL, 0);
	CHECK_OR_RETURN(buf != NULL, 0);

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;

	fifo_copy_out(fifo, buf, len, fifo->out);
	return len;
}

/**
 * @brief remove memory from fifo
 *
 * @param[in]   fifo: fifo object to operate
 * @param[in]    buf: memory pointer to remove from fifo
 * @param[in]    len: length of memory to remove
 *
 * @retval [0-len] sizeof memory removed
 */
uint32_t osal_cmn_fifo_out(osal_fifo_t *fifo, void *buf, uint32_t len)
{
	CHECK_OR_RETURN(fifo != NULL, 0);
	CHECK_OR_RETURN(buf != NULL, 0);

	len = osal_fifo_out_peek(fifo, buf, len);
	fifo->out += len;
	return len;
}

static uint32_t fifo_copy_from_user(osal_fifo_t *fifo,
	const void __user *from, uint32_t len, uint32_t off,
	uint32_t *copied)
{
	uint32_t size = fifo->mask + 1;
	uint32_t l;
	uint32_t ret;

	off &= fifo->mask;

	l = osal_fifo_min(len, size - off);

	ret = copy_from_user(fifo->data + off, from, l);
	if (osal_cmn_unlikely(ret)) {
		ret = ret + len - l;
	} else {
		ret = copy_from_user(fifo->data, from + l, len - l);
	}
	/*
	 * make sure that the data in the fifo is up to date before
	 * incrementing the fifo->in index counter
	 */
	smp_wmb();
	*copied = len - ret;
	/* return the number of elements which are not copied */
	return ret;
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
int32_t osal_cmn_fifo_copy_from_user(osal_fifo_t *fifo, const void __user *from,
		uint32_t len, uint32_t *copied)
{
	uint32_t l;
	uint32_t ret;
	int32_t err;

	CHECK_OR_RETURN(fifo != NULL, -EINVAL);
	CHECK_OR_RETURN(from != NULL, -EINVAL);
	CHECK_OR_RETURN(copied != NULL, -EINVAL);

	l = fifo_unused(fifo);
	if (len > l)
		len = l;

	ret = fifo_copy_from_user(fifo, from, len, fifo->in, copied);
	if (osal_cmn_unlikely(ret)) {
		len -= ret;
		err = -EFAULT;
	} else {
		err = 0;
	}
	fifo->in += len;
	return err;
}

static uint32_t fifo_copy_to_user(osal_fifo_t *fifo, void __user *to,
		uint32_t len, uint32_t off, uint32_t *copied)
{
	uint32_t l;
	uint32_t ret;
	uint32_t size = fifo->mask + 1;

	off &= fifo->mask;

	l = osal_fifo_min(len, size - off);

	ret = copy_to_user(to, fifo->data + off, l);
	if (osal_cmn_unlikely(ret)) {
		ret = ret + len - l;
	} else {
		ret = copy_to_user(to + l, fifo->data, len - l);
	}
	/*
	 * make sure that the data is copied before
	 * incrementing the fifo->out index counter
	 */
	smp_wmb();
	*copied = len - ret;
	/* return the number of elements which are not copied */
	return ret;
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
int32_t osal_cmn_fifo_copy_to_user(osal_fifo_t *fifo, void __user *to,
		uint32_t len, uint32_t *copied)
{
	uint32_t l;
	uint32_t ret;
	int32_t err;

	CHECK_OR_RETURN(fifo != NULL, -EINVAL);
	CHECK_OR_RETURN(to   != NULL, -EINVAL);
	CHECK_OR_RETURN(copied != NULL, -EINVAL);

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;
	ret = fifo_copy_to_user(fifo, to, len, fifo->out, copied);
	if (osal_cmn_unlikely(ret)) {
		len -= ret;
		err = -EFAULT;
	} else {
		err = 0;
	}
	fifo->out += len;

	return err;
}

/**
 * @brief check if the fifo is initialized
 *
 * @param[in]  fifo: fifo object to operate
 *
 * @return true of initialized or false if not intializaed
 */
inline bool osal_cmn_fifo_initialized(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, false);
	return !!(fifo->mask);
}


/**
 * @brief get the full size of the fifo
 *
 * @param[in]  fifo: fifo object to operate
 *
 * @return full size of the fifo
 */
inline uint32_t osal_cmn_fifo_size(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, -EINVAL);
	return (fifo->mask + 1);
}

/**
 * @brief reset the fifo object
 *
 * @param[in]  fifo: fifo object to operate
 */
inline void osal_cmn_fifo_reset(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, );
	fifo->in = fifo->out = 0;
}

/**
 * @brief get the data length in the fifo
 *
 * @param[in]  fifo: fifo object to operate
 *
 * @return data length in the fifo
 */
inline uint32_t osal_cmn_fifo_len(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, -EINVAL);
	return fifo->in - fifo->out;
}

/**
 * @brief check if the fifo is empty
 *
 * @param[in]  fifo: fifo object to operate
 *
 * @return true of empty or false if not empty
 */
inline bool osal_cmn_fifo_is_empty(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, false);
	return fifo->in == fifo->out;
}

/**
 * @brief check if the fifo is empty with spinlock
 *
 * @param[in]   fifo: fifo object to operate
 * @param[in]  slock: spinlock pointer to use when operating the fifo
 *
 * @return true of empty or false if not empty
 */
inline bool osal_cmn_fifo_is_empty_spinlocked(osal_fifo_t *fifo, osal_spinlock_t *lock)
{
	unsigned long flags;
	bool ret;

	CHECK_OR_RETURN(fifo != NULL, false);
	CHECK_OR_RETURN(lock != NULL, false);

	osal_spin_lock_irqsave(lock, &flags);
	ret = osal_cmn_fifo_is_empty(fifo);
	osal_spin_unlock_irqrestore(lock, &flags);

	return ret;
}

/**
 * @brief check if the fifo is full
 *
 * @param[in]  fifo: fifo object to operate
 *
 * @return true of full or false if not full
 */
inline bool osal_cmn_fifo_is_full(osal_fifo_t *fifo)
{
	CHECK_OR_RETURN(fifo != NULL, false);
	return osal_cmn_fifo_len(fifo) > fifo->mask;
}


/**
 * @brief add memory to fifo
 *
 * @param[in]  fifo: fifo object to operate
 * @param[in]    buf: memory pointer to add to fifo
 * @param[in]    len: length of memory to add
 * @param[in]  slock: spinlock pointer to use when operating the fifo
 *
 * @retval [0-len] sizeof memory added
 */
inline uint32_t osal_cmn_fifo_in_spinlocked(osal_fifo_t *fifo, void *buf,
		uint32_t n, osal_spinlock_t *lock)
{
	unsigned long flags;
	uint32_t ret;

	CHECK_OR_RETURN(fifo != NULL, false);
	CHECK_OR_RETURN(lock != NULL, false);
	CHECK_OR_RETURN(buf != NULL, false);

	osal_spin_lock_irqsave(lock, &flags);
	ret = osal_cmn_fifo_in(fifo, buf, n);
	osal_spin_unlock_irqrestore(lock, &flags);

	return ret;
}


/**
 * @brief remove memory from fifo
 *
 * @param[in]  fifo: fifo object to operate
 * @param[in]    buf: memory pointer to remove from fifo
 * @param[in]    len: length of memory to remove
 * @param[in]  slock: spinlock pointer to use when operating the fifo
 *
 * @retval [0-len] sizeof memory removed
 */
inline uint32_t osal_cmn_fifo_out_spinlocked(osal_fifo_t *fifo, void *buf,
		uint32_t n, osal_spinlock_t *lock)
{
	unsigned long flags;
	uint32_t ret;

	CHECK_OR_RETURN(fifo != NULL, false);
	CHECK_OR_RETURN(lock != NULL, false);
	CHECK_OR_RETURN(buf != NULL, false);

	osal_spin_lock_irqsave(lock, &flags);
	ret = osal_cmn_fifo_out(fifo, buf, n);
	osal_spin_unlock_irqrestore(lock, &flags);

	return ret;
}