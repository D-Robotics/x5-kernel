/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_hal.h
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#ifndef IPC_HAL_H
#define IPC_HAL_H
#include <linux/types.h>
#include <linux/kfifo.h>

#include "hb_ipc_interface.h"

#define	DEVICE_MAJOR			188
#define	DEVICE_NAME			"ipc-shm-hal"
#define	IPCF_IOC_MAGIC			'i'

#define	IPCF_INSTANCES_NUM		(1)		/* number of instacne */
#define	IPCF_INSTANCES_ID0		(2)		/* instacne id  default 0*/

#define	HAL_IPC_SEND			(0)
#define	HAL_IPC_READ			(1)

#define	IPCF_HAL_CHANNEL_NUM_MIN	(0)		/* min channel */
#define	IPCF_HAL_CHANNEL_NUM_MAX	(2)		/* max channel num */
#define	IPCF_HAL_POOL_NUM		(1)		/* number of buffer pools */
#define	IPCF_HAL_BUF_NUM		(16)		/* number of buffer in an channel src 5*/
#define	IPCF_HAL_BUF_SIZE		(0x100)		/* per buffer size, src 4096*/
#define	IPCF_HAL_LOCAL_ADDR		(0xd3001000)	/* local shm address*/
#define	IPCF_HAL_REMOTE_ADDR	(0xd3004000)	/* remote shm address*/
#define	IPCF_HAL_SHM_SIZE		(0x3000)	/* shm size,default(0x100000)*/
#define	IPCF_HAL_MBOX_IDX		(0)
#define	IPCF_HAL_TIMEOUT		(10000)

#define	LEN_FIFO_SIZE			(4 << 10)	/* Len FIFO size*/
#define	DATA_FIFO_SIZE			(64 << 10)	/* Data FIFO size*/

#define DEV_TIMESTAMP_INVALID		(0)
#define DEV_TIMESTAMP_IRQ		BIT(0)
#define DEV_TIMESTAMP_WRITE		BIT(1)

#define DEV_DUMP_DISABLE		(-1)
#define DEV_DUMP_BUFSIZE		(128u)
#define DEV_TEMP_BUFSIZE		(256u)
#define DEV_README_BUFSIZE		(2048u)

/* convenience wrappers for printing errors and debug messages */
#define shm_fmt(fmt) DEVICE_NAME": %s(): "fmt
#define shm_err(fmt, ...) pr_err(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_dbg(fmt, ...) pr_debug(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_info(fmt, ...) pr_info(shm_fmt(fmt), __func__, ##__VA_ARGS__)
#define shm_log_ratelimited(level, fmt, ...) \
		printk_ratelimited(level shm_fmt(fmt), __func__, ##__VA_ARGS__)

struct ipc_statistic_t {
	uint32_t acq_cnt;
	uint32_t shm_tx_cnt;
	uint32_t cb_cnt;
	uint32_t err_acq;
	uint32_t err_shm_tx;
	uint32_t err_cb;

	uint32_t packages;
	uint64_t datalen;
};

/**
 * @struct ipcf_ver_check_info
 * Define the descriptor of IPCF version check
 * @NO{S17E09C06}
 */
struct ipcf_ver_check_info {
	uint32_t major;/**< the major version number*/
	uint32_t minor;/**< the minor version number*/
};

/**
 * kfifo_out_throw - throw away data starting from the fifo out
 * @fifo: address of the fifo to be used
 * @n: max. number of elements to throw away
 *
 * This macro throw away data starting from the fifo out
 *
 * Note that with only one concurrent reader and one concurrent
 * writer, you don't need extra locking to use these macro.
 */
#define	kfifo_out_throw(fifo, n)			\
({ \
	typeof((fifo) + 1) __tmp = (fifo);		\
	unsigned long __n = (n);			\
	struct __kfifo *__kfifo = &__tmp->kfifo;	\
	unsigned int l = __kfifo->in - __kfifo->out;	\
	if (__n > l)					\
		__n = l;				\
	__kfifo->out += __n;				\
})
#endif
