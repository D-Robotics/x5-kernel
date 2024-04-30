/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_platform.h
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#ifndef IPC_PLATFORM_H
#define IPC_PLATFORM_H

#define MAX_MBOX_IDX			(2)/**< max index of mailbox channel*/
#define MAX_NUM_INSTANCE		(3)/**< max index of ipc instance*/
#define MAX_NUM_CHAN_PER_INSTANCE	(4)/**< max number of channel per instance*/
#define MAX_NUM_POOL_PER_CHAN		(4)/**< max number of pool per channel*/
#define MAX_NUM_BUF_PER_POOL		(64u)/**< max number of buffer per pool*/
#define MAX_TIMEOUT			(30000)/**< max value of timeout*/

#endif /* IPC_PLATFORM_H */
