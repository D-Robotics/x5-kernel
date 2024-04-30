
/*
 * Horizon Robotics
 *
 *  Copyright (C) 2021 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#define BUF_SIZE_KB_DEFAULT (1024)
#define BUF_SIZE_KB_LIMIT (204800)
#define KB_TO_B_SHIFT (10)
#define NS_TO_SEC (1000000000)
#define NS_TO_US (1000)

enum SCHED_MODE {
	SCHED_MODE_VADDR = 1,
	SCHED_MODE_PSTORE
};

struct persistent_ram_buffer_temp {
	uint32_t    sig;
	atomic_t    start;
	atomic_t    size;
	uint8_t     data[0];
};

#define TASK_STAT_BUF_LEN	4
/* fits into a single 64-byte cache line.*/
struct hobot_sched_log {
	u64 ts;
	s16 cpuid;
	u8 prev_prio;
	u8 next_prio;
	s32 prev_pid;
	s32 prev_tgid;
	s32 next_pid;
	s32 next_tgid;
	char prev_stat[TASK_STAT_BUF_LEN];
	char prev_comm[TASK_COMM_LEN];
	char next_comm[TASK_COMM_LEN];
} ____cacheline_aligned;

#define REC_SIZE sizeof(struct hobot_sched_log)
#define BUF_REMAINING_MIN (REC_SIZE * 2)

extern void * pstore_get_sched_area_vaddr(void);
extern size_t pstore_get_sched_area_size(void);

static int buf_size_kbytes = BUF_SIZE_KB_DEFAULT;
module_param(buf_size_kbytes, int, 0644);
static int mode = SCHED_MODE_PSTORE;
module_param(mode, int, 0644);

static char **buf;
static int *pos;
static int bufsz = 0;

/* copy from fs/proc/array.c */
static const char * const task_state_array[] = {

	/* states in TASK_REPORT: */
	"R", 		/* (running)  0x00 */
	"S", 		/* (sleeping)"  0x01 */
	"D", 		/* (disk sleep) 0x02 */
	"T", 		/* (stopped) 0x04 */
	"t", 		/* (tracing stop) 0x08 */
	"X",		/* (dead) 0x10 */
	"Z", 		/* (zombie) 0x20 */
	"P", 		/* (parked) 0x40 */

	/* states beyond TASK_REPORT: */
	"I",		/* (idle) 0x80 */
};

static inline const char *get_task_state(struct task_struct *tsk)
{
	BUILD_BUG_ON(1 + ilog2(TASK_REPORT_MAX) != ARRAY_SIZE(task_state_array));
	return task_state_array[task_state_index(tsk)];
}

static int hobot_sched_store_info(bool preempt, struct task_struct *prev, struct task_struct *next)
{
	struct hobot_sched_log rec;
	int cpuid = smp_processor_id();
	const char *stat = preempt ? "R+" : get_task_state(prev);

	rec.ts = local_clock();
	rec.cpuid = cpuid;
	rec.prev_pid = task_pid_nr(prev);
	rec.prev_tgid = task_tgid_nr(prev);
	rec.prev_prio = prev->prio;
	rec.next_pid = task_pid_nr(next);
	rec.next_tgid = task_tgid_nr(next);
	rec.next_prio = next->prio;
	strncpy(rec.prev_stat, stat, strlen(stat) + 1);
	strncpy(rec.prev_comm, prev->comm, sizeof(rec.prev_comm));
	strncpy(rec.next_comm, next->comm, sizeof(rec.next_comm));

	memcpy(buf[cpuid] + pos[cpuid], &rec, sizeof(struct hobot_sched_log));

	if (bufsz - pos[cpuid] > BUF_REMAINING_MIN)
		pos[cpuid] += REC_SIZE;
	else
		pos[cpuid] = 0;

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int hobot_sched_logger_show(struct seq_file *s, void *unused)
{
	int i;
	struct hobot_sched_log *rec;

	for (i = 0; i < num_possible_cpus(); i++) {
		rec = (struct hobot_sched_log *)buf[i];

		while ((char *)rec < (buf[i] + bufsz - REC_SIZE)) {
			if (rec->ts == 0) {
				pr_info("Stop dumping at offset 0x%lx\n", (char *)rec - buf[i]);
				break;
			}
			seq_printf(s, "[%5llu.%06llu] [%d] : %s(%d:%d:%d:%s) -> %s(%d:%d:%d)\n",
				rec->ts / NS_TO_SEC, (rec->ts % NS_TO_SEC) / NS_TO_US, rec->cpuid,
				rec->prev_comm, rec->prev_tgid, rec->prev_pid, rec->prev_prio, rec->prev_stat,
				rec->next_comm, rec->next_tgid, rec->next_pid, rec->next_prio);

			rec++;
		}
	}

	return 0;
}

static int hobot_sched_logger_open(struct inode *inode, struct file *file)
{
    return single_open(file, hobot_sched_logger_show, NULL);
}

static const struct file_operations sched_switch_debug_fops = {
    .open           = hobot_sched_logger_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

static struct dentry *sched_switch_debugfs_dir;

static int hobot_sched_init_debugfs(void)
{
    struct dentry *d;

    d = debugfs_create_dir("sched_logger", NULL);
    if (d == NULL)
        return -ENOMEM;
    sched_switch_debugfs_dir = d;
    d = debugfs_create_file("buf", S_IRUGO, sched_switch_debugfs_dir,
            NULL, &sched_switch_debug_fops);
    if (d == NULL) {
        pr_err("Failed to create debugfs node\n");
        debugfs_remove_recursive(sched_switch_debugfs_dir);
        sched_switch_debugfs_dir = NULL;
        return -ENOMEM;
    }

    return 0;
}

static void hobot_sched_uninit_debugfs(void)
{
    if (sched_switch_debugfs_dir)
        debugfs_remove_recursive(sched_switch_debugfs_dir);
}
#else
static inline int hobot_sched_init_debugfs(void)
{
    return 0;
}
static inline void hobot_sched_uninit_debugfs(void)
{
}
#endif

static int __init hobot_sched_logger_init(void)
{
    int i;
    int ret;
    void *sched_vaddr;
    size_t sched_total_size;
    size_t sched_percpu_size;
    int num_cpus = num_possible_cpus();

    if (buf_size_kbytes > BUF_SIZE_KB_LIMIT) {
        buf_size_kbytes = BUF_SIZE_KB_LIMIT;
	}

    if (mode != SCHED_MODE_VADDR
        && mode != SCHED_MODE_PSTORE) {
            mode = SCHED_MODE_VADDR;
    }
    buf = kzalloc(sizeof(char *) *num_cpus, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    pos = kzalloc(sizeof(int) *num_cpus, GFP_KERNEL);
    if (!pos) {
        kfree(buf);
        return -ENOMEM;
    }

    if (mode == SCHED_MODE_VADDR) {
        bufsz = buf_size_kbytes << KB_TO_B_SHIFT;
        for (i = 0; i < num_cpus; i++) {
            buf[i] = (char *)vmalloc(buf_size_kbytes << KB_TO_B_SHIFT);
            if (buf[i] == NULL) {
                pr_err("Failed to vmalloc %d KB\n", buf_size_kbytes);
                ret = -ENOMEM;
                goto free;
            }
            memset(buf[i], 0, buf_size_kbytes << KB_TO_B_SHIFT);
        }
        pr_info("vmalloc 4 * %d KBytes buffer\n", buf_size_kbytes);
    } else {
        struct persistent_ram_buffer_temp *prb;

        sched_vaddr = pstore_get_sched_area_vaddr();
        sched_total_size = pstore_get_sched_area_size();
        pr_debug("sched_vaddr=%px, sched_total_size=%ld\n",
                    sched_vaddr, sched_total_size);

        if (!sched_vaddr || !sched_total_size) {
            ret = -EFAULT;
            goto free;
        }

        prb = (struct persistent_ram_buffer_temp *)sched_vaddr;
        mode = SCHED_MODE_PSTORE;
        /*  */
        //coverity[divide_by_zero:SUPPRESS]
        sched_percpu_size = sched_total_size / num_cpus - L1_CACHE_BYTES;
        /* reserved 64 Bytes for persistent_ram_buffer_temp */
        sched_vaddr += L1_CACHE_BYTES;
        bufsz = sched_percpu_size;
        for (i = 0; i < num_cpus; i++) {
            buf[i] = (char *)(sched_vaddr + sched_percpu_size * i);
            if (!IS_ALIGNED((u64)buf[i], L1_CACHE_BYTES))
                pr_warn("warn:sched_vaddr is non-aligned to 64-byte cache line\n");
            memset(buf[i], 0, bufsz);
            pr_debug("buf[%d]=%px, size=%ld\n", i, buf[i], sched_percpu_size);
        }
        atomic_set(&(prb->start), 0);
        atomic_set(&(prb->size),
                sched_total_size - sizeof(struct persistent_ram_buffer_temp));
	}
    register_sched_logger(hobot_sched_store_info);
    hobot_sched_init_debugfs();

    pr_info("success mode=%d\n", mode);
    return 0;

free:
    kfree(pos);
    kfree(buf);
    return ret;
}

static void __exit hobot_sched_logger_exit(void)
{
    int i;

    hobot_sched_uninit_debugfs();
    unregister_sched_logger();

    if (mode == SCHED_MODE_VADDR) {
    for (i = 0; i < num_possible_cpus(); i++)
        if(buf[i])
            vfree(buf[i]);
    }
    kfree(pos);
    kfree(buf);
}

module_init(hobot_sched_logger_init)
module_exit(hobot_sched_logger_exit)

MODULE_AUTHOR("Horizon Inc.");
MODULE_DESCRIPTION("sched switch logger module");
MODULE_LICENSE("GPL");
