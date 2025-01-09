/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * Zhang Guoying <guoying.zhang@horizon.ai>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#ifndef _UAPI_HOBOT_BPU_H
#define _UAPI_HOBOT_BPU_H /* PRQA S 0603 */

#include <linux/ioctl.h>
#include <linux/types.h>
#include <stdint.h>

/* The magic IOCTL value for this interface. */
#define BPU_IOC_MAGIC 'B'

/**
 * struct user_bpu_fc - userspace bpu fc task header and status
 * @id: communicate id with user
 * @slice_num: contained slice fc number
 * @length: band data length if the struct is data head
 * @core_mask: user set fc can run cores bit
 * @run_c_mask: real fc run cores bit, 0 is anyone
 * @g_id: bpu fc task bind group id
 * @priority: bpu fc task priority level
 * @status: the follow element read after fc process done
 * 			0: process succeed, not 0: error code
 * @process_time: maybe the real time bpu core process
 *
 * when userspace set bpu fc task to kernel, struct user_bpu_fc
 * is the header of the bpu task, following raw fc data.
 * In this scenario:
 * run_c_mask can use specify the running bpu core
 * process_time use to set the the estimate elapsed time
 * of the task
 *
 * when userspace get the bpu fc task process result, struct
 * user_bpu_fc is the status of the bpu task.
 * In this scenario:
 * run_c_mask show the real run bpu core bit;
 * status show the task process status;
 * process_time show the driver duration time of the bpu task
 */
struct user_bpu_fc {
	uint32_t id;

	uint32_t slice_num;

	uint32_t length;

	uint64_t core_mask;

	uint64_t run_c_mask;

	uint32_t g_id;

	uint32_t priority;

	int32_t status;

	uint64_t process_time;
};

/**
 * struct bpu_group - group ioctl data format
 * @group_id: group id to for distinguish groups
 * @prop: group run proportion
 * @ratio: the running ratio of the group
 *
 * ratio valid when use get ioctl
 */
struct bpu_group {
	uint32_t group_id;
	uint32_t prop;
	uint32_t ratio;
};

/**
 * struct bpu_iommu_map - iommu map/unmap ioctl data format
 * @raw_addr: raw ddr address
 * @map_addr: mapped iommu address
 * @size: map/unmap address size
 *
 * when map, set raw address and return map address
 * when unmap, set map address
 */
struct bpu_iommu_map {
	uint64_t raw_addr;
	uint64_t map_addr;
	uint64_t size;
};

/*
 * For user to get the special fc processed time by bpu hw
 * the time is alculated by the software to be close to
 * the true processed time (Lack of accuracy)
 */
struct bpu_fc_run_time {
	/* the core mask checked by the user */
	uint64_t core_mask;
	/*
	 * the of of the fc, which same with struct user_bpu_fc id
	 * if the id == 0, use the recent core mask fc processed time
	 * to user
	 */
	uint32_t id;
	/*
	 * the processed time(us) by bpu hardware, if hasn't been
	 * processed yet, of has been processed done, return 0.
	 */
	uint32_t run_time;
};

#define BPU_SET_GROUP _IOW(BPU_IOC_MAGIC, 1, struct bpu_group) /* set group information */
#define BPU_GET_RATIO _IOWR(BPU_IOC_MAGIC, 2, uint32_t) /* get bpu(core) ratio */
#define BPU_GET_CAP _IOWR(BPU_IOC_MAGIC, 3, uint16_t) /* get bpu(core) capacity */
#define BPU_RESET _IOW(BPU_IOC_MAGIC, 4, uint16_t) /* reset bpu(core) */
#define BPU_GET_GROUP _IOWR(BPU_IOC_MAGIC, 5, struct bpu_group) /* get group information */
#define BPU_OPT_CORE _IOWR(BPU_IOC_MAGIC, 6, uint64_t) /* get optimal bpu core */
#define BPU_SET_POWER _IOW(BPU_IOC_MAGIC, 7, int16_t) /* set bpu core power level */
#define BPU_SET_FREQ_LEVEL _IOW(BPU_IOC_MAGIC, 9, int16_t) /* set bpu core frequency level */
#define BPU_GET_FREQ_LEVEL _IOR(BPU_IOC_MAGIC, 10, int16_t) /* get bpu core frequency level */
#define BPU_GET_FREQ_LEVEL_NUM _IOR(BPU_IOC_MAGIC, 11, int16_t) /* get all bpu core frequency level number */
#define BPU_SET_LIMIT _IOW(BPU_IOC_MAGIC, 12, uint32_t) /* set bpu(core) software priority buffer limit */
#define BPU_SET_CLK _IOW(BPU_IOC_MAGIC, 13, uint64_t) /* set bpu core clock on/off/rate */
#define BPU_GET_CLK _IOR(BPU_IOC_MAGIC, 14, uint64_t) /* get bpu core clock rate */
#define BPU_CORE_TYPE _IOR(BPU_IOC_MAGIC, 15, uint8_t) /* get bpu core support type */
#define BPU_IOMMU_MAP _IOW(BPU_IOC_MAGIC, 16, struct bpu_iommu_map) /* bpu iommu map address */
#define BPU_IOMMU_UNMAP _IOW(BPU_IOC_MAGIC, 17, struct bpu_iommu_map) /* bpu iommu unmap address */
#define BPU_FC_RUN_TIME _IOR(BPU_IOC_MAGIC, 18, struct bpu_fc_run_time)
#define BPU_EST_TIME _IOWR(BPU_IOC_MAGIC, 19, uint64_t)

#endif
