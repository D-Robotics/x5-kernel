/*
 * Copyright 2022, Horizon Robotics
 */

#include "osal_fifo.h"

int32_t osal_fifo_init(osal_fifo_t *fifo, void *buffer, uint32_t size)
{
	return osal_cmn_fifo_init(fifo, buffer, size);
}


int32_t osal_fifo_alloc(osal_fifo_t *fifo, uint32_t size)
{
	return osal_cmn_fifo_alloc(fifo, size, 0);
}

void osal_fifo_free(osal_fifo_t *fifo)
{
	osal_cmn_fifo_free(fifo);
}

uint32_t osal_fifo_in(osal_fifo_t *fifo, void *buf, uint32_t len)
{
	return osal_cmn_fifo_in(fifo, buf, len);
}

uint32_t osal_fifo_in_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t len, osal_spinlock_t *slock)
{
	return osal_cmn_fifo_in_spinlocked(fifo, buf, len, slock);
}

uint32_t osal_fifo_out(osal_fifo_t *fifo, void *buf, uint32_t len)
{
	return osal_cmn_fifo_out(fifo, buf, len);
}

uint32_t osal_fifo_out_spinlocked(osal_fifo_t *fifo, void *buf, uint32_t len, osal_spinlock_t *slock)
{
	return osal_cmn_fifo_out_spinlocked(fifo, buf, len, slock);
}

void osal_fifo_reset(osal_fifo_t *fifo)
{
	osal_cmn_fifo_reset(fifo);
}

bool osal_fifo_initialized(osal_fifo_t *fifo)
{
	return osal_cmn_fifo_initialized(fifo);
}

bool osal_fifo_is_full(osal_fifo_t *fifo)
{
	return osal_cmn_fifo_is_full(fifo);
}

uint32_t osal_fifo_len(osal_fifo_t *fifo)
{
	return osal_cmn_fifo_len(fifo);
}

bool osal_fifo_is_empty(osal_fifo_t *fifo)
{
	return osal_cmn_fifo_is_empty(fifo);
}

bool osal_fifo_is_empty_spinlocked(osal_fifo_t *fifo, osal_spinlock_t *slock)
{
	return osal_fifo_is_empty_spinlocked(fifo, slock);
}