/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/
#ifndef IPC_OS_H
#define IPC_OS_H

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/io.h>

#include "hb_ipc_interface.h"
#include "ipc_platform.h"
#include "ipc_shm.h"
#include "ipc_queue.h"

#define DRIVER_NAME			"ipc-shm-dev"

/* softirq work budget used to prevent CPU starvation */
#define IPC_SOFTIRQ_BUDGET		128

/* the data segment content required for IPC. */
#define IPC_DATA_SIZE                   (0x00800000)

#define IPC_SHM_INSTANCE_DISABLED	0
#define IPC_SHM_INSTANCE_ENABLED	1

/* convenience wrappers for printing errors and debug messages */
#define ipc_fmt(fmt)			"ipc-drv: %s() [%d]: "fmt
#define ipc_err(fmt, ...)		pr_err(ipc_fmt(fmt), __func__, __LINE__, ##__VA_ARGS__)
#define ipc_dbg(fmt, ...)		pr_debug(ipc_fmt(fmt), __func__, __LINE__, ##__VA_ARGS__)

#ifndef BIT
#define BIT(n)				(1u << n)
#endif /*BIT*/

/* forward declarations */
struct ipc_shm_cfg;
struct ipc_instance_cfg;

/* function declarations */
int32_t ipc_os_init(int32_t instance, const struct ipc_shm_cfg *cfg,
		int32_t (*rx_cb)(int32_t, int32_t));
void ipc_os_free(int32_t instance);
uint64_t ipc_os_get_local_shm(int32_t instance);
uint64_t ipc_os_get_remote_shm(int32_t instance);
int32_t ipc_os_poll_channels(int32_t instance);
struct ipc_instance_cfg *ipc_os_get_def_info(int32_t instance, const struct ipc_instance_cfg *cfg);
int32_t ipc_os_mbox_open(int32_t instance);
int32_t ipc_os_mbox_close(int32_t instance);
int32_t ipc_os_mbox_notify(int32_t instance, int32_t chan_id);

#endif /* IPC_OS_H */
