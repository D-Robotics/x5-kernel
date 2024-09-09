/*
 *			 COPYRIGHT NOTICE
 *		 Copyright 2020 Horizon Robotics, Inc.
 *			 All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/remoteproc.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <asm/unistd.h>
#include <linux/list.h>
#include <linux/genalloc.h>
#include <linux/pfn.h>
#include <linux/idr.h>
#include <linux/completion.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include "remoteproc_internal.h"
#include "hobot_remoteproc.h"
#include "gua_audio_rpc_protocol.h"
#include "gua_audio_ipc.h"
#include "gua_audio_struct_define.h"
#include "gua_pcm.h"
#include "remoteproc_elf_helpers.h"

typedef struct _control {
	smf_packet_head_t header;
	u8 payload[0];
} control_msg_s_t;

#define CONFIG_HOBOT_ADSP_CTRL 1

#define HORIZON_SIP_HIFI5_VEC_SET		0xc2000007
#define HORIZON_SIP_HIFI5_VEC_GET		0xc2000008


#define X5_DDR_HIGH_MASK  (0x3)

#define CONFIG_HOBOT_REMOTEPROC_PM (1)
#define HOBOT_HIFI5_PREP_CLK

#define HORIZON_SMF_POWER_CHAN	(0)
#define HORIZON_SMF_PCM_CHAN	(0)

#define HIFI5_LOW_POWER_DEV 3

#define DSP_LOGLEVEL "loglevel"
#define DBG_TYPE_LEVEL (3)

// static struct clk *hifi5_clk;
// static struct completion hifi5_coredump_complete;

static bool autoboot __read_mostly;
#ifdef CONFIG_HOBOT_ADSP_CTRL
static void __iomem *timesync_acore_to_adsp;
#endif
static uint32_t adsp_boot_flag = 0;

struct x5_hifi5_resous {
	struct rproc *rproc;
	void __iomem * nonsec_reg;
	void __iomem * sec_reg;
	void __iomem * crm_reg;
	void __iomem * pmu_reg;
	dma_addr_t reset_vector;
	// struct mbox_chan mbox_chan;
};

static struct x5_hifi5_resous g_hifi5_res;

static uint32_t litemmu_table[4] = {0x03020100, 0x07060504, 0x0B0A0908, 0x0F0E0D0C};
static char *ramdump_name[] = {"ddr", "iram0", "dram0", "dram1"};

extern int32_t sync_pipeline_state(uint16_t pcm_device, uint8_t stream, uint8_t state);
static int32_t hobot_dsp_ipc_close_instance(void);
static void hobot_log_handler(uint8_t *userdata, int32_t instance, int32_t chan_id,
	uint8_t *buf, uint64_t size);

#ifdef CONFIG_HOBOT_REMOTEPROC_PM
int32_t hobot_remoteproc_pm_ctrl(const struct hobot_rproc_pdata *pdata, uint16_t init, uint16_t ops) {
       int32_t ret = 0;
       struct device *dev = pdata->dev;

       if (pdata == NULL) {
               pr_err("To pm ctrl invalid HIFI\n");
               return -EINVAL;
       }

       if (ops > 0u) {
               if (init > 0u) {
                       pm_runtime_set_autosuspend_delay(dev, 0);
                       pm_runtime_enable(dev);
               }
               ret = pm_runtime_get_sync(dev);
               if (ret < 0) {
                       pm_runtime_put_noidle(dev);
                       dev_err(dev, "pm runtime get sync failed\n");
                       return ret;
               }
       } else {
               ret = pm_runtime_put_sync_suspend(dev);
               if (init > 0u) {
                       pm_runtime_disable(dev);
               }
       }
       udelay(100);

       return ret;
}
#endif

int32_t hobot_remoteproc_reset_hifi5(uint32_t id)
{
	int32_t ret = 0;
	if (g_hifi5_res.rproc->state != (uint32_t)RPROC_RUNNING)
		return -EINVAL;
	rproc_shutdown(g_hifi5_res.rproc);
	msleep(50);
	if (g_hifi5_res.rproc->state == (uint32_t)RPROC_RUNNING)
		return -EBUSY;
	ret = rproc_boot(g_hifi5_res.rproc);

	return ret;
}
EXPORT_SYMBOL(hobot_remoteproc_reset_hifi5);/*PRQA S 0307*//*kernel function*/

#ifndef HOBOT_HIFI5_PREP_CLK
static void hifi5_enable_clk(int32_t index)
{
	int32_t ret;
	uint64_t clk_op;
	uint64_t clk_rate;

	if(index  == 0) {
		if ((hifi5_clk != NULL) && (!__clk_is_enabled(hifi5_clk))) {
			pr_info("hifi5 clk have not enabled\n");
			return;
		}
		clk_op = clk_get_rate(hifi5_clk);

		if(clk_op != MAX_DSP_FREQ)
			clk_op = MAX_DSP_FREQ;


		clk_disable(hifi5_clk);
		clk_rate = (uint64_t)clk_round_rate(hifi5_clk, clk_op);
		ret = clk_set_rate(hifi5_clk, clk_rate);
		if (ret) {
			pr_err("%s: err checkout hifi5 clock rate!!\n", __func__);
			return;
		}

		clk_enable(hifi5_clk);
		clk_op = clk_get_rate(hifi5_clk);
		pr_info("%s: This hifi5 rate is %lld\n", __func__, clk_op);
	} else {
		if ((hifi51_clk != NULL) && (!__clk_is_enabled(hifi51_clk))) {
			pr_info("hifi51 clk have not enabled\n");
			return;
		}

		clk_op = clk_get_rate(hifi51_clk);

		if(clk_op != MAX_DSP_FREQ)
			clk_op = MAX_DSP_FREQ;

		clk_disable(hifi51_clk);
		clk_rate = (uint64_t)clk_round_rate(hifi51_clk, clk_op);
		ret = clk_set_rate(hifi51_clk, clk_rate);
		if (ret) {
			pr_err("%s: err checkout hifi51 clock rate!!\n", __func__);
			return;
		}

		clk_enable(hifi51_clk);

		clk_op = clk_get_rate(hifi51_clk);
		pr_info("%s: This hifi51 rate is %lld\n", __func__, clk_op);
	}
}
#endif

static int32_t hobot_dsp_rproc_start(struct rproc *rproc)
{
	struct hobot_rproc_pdata *pdata = rproc->priv;
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	mdelay(10);
	pdata->ipc_ops->start_remoteproc(pdata);
	return 0;
}

static int32_t hobot_dsp_rproc_stop(struct rproc *rproc)
{
	int32_t ret = 0;
	struct rproc_mem_entry *entry, *tmp;
	struct hobot_rproc_pdata *pdata = rproc->priv;
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	pdata->ipc_ops->release_remoteproc(pdata);

	list_for_each_entry_safe(entry, tmp, &rproc->mappings, node) {
		iounmap(entry->va);

		list_del(&entry->node);
		kfree(entry);
	}

	ret = hobot_dsp_ipc_close_instance();
	if (ret < 0)
		return ret;

	return 0;
}

static void hobot_timesync(struct device *dev,u32 second,u32 nanosec,u32 diff) {
#ifdef CONFIG_HOBOT_ADSP_CTRL
	uint32_t value;
#endif
	struct rproc *rproc = container_of(dev, struct rproc, dev);
	struct hobot_rproc_pdata *pdata = rproc->priv;

	writel(second, pdata->timesync_sec_reg_va);
	writel(nanosec, pdata->timesync_nanosec_reg_va);
	writel(diff, pdata->timesync_sec_diff_reg_va);

#ifdef CONFIG_HOBOT_ADSP_CTRL
	if (adsp_boot_flag == 1u && timesync_acore_to_adsp != NULL) {
		value = readl(timesync_acore_to_adsp);
		value |= (0x1u << 0);
		writel(value, timesync_acore_to_adsp);
	}
#endif
}

static void get_read_index_by_euid(struct hobot_rproc_pdata *pdata, int32_t pid, uint32_t *r_i)
{
        uint32_t i;
        struct log_readindex_addr_pid *r_index_p;
        r_index_p=pdata->r_index_array;

        for (i = 0; i < MAX_R_INDEX_NUM; i++) {
                if (r_index_p[i].pid == pid) {
                        *r_i = r_index_p[i].read_index;
                        return;
                }
        }

        spin_lock(&(pdata->r_index_lock));
        for (i = 0; i < MAX_R_INDEX_NUM; i++) {
                (void)dev_info(&pdata->rproc->dev, "new: %d, %d   %d\n", pid, i, r_index_p[i].pid);
                if (r_index_p[i].pid == 0) {
                        r_index_p[i].pid = pid;
                        *r_i = r_index_p[i].read_index;
                        spin_unlock(&(pdata->r_index_lock));
                        return;
                }
        }
        spin_unlock(&(pdata->r_index_lock));

        *r_i = r_index_p[MAX_R_INDEX_NUM - 1u].read_index;
}

static void clr_euid_by_euid(struct hobot_rproc_pdata *pdata, int32_t pid)
{
        uint32_t i;
        struct log_readindex_addr_pid *r_index_p;
        r_index_p = pdata->r_index_array;
        (void)dev_info(&pdata->rproc->dev, "clr: %d\n", pid);

        for (i = 0; i < MAX_R_INDEX_NUM; i++){
                if (r_index_p[i].pid == pid){
                        (void)dev_info(&pdata->rproc->dev, "clr: %d,  %d   %d\n", pid, i, r_index_p[i].pid);
                        r_index_p[i].pid = 0;
                        r_index_p[i].read_index = 0;
                        break;
                }
        }
}

static void set_read_index_by_euid(struct hobot_rproc_pdata *pdata, int32_t pid, uint32_t r_i)
{
        uint32_t i;
        struct log_readindex_addr_pid *r_index_p;
        r_index_p = pdata->r_index_array;

        for (i = 0; i < MAX_R_INDEX_NUM; i++) {
                if (r_index_p[i].pid == pid) {
                        r_index_p[i].read_index = r_i;
                        break;
                }
        }
        if (i == MAX_R_INDEX_NUM) {
                r_index_p[MAX_R_INDEX_NUM - 1u].read_index = r_i;
        }
}

static int hobot_log_show(struct device *dev, struct device_attribute *attr, char *buf) {
	uint32_t write_index = 0;
	uint32_t read_index = 0;
	uint32_t pos;
	uint8_t *p;
	uint32_t len = 0;
	uint32_t need_read;
	//void __iomem *log_write_index_reg_va;
	void __iomem *log_addr_va;
	uint32_t log_size;
	int32_t ret;
	//unsigned long flags;
	struct rproc *rproc = container_of(dev, struct rproc, dev);
	struct hobot_rproc_pdata *pdata = rproc->priv;
	char *buftmp;

	//log_write_index_reg_va = pdata->log_write_index_reg_va;
	log_addr_va = pdata->log_addr_va;
	log_size = pdata->log_size;

	spin_lock(&(pdata->w_index_lock));
	write_index = pdata->log_write_index;
	spin_unlock(&(pdata->w_index_lock));
	get_read_index_by_euid(pdata, current->pid, &read_index);

	while (write_index == read_index) {
		if (signal_pending(current)) {
			clr_euid_by_euid(pdata, current->pid);
                        return -EINTR;
		}

		if ((ret = wait_for_completion_interruptible(&pdata->completion_log)) < 0) {
			pr_debug("wait_for_log_complete_interruptible error ret %d\n",ret);
			break;
		}
		spin_lock(&(pdata->w_index_lock));
        	write_index = pdata->log_write_index;
	        spin_unlock(&(pdata->w_index_lock));
	}

	ret = mutex_lock_interruptible(&(pdata->log_read_mutex));
	if (ret) {
		clr_euid_by_euid(pdata,current->pid);
                return -EINVAL;
	}

	spin_lock(&(pdata->w_index_lock));
	write_index = pdata->log_write_index;
	spin_unlock(&(pdata->w_index_lock));

	write_index = (uint32_t)roundup(write_index, 32);

	if (write_index < log_size) {
		if (read_index > write_index) {
			read_index = 0;
		}
		need_read = write_index - read_index;
		p = (uint8_t *)(log_addr_va + read_index);
		(void)log_memcpy(buf, p, need_read);
		len += need_read;
	} else {
		if ((write_index - read_index) < log_size) {
			if ((write_index / log_size) == (read_index / log_size)) {
				pos = (read_index) % log_size;
				p = (uint8_t *)(log_addr_va + pos);
				need_read = write_index - read_index;
				len += need_read;
				(void)log_memcpy(buf, p, need_read);
			} else {
				pos = (read_index) % log_size;
				p = (uint8_t *)(log_addr_va + pos);

				need_read=log_size - (read_index % log_size);
				len += need_read;
				(void)log_memcpy(buf, p, need_read);

				buftmp = buf;
                                buftmp = buftmp + need_read;
                                need_read = write_index % log_size;
                                if (need_read != 0u) {
                                        p = (uint8_t *)(log_addr_va);
                                        (void)log_memcpy(buftmp, p, need_read);
                                        //*(buf+need_read-1)='\0';
                                        len += need_read;
                                }
			}
		} else {
			pos = (write_index) % log_size;
			p = (uint8_t *)(log_addr_va + pos);
			need_read = log_size - (write_index % log_size);
			len += need_read;
			(void)log_memcpy(buf, p, need_read);
			buf = buf + need_read;
			need_read = write_index % log_size;
			if (need_read != 0u) {
				p = (uint8_t *)(log_addr_va);
				(void)log_memcpy(buf, p, need_read);
				//*(buf+need_read-1)='\0';
				len += need_read;
			}
		}
	}

	mutex_unlock(&(pdata->log_read_mutex));

	set_read_index_by_euid(pdata, current->pid, write_index);

        if (signal_pending(current)) {
                clr_euid_by_euid(pdata, current->pid);
        }

        if(log_size == len){
                len -= 1u;
        }

	return len;
}

static void hobot_dsp_rproc_kick(struct rproc *rproc, int32_t vqid)
{
	struct hobot_rproc_pdata *pdata = rproc->priv;
	wmb();
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	pdata->ipc_ops->trigger_interrupt(pdata);
}

static void *hobot_dsp_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct rproc_mem_entry *mem = NULL;
	void *va = NULL;
	pr_debug("%s find 0x%llx:0x%lx\n", __func__, da, len);
	// find suitable mem entry
	list_for_each_entry(mem, &rproc->mappings, node) {/*PRQA S 2810, 0497*//*kernel function*/
		int64_t offset = da - mem->da;

		if (offset < 0)
			continue;

		if ((size_t)offset + len > mem->len)
			continue;
		pr_debug("%s find mem->da/pa/va = 0x%x:0x%llx:%p len = 0x%lx, is_iomem: %d\n",
				__func__, mem->da, mem->dma, mem->va, mem->len, mem->is_iomem);
		va = mem->va + offset;

		if (is_iomem)
			*is_iomem = mem->is_iomem; // todo now why sram need memcpy_toio rather than memcpy

		break;
	}

	pr_debug("convert: 0x%llx --> 0x%px\n", da, va);
	return va;
}

#define IPC_REMOTEPROC_LOG_INSTANCE 1

static int32_t hobot_dsp_log_open_instance(struct hobot_rproc_pdata *pdata) {
	int32_t ret = 0;
	int32_t instance = IPC_REMOTEPROC_LOG_INSTANCE;
	struct ipc_instance_cfg *ipc_cfg = pdata->ipc_cfg;
	struct ipc_channel_info *chan;

	ipc_cfg->timeout = 100;
	ipc_cfg->trans_flags = SYNC_TRANS | SPIN_WAIT;
	ipc_cfg->mbox_chan_idx = 0;
	if (ipc_cfg->mode == 0) {
		ipc_cfg->info.def_cfg.userdata = (uint8_t *)pdata;
		ipc_cfg->info.def_cfg.recv_callback = hobot_log_handler;
	} else {
		for (int i = 0; i < ipc_cfg->info.custom_cfg.num_chans; i++) {
			chan = ipc_cfg->info.custom_cfg.chans + i;
			chan->recv_callback = hobot_log_handler;
			chan->userdata = (uint8_t *)pdata;
		}

	}

	ret = hb_ipc_open_instance(instance, ipc_cfg);
        if (ret) {
                dev_err(pdata->dev, "hb_ipc_open_instance failed\n");
        }

	return ret;
}

static int32_t hobot_dsp_ipc_wrapper_open_instance(struct hobot_rproc_pdata *pdata) {
	ipc_wrapper_data_t *smf_wrapper_data = pdata->smf_wrapper_data;

	return ipc_wrapper_open_instance(smf_wrapper_data->ipc_cfg);
}

static int32_t hobot_dsp_ipc_wrapper_close_instance(void) {
	return ipc_wrapper_close_instance();
}

static int32_t hobot_dsp_ipc_open_instance(struct hobot_rproc_pdata *pdata) {
	int32_t ret = 0;

	//log ipc
	ret = hobot_dsp_log_open_instance(pdata);
	if (ret < 0) {
		return ret;
	}

	//ipc wrapper
	ret = hobot_dsp_ipc_wrapper_open_instance(pdata);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

static int32_t hobot_dsp_ipc_close_instance(void) {
	int32_t ret = 0;

	ret = hb_ipc_close_instance(IPC_REMOTEPROC_LOG_INSTANCE);
	if (ret < 0) {
		pr_err("%s close instance%d failed\n", __func__, IPC_REMOTEPROC_LOG_INSTANCE);
	}

	ret = hobot_dsp_ipc_wrapper_close_instance();

	return ret;
}

static int32_t hobot_dsp_rproc_pre_load(struct rproc *rproc)
{
	int32_t ret;
	struct hobot_rproc_pdata *pdata = rproc->priv;
	struct rproc_mem_entry *mem = NULL;
	pr_debug("%s\n", __FUNCTION__);// dump_stack();

#ifdef CONFIG_HOBOT_REMOTEPROC_PM
	ret = hobot_remoteproc_pm_ctrl(pdata, 0, 1);
	if (ret < 0) {
		dev_err(pdata->dev, "HIFI pm start error\n");
		return ret;
	}
#endif

	ret = hobot_dsp_ipc_open_instance(pdata);
	if (ret < 0)
		return ret;

	ret = pdata->ipc_ops->pre_load_remoteproc(pdata);
	if (ret < 0) {
		pr_err("pre_load_remoteproc failed\n");
		return ret;
	}
	// clear sram
	list_for_each_entry(mem, &rproc->mappings, node)/*PRQA S 2810, 0497*//*kernel function*/
		//pr_info("mem->va is 0x%px, mem->len is 0x%x, mem->da is 0x%x\n", mem->va, mem->len, mem->da);
		if(mem->da ==  0x50400000)
			memset_io(mem->va, 0, mem->len);

	return 0;
}

static int32_t hobot_dsp_rproc_pre_stop(struct rproc *rproc)
{
	struct hobot_rproc_pdata *pdata = rproc->priv;
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	pdata->ipc_ops->pre_stop_remoteproc(pdata);

	return 0;
}

/*
 * copy from remoteproc_elf_loader.c
 * Parse the firmware, and use find_table func to find the address of the resource_table within the firmware when updating the resource_table.
 */
static const void *
find_table(struct device *dev, const struct firmware *fw)
{
	const void *shdr, *name_table_shdr;
	int i;
	const char *name_table;
	struct resource_table *table = NULL;
	const u8 *elf_data = (void *)fw->data;
	u8 class = fw_elf_get_class(fw);
	size_t fw_size = fw->size;
	const void *ehdr = elf_data;
	u16 shnum = elf_hdr_get_e_shnum(class, ehdr);
	u32 elf_shdr_get_size = elf_size_of_shdr(class);
	u16 shstrndx = elf_hdr_get_e_shstrndx(class, ehdr);

	/* look for the resource table and handle it */
	/* First, get the section header according to the elf class */
	shdr = elf_data + elf_hdr_get_e_shoff(class, ehdr);
	/* Compute name table section header entry in shdr array */
	name_table_shdr = shdr + (shstrndx * elf_shdr_get_size);
	/* Finally, compute the name table section address in elf */
	name_table = elf_data + elf_shdr_get_sh_offset(class, name_table_shdr);

	for (i = 0; i < shnum; i++, shdr += elf_shdr_get_size) {
		u64 size = elf_shdr_get_sh_size(class, shdr);
		u64 offset = elf_shdr_get_sh_offset(class, shdr);
		u32 name = elf_shdr_get_sh_name(class, shdr);

		if (strcmp(name_table + name, ".resource_table"))
			continue;

		table = (struct resource_table *)(elf_data + offset);

		/* make sure we have the entire table */
		if (offset + size > fw_size || offset + size < size) {
			dev_err(dev, "resource table truncated\n");
			return NULL;
		}

		/* make sure table has at least the header */
		if (sizeof(struct resource_table) > size) {
			dev_err(dev, "header-less resource table\n");
			return NULL;
		}

		/* we don't support any version beyond the first */
		if (table->ver != 1) {
			dev_err(dev, "unsupported fw ver: %d\n", table->ver);
			return NULL;
		}

		/* make sure reserved bytes are zeroes */
		if (table->reserved[0] || table->reserved[1]) {
			dev_err(dev, "non zero reserved bytes\n");
			return NULL;
		}

		/* make sure the offsets array isn't truncated */
		if (struct_size(table, offset, table->num) > size) {
			dev_err(dev, "resource table incomplete\n");
			return NULL;
		}

		return shdr;
	}

	return NULL;
}

static int32_t hobot_dsp_resource_table_update(struct rproc *rproc, const struct firmware *fw) {
	const void *shdr;
	struct device *dev = &rproc->dev;
	struct resource_table *table = NULL;
	const u8 *elf_data = fw->data;
	size_t tablesz;
	u8 class = fw_elf_get_class(fw);
	u64 sh_offset;
	struct hobot_rproc_pdata *pdata = rproc->priv;

	shdr = find_table(dev, fw);
	if (!shdr)
		return -EINVAL;

	sh_offset = elf_shdr_get_sh_offset(class, shdr);
	table = (struct resource_table *)(elf_data + sh_offset);
	tablesz = elf_shdr_get_sh_size(class, shdr);

	for (int i = 0; i < table->num; i++) {
		int offset = table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)table + offset;
		int avail = tablesz - offset - sizeof(*hdr);
		void *_rsc = (void *)hdr + sizeof(*hdr);

		if (avail < 0) {
			dev_err(dev, "%s failed\n", __func__);
			return -EINVAL;
		}

		struct fw_rsc_devmem *rsc = _rsc;
		if (i == 0) //sram0
			rsc->pa = pdata->sram0.pa;
		else if (i == 1) //sram1
			rsc->pa = pdata->sram0.pa + (AON_SRAM1_BASE - AON_SRAM0_BASE);
		else if (i == 3) //ddr0
			rsc->pa = pdata->sram0.pa + (AON_DDR_BASE - AON_SRAM0_BASE);
		else if (i == 4) //adsp bsp
			rsc->pa = pdata->bsp.pa;
		else if (i == 5) //adsp ipc
			rsc->pa = pdata->ipc.pa;
		dev_dbg(dev, "%s da = 0x%x pa = 0x%x\n", __func__, rsc->da, rsc->pa);
	}

       return 0;
}

static int32_t hobot_dsp_elf_load_rsc_table(struct rproc *rproc, const struct firmware *fw)
{
	int32_t ret = 0;
	pr_debug("%s\n", __FUNCTION__);// dump_stack();

	ret = hobot_dsp_resource_table_update(rproc, fw);
	if (ret) {
		return ret;
	}

	ret = rproc_elf_load_rsc_table(rproc, fw);
	if (ret)
		pr_debug("%s load resource table failed, ret = %d\n", __FUNCTION__, ret);
	return ret;
}

static int32_t hobot_dsp_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	return rproc_elf_load_segments(rproc, fw);
}

static struct resource_table *hobot_dsp_elf_find_loaded_rsc_table(struct rproc *rproc,
						       const struct firmware *fw)
{
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	return rproc_elf_find_loaded_rsc_table(rproc, fw);
}

static int32_t hobot_dsp_elf_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	return rproc_elf_sanity_check(rproc, fw);
}

static uint64_t hobot_dsp_elf_get_boot_addr(struct rproc *rproc, const struct firmware *fw)
{
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	return rproc_elf_get_boot_addr(rproc, fw);
}

static int32_t hobot_dsp_handle_rsc(struct rproc *rproc, u32 rsc_type, void *_rsc,
			  int offset, int avail)
{
	struct rproc_mem_entry *mapping;
	struct device *dev = &rproc->dev;
	struct fw_rsc_devmem *rsc = _rsc;
	int ret = 0;
	dma_addr_t pa;
	uint32_t da_hi, pa_hi, da_of;

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	if (rsc_type == RSC_LITE_DEVMEM) {
		pa = rsc->pa & 0xFFFFFFFF;
		pa |= ((dma_addr_t)(rsc->reserved & X5_DDR_HIGH_MASK)) << 32;
		mapping->va = ioremap(pa, rsc->len);
		if (!mapping->va) {
			ret = -1;
			pr_err("%s ioremap_nocache error\n", __func__);
			goto out;
		}
		mapping->dma = rsc->pa;
		mapping->da = rsc->da;
		mapping->len = rsc->len;

		if (rsc->pa != rsc->da) {
			da_hi = rsc->da >> 28;
			pa_hi = rsc->pa >> 28;
			da_of = ((da_hi % 4) * 8);
			litemmu_table[da_hi / 4] = litemmu_table[da_hi / 4] & (~(0xff << da_of));
			litemmu_table[da_hi / 4] = litemmu_table[da_hi / 4] | (pa_hi << da_of);
		}

		if (rsc->flags & HOBOT_RESOURCE_IS_IOMEM)
			mapping->is_iomem = true;

		list_add_tail(&mapping->node, &rproc->mappings);

		dev_dbg(dev, "[%s]%s: da = 0x%x pa = 0x%x/0x%llx va = 0x%px len = 0x%x\n",
			__func__, rsc->name, rsc->da, rsc->pa, pa, mapping->va, rsc->len);
	} else {
		ret = -ENAVAIL;
		dev_err(dev, "%s bad rsc_type of %d\n", __func__, rsc_type);
		goto out;
	}

	return 0;

out:
	kfree(mapping);
	return ret;
}

static struct rproc_ops hobot_dsp_rproc_ops = {
	.start = hobot_dsp_rproc_start,
	.stop = hobot_dsp_rproc_stop,
	.kick = hobot_dsp_rproc_kick,
	.da_to_va = hobot_dsp_rproc_da_to_va,
	.prepare = hobot_dsp_rproc_pre_load,
	.unprepare = hobot_dsp_rproc_pre_stop,
	.log = hobot_log_show,
	.timesync = hobot_timesync,
	.handle_rsc = hobot_dsp_handle_rsc,
	.load = hobot_dsp_elf_load_segments,
	.parse_fw = hobot_dsp_elf_load_rsc_table,
	.find_loaded_rsc_table = hobot_dsp_elf_find_loaded_rsc_table,
	.sanity_check = hobot_dsp_elf_sanity_check,
	// .get_chksum = rproc_elf_get_chksum,
	.get_boot_addr = hobot_dsp_elf_get_boot_addr
};

static int32_t hifi5_interrupt_judge(struct hobot_rproc_pdata *pdata)
{
	int32_t ret = 0;
	// read from ipc directly
	return ret;
}

static int32_t hifi5_trigger_interrupt(struct hobot_rproc_pdata *pdata)
{
	// mbox_send_message
	return 0;
}

static int32_t hifi5_clear_interrupt(struct hobot_rproc_pdata *pdata)
{
	// mbox_client_txdone
	return 0;
}

static void hifi5_set_litemmu(void)
{
	int32_t i = 0;
	writel(0x0, g_hifi5_res.nonsec_reg + HIFI5_LITEMMU_EN);
	for (i = 0; i < 4; i++) {
		writel(litemmu_table[i], g_hifi5_res.nonsec_reg + HIFI5_LITEMMU_EN + 4 + i * 4);
		pr_debug("%s litemmu_table[%d] = 0x%x\n", __func__, i, litemmu_table[i]);
	}
	writel(0x1, g_hifi5_res.nonsec_reg + HIFI5_LITEMMU_EN);
}

static void hifi5_clear_litemmu(void)
{
	int32_t i = 0;
	uint32_t tmp[4] = {0x03020100, 0x07060504, 0x0B0A0908, 0x0F0E0D0C};
	writel(0x0, g_hifi5_res.nonsec_reg + HIFI5_LITEMMU_EN);
	for (i = 0; i < 4; i++) {
		litemmu_table[i] = tmp[i];
		writel(litemmu_table[i], g_hifi5_res.nonsec_reg + HIFI5_LITEMMU_EN + i * 4 + 4);
	}
}

static int32_t hifi5_start_remoteproc(struct hobot_rproc_pdata *pdata)
{
	pr_debug("%s\n", __FUNCTION__);// dump_stack();
	hifi5_set_litemmu();
	writel(HIFI5_START_RUN, g_hifi5_res.nonsec_reg + HIFI5_RUNSTALL_OFFSET);

	adsp_boot_flag = 1;
	return 0;
}

static int32_t hifi5_rproc_pre_stop(struct hobot_rproc_pdata *pdata)
{
	// pause wtd
	// mbox_send
	return 0;
}

static int32_t hifi5_release_remoteproc(struct hobot_rproc_pdata *pdata)
{
	int32_t ret = 0;
	uint32_t val = 0;

	if (g_hifi5_res.nonsec_reg)
		writel(HIFI5_STOP_RUN, g_hifi5_res.nonsec_reg + HIFI5_RUNSTALL_OFFSET);
	else
		pr_err("g_hifi5_res.nonsec_reg is not initted\n");

	if (g_hifi5_res.pmu_reg) {
		val = readl(g_hifi5_res.pmu_reg + 0x2000);
		val |= 0x100;
		writel(val, g_hifi5_res.pmu_reg + 0x2000);

		val = readl(g_hifi5_res.pmu_reg + 0x202C);
		val &= ~(uint32_t)0x1ff;
		val |= 0x100;
		writel(val, g_hifi5_res.pmu_reg + 0x202C);

		val = readl(g_hifi5_res.pmu_reg + 0x2008);
		val &= ~(uint32_t)0x1ff;
		val |= 0x100;
		writel(val, g_hifi5_res.pmu_reg + 0x2008);
	} else {
		pr_err("g_hifi5_res.pmu_reg is not initted\n");
	}

	if (g_hifi5_res.crm_reg) {
		val = readl(g_hifi5_res.crm_reg + 0x14);
		val |= 0x4;
		writel(val, g_hifi5_res.crm_reg + 0x14);
		udelay(10);
		val &= ~(uint32_t)0x4;
		writel(val, g_hifi5_res.crm_reg + 0x14);
	} else {
		pr_err("g_hifi5_res.crm_reg is not initted\n");
	}

	hifi5_clear_litemmu();

#ifdef CONFIG_HOBOT_REMOTEPROC_PM
	ret = hobot_remoteproc_pm_ctrl(pdata, 0, 0);
	if (ret < 0) {
		dev_err(pdata->dev, "HIFI pm stop error\n");
	}
#endif

	if (g_hifi5_res.pmu_reg)
		devm_iounmap(pdata->dev, g_hifi5_res.pmu_reg);

	if (g_hifi5_res.crm_reg)
		devm_iounmap(pdata->dev, g_hifi5_res.crm_reg);

	if (g_hifi5_res.nonsec_reg)
		devm_iounmap(pdata->dev, g_hifi5_res.nonsec_reg);

	if(g_hifi5_res.sec_reg)
		devm_iounmap(pdata->dev, g_hifi5_res.sec_reg);

	if (WWDT_DISABLE == pdata->is_wwdt_enable) {
		enable_irq(pdata->irq_wwdt_reset);
		pdata->is_wwdt_enable = WWDT_ENABLE;
	}

	adsp_boot_flag = 0;

	return ret;
}


static int32_t hifi5_pre_load_remoteproc(struct hobot_rproc_pdata *pdata)
{
	uint32_t val = 0;
	struct arm_smccc_res res;
#ifndef HOBOT_HIFI5_PREP_CLK
	hifi5_enable_clk(0);
#endif

	g_hifi5_res.nonsec_reg = devm_ioremap(pdata->dev, HIFI5_NONSEC_REG_BASE, HIFI5_NONSEC_REG_SIZE);
	if (!g_hifi5_res.nonsec_reg) {
		pr_err("iormap nonsec_reg failed\n");
		goto nonsec_reg_error;
	}

	g_hifi5_res.sec_reg = devm_ioremap(pdata->dev, HIFI5_SEC_REG_BASE, HIFI5_SEC_REG_SIZE);
	if (!g_hifi5_res.sec_reg) {
		pr_err("iormap sec_reg failed\n");
		goto sec_reg_error;
	}

	g_hifi5_res.crm_reg = devm_ioremap(pdata->dev, HIFI5_CRM_BASE, HIFI5_CRM_SIZE);
	if (!g_hifi5_res.crm_reg) {
		pr_err("iormap g_hifi5_res.crm_reg failed\n");
		goto crm_reg_error;
	}
	g_hifi5_res.pmu_reg = devm_ioremap(pdata->dev, AON_PMU_BASE, AON_PMU_SIZE);
	if (!g_hifi5_res.pmu_reg) {
		pr_err("iormap g_hifi5_res.pmu_reg failed\n");
		goto pmu_reg_error;
	}

	// hold stall
	writel(HIFI5_STOP_RUN, g_hifi5_res.nonsec_reg + HIFI5_RUNSTALL_OFFSET);

	// hold reset
	val = readl(g_hifi5_res.crm_reg + 0x14);
	val |= 0x18;
	writel(val, g_hifi5_res.crm_reg + 0x14);
	// set vector
	g_hifi5_res.reset_vector = HIFI5_VECTOR_BASE; //todo will get from elf
	arm_smccc_smc(HORIZON_SIP_HIFI5_VEC_SET, g_hifi5_res.reset_vector, 0, 0, 0, 0, 0, 0, &res);
	//pr_err("%s res = 0x%lx:%lx:%lx:%lx\n", __FUNCTION__, res.a0, res.a1, res.a2, res.a3);
	arm_smccc_smc(HORIZON_SIP_HIFI5_VEC_GET, 0, 0, 0, 0, 0, 0, 0, &res);
	//pr_err("%s res = 0x%lx:%lx:%lx:%lx\n", __FUNCTION__, res.a0, res.a1, res.a2, res.a3);
	if (res.a0 != g_hifi5_res.reset_vector) {
		pr_err("set hiti5 vector failed\n");
		goto power_check_err;
	}

	// release reset
	val &= ~(uint32_t)0x1818;
	writel(val, g_hifi5_res.crm_reg + 0x14);

	// pmu setting
	writel(0x00, g_hifi5_res.pmu_reg + 0x2000);
	writel(0x00, g_hifi5_res.pmu_reg + 0x2004);
	writel(0xffffffff, g_hifi5_res.pmu_reg + 0x3000);
	writel(0xffffffff, g_hifi5_res.pmu_reg + 0x3010);
	// val = readl(g_hifi5_res.pmu_reg + 0x202c);
	// if (val != 0x100000) {
	// 	pr_err("read 0x%x = 0x%x which is not 0x100000\n", AON_PMU_BASE + 0x202c, val);
	// 	goto power_check_err;
	// }

	return 0;
power_check_err:
	devm_iounmap(pdata->dev, g_hifi5_res.pmu_reg);
pmu_reg_error:
	devm_iounmap(pdata->dev, g_hifi5_res.crm_reg);
crm_reg_error:
	devm_iounmap(pdata->dev, g_hifi5_res.sec_reg);
sec_reg_error:
	devm_iounmap(pdata->dev, g_hifi5_res.nonsec_reg);
nonsec_reg_error:
	return -1;
}

static struct rproc_ipc_ops hobot_hifi5_ipc_ops = {
	.interrupt_judge = hifi5_interrupt_judge,
	.trigger_interrupt = hifi5_trigger_interrupt,
	.clear_interrupt = hifi5_clear_interrupt,
	.start_remoteproc = hifi5_start_remoteproc,
	.release_remoteproc = hifi5_release_remoteproc,
	.pre_load_remoteproc = hifi5_pre_load_remoteproc,
	.pre_stop_remoteproc = hifi5_rproc_pre_stop,
};

#if 0
void hifi5_wdg_handle(uint32_t id)
{
	pr_err("%s, id  = %d\n", __FUNCTION__, id);
	complete(&hifi5_coredump_complete);
}


static irqreturn_t hobot_remoteproc_isr(int32_t irq, void *param)
{
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)param;
	int32_t ret;

	ret = pdata->ipc_ops->interrupt_judge(pdata);
	if ((uint32_t)ret & IPI_REASON_LOG) {
		complete(&pdata->log_complete);
	}
	complete(&pdata->notify_complete);

	if ((uint32_t)ret & IPI_REASON_DSP0_WDG) {
			hifi5_wdg_handle(0);
	}

	++pdata->statistics.irq_handler_count;
	pdata->ipc_ops->clear_interrupt(pdata);

	return IRQ_HANDLED;
}
#endif

static int32_t proc_info_show(struct seq_file *m, void *v)
{
	// struct hobot_rproc_pdata *pdata =
	// (struct hobot_rproc_pdata *)(m->private);

	seq_printf(m, "hifi5_info\n");

	return 0;
}

static ssize_t proc_info_write(struct file *file, const char __user *buffer,
size_t count, loff_t *ppos)
{
	// do nothing, prevent modifing hobot_rproc_pdata

	return count;
}

static int32_t proc_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_info_show, pde_data(inode));
}

static const struct proc_ops info_ops = {
//	.owner = THIS_MODULE,
	.proc_open = proc_info_open,
	.proc_read = seq_read,
	.proc_write = proc_info_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int32_t proc_statistics_show(struct seq_file *m, void *v)
{
	struct hobot_rproc_pdata *pdata =
	(struct hobot_rproc_pdata *)(m->private);

	seq_printf(m, "hifi5_statistics\n");
	seq_printf(m, "irq_handler_count = %d\n",
	pdata->statistics.irq_handler_count);

	return 0;
}

static ssize_t proc_statistics_write(struct file *file,
const char __user *buffer, size_t count, loff_t *ppos)
{
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)
	(((struct seq_file *)file->private_data)->private);

	memset(&pdata->statistics, 0, sizeof(pdata->statistics));

	return count;
}

static int32_t proc_statistics_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_statistics_show, pde_data(inode));
}

static const struct proc_ops statistics_ops = {
//	.owner = THIS_MODULE,
	.proc_open = proc_statistics_open,
	.proc_read = seq_read,
	.proc_write = proc_statistics_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int32_t proc_node_init(struct platform_device *pdev)
{
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)(
	pdev->dev.driver_data);

	// make top_entry
	pdata->top_entry = proc_mkdir("hifi5", NULL);
	if (!pdata->top_entry) {
		pr_err("create top_entry error\n");
		goto create_top_entry_err;
	}

	// make info entry
	pdata->info_entry = proc_create_data("info", 0777,
	pdata->top_entry, &info_ops, pdata);
	if (!pdata->info_entry) {
		pr_err("create info_entry error\n");
		goto create_info_entry_err;
	}

	// make statistics entry
	pdata->statistics_entry = proc_create_data("statistics", 0777,
	pdata->top_entry, &statistics_ops, pdata);
	if (!pdata->statistics_entry) {
		pr_err("create statistics_entry error\n");
		goto create_statistics_entry_err;
	}

	return 0;
create_statistics_entry_err:
	remove_proc_entry("info", pdata->top_entry);
create_info_entry_err:
	remove_proc_entry("hifi5", NULL);
create_top_entry_err:
	return -1;
}

static void proc_node_deinit(struct platform_device *pdev)
{
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)(
	pdev->dev.driver_data);

	remove_proc_entry("statistics", pdata->top_entry);
	remove_proc_entry("info", pdata->top_entry);
	remove_proc_entry("hifi5", NULL);
}
/*
static struct resource_table hobot_rproc_empty_rsc_table = {
	.ver = 1,
	.num = 0,
};

static struct resource_table *hobot_rproc_find_rsc_table(
struct rproc *rproc, const struct firmware *fw, int32_t *tablesz)
{
	struct hobot_rproc_pdata *pdata = rproc->priv;
	struct resource_table *rsc = NULL;

	rsc = pdata->default_fw_ops->find_rsc_table(rproc, fw, tablesz);
	if (!rsc) {
		pr_err("return empty rsc table\n");
		*tablesz = sizeof(hobot_rproc_empty_rsc_table);
		return &hobot_rproc_empty_rsc_table;
	} else
		return rsc;
}
*/
#ifndef  HOBOT_HIFI5_PREP_CLK
static int32_t parse_hifi5_clk(struct platform_device *pdev, int32_t index)
{
	hifi5_clk = devm_clk_get(&pdev->dev, "cv_hifi5_clk");
	if (IS_ERR(hifi5_clk) || (hifi5_clk == NULL)) {
			hifi5_clk = NULL;
			dev_err(&pdev->dev, "Can't get hifi5 clk\n");
			return -1;
	}
	clk_prepare(hifi5_clk);
	clk_enable(hifi5_clk);
return 0;
}
#endif
int32_t hobot_remoteproc_shutdown_hifi5(uint32_t id)
{
	if(g_hifi5_res.rproc->state != (uint32_t)RPROC_RUNNING)
		return -EINVAL;
	rproc_shutdown(g_hifi5_res.rproc);
	return 0;
}
EXPORT_SYMBOL(hobot_remoteproc_shutdown_hifi5);/*PRQA S 0307*//*kernel macro*/


int32_t hobot_remoteproc_boot_hifi5(uint32_t id)
{
	int32_t ret;
	if(g_hifi5_res.rproc->state == (uint32_t)RPROC_RUNNING)
		return -EBUSY;
	ret = rproc_boot(g_hifi5_res.rproc);
	return ret;
}
EXPORT_SYMBOL(hobot_remoteproc_boot_hifi5);/*PRQA S 0307*//*kernel macro*/

static int timesync_init(struct platform_device *pdev) {
	int ret = 0;
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)(pdev->dev.driver_data);
	u32 timesync_sec_offset;
	u32 timesync_sec_diff_offset;
	u32 timesync_nanosec_offset;
	//void __iomem *aon_sram_base_va;
	struct device_node *node;
	struct resource res;

	node = of_parse_phandle(pdev->dev.of_node, "shm-addr", 0);
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "Get adsp_bsp_reserved failed\n");
		return ret;
	}

#ifdef CONFIG_HOBOT_ADSP_CTRL
	timesync_acore_to_adsp = ioremap(res.start + TIMESYNC_ADSP_NOTIFY_OFF, 0x4);
#endif

	ret = of_property_read_u32(pdev->dev.of_node, "timesync-sec-offset", &timesync_sec_offset);
	if (ret) {
		dev_err(&pdev->dev, "get timesync-sec-offset error\n");
		return -1;
	}
	pdata->timesync_sec_reg_va = //aon_sram_base_va + (timesync_sec_reg - AON_SRAM_BASE);
		ioremap(res.start + timesync_sec_offset, 0x4);

	ret = of_property_read_u32(pdev->dev.of_node, "timesync-sec-diff-offset", &timesync_sec_diff_offset);
	if (ret) {
		dev_err(&pdev->dev, "get timesync-sec-diff-offset error\n");
		return -1;
	}

	pdata->timesync_sec_diff_reg_va = //aon_sram_base_va + (timesync_sec_diff_reg - AON_SRAM_BASE);
		ioremap(res.start + timesync_sec_diff_offset, 0x4);

	ret = of_property_read_u32(pdev->dev.of_node, "timesync-nanosec-offset", &timesync_nanosec_offset);
	if (ret) {
		dev_err(&pdev->dev, "get timesync-nanosec-offset error\n");
		return -1;
	}
	pdata->timesync_nanosec_reg_va = //aon_sram_base_va + (timesync_nanosec_reg - AON_SRAM_BASE);
		ioremap(res.start + timesync_nanosec_offset, 0x4);


	dev_dbg(&pdev->dev, "base (0x%llx) sec (0x%llx) nsec(0x%llx) diff(0x%llx)\n",
		res.start, res.start + timesync_sec_offset, res.start + timesync_sec_diff_offset,
		res.start + timesync_nanosec_offset);
	return 0;
}

static void hobot_log_handler(uint8_t *userdata, int32_t instance, int32_t chan_id,
		uint8_t *buf, uint64_t size) {
	struct hobot_rproc_pdata *pdata;
	uint32_t *ipcdata;

	if (userdata == NULL) {
		pr_err("%s:%d pdata NULL\n", __func__, __LINE__);
		return;
	}
	if (buf == NULL) {
		pr_err("%s:%d instance[%d] chan_id[%d] buf invalid NULL\n",
			__func__, __LINE__, instance, chan_id);
		return;
	}

	if (size <= 0) {
		pr_err("invalid size parameter\n");
		return;
	}

	if (hb_ipc_is_remote_ready(instance)) {
		pr_warn_ratelimited("%s:%d hb_ipc_is_remote_ready error\n", __func__, __LINE__);
		return;
	}
	pr_debug("userdata(%px) instance %d, chan_id %d, buf(%px), size %lld\n",
			pdata, instance, chan_id, buf, size);
	pdata = (struct hobot_rproc_pdata *)userdata;
	ipcdata = (uint32_t *)buf;
	spin_lock(&(pdata->w_index_lock));
	pdata->log_write_index = *ipcdata;
	spin_unlock(&(pdata->w_index_lock));

	dev_dbg(&pdata->rproc->dev, "%s:%d instance[%d] chan_id[%d] write_index %d\n", __func__, __LINE__,
			instance, chan_id, pdata->log_write_index);
	(void)hb_ipc_release_buf(instance, chan_id, buf);
	complete(&pdata->completion_log);
}

static int32_t log_init(struct platform_device *pdev) {
	int32_t ret = 0;
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)(pdev->dev.driver_data);
	u32 log_offset;
	u32 log_size;
	u32 log_write_index_offset;
	u32 log_read_index_reg;
	struct device_node *ipc_np;
	struct platform_device *ipc_pdev;
	struct ipc_dev_instance *ipc_dev;
	struct device_node *node;
	struct resource res;

	node = of_parse_phandle(pdev->dev.of_node, "shm-addr", 0);
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "Get adsp_bsp_reserved failed\n");
		return ret;
	}
	ipc_np = of_parse_phandle(pdev->dev.of_node, "remoteproc-ipc", 0);
	if (!ipc_np) {
		dev_err(&pdev->dev, "get remoteproc-ipc error\n");
		return -1;
	}
	ipc_pdev = of_find_device_by_node(ipc_np);
	if (!ipc_pdev) {
		dev_err(&pdev->dev, "find remoteproc-ipc error\n");
		return -1;
	}
	ipc_dev = (struct ipc_dev_instance *)platform_get_drvdata(ipc_pdev);
	pdata->ipc_cfg = &ipc_dev->ipc_info;

	ret = of_property_read_u32(pdev->dev.of_node, "log-offset", &log_offset);
	if (ret) {
		dev_err(&pdev->dev, "get log-offset error\n");
		return -1;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "log-size", &log_size);
	if (ret) {
		dev_err(&pdev->dev, "get log-size error\n");
		return -1;
	}

	pdata->log_addr_va = ioremap(res.start + log_offset, log_size);
	if (pdata->log_addr_va == NULL) {
		dev_err(&pdev->dev, "ioremap log_addr error\n");
		return -1;
	}

	pdata->log_size = log_size;

	ret = of_property_read_u32(pdev->dev.of_node, "log-write-index-offset",
		&log_write_index_offset);
	if (ret) {
		dev_err(&pdev->dev, "get log-write-index-offset error\n");
		return -1;
	}
	pdata->log_write_index_reg_va = //aon_sram_base_va + (log_write_index_reg - AON_SRAM_BASE);
		ioremap(res.start + log_write_index_offset, 0x4);
	(void)memset(pdata->log_write_index_reg_va, 0, 0x4);

	log_read_index_reg = res.start + log_write_index_offset;
	pdata->log_read_index_reg_va = //aon_sram_base_va + (log_read_index_reg - AON_SRAM_BASE);
		ioremap(log_read_index_reg, 0x4);

	init_completion(&pdata->completion_log);
	spin_lock_init(&(pdata->r_index_lock));
	spin_lock_init(&(pdata->w_index_lock));

	dev_dbg(&pdev->dev, "start(0x%llx) offset(0x%llx) index(0x%llx)\n",
			res.start, res.start + log_offset, res.start + log_write_index_offset);
	return 0;
}

static ssize_t hb_store_remoteproc_fw_dump(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, const char *buf, size_t count)/*PRQA S ALL*/
{
	loff_t pos;
	uint64_t i = 0;
	void *va = NULL;
	struct file *fp = NULL;
	uint32_t cmd = 0;
	uint32_t val;
	int32_t ret = 0;
	uint32_t read_size = 0;
	struct arm_smccc_res res;

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection.\n", __FUNCTION__);
		return ret;
	}
	if (cmd == 1) {
		va = ioremap(HIFI5_VECTOR_BASE, 0x8000);
		if (!va) {
			ret = -1;
			pr_err("%s ioremap 0x%x error\n", __func__, HIFI5_VECTOR_BASE);
			return -1;
		}
		fp = filp_open("/tmp/adsp.dump", O_CREAT | O_TRUNC | O_WRONLY, S_IWUSR | S_IRUSR);
		if (!fp) {
			pr_err("%s filp_open failed\n", __FUNCTION__);
			return -1;
		}
		pos = fp->f_pos;

		for (i = 0; i < 0x10000; i += 4) {
			val = readl(va + i);
			kernel_write(fp, (char *)(&val), sizeof(val), &pos);
		}

		filp_close(fp, NULL);

		iounmap(va);
	} else if (cmd == 2) {
		va = ioremap(HIFI5_VECTOR_BASE, 0x8000);
		if (!va) {
			ret = -1;
			pr_err("%s ioremap 0x%x error\n", __func__, HIFI5_VECTOR_BASE);
			return -1;
		}
		fp = filp_open("/lib/firmware/adsp.bin", O_CREAT | O_TRUNC | O_RDWR, S_IWUSR | S_IRUSR);
		if (!fp) {
			pr_err("%s filp_open failed\n", __FUNCTION__);
			return -1;
		}
		pos = fp->f_pos;

		for (i = 0; i < 0x10000; i++) {
			read_size = kernel_read(fp, (char *)(&val), 1, &pos);
			if (read_size != 1) {
				break;
			}
			writeb((char)val, va + i);
		}

		filp_close(fp, NULL);

		iounmap(va);
	} else if (cmd == 3) {
		g_hifi5_res.reset_vector = HIFI5_VECTOR_BASE; //todo will get from elf
		arm_smccc_smc(HORIZON_SIP_HIFI5_VEC_SET, g_hifi5_res.reset_vector, 0, 0, 0, 0, 0, 0, &res);
	} else if (cmd == 4) {
		arm_smccc_smc(HORIZON_SIP_HIFI5_VEC_GET, 0, 0, 0, 0, 0, 0, 0, &res);
		pr_info("%s hifi5 vector is 0x%lx\n", __FUNCTION__, res.a0);
	} else if (cmd == 5) {
		void __iomem * pmu_reg;
		void __iomem * crm_reg;
		crm_reg = devm_ioremap(dev, HIFI5_CRM_BASE, HIFI5_CRM_SIZE);
		if (crm_reg) {
			pr_err("%s reg[0x%x] = 0x%x\n", __FUNCTION__, HIFI5_CRM_BASE + 0x14, readl(g_hifi5_res.crm_reg + 0x14));
			iounmap(crm_reg);
		}

		pmu_reg = devm_ioremap(dev, AON_PMU_BASE, AON_PMU_SIZE);
		if (pmu_reg) {
			pr_err("%s reg[0x%x] = 0x%x\n", __FUNCTION__, HIFI5_CRM_BASE + 0x2000, readl(pmu_reg + 0x2000));
			pr_err("%s reg[0x%x] = 0x%x\n", __FUNCTION__, HIFI5_CRM_BASE + 0x2004, readl(pmu_reg + 0x2004));
			pr_err("%s reg[0x%x] = 0x%x\n", __FUNCTION__, HIFI5_CRM_BASE + 0x3000, readl(pmu_reg + 0x3000));
			pr_err("%s reg[0x%x] = 0x%x\n", __FUNCTION__, HIFI5_CRM_BASE + 0x3010, readl(pmu_reg + 0x3010));
			pr_err("%s reg[0x%x] = 0x%x\n", __FUNCTION__, HIFI5_CRM_BASE + 0x202c, readl(pmu_reg + 0x202c));
			iounmap(pmu_reg);
		}
	} else {
		pr_err("%s Not right command of %d\n", __FUNCTION__, cmd);
	}
	return count;
}

static ssize_t hb_show_remoteproc_fw_dump(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, char *buf)/*PRQA S ALL*/
{
	return 0;
}

static DEVICE_ATTR(fw_dump, 0644, hb_show_remoteproc_fw_dump, hb_store_remoteproc_fw_dump);

static uint32_t hb_get_wakeup_reason(struct hobot_rproc_pdata *pdata)
{
	return HOBOT_AUDIO_WAKEUP;
	// return HOBOT_GPIO_WAKEUP;
}

static uint32_t hb_get_sleep_mode(struct hobot_rproc_pdata *pdata)
{
	return pdata->sleep_mode; // maybe get from register HOBOT_LITE_SLEEP or HOBOT_DEEP_SLEEP
	// return HOBOT_LITE_SLEEP;
	// return HOBOT_DEEP_SLEEP;
}

static int32_t hb_send_power_cmd(struct hobot_rproc_pdata *pdata, uint32_t cmd)
{
	int32_t ret = 0;
	uint8_t local_buf[GUA_MISC_MSG_MAX_SIZE] = {0}; // 1. no malloc and free. 2. XXX expand buf size?
	uint8_t payload_buf[GUA_MISC_MSG_MAX_SIZE] = {0};
	control_msg_s_t *msg = (control_msg_s_t*)local_buf;
	gua_audio_rpc_data_t *msg_payload = (gua_audio_rpc_data_t*)payload_buf;
	gua_audio_power_t *power = NULL;
	// uint32_t timeout = msecs_to_jiffies(500);
	msg_payload->header.group = GROUP_POWER;
	msg_payload->header.direction = 0;
	msg_payload->header.sync = 1;
	msg_payload->header.type = 2;
	msg_payload->header.priority = 1;
	msg_payload->header.serial_id = 0;
	msg_payload->header.uuid = UUID_POWER;
	power = (gua_audio_power_t *)msg_payload->body;
	power->state = cmd;
	msg_payload->header.payload_size = sizeof(*power);

	msg->header.category = SMF_AUDIO_DSP_OUTPUT_SERVICE_ID;
	msg->header.type = SMF_NOTIFICATION;
	msg->header.payload_len = sizeof(msg_payload->header) + msg_payload->header.payload_size;
	memcpy(msg->payload, msg_payload, msg->header.payload_len);

	ret = pdata->smf_wrapper_data->send_msg(&pdata->smf_wrapper_data->poster[HORIZON_SMF_POWER_CHAN],
										(void*)msg, msg->header.payload_len + sizeof(msg->header));
	// if (wait_for_completion_timeout(&pdata->smf_power_comp, timeout) == 0u) {
	// 	pr_err("%s send power cmd(%d) timeout\n", __FUNCTION__, cmd);
	// 	return -EAGAIN;
	// }
	return 0;
}

static int32_t gua_pcm_hw_params(struct hobot_rproc_pdata *pdata)
{
	struct pcm_rpmsg rpmsg;
	int ret;

	rpmsg.send_msg.header.category = GUA_AUDIO_CATE;
	rpmsg.send_msg.header.payload_len = (uint16_t)sizeof(struct pcm_rpmsg_s);
	rpmsg.send_msg.header.type = SMF_REQUEST;

	rpmsg.send_msg.param.sw_pointer = 0;
	rpmsg.send_msg.param.cmd = PCM_RX_HW_PARAM;
	rpmsg.send_msg.param.sample_rate = 16000;

	rpmsg.send_msg.param.format = AUDIO_FORMAT_I_S16LE;
	// rpmsg.send_msg.param.channels = AUDIO_CHANNEL_STEREO; // NO effect for now
	rpmsg.send_msg.param.pcm_device = HIFI5_LOW_POWER_DEV;

	ret = pdata->smf_wrapper_data->send_msg(&pdata->smf_wrapper_data->poster[HORIZON_SMF_POWER_CHAN],
											(void *)&rpmsg.send_msg, sizeof(struct pcm_rpmsg_s));

	return ret;
}

static int32_t hobot_remoteproc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)platform_get_drvdata(pdev);
	int32_t ret;
	uint32_t mode;
	mode = hb_get_sleep_mode(pdata);
	if (adsp_boot_flag == 0u)
		return 0;

	if (mode == HOBOT_LITE_SLEEP && pdata->wakeup_status != HOBOT_AUDIO_WAKEUP_FAILED) {
		device_wakeup_enable(dev);
		ret = hb_send_power_cmd(pdata, AUDIO_POWER_TO_LOW_POWER);
		if (ret < 0) {
			dev_err(dev, "send power cmd error\n");
			goto err;
		}
		ret = gua_pcm_hw_params(pdata);
		if (ret < 0) {
			dev_err(dev, "gua_pcm_hw_params error\n");
			goto err;
		}

		ret = sync_pipeline_state(HIFI5_LOW_POWER_DEV, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_OPEN);
		if (ret < 0) {
			dev_err(dev, "ssf sync_pipeline_state error\n");
			goto err;
		}

		ret = sync_pipeline_state(HIFI5_LOW_POWER_DEV, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_START);
		if (ret < 0) {
			dev_err(dev, "ssf sync_pipeline_state error\n");
			goto err;
		}
	} else if (mode == HOBOT_DEEP_SLEEP) {
		clk_disable_unprepare(pdata->clk);
	}

	dev_info(dev, "Exit %s\n", __func__);
	pdata->sleep_mode = mode;
	return 0;
err:
	pdata->sleep_mode = mode;
	return -EBUSY;
}

static int32_t hobot_remoteproc_close_adsp_power_dev(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)platform_get_drvdata(pdev);
	int32_t ret = 0;

	ret = sync_pipeline_state(HIFI5_LOW_POWER_DEV, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_STOP);
	if (ret < 0) {
		pr_err("ssf sync_pipeline_state PCM_RX_STOP error\n");
		return ret;
	}

	ret = sync_pipeline_state(HIFI5_LOW_POWER_DEV, SNDRV_PCM_STREAM_CAPTURE, PCM_RX_CLOSE);
	if (ret < 0) {
		pr_err("ssf sync_pipeline_state  PCM_RX_CLOSE error\n");
		return ret;
	}

	ret = hb_send_power_cmd(pdata, AUDIO_POWER_TO_NORMAL);
	if (ret < 0) {
		pr_err("send power cmd error\n");
		return ret;
	}

	dev_info(dev, "Exit %s\n", __func__);
	return 0;
}

static int32_t hobot_remoteproc_resume(struct device *dev)
{
	int32_t ret;
	struct platform_device *pdev = to_platform_device(dev);
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)platform_get_drvdata(pdev);
	uint32_t reason, mode;
	mode = hb_get_sleep_mode(pdata);
	reason = hb_get_wakeup_reason(pdata);
	pdata->sleep_mode = mode;
	if (adsp_boot_flag == 0u)
		return 0;

	if (reason == HOBOT_GPIO_WAKEUP && mode == HOBOT_LITE_SLEEP) {
		ret = hobot_remoteproc_close_adsp_power_dev(dev);
		if (ret < 0) {
			dev_err(dev, "ssf sync_pipeline_state PCM_RX_STOP error\n");
			return -EBUSY;
		}
	} else if (reason != HOBOT_AUDIO_WAKEUP && mode == HOBOT_DEEP_SLEEP) {
		clk_prepare_enable(pdata->clk);
	} else if (reason == HOBOT_AUDIO_WAKEUP && mode == HOBOT_LITE_SLEEP) {
		device_wakeup_disable(dev);
	}

	return 0;
}

static ssize_t hb_store_remoteproc_suspend(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, const char *buf, size_t count)/*PRQA S ALL*/
{
	uint32_t cmd = 0;
	int32_t ret = 0;

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection.\n", __FUNCTION__);
		return ret;
	}
	if (cmd == 1) {
		hobot_remoteproc_suspend(dev);
	} else if (cmd == 2) {
		hobot_remoteproc_resume(dev);
	} else {
		pr_err("%s Not right command of %d\n", __FUNCTION__, cmd);
	}
	return count;
}

static ssize_t hb_show_remoteproc_suspend(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, char *buf)/*PRQA S ALL*/
{
	return 0;
}

static DEVICE_ATTR(suspend_test, 0644, hb_show_remoteproc_suspend, hb_store_remoteproc_suspend);

static ssize_t hb_store_wakeup_status(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, const char *buf, size_t count)/*PRQA S ALL*/
{
	uint32_t cmd = 0;
	int32_t ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct hobot_rproc_pdata *pdata = NULL;
	pdata = platform_get_drvdata(pdev);

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		pr_err("%s Invalid Injection.\n", __FUNCTION__);
		return ret;
	}
	if (cmd == 1) {
		pdata->wakeup_status = HOBOT_AUDIO_WAKEUP_SUCCEED;
		if (pdata->sleep_mode == HOBOT_LITE_SLEEP && adsp_boot_flag == 1u) {
			ret = hobot_remoteproc_close_adsp_power_dev(dev);
			if (ret)
				return ret;
		}
	} else if (cmd == 2) {
		pdata->wakeup_status = HOBOT_AUDIO_WAKEUP_FAILED;
	} else if (cmd == 3) {
		pdata->sleep_mode = HOBOT_LITE_SLEEP;
	} else if (cmd == 4) {
		pdata->sleep_mode = HOBOT_DEEP_SLEEP;
	} else {
		pr_err("%s Not right command of %d\n", __FUNCTION__, cmd);
	}
	return count;
}

static ssize_t hb_show_wakeup_status(struct device *dev,/*PRQA S ALL*/
	struct device_attribute *attr, char *buf)/*PRQA S ALL*/
{
	pr_info("================================\n");
	pr_info("1: audio wakeup succeed\n");
	pr_info("2: audio wakeup failed\n");
	pr_info("3: set remoteproc sleep mode as lite sleep\n");
	pr_info("4: set remoteproc sleep mode as deep sleep\n");
	pr_info("================================\n");
	return 0;
}

static DEVICE_ATTR(wakeup_status, 0644, hb_show_wakeup_status, hb_store_wakeup_status);

static ssize_t hb_store_loglevel(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count) {
	struct hobot_rproc_pdata *pdata = dev_get_drvdata(dev);
	int32_t ret = 0;
	uint32_t value;
	uint8_t *send_buf;
	int32_t size = 16;
	int32_t instance = 1;
	int32_t channel = 2;

	ret = kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(pdata->dev, "%s Invalid Injection.\n", __func__);
		return ret;
	}

	if (value > DBG_TYPE_LEVEL) {
		dev_err(pdata->dev, "%s Invalid log value %d\n", __func__, value);
		return -EINVAL;
	}

	sprintf(pdata->loglevel, "%s%d", DSP_LOGLEVEL, value);

	ret = hb_ipc_acquire_buf(instance, channel, size, &send_buf);
	if (ret < 0) {
		dev_err(pdata->dev, "%s ipc acquire buffer failed\n", __func__);
		return ret;
	}

	strcpy(send_buf, pdata->loglevel);

	ret = hb_ipc_send(instance, channel, send_buf, size);
	if (ret < 0) {
		dev_err(pdata->dev, "%s ipc send buffer failed\n", __func__);
		return ret;
	}

	return strnlen(buf, count);
}

static ssize_t hb_show_loglevel(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct hobot_rproc_pdata *pdata = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pdata->loglevel), "%s\n", pdata->loglevel);
}

static DEVICE_ATTR(loglevel, 0644, hb_show_loglevel, hb_store_loglevel);

static ssize_t hb_store_dspthread(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count) {
	struct hobot_rproc_pdata *pdata = dev_get_drvdata(dev);
	int32_t ret = 0;
	int32_t size = 16;
	uint8_t *send_buf;
	int32_t instance = 1;
	int32_t channel = 1;

	sprintf(pdata->thread_status, "%s", buf);

	if (strcmp(pdata->thread_status, "on") && strcmp(pdata->thread_status, "off")) {
		dev_err(pdata->dev, "%s Invalid thread_status %s\n", __func__, pdata->thread_status);
		return -EINVAL;
	}

	if (strcmp(pdata->thread_status, "on") == 0) {
		ret = hb_ipc_acquire_buf(instance, channel, size, &send_buf);
		if (ret < 0) {
			dev_err(pdata->dev, "%s ipc acquire buffer failed\n", __func__);
			return ret;
		}

		strcpy(send_buf, "thread_on");

		ret = hb_ipc_send(instance, channel, send_buf, size);
		if (ret < 0) {
			dev_err(pdata->dev, "%s ipc send buffer failed\n", __func__);
			return ret;
		}
	} else {
		ret = hb_ipc_acquire_buf(instance, channel, size, &send_buf);
		if (ret < 0) {
			dev_err(pdata->dev, "%s ipc acquire buffer failed\n", __func__);
			return ret;
		}

		strcpy(send_buf, "thread_off");

		ret = hb_ipc_send(instance, channel, send_buf, size);
		if (ret < 0) {
			dev_err(pdata->dev, "%s ipc send buffer failed\n", __func__);
			return ret;
		}
	}

	return strnlen(buf, count);
}

static ssize_t hb_show_dspthread(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct hobot_rproc_pdata *pdata = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pdata->thread_status), "%s\n", pdata->thread_status);
}

static DEVICE_ATTR(dspthread, 0644, hb_show_dspthread, hb_store_dspthread);

static ssize_t hb_show_hifi5_clk_rate(struct device *dev, struct device_attribute *attr, char *buf) {
	struct platform_device *pdev = to_platform_device(dev);
	struct hobot_rproc_pdata *pdata = platform_get_drvdata(pdev);

	return sprintf(buf, "Get real hifi5 clk rate: %ldHz\n", clk_get_rate(pdata->clk));
}

static ssize_t hb_store_hifi5_clk_rate(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	uint32_t cmd = 0;
	int32_t ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct hobot_rproc_pdata *pdata = platform_get_drvdata(pdev);
	uint64_t hifi5_round_rate;

	ret = kstrtouint(buf, 10, &cmd);
	if (ret) {
		dev_err(pdata->dev, "%s Invalid Injection. %d\n", __func__, ret);
		return ret;
	}

	hifi5_round_rate = (uint64_t)clk_round_rate(pdata->clk, cmd);
	ret = clk_set_rate(pdata->clk, hifi5_round_rate);
	if (ret) {
		dev_err(pdata->dev, "set %lld hifi5 clk rate failed\n", hifi5_round_rate);
		return ret;
	}

	dev_info(pdata->dev, "hifi5_round_rate %lld hifi5_clk_rate %d\n", hifi5_round_rate, cmd);

	return count;
}

static DEVICE_ATTR(hifi5_clk_rate, 0644, hb_show_hifi5_clk_rate, hb_store_hifi5_clk_rate);

static int32_t hobot_smf_power_recv_cb(uint8_t *payload, uint32_t payload_size, void *priv)
{
	gua_audio_rpc_data_t *msg = NULL;
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)priv;
	dev_dbg(NULL, "%s\n", __FUNCTION__);
	if (payload == NULL) {
		dev_err(NULL, "wrapper recv cb msg NULL\n");
		return -EINVAL;
	}
	msg = (gua_audio_rpc_data_t*)&(((struct ipc_wrapper_rpmsg_r *)payload)->body);
	if (msg == NULL) {
		dev_err(NULL, "wrapper recv cb msg body NULL\n");
		return -EINVAL;
	}
	if (msg->header.group != GROUP_CONTROL && msg->header.group != GROUP_POWER) {
		dev_dbg(NULL, "%s group:%d\n", __func__, msg->header.group);
		return -EINVAL;
	}
	dev_dbg(NULL, "%s get response from smf\n", __func__);

	complete(&pdata->smf_power_comp);

	return 0;
}

static int32_t hobot_remoteproc_smf_init(struct platform_device *pdev)
{
	struct device_node *smf_power_np = NULL;
	struct platform_device *smf_power_pdev = NULL;
	struct hobot_rproc_pdata *pdata = NULL;
	pdata = platform_get_drvdata(pdev);
	pdata->smf_wrapper_in_use = 0;
	smf_power_np = of_parse_phandle(pdev->dev.of_node, "msg-wrapper", 0);
	if (IS_ERR_OR_NULL(smf_power_np)) {
		dev_info(&pdev->dev, "audio wrapper not used\n");
		return 0;
	}
	smf_power_pdev = of_find_device_by_node(smf_power_np);
	if (IS_ERR_OR_NULL(smf_power_pdev)) {
		dev_err(&pdev->dev, "failed to find audio wrapper device\n");
		return -EINVAL;
	}
	pdata->smf_wrapper_in_use = 1;
	pdata->smf_wrapper_data = (ipc_wrapper_data_t *)platform_get_drvdata(smf_power_pdev);
	if (IS_ERR_OR_NULL(pdata->smf_wrapper_data)) {
		dev_err(&pdev->dev, "failed to get audio wrapper data\n");
		return -EINVAL;
	}

	pdata->smf_wrapper_data->poster[HORIZON_SMF_POWER_CHAN].priv_data[2] = pdata;
	pdata->smf_wrapper_data->poster[HORIZON_SMF_POWER_CHAN].recv_msg_cb[2] = hobot_smf_power_recv_cb;

	init_completion(&pdata->smf_power_comp);
	return 0;
}


static void hobot_remoteproc_coredump_work(struct work_struct *work) {
	struct file *fp[4];
	struct timespec64 current_time;
	struct tm tm;
	char file_name[4][128];
	struct hobot_rproc_pdata *pdata = container_of(work, struct hobot_rproc_pdata, work_coredump);
	loff_t pos;
	uint32_t i;
	ssize_t wsize;

	while (1) {
		if (wait_for_completion_interruptible(&pdata->completion_coredump) < 0) {
			break;
		}

		ktime_get_real_ts64(&current_time);
		time64_to_tm(current_time.tv_sec, 0, &tm);
		for (i = 0; i < 4; i++) {
			snprintf(file_name[i], sizeof(file_name[i]),
				"/userdata/log/coredump/dsp_%s_%04ld-%02d-%02d-%02d-%02d-%02d.hex",
				ramdump_name[i], tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);

			fp[i] = filp_open(file_name[i], O_CREAT | O_TRUNC | O_WRONLY, S_IWUSR | S_IRUSR);
			if (IS_ERR(fp[i])) {
				dev_err(&pdata->rproc->dev, "filp_open %s failed\n", file_name[i]);
				return;
			}

			pos = fp[i]->f_pos;
			wsize = kernel_write(fp[i], pdata->mem_reserved[i], pdata->mem_reserved_size[i],
					&pos);
			fp[i]->f_pos = pos;

			filp_close(fp[i], NULL);
		}

		//dsp reset
		reset_control_assert(pdata->rst);
	}
}

static irqreturn_t dsp_wdt_isr(int32_t irq, void *param) {
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)param;
	pdata->is_wwdt_enable = WWDT_DISABLE;
	disable_irq_nosync(irq);
	complete(&pdata->completion_coredump);
	dev_info(&pdata->rproc->dev, "%s interrupt\n", __func__);

	return IRQ_HANDLED;
}

static int32_t irq_init(struct platform_device *pdev) {
	struct hobot_rproc_pdata *pdata = platform_get_drvdata(pdev);
	int32_t ret = 0;

	pdata->irq_wwdt_reset = platform_get_irq(pdev, 0);
	if (pdata->irq_wwdt_reset < 0) {
		dev_err(&pdev->dev, "platform_get_irq irq_wwdt_reset error\n");
		return -EINVAL;
	}

	pdata->wq_coredump = create_singlethread_workqueue("dsp_dump_wq");
	if (!pdata->wq_coredump) {
		dev_err(&pdev->dev, "create_singlethread_workqueue error\n");
		return -EINVAL;
	}

	init_completion(&pdata->completion_coredump);
	INIT_WORK(&(pdata->work_coredump), hobot_remoteproc_coredump_work);

	if (queue_work(pdata->wq_coredump, &pdata->work_coredump) == false) {
		dev_err(&pdev->dev, "queue_work dsp coredump error\n");
		return -EINVAL;
	}

	ret = devm_request_irq(&pdev->dev, (uint32_t)pdata->irq_wwdt_reset,
			dsp_wdt_isr, IRQF_SHARED, "dsp_wwdt_irq", pdata);
	if (ret) {
		dev_err(&pdev->dev, "devm_request_irq dsp_wwdt_irq error\n");
		return -EINVAL;
	}
	pdata->is_wwdt_enable = WWDT_ENABLE;
	return ret;
}

static int32_t parse_reserve_mem(struct platform_device *pdev) {
	int32_t ret = 0;

	struct device_node *node;
        struct resource res;
	u32 dsp_ddr_offset;
	struct hobot_rproc_pdata *pdata = platform_get_drvdata(pdev);

	ret = of_property_read_u32_index(pdev->dev.of_node, "dsp_iram0_map_addr", 0,
			&(pdata->dsp_iram0_map_addr));
	if (ret) {
		dev_err(&pdev->dev, "get dsp_iram0_map_addr failed\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "dsp_iram0_map_addr", 1,
                        &(pdata->dsp_iram0_size));
        if (ret) {
                dev_err(&pdev->dev, "get dsp_iram0_map_addr size failed\n");
                return -EINVAL;
        }

	ret = of_property_read_u32_index(pdev->dev.of_node, "dsp_dram0_map_addr", 0,
			&(pdata->dsp_dram0_map_addr));
	if (ret) {
		dev_err(&pdev->dev, "get dsp_dram0_map_addr failed\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "dsp_dram0_map_addr", 1,
                        &(pdata->dsp_dram0_size));
        if (ret) {
                dev_err(&pdev->dev, "get dsp_dram0_map_addr size failed\n");
                return -EINVAL;
        }

	ret = of_property_read_u32_index(pdev->dev.of_node, "dsp_dram1_map_addr", 0,
                        &(pdata->dsp_dram1_map_addr));
        if (ret) {
                dev_err(&pdev->dev, "get dsp_dram1_map_addr failed\n");
                return -EINVAL;
        }

        ret = of_property_read_u32_index(pdev->dev.of_node, "dsp_dram1_map_addr", 1,
                        &(pdata->dsp_dram1_size));
        if (ret) {
                dev_err(&pdev->dev, "get dsp_dram1_map_addr size failed\n");
                return -EINVAL;
        }

	node = of_parse_phandle(pdev->dev.of_node, "dsp_ddr", 0);
        ret = of_address_to_resource(node, 0, &res);
        if (ret) {
                dev_err(&pdev->dev, "Get adsp_ddr failed\n");
                return ret;
        }
	ret = of_property_read_u32(pdev->dev.of_node, "dsp_ddr_offset", &dsp_ddr_offset);
	if (ret) {
		dev_err(&pdev->dev, "get ddr-offset error\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "dsp_ddr_size", &pdata->dsp_ddr_size);
	if (ret) {
		dev_err(&pdev->dev, "get ddr-size error\n");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "iram0 0x%x dram0 0x%x dram1 0x%x ddr 0x%llx\n",
		pdata->dsp_iram0_map_addr, pdata->dsp_dram0_map_addr,
		pdata->dsp_dram1_map_addr, res.start + dsp_ddr_offset);
	pdata->mem_reserved[0] = ioremap_wc(res.start + dsp_ddr_offset,
			pdata->dsp_ddr_size);
	pdata->mem_reserved[1] = ioremap_wc(pdata->dsp_iram0_map_addr,
			pdata->dsp_iram0_size);
	pdata->mem_reserved[2] = ioremap_wc(pdata->dsp_dram0_map_addr,
			pdata->dsp_dram0_size);
	pdata->mem_reserved[3] = ioremap_wc(pdata->dsp_dram1_map_addr,
			pdata->dsp_dram1_size);

	if (!pdata->mem_reserved[0] || !pdata->mem_reserved[1] || !pdata->mem_reserved[2] || !pdata->mem_reserved[3]) {
		dev_err(&pdev->dev, "ioremap dsp tcm failed\n");
		return -EINVAL;
	}

	pdata->mem_reserved_size[0] = pdata->dsp_ddr_size;
	pdata->mem_reserved_size[1] = pdata->dsp_iram0_size;
	pdata->mem_reserved_size[2] = pdata->dsp_dram0_size;
	pdata->mem_reserved_size[3] = pdata->dsp_dram1_size;

	return ret;
}

static void deinit_reserve_mem(struct platform_device *pdev) {
	struct hobot_rproc_pdata *pdata = platform_get_drvdata(pdev);

	for (int i = 0; i < 4; i++) {
		if (pdata->mem_reserved[i])
			iounmap(pdata->mem_reserved[i]);
	}
}

static int32_t hobot_resource_table_init(struct platform_device *pdev) {
	struct hobot_rproc_pdata *pdata = platform_get_drvdata(pdev);
	struct device_node *node;
	struct resource res;
	int32_t ret = 0;

	node = of_parse_phandle(pdev->dev.of_node, "adsp-sram0-addr", 0);
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		return ret;
	}
	pdata->sram0.pa = res.start;

	node = of_parse_phandle(pdev->dev.of_node, "adsp-bsp-addr", 0);
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		return ret;
	}
	pdata->bsp.pa = res.start;

	node = of_parse_phandle(pdev->dev.of_node, "adsp-ipc-addr", 0);
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		return ret;
	}
	pdata->ipc.pa = res.start;

	dev_dbg(&pdev->dev, "sram0 0x%x bsp 0x%x ipc 0x%x\n", pdata->sram0.pa, pdata->bsp.pa, pdata->ipc.pa);

	return 0;
}


static int32_t hobot_remoteproc_probe(struct platform_device *pdev)
{
	int32_t device_index = 0;
	struct rproc *rproc = NULL;
	struct hobot_rproc_pdata *pdata = NULL;
	const char *clk;
	int32_t ret = 0;

	pr_info("hifi5 hobot_remoteproc_probe start\n");

	device_index = HIFI5_DEV_IDX;
#ifndef  HOBOT_HIFI5_PREP_CLK
	ret = parse_hifi5_clk(pdev, 0);
	if(ret != 0)
		return ret;
#endif
	rproc = rproc_alloc(&pdev->dev, dev_name(&pdev->dev),
						&hobot_dsp_rproc_ops, NULL,
						sizeof(struct hobot_rproc_pdata));

	if (!rproc) {
		dev_err(&pdev->dev, "rproc_alloc error\n");
		goto no_revoke_err;
	}

	pdata = rproc->priv;
	pdata->rproc = rproc;
	pdata->device_index = device_index;
	pdata->dev = &pdev->dev;
	pdata->sleep_mode = HOBOT_DEEP_SLEEP;
	pdata->wakeup_status = HOBOT_AUDIO_WAKEUP_SUCCEED;
	platform_set_drvdata(pdev, pdata);

#ifdef CONFIG_HOBOT_REMOTEPROC_PM
	ret = hobot_remoteproc_pm_ctrl(pdata, 1, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "HIFI pm enable error\n");
		return ret;
	}
#endif

	/* FIXME: it may need to extend to 64/48 bit */
	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(48));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_coherent_mask: %d\n", ret);
		goto free_rproc_out;
	}
	pdata->ipc_ops = &hobot_hifi5_ipc_ops;

	ret = timesync_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "timesync_init error\n");
		//goto deinit_irq_out;
	}

	ret = log_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "log_init error\n");
		goto deinit_irq_out;
	}

	ret = hobot_remoteproc_smf_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "log_init error\n");
		goto deinit_irq_out;
	}

	ret = hobot_resource_table_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "resource_table_init error\n");
		goto deinit_irq_out;
	}

	clk = "hifi5_clk";
	pdata->clk = devm_clk_get(&pdev->dev, clk);
	if (IS_ERR(pdata->clk))
		dev_warn(&pdev->dev, "Warning: clock not configured\n");

	clk = "hifi5_pbclk";
	pdata->p_clk = devm_clk_get(&pdev->dev, clk);
	if (IS_ERR(pdata->p_clk))
		dev_warn(&pdev->dev, "Warning: APB clock not configured\n");

	ret = clk_prepare_enable(pdata->clk);
	if (ret) {
		dev_err(&pdev->dev, "clk enabled failed\n");
		return ret;
	}

	ret = parse_reserve_mem(pdev);
	ret = irq_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "irq_init error\n");
		goto deinit_irq_out;
	}

	pdata->rst = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(pdata->rst)) {
		dev_err(&pdev->dev, "Missing reset controller\n");
		return PTR_ERR_OR_ZERO(pdata->rst);
	}

	ret = proc_node_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "proc_node_init error\n");
		goto deinit_irq_out;
	}

	// we will complete remoteproc init
	rproc->auto_boot = autoboot;
/*
	memcpy(&pdata->fw_ops, rproc->fw_ops, sizeof(pdata->fw_ops));
	pdata->fw_ops.find_rsc_table = hobot_rproc_find_rsc_table;
	pdata->default_fw_ops = rproc->fw_ops;
	rproc->fw_ops = &pdata->fw_ops;
*/
	ret = rproc_add(pdata->rproc);
	if (ret) {
		dev_err(&pdev->dev, "rproc_add error\n");
		goto deinit_proc_out;
	}
	g_hifi5_res.rproc = rproc;

	ret = device_create_file(&pdev->dev, &dev_attr_fw_dump);
	if (ret != 0) {
		dev_err(&pdev->dev, "BUG: Can not creat dump kobject\n");
		goto create_file_out; /*PRQA S ALL*/ /*qac-0.7.0-2001*/
	}

	ret = device_create_file(&pdev->dev, &dev_attr_suspend_test);
	if (ret != 0) {
		dev_err(&pdev->dev, "BUG: Can not creat dump kobject\n");
		goto create_file_out1; /*PRQA S ALL*/ /*qac-0.7.0-2001*/
	}

	ret = device_create_file(&pdev->dev, &dev_attr_wakeup_status);
	if (ret != 0) {
		dev_err(&pdev->dev, "BUG: Can not creat dump kobject\n");
		goto create_file_out2; /*PRQA S ALL*/ /*qac-0.7.0-2001*/
	}

	ret = device_create_file(&pdev->dev, &dev_attr_hifi5_clk_rate);
	if (ret != 0) {
		dev_err(&pdev->dev, "BUG: Can not create hifi5 clk rate\n");
		goto create_file_out3;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_dspthread);
	if (ret != 0) {
		dev_err(&pdev->dev, "BUG: Can not create dspthread\n");
		goto create_file_out4;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_loglevel);
	if (ret != 0) {
		dev_err(&pdev->dev, "BUG: Can not create loglevel\n");
		goto create_file_out5;
	}

#ifdef CONFIG_HOBOT_REMOTEPROC_PM
	ret = hobot_remoteproc_pm_ctrl(pdata, 0, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "HIFI pm disable error\n");
		goto create_file_out4;
	}
#endif

	device_init_wakeup(&pdev->dev, true);
	pr_info("hobot_remoteproc_probe end\n");

	return 0;
create_file_out5:
	device_remove_file(&pdev->dev, &dev_attr_loglevel);
create_file_out4:
	device_remove_file(&pdev->dev, &dev_attr_hifi5_clk_rate);
create_file_out3:
	device_remove_file(&pdev->dev, &dev_attr_wakeup_status);
create_file_out2:
	device_remove_file(&pdev->dev, &dev_attr_suspend_test);
create_file_out1:
	device_remove_file(&pdev->dev, &dev_attr_fw_dump);
create_file_out:
	rproc_del(pdata->rproc);
deinit_proc_out:
	clk_disable_unprepare(pdata->clk);
	clk_disable_unprepare(pdata->p_clk);
	proc_node_deinit(pdev);
deinit_irq_out:
	// wait for ipc porting
	// irq_deinit(pdev);
free_rproc_out:
	rproc_free(pdata->rproc);
no_revoke_err:
	return -1;
}

static int32_t hobot_remoteproc_remove(struct platform_device *pdev) {
	int32_t ret = 0;
	struct hobot_rproc_pdata *pdata = (struct hobot_rproc_pdata *)dev_get_drvdata(&pdev->dev);

	proc_node_deinit(pdev);
	device_remove_file(&pdev->dev, &dev_attr_fw_dump);
	device_remove_file(&pdev->dev, &dev_attr_suspend_test);
	device_remove_file(&pdev->dev, &dev_attr_wakeup_status);
	device_remove_file(&pdev->dev, &dev_attr_hifi5_clk_rate);
	device_remove_file(&pdev->dev, &dev_attr_dspthread);
	device_remove_file(&pdev->dev, &dev_attr_loglevel);

#ifdef CONFIG_HOBOT_ADSP_CTRL
	iounmap(timesync_acore_to_adsp);
#endif

#ifdef CONFIG_HOBOT_REMOTEPROC_PM
	ret = hobot_remoteproc_pm_ctrl(pdata, 1, 0);
	if (ret < 0) {
		dev_err(pdata->dev, "HIFI pm disable error\n");
		return ret;
	}
#endif

	rproc_free(pdata->rproc);

	deinit_reserve_mem(pdev);

	return ret;
}

static const struct dev_pm_ops hobot_remoteproc_pm_ops = {
	.suspend = hobot_remoteproc_suspend,
	.resume = hobot_remoteproc_resume,
};

static const struct of_device_id hobot_remoteproc_dt_ids[] = {
	{ .compatible = "hobot,remoteproc-hifi5", },
	{ /* sentinel */ }
};

static struct platform_driver hobot_remoteproc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "hifi5-remoteproc",
		.of_match_table = hobot_remoteproc_dt_ids,
		.pm = &hobot_remoteproc_pm_ops,
	},
	.probe = hobot_remoteproc_probe,
	.remove = hobot_remoteproc_remove,
};

static int32_t __init hobot_remoteproc_init(void)
{
	int32_t ret = 0;

	pr_info("hobot_remoteproc_init start\n");

	ret = platform_driver_register(&hobot_remoteproc_driver);
	if (ret)
		pr_err("platform_driver_register error\n");

	pr_info("hobot_remoteproc_init end\n");

	return ret;
}

MODULE_AUTHOR("Horizon Robotics, Inc");
MODULE_DESCRIPTION("Hobot remote processor device");
MODULE_LICENSE("GPL_v2");
late_initcall(hobot_remoteproc_init);/*PRQA S 0605, 0307*//*kernel function*/
