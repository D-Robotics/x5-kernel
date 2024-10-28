/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright (C) 2020-2023, Horizon Robotics Co., Ltd.
 *                    All rights reserved.
 *************************************************************************/
/**
 * @file hobot_dsp_ioctl.h
 *
 * @NO{S05E05C01}
 * @ASIL{B}
 */

#ifndef VDSP_IOCTL_H
#define VDSP_IOCTL_H

#define VDSP_DEVICE_NAME				"/dev/dsp"

#define VDSP_PATH_BUF_SIZE				(256)
#define VDSP_PATHNAME_BUF_SIZE				(512)

#define VDSP_IOCTL_MAGIC				(68U)
#define VDSP_IOCTL_START_VDSP				_IOWR(VDSP_IOCTL_MAGIC, 1U, struct dsp_info_ctl)
#define VDSP_IOCTL_STOP_VDSP				_IOWR(VDSP_IOCTL_MAGIC, 2U, struct dsp_info_ctl)
#define VDSP_IOCTL_GET_VDSP_STATUS			_IOWR(VDSP_IOCTL_MAGIC, 3U, struct dsp_info_ctl)
#define VDSP_IOCTL_RESET_VDSP				_IOWR(VDSP_IOCTL_MAGIC, 4U, struct dsp_info_ctl)
#define VDSP_IOCTL_SET_FIRMWARE_PATH			_IOWR(VDSP_IOCTL_MAGIC, 5U, struct dsp_info_ctl)
#define VDSP_IOCTL_SET_FIRMWARE_NAME			_IOWR(VDSP_IOCTL_MAGIC, 6U, struct dsp_info_ctl)
#define VDSP_IOCTL_MEM_ALLOC				_IOWR(VDSP_IOCTL_MAGIC, 7U, struct dsp_info_smmu_data)
#define VDSP_IOCTL_MEM_FREE				_IOWR(VDSP_IOCTL_MAGIC, 8U, struct dsp_info_smmu_data)
#define VDSP_IOCTL_MMU_MAP				_IOWR(VDSP_IOCTL_MAGIC, 9U, struct dsp_info_smmu_data)
#define VDSP_IOCTL_MMU_UNMAP				_IOWR(VDSP_IOCTL_MAGIC, 10U, struct dsp_info_smmu_data)
#define VDSP_IOCTL_GET_VERSION_INFO			_IOWR(VDSP_IOCTL_MAGIC, 11U, struct dsp_info_version_data)
#ifdef CONFIG_HOBOT_VDSP_STL
#define VDSP_IOCTL_STL_RESET_VDSP			_IOWR(VDSP_IOCTL_MAGIC, 12U, struct dsp_stl_info_ctl)
#endif

/**
 * @struct dsp_info_version_data
 * Define the descriptor of version info data.
 * @NO{S05E06C01I}
 */
struct dsp_info_version_data {
	uint32_t major;			/* the major version number*/
	uint32_t minor;			/* the minor version number.*/
};

/**
 * @struct dsp_info_ctl
 * Define the dsp info struct.
 * @NO{S05E06C01I}
 */
struct dsp_info_ctl {
	int32_t dsp_id;			/* dsp id */
	int32_t timeout;		/* boot timeout */
	uint32_t status;		/* dsp run status */
	char *pathname;			/* dsp firmware path & name */
	int32_t pathnamelen;		/* dsp firmware path & name length */
};

/**
 * @struct dsp_info_smmu_data
 * Define the dsp smmu info struct.
 * @NO{S05E06C01I}
 */
struct dsp_info_smmu_data {
	int32_t dsp_id;			/* dsp id */
	int32_t smmufd;			/* hbmem buf file descriptor */
	uint64_t va;			/* hbmem buffer virtual address */
	uint64_t size;			/* hbmem buffer size */
	uint64_t smmuiova;			/* smmu map buffer virtual address */
};

#ifdef CONFIG_HOBOT_VDSP_STL
struct dsp_stl_info_ctl {
	int32_t dsp_id;				/* dsp id */
	uint32_t reset_cnt;			/* dsp reset count */
	uint32_t reset_reason;			/* dsp reset reason */
};
#endif

#endif /*VDSP_IOCTL_H*/
