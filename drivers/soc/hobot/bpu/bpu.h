/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * Zhang Guoying <guoying.zhang horizon.ai>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#ifndef __BPU_H__
#define __BPU_H__ /*PRQA S 0603*/ /* Linux header define style */

#include <linux/time.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <uapi/hobot/bpu.h>

#define ALL_CORE_MASK	(0xFFFFFFFFu)
#define PERSENT		(100u)
#define SECTOMS		(1000)
#define SECTOUS		(1000000)

/* user priority high bit to confirm bind fc mode */
#define USER_PRIO_MASK (0x7FFFFFFFu)

/* must be a power of 2 */
#define BPU_CORE_RECORE_NUM		(64)

#define USER_RANDOM_SHIFT		(32u)
#define USER_MASK				(0xFFFFFFFFu)

struct bpu_core;

/**
 * struct bpu_fc - bpu fc task struct in kernel space
 * @info: user_bpu_fc which from userspace
 * @hw_id: the fc task id which record for bpu hardware
 * @fc_data: raw bpu fc data which parsed from user fc
 * @core_mask: user set fc can run cores bit
 * @run_c_mask: real fc run cores bit, 0 is anyone
 * @g_id: group pointer which record in driver
 * @user: user pointer which identify diff file user
 * @index: record the index of bpu hw fifo when set in
 * @bind: if true, hw fcs which in bpu_fc need bind
 *		  set to hardware, can't be split by priority
 *		  policy
 * @start_point: record time point when set to kernel
 * @end_point: record time point when bpu process done
 *
 * bpu_fc use in bpu driver, to transfer the bpu task,
 * information can be used for status and debug, for
 * example, process_time from end_point - start_point.
 */
struct bpu_fc {
	struct user_bpu_fc info;

	uint32_t hw_id;

	void *fc_data;

	uint32_t g_id;

	uint64_t user_id;
	void **user;

	uint32_t index;

	bool bind;

	ktime_t start_point;
	ktime_t end_point;
};

/**
 * struct bpu_fc_group - struct to store fc group in kernel
 * @node: list node which will store in framework
 * @id: group id
 * @proportion: group running ratio proportion which set from user
 * @spin_lock: to protect information record about group
 * @p_run_time: to record group fcs process time in statistical period
 *
 * bpu fc bind the bpu fc group to store some information
 * about fc's group
 */
struct bpu_fc_group {
	struct list_head node;
	uint32_t id;
	int32_t proportion;

	spinlock_t spin_lock;

	uint64_t p_run_time;
};

/**
 * struct bpu_user - struct to store user in kernel
 * @node: list node which will store in framework
 * @id: user id
 * @is_alive: record the user alive status
 * @running_task_num: record the user running fc number
 * @poll_wait: for vfs poll
 * @spin_lock: to protect operations of the user
 * @mutex_lock: to protect operations of the user
 * @no_task_comp: for user not task running
 * @done_fcs: fifo to store user's fcs which has been process done
 * @p_run_time: to record user fcs process time in statistical period
 * @p_file_private: use the pointer's point to user for judging
 * 					whether valid of user after file released
 * @host: to store bpu framework or bpu core
 *
 * in linux user from the open file pointer, create when file
 * open.
 */
struct bpu_user {
	struct list_head node;
	uint64_t version;
	uint64_t id;
	uint16_t is_alive;
	int32_t running_task_num;

	wait_queue_head_t poll_wait;
	spinlock_t spin_lock;

	struct mutex mutex_lock;
	struct completion no_task_comp;

	DECLARE_KFIFO_PTR(done_fcs, struct user_bpu_fc);/*PRQA S 1061*/ /* Linux Macro */

	uint64_t p_run_time;

	void **p_file_private;

	void *host;
};

/**
 * struct bpu_extra_ops - extended callback for special function
 */
struct bpu_extra_ops {
	/**
	 * init()- call when bpu hardware power/clock on
	 * @bpu_core: could not be null
	 *
	 * Return:
	 * * =0				- success
	 * * <0				- error code
	 */
	int32_t (*init)(const struct bpu_core *core);
	/**
	 * post_init()- call when bpu hardware inited
	 * @bpu_core: could not be null
	 *
	 * Return:
	 * * =0				- success
	 * * <0				- error code
	 */
	int32_t (*post_init)(const struct bpu_core *core);
	/**
	 * report()- report info when needed
	 * @bpu_core: could not be null
	 * @info: report infomation, which define appoint
	 * @data: report data if have, can be 0
	 * @error: 0,no error; not 0, error happen
	 *
	 * Return:
	 * * =0				- report success
	 * * <0				- error code
	 */
	int32_t (*report)(const struct bpu_core *core, uint32_t info, uint64_t data, uint32_t error);
	/**
	 * deinit()- call when bpu hardware disable (before power/clock off)
	 * @bpu_core: could not be null
	 */
	void (*deinit)(const struct bpu_core *core);
};

/**
 * struct bpu - bpu framework structure
 * @miscdev: for misc device node
 * @open_counter: record bpu framework dev file open number
 * @sched_timer: timer for bpu framework scheduling policy
 * @extra_ops: extended callback pointer
 * @mutex_lock: lock to protect framework
 * @spin_lock: lock to protect framework
 * @core_list: list for recording registed bpu cores
 * @user_list: list for recording users
 * @group_list: list for recording groups
 * @user_spin_lock: lock to protect users add/del/user
 * @group_spin_lock: lock to protect group list add/del/user
 * @slow_task_time: record slowest task time in priod of time
 * @sched_seed: use the value to adjust sched time
 * @stat_reset_count: used for statistical approach
 * @bus: for os sys bus
 * @sched_spin_lock: lock to protect bpu sched resource
 * @ratio: record bpu framework running ratio
 *
 * In bpu framework, Any BPU core, any user, any group
 * can be accessed via a struct BPU.
 */
struct bpu {
	struct miscdevice miscdev;
	atomic_t open_counter;

	struct timer_list sched_timer;
	struct bpu_extra_ops *extra_ops;

	struct mutex mutex_lock;
	spinlock_t spin_lock;

	struct list_head core_list;

	struct list_head user_list;
	struct list_head group_list;
	spinlock_t user_spin_lock;
	spinlock_t group_spin_lock;

	uint64_t slow_task_time;

	uint32_t sched_seed;
	uint32_t stat_reset_count;
	struct mutex sched_mutex_lock;

	struct bus_type *bus;

	uint32_t ratio;
};

/**
 * @def BPU_HW_IO_VERSION
 * the bpu framework support hw io version
 */
#define BPU_HW_IO_VERSION	(0xFFFF0001)
/**
 * @def MAX_HW_FIFO_NUM
 * The max bpu hw fifo number
 */
#define MAX_HW_FIFO_NUM	(8)
/**
 * @def MAX_HW_FIFO_CAP
 * The max bpu hw fifo capacity
 */
#define MAX_HW_FIFO_CAP	(1024)

/**
 * @struct bpu_core_hw_inst_t
 * struct for store resource about bpu hardware
 * @NO{S04E02C01}
 *
 * @var bpu_core_hw_inst_t::hw_io_ver
 * the bpu hw io version
 * @var bpu_core_hw_inst_t::index
 * the bpu core index
 * @var bpu_core_hw_inst_t::host
 * store host pointer, as the bpu core
 * @var bpu_core_hw_inst_t::base
 * the bpu hw reg base address
 * @var bpu_core_hw_inst_t::task_base
 * the address to put bpu task data
 * @var bpu_core_hw_inst_t::task_phy_base
 * the phy address to put bpu task data
 * @var bpu_core_hw_inst_t::task_range_size
 * the range size fo put bpu task data
 * @var bpu_core_hw_inst_t::task_buf_limit
 * the max number for bpu hw buffer bpu task
 * @var bpu_core_hw_inst_t::hw_prio_en
 * if enable bpu hardware priority
 * @var bpu_core_hw_inst_t::burst_len
 * for user set burst length
 * @var bpu_core_hw_inst_t::reserved
 * reserved
 */
typedef struct bpu_core_hw_inst_t {
	uint32_t hw_io_ver;
	int32_t index;
	void *host;
	void *base;
	void *task_base[MAX_HW_FIFO_NUM];
	uint64_t task_phy_base[MAX_HW_FIFO_NUM];
	uint64_t task_map_base[MAX_HW_FIFO_NUM];
	uint32_t task_range_size;
	int32_t task_buf_limit;
	int32_t hw_prio_en;
	uint32_t burst_len;
	uint64_t reserved[8];
} bpu_core_hw_inst_t;

/**
 * @enum bpu_arch_t
 * @NO{S04E02C01}
 *
 * @var ARCH_UNKNOWN
 * unknow bpu arch
 * @var ARCH_BERNOULLI
 * bpu bernoulli arch
 * @var ARCH_BAYES
 * bpu bayes arch
 * @var ARCH_B30
 * bpu nash arch
 */

typedef enum bpu_arch_t {
    ARCH_UNKNOWN = 0,
	ARCH_BERNOULLI,
	ARCH_BAYES,
	ARCH_B30,
	ARCH_MAX,
} bpu_arch_t;

typedef struct {
} bernoulli_feat_t;

typedef bernoulli_feat_t bayes_feat_t;

typedef struct {

} b30_feat_t;

typedef union {
	bernoulli_feat_t bernoulli;
	bayes_feat_t bayes;
	b30_feat_t b30;
} bpu_hw_feat_t;

/**
 * @struct bpu_hw_ops_t
 * struct for bpu hardware operations
 * @NO{S04E02C01}
 *
 * @var bpu_hw_ops_t::enable
 * enable bpu hardware
 * @var bpu_hw_ops_t::disable
 * disable bpu hardware
 * @var bpu_hw_ops_t::reset
 * reset bpu hardware
 * @var bpu_hw_ops_t::set_clk
 * if set clock need special slow, define the callback
 * @var bpu_hw_ops_t::set_volt
 * if set voltage need special slow, define the callback
 * @var bpu_hw_ops_t::write_fc
 * write task data to bpu hardware to process
 * @var bpu_hw_ops_t::read_fc
 * read task processed infomation from bpu hardware
 * @var bpu_hw_ops_t::cmd
 * support some misc command for software use bpu hardware
 * @var bpu_hw_ops_t::debug
 * for debug bpu hardware information
 */
typedef struct bpu_hw_ops_t {
	int32_t (*enable)(bpu_core_hw_inst_t *);
	int32_t (*disable)(bpu_core_hw_inst_t *);
	int32_t (*reset)(bpu_core_hw_inst_t *);
	int32_t (*set_clk)(const bpu_core_hw_inst_t *, uint64_t);
	int32_t (*set_volt)(const bpu_core_hw_inst_t *, int32_t);
	int32_t (*write_fc)(const bpu_core_hw_inst_t *, struct bpu_fc *task, uint32_t);
	int32_t (*read_fc)(const bpu_core_hw_inst_t *, uint32_t *, uint32_t *);
	int32_t (*cmd)(const bpu_core_hw_inst_t *, uint32_t);
	int32_t (*debug)(const bpu_core_hw_inst_t *, int32_t);
} bpu_hw_ops_t;

/**
 * @struct bpu_hw_io_t
 * struct for bpu hardware io instance
 * @NO{S04E02C01}
 *
 * @var bpu_hw_io_t::version
 * the version of the hw io, if not match with bpu framework, can not use
 * @var bpu_hw_io_t::prio_num
 * the hardware support prio number
 * @var bpu_hw_io_t::prio_offset
 * the bit shift for hw use the prio level
 * @var bpu_hw_io_t::task_version
 * the support bpu task version
 * @var bpu_hw_io_t::task_size
 * the support bpu task data size
 * @var bpu_hw_io_t::task_capacity
 * the capacity of the bpu hw for write bpu task data
 * @var bpu_hw_io_t::task_max
 * the max task id value
 * @var bpu_hw_io_t::arch
 * the bpu hardware architecture
 * @var bpu_hw_io_t::feat
 * the special infomation of diff arch
 * @var bpu_hw_io_t::ops
 * bpu hardware operations
 */
typedef struct bpu_hw_io_t {
	uint32_t version;
	uint16_t prio_num;
	uint16_t prio_offset;
	uint32_t task_version;
	uint32_t task_size;
	uint32_t task_capacity;
	uint32_t task_id_max;
	bpu_arch_t arch;
	bpu_hw_feat_t feat;
	bpu_hw_ops_t ops;
} bpu_hw_io_t;

int32_t bpu_hw_io_register(bpu_hw_io_t *hw_io);
int32_t bpu_hw_io_unregister(bpu_hw_io_t *hw_io);

extern struct bpu *g_bpu;

void bpu_fc_clear(struct bpu_fc *fc);
int32_t bpu_write_with_user(const struct bpu_core *core,
			struct bpu_user *user,
			const char __user *buf, size_t len);
int32_t bpu_read_with_user(struct bpu_core *core,
			struct bpu_user *user,
			const char __user *buf, size_t len);

struct bpu_user *bpu_get_user(struct bpu_fc *fc,
		const struct list_head *user_list);


/**
 * bpu_get_fc_group() - get native group id from integrated id
 * @raw_id: integrated id
 *
 * value bit choose, group id is low 16bits
 *
 * Return: group id
 */
static inline uint32_t bpu_group_id(uint32_t raw_id)
{
	return raw_id & 0xFFFFu;
}

/**
 * bpu_get_fc_group() - get native user id from integrated id
 * @raw_id: integrated id
 *
 * value bit choose, user id is high 16bits
 *
 * Return: user id
 */
static inline uint32_t bpu_group_user(uint32_t raw_id)
{
	return raw_id >> 16u;
}

/* register core apis */
int32_t bpu_core_register(struct bpu_core *core);
void bpu_core_unregister(struct bpu_core *core);
int32_t bpu_core_driver_register(struct platform_driver *driver);
void bpu_core_driver_unregister(struct platform_driver *driver);
int32_t bpu_write_fc_to_core(struct bpu_core *core,
		struct bpu_fc *bpu_fc, uint32_t offpos);
bool bpu_core_is_online(const struct bpu_core *core);

/* statusis apis */
uint64_t bpu_get_time_point(void);
uint32_t bpu_ratio(struct bpu *bpu);
uint32_t bpu_fc_group_ratio(struct bpu_fc_group *group);
uint32_t bpu_user_ratio(struct bpu_user *user);
int32_t bpu_check_fc_run_time(struct bpu_core *core,
		struct bpu_fc_run_time *fc_run_time);
uint64_t bpu_core_bufferd_time(struct bpu_core *core, uint32_t level);
uint64_t bpu_core_last_done_time(struct bpu_core *core);
uint64_t bpu_core_pending_task_est_time(struct bpu_core *core);

/* sched apis*/
int32_t bpu_sched_start(struct bpu *bpu);
void bpu_sched_stop(struct bpu *bpu);
void bpu_core_update(struct bpu_core *core, struct bpu_fc *fc);
uint32_t bpu_core_ratio(struct bpu_core *core);
void bpu_sched_seed_update(void);
int32_t bpu_sched_recover_core(struct bpu_core *core);

/* ctrl apis */
int32_t bpu_stat_reset(struct bpu *bpu);

/* bpu state notifier notify */
int32_t bpu_notifier_notify(uint64_t val, void *v);

uint16_t bpu_core_avl_cap(struct bpu_core *core);

/* sys apis */
int32_t bpu_device_add_group(struct device *dev, const struct attribute_group *grp);
void bpu_device_remove_group(struct device *dev, const struct attribute_group *grp);
int32_t bpu_sys_system_init(struct bpu *bpu);
void bpu_sys_system_exit(const struct bpu *bpu);
int32_t bpu_core_create_sys(struct bpu_core *core);
void bpu_core_discard_sys(const struct bpu_core *core);

/* apis for register extra_ops to bpu core, fusa can use */
int32_t bpu_extra_ops_register(struct bpu_extra_ops *ops);
void bpu_extra_ops_unregister(const struct bpu_extra_ops *ops);

#ifdef CONFIG_FAULT_INJECTION_ATTR
int bpu_core_fault_injection_store(struct device *dev,
		const char *buf, size_t len);
int bpu_core_fault_injection_show(struct device *dev,
		char *buf, size_t len);
int32_t bpu_property_read_u32(const struct device_node *np,
		const char *propname,
		u32 *out_value);
#endif

#endif
