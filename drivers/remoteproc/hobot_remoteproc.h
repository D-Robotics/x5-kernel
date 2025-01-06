/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2020-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/
/**
 * @file hobot_remoteproc.h
 *
 * @NO{S05E05C01}
 * @ASIL{B}
 */
#ifndef HOBOT_DSP_REMOTEPROC_H
#define HOBOT_DSP_REMOTEPROC_H

#include <linux/reset.h>
#include <linux/clk.h>
#include "hb_ipc_interface.h"
#include "ipc_wrapper.h"

#define HIFI5_DEV_IDX  (0)

#define HIFI5_NONSEC_REG_BASE (0x32130000)
#define HIFI5_NONSEC_REG_SIZE (0x1018)

#define HIFI5_SEC_REG_BASE (0x32138000)
#define HIFI5_SEC_REG_SIZE (0x80)

#define HIFI5_RUNSTALL_OFFSET (0x30)

#define AON_DDR_BASE (0x20300000)
#define AON_SRAM0_BASE (0x1fe80000)
#define AON_SRAM1_BASE (0x1ff00000)
#define HIFI5_IRAM_BASE (0x20200000)
#define HIFI5_DRAM0_BASE (0x20210000)
#define HIFI5_DRAM1_BASE (0x20220000)

#define HIFI5_VECTOR_BASE HIFI5_IRAM_BASE

#define HIFI5_CRM_BASE (0x32140000)
#define HIFI5_CRM_SIZE (0x18)

#define AON_PMU_BASE (0x31030000)
#define AON_PMU_SIZE (0x3100)

#define HIFI5_LITEMMU_EN (0x10)

enum hifi5_runstall {
    HIFI5_START_RUN = 0,
    HIFI5_STOP_RUN = 1,
};

#define HOBOT_RESOURCE_IS_IOMEM BIT(0)

//
#define ACORE_VIC_OFFSET           (0x204)
#define m0_to_a_int_status_bit     (8)
#define hifi5_to_a_int_status_bit  (16)
#define hifi51_to_a_int_status_bit  (24)

#define HIFI5_VIC_OFFSET           (0x224)
#define a_to_hifi5_int_status      (8)
#define a_to_hifi5_int_en_bit      (9)


#define IPI_REASON_LOG (0x10u)
#define IPI_REASON_DSP0_WDG (0x20u)

#define DCORE_IPC_INT_REG_RANGE   (0x300)

#define MAX_DSP_FREQ (786432000)


#define RSC_LITE_DEVMEM	128
#define RSC_RPROC_MEM	129
#define RSC_FW_CHKSUM	130
#define RSC_VERSION		131

//acore timesync notify
#define TIMESYNC_ADSP_NOTIFY_ADDR	(0xd3ffff0cu)
#define TIMESYNC_ADSP_NOTIFY_OFF	(0x7fff0cu)

#define WWDT_ENABLE 	(0x1u)
#define  WWDT_DISABLE	(0x0u)

#define MAX_R_INDEX_NUM 16u

enum hb_wakeup_reason {
	HOBOT_AUDIO_WAKEUP = 0,
	HOBOT_GPIO_WAKEUP,
};

enum hb_sleep_mode {
	HOBOT_LITE_SLEEP = 0,
	HOBOT_DEEP_SLEEP,
};

enum hb_audio_wakeup_status {
	HOBOT_AUDIO_WAKEUP_SUCCEED = 0,
	HOBOT_AUDIO_WAKEUP_FAILED,
};

enum hb_ssf_pcm_cmd {
	HOBOT_SSF_START = 0,
	HOBOT_SSF_STOP,
};

struct debug_statistics {
	int irq_handler_count;
};

struct hobot_rproc_pdata;

struct rproc_ipc_ops {
	int (*interrupt_judge)(struct hobot_rproc_pdata *pdata);
	int (*trigger_interrupt)(struct hobot_rproc_pdata *pdata);
	int (*clear_interrupt)(struct hobot_rproc_pdata *pdata);
	int (*start_remoteproc)(struct hobot_rproc_pdata *pdata);
	int (*release_remoteproc)(struct hobot_rproc_pdata *pdata);
	int (*pre_load_remoteproc)(struct hobot_rproc_pdata *pdata);
	int (*pre_stop_remoteproc)(struct hobot_rproc_pdata *pdata);
};

struct log_readindex_addr_pid {
        int32_t pid;
        uint32_t read_index;
};

struct hobot_rsc_table {
	uint32_t da;
	uint32_t pa;
};

#define VERSION_LEN (48)
#define COMPILE_TIME_LEN (16)
#define GIT_HASH_ID_LEN (40)

struct hobot_rproc_version {
	char version[VERSION_LEN];
	char compile_time[COMPILE_TIME_LEN];
	char git_hash_id[GIT_HASH_ID_LEN];
};

struct fw_rsc_version {
	u8 version[VERSION_LEN];
	u8 compile_time[COMPILE_TIME_LEN];
	u8 git_hash_id[GIT_HASH_ID_LEN];
} __packed;

/**
 * struct hobot_rproc_pdata - hobot remote processor instance state
 * @rproc: rproc handle
 * @fw_ops: local firmware operations
 * @default_fw_ops: default rproc firmware operations
 * @mem_pools: list of gen_pool for firmware mmio_sram memory and their
 *             power domain IDs
 * @mems: list of rproc_mem_entries for firmware
 * @vring0: IRQ number used for vring0, just trigger rvq interrupt
 * @ipi_dest_mask: IPI destination mask for the IPI channel
 * @device_index: identify mcore & dcore remoteproc instance
 * @ipc_int_pa: ipc interrupt register base physical address
 * @ipc_int_va: ipc interrupt register base virtual address
 * @work_queue: interrupt bottom half singlethread workqueue
 * @work: interrupt bottom half work
 * @notify_complete: synchronization method between interrupt top & bottom half
 * @should_stop: work stop identification
 * @statistics: debug statistics
 * @top_entry: proc filesystem top entry
 * @info_entry: proc filesystem info entry
 * @statistics_entry: proc filesystem statistics entry
 * @fix_map_mode: whether use fix map mode
 * @ipc_ops: ipc memory operations
 */
struct hobot_rproc_pdata {
	struct rproc *rproc;
	struct device *dev;
//	struct rproc_fw_ops fw_ops;
//	const struct rproc_fw_ops *default_fw_ops;
	int vring0;
	int device_index;
	u32 ipc_int_pa;
	void __iomem *ipc_int_va;
	struct workqueue_struct *work_queue;
	struct workqueue_struct *work_coredumpqueue;
	struct work_struct work;
	struct work_struct coredump_work;
	struct completion notify_complete;
	int should_stop;
	struct debug_statistics statistics;
	struct proc_dir_entry *top_entry;
	struct proc_dir_entry *info_entry;
	struct proc_dir_entry *statistics_entry;
	struct rproc_ipc_ops *ipc_ops;

	//timesync
	void __iomem *timesync_sec_reg_va;
	void __iomem *timesync_sec_diff_reg_va;
	void __iomem *timesync_nanosec_reg_va;

	//log
	spinlock_t r_index_lock;
	void __iomem *log_addr_va;
	u32 log_size;
	struct log_readindex_addr_pid r_index_array[MAX_R_INDEX_NUM];
	void __iomem *log_write_index_reg_va;
	void __iomem *log_read_index_reg_va;
	struct mutex log_read_mutex;

	struct completion completion_log;
	spinlock_t w_index_lock;
	uint32_t log_write_index;

	struct ipc_instance_cfg *ipc_cfg;

	// lite sleep
	enum hb_sleep_mode sleep_mode;
	enum hb_audio_wakeup_status wakeup_status;
	uint32_t smf_wrapper_in_use;
	ipc_wrapper_data_t *smf_wrapper_data;
	struct completion smf_power_comp;
	struct completion smf_pcm_comp;

	// clk
	struct clk *clk;
	struct clk *p_clk;

	//coredump
	void __iomem *mem_reserved[4];
	u32 mem_reserved_size[4];

	uint32_t dsp_iram0_map_addr;
	uint32_t dsp_iram0_size;
	uint32_t dsp_dram0_map_addr;
	uint32_t dsp_dram0_size;
	uint32_t dsp_dram1_map_addr;
	uint32_t dsp_dram1_size;
	uint32_t dsp_ddr_addr;
	uint32_t dsp_ddr_size;

	struct workqueue_struct *wq_coredump;
	struct work_struct work_coredump;
	struct completion completion_coredump;

	int32_t is_wwdt_enable;
	int32_t irq_wwdt_reset;

	struct reset_control *rst;

#if 0
	int32_t dsp_pwait_int;
	int32_t should_stop;
	int32_t vring0;
#endif

	struct hobot_rsc_table sram0;
	struct hobot_rsc_table bsp;
	struct hobot_rsc_table ipc;

	void *bsp_base;
	void *ipc_base;

	int8_t thread_status[8];
	int8_t loglevel[10];

	void *dspthread_info_addr_va;
	void *dspthread_status_reg_va;

	struct mutex mutex_boot;
	int32_t wait_timeout;
	struct completion completion_boot;

	struct hobot_rproc_version version;
};

static inline void *log_memcpy(void *dest, void *src, unsigned int num)
{
        char *pdest;
        char *psrc;

        if((dest == NULL) ||(src == NULL)){
                return NULL;
        }

        pdest =(char *)dest;
        psrc =(char *)src;
        while(num --) {
                *pdest = *psrc;
                pdest++;
                psrc++;
        }
        return (void *)dest;
}

int32_t hobot_remoteproc_boot_hifi5(uint32_t dsp_id, int32_t timeout, const char *total_path);
int32_t hobot_remoteproc_shutdown_hifi5(uint32_t dsp_id);
int32_t hobot_remoteproc_set_dsp_firmware_name(const char *name);
int32_t hobot_remoteproc_get_dsp_status(uint32_t dsp_id, uint32_t *status);
int32_t hobot_remoteproc_reset_hifi5(uint32_t dsp_id);

#endif /*HOBOT_VDSP_REMOTEPROC_H*/
