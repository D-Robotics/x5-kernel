/*
* hbi_k.c - kernel space driver for hbi over linux platform
*
* Copyright 2018 Microsemi Inc. All rights reserved.
* Licensed under the MIT License. See LICENSE.txt in the project root for license information.
*/
#include <linux/i2c.h>
#include "typedefs.h"
#include "chip.h"
#include "hbi.h"
#include "hbi_k.h"
#include "vproc_dbg.h"
#include "fwr_image_headers.h"

#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#if HBI_ENABLE_FWR_BIN
#include <linux/firmware.h>
#endif

/* Take these defines from chip specific make options instead */
#define HBI_CFGREC_MAX_SIZE     0xE00
#define HBI_CFGREC_BASE         0x200
#define HBI_LNX_BUF_MAX_LEN     (512*1024)/*TODO: can be made reconfigurable through config file */
#define HBI_CMD(dev,cmd,args)    HBI_set_command((dev->hbi_handle),cmd,args)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define list_next_entry(pos, member) \
        list_entry((pos)->member.next, typeof(*(pos)), member)
#endif

static DECLARE_BITMAP(minors, VPROC_MAX_NUM_DEVS);
int module_usage_count;

void hbi_drv_exit(void);
#if (HBI_ENABLE_PROCFS)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define DEVP(filp) (PDE(filp->f_dentry->d_inode)->data)
#else
//#define DEVP(filp)     (PDE_DATA(filp->f_inode))
#include <linux/proc_fs.h>
#define DEVP(filp)     (pde_data(filp->f_inode))
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define PROC_REMOVE(dir)      remove_proc_entry(dir->name,dir->parent);
#else
#define PROC_REMOVE(dir)      proc_remove(dir);
#endif


ssize_t hbi_proc_reg_rd_dump(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_reg_rd(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_reg_wr(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_open_dev_wr(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_open_dev_rd(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_close_dev_wr(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_wr_cfgrec(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_rd_cfgrec(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_load_fw(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_start_fw(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_save_fwrcfgrec_to_flash(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_load_fwrcfgrec_from_flash(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_erase_img_from_flash(struct file *filp, const char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_erase_flash(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_drv_init(struct file *filp, char __user *buf, size_t size, loff_t *offset);
ssize_t hbi_proc_drv_term(struct file *filp, char __user *buf, size_t size, loff_t *offset);



struct hbi_lnx_drv_proc_entries {
    const char *name;
    umode_t    mode;
    struct proc_dir_entry *proc_entry;
    //struct file_operations ops;
    struct proc_ops ops;
};
typedef enum
{
   HBI_PROC_ENTRY_OPEN_DEV,
   HBI_PROC_ENTRY_CLOSE_DEV,
   HBI_PROC_ENTRY_LAST,
   HBI_PROC_ENTRY_FIRST = HBI_PROC_ENTRY_OPEN_DEV
}HBI_PROC_ENTRIES;

typedef enum
{
    HBI_DEV_PROC_ENTRY_READ_REG,
    HBI_DEV_PROC_ENTRY_FIRST=HBI_DEV_PROC_ENTRY_READ_REG,
    HBI_DEV_PROC_ENTRY_WRITE_REG,
    HBI_DEV_PROC_ENTRY_CFGREC,
    HBI_DEV_PROC_ENTRY_LOAD_FW,
    HBI_DEV_PROC_ENTRY_START_FW,
#ifdef FLASH_PRESENT
    HBI_DEV_PROC_ENTRY_FLASH_SAVE_FWCFGREC,
    HBI_DEV_PROC_ENTRY_FLASH_LOAD_FWCFGREC,
    HBI_DEV_PROC_ENTRY_FLASG_ERASE,
#endif
    HBI_DEV_PROC_ENTRY_LAST
}HBI_DEV_PROC_ENTRIES;

/* device specific proc entries */
struct hbi_lnx_drv_proc_entries dev_proc_entry[] =
{
    {
        .name = "read_reg",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO|S_IRWXU),
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_reg_rd,       // .write -> .proc_write
            .proc_read = hbi_proc_reg_rd_dump   // .read -> .proc_read

        }
    },
    {
        .name = "write_reg",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_reg_wr,      // .write -> .proc_write

        }
    },
    {
        .name = "cfgrec",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_wr_cfgrec,   // .write -> .proc_write
            .proc_read = hbi_proc_rd_cfgrec     // .read -> .proc_read

        }
    },
    {
        .name = "load_fw",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_load_fw,     // .write -> .proc_write

        }
    },
    {
        .name = "start_fw",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_read = hbi_proc_start_fw,     // .read -> .proc_read

        }
    },
    #ifdef FLASH_PRESENT
    {
        .name = "flash_save_fwrcfgrec",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_read = hbi_proc_save_fwrcfgrec_to_flash,  // .read -> .proc_read

        }
    },
    {
        .name = "flash_load_fwrcfgrec",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_load_fwrcfgrec_from_flash,  // .write -> .proc_write

        }
    },
    {
        .name = "flash_erase",
        .mode = (S_IFREG|S_IRUGO|S_IWUGO),
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_erase_img_from_flash,  // .write -> .proc_write
            .proc_read = hbi_proc_erase_flash,            // .read -> .proc_read

        }
    },
    #endif
};


/* HBI Driver Procfs entry */
struct hbi_lnx_drv_proc_entries drv_proc_entry[] = {
    {
        .name = "open_device",
        .mode = 0,  // 如果没有特殊权限要求，设为 0
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_open_dev_wr,  // .write -> .proc_write
            .proc_read = hbi_proc_open_dev_rd    // .read -> .proc_read

        }
    },
    {
        .name = "close_device",
        .mode = 0,  // 如果没有特殊权限要求，设为 0
        .proc_entry = NULL,
        .ops = {
            .proc_write = hbi_proc_close_dev_wr,  // .write -> .proc_write

        }
    }
};
static unsigned max_rw_size=ZL380xx_MAX_ACCESS_SIZE_IN_BYTES;
struct {
   unsigned char       buf[ZL380xx_MAX_ACCESS_SIZE_IN_BYTES];/* buffer to store reg_rd transaction */
   size_t        len; /* length of data read*/
}hbi_rw;

module_param(max_rw_size,uint,S_IRUGO);
MODULE_PARM_DESC(max_rw_size,"maximum size of 1 read/write transaction\n");

#endif /* HBI_ENABLE_PROCFS */


struct dev_cfgrec
{
    int size;
};

typedef struct{
    unsigned char  buf[HBI_LNX_BUF_MAX_LEN];
    int32_t  len;
}HBI_LNX_BUF;

struct hbi_dev{
    hbi_handle_t            hbi_handle;/* HBI handle of current device */
    hbi_dev_cfg_t           devcfg;    /* user passed device configuration */
    struct list_head        list;       /* pointer to next opened device */
    struct dev_cfgrec       cfgrec;
    HBI_LNX_BUF             hbi_buf;
    struct device           *device; /* pointer to device file of this driver */
    dev_t                   t_dev;
#if (HBI_ENABLE_PROCFS)
    struct proc_dir_entry   *dev_proc_entry[HBI_DEV_PROC_ENTRY_LAST];
    struct proc_dir_entry   *dev_proc_dir;
#endif /*HBI_ENABLE_PROCFS */
};

struct hbi_lnx_drv
{
    struct list_head     list; /* pointer to list of devices */
    struct cdev         *cdev; /* pointer to HBI character device driver correspond to */
    dev_t                dev_t; /* device number of registered char devices */
    struct class        *dev_class;/* pointer to device class this driver correspond to */

#if (HBI_ENABLE_PROCFS)
    struct proc_dir_entry *drv_proc_dir;
#endif
}hbi_lnx_drv_priv;

hbi_dev_info_t hbi_dev_info[VPROC_MAX_NUM_DEVS];

#if HBI_LOAD_FWR_STATIC || HBI_LOAD_CFGREC_STATIC
static user_buffer_t image[HBI_BUFFER_SIZE];

static hbi_status_t hbi_load_staticimage_from_host(hbi_handle_t handle,uint8_t *pArg) {

   hbi_status_t   status = HBI_STATUS_SUCCESS;
   int            i;
   hbi_data_t     data;
   uint32_t       block_size;
   hbi_img_hdr_t hdr;
   size_t        fwr_len;

   data.pData=image;

   SSL_memcpy(data.pData,pArg,HBI_BUFFER_SIZE);

   /*Firmware image is organised into chunks of fixed length and this information
     is embedded in image header. Thus first read image header and
     then start reading chunks and loading on to device
   */
   data.size = HBI_BUFFER_SIZE;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Calling HBI_get_header()\n");

   status = HBI_get_header(&data,&hdr);
   if(status != HBI_STATUS_SUCCESS)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_get_header() err 0x%x \n",status);
      HBI_close(handle);
      HBI_term();
      return -1;
   }

   /* length is in unit of 16-bit words */
   block_size = (hdr.block_size)*2;
   data.size = block_size;
   fwr_len = hdr.img_len;

   if(block_size > HBI_BUFFER_SIZE)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Insufficient buffer size. please recompiled with increased HBI_BUFFER_SIZE\n");
      return HBI_STATUS_RESOURCE_ERR;
   }

   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "\nStarting image loading ...\n");

   /* Somehow direct memcpy from buffer is  giving me some memory issues.
   thus using pointer to pass on data from global buffer.
   */

   /* This loops covers all firmware loading */
   for(i=hdr.hdr_len;i<fwr_len;i+=block_size)
   {
      SSL_memcpy((void *)(data.pData),(const void *)(pArg+i),block_size);
        if (hdr.image_type == HBI_IMG_TYPE_FWR)
        {
            status = HBI_set_command(handle,HBI_CMD_LOAD_FWR_FROM_HOST,&data);
        } else if (hdr.image_type == HBI_IMG_TYPE_CR)
        {
            status = HBI_set_command(handle,HBI_CMD_LOAD_CFGREC_FROM_HOST,&data);
        } else {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Invalid image type %d\n", hdr.image_type);
            status = HBI_STATUS_INVALID_ARG;
        }

        if (status != HBI_STATUS_SUCCESS)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error %d:HBI_set_command(HBI_CMD_LOAD_FWR_FROM_HOST)\n", status);
            return status;
        }
   }
   if (hdr.image_type == HBI_IMG_TYPE_FWR)
   {
       status = HBI_set_command(handle,HBI_CMD_LOAD_FWR_COMPLETE,NULL);
       if (status != HBI_STATUS_SUCCESS) {
           VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error %d:HBI_set_command(HBI_CMD_LOAD_FWR_COMPLETE)\n", status);
           return status;
       }
   }
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Firmware loaded into Device\n");

   return status;
}
#endif /*HBI_BOOTLOAD_FWR_STATIC || HBI_LOAD_CFGREC_STATIC*/

int internal_hbi_drv_open (struct inode *node, struct file *filp)
{
    return 0;
}

static hbi_status_t internal_hbi_open(void **device,hbi_dev_cfg_t *devcfg)
{
    struct hbi_dev *dev=NULL;
    hbi_status_t      status;
    unsigned long dev_minor;
#if (HBI_ENABLE_PROCFS)
    int             i;
    uint8_t         dir_name[32];
#endif

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");


    dev = kmalloc(sizeof(struct hbi_dev),GFP_KERNEL);
    if(dev==NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Resource Error \n");
        return -1;
    }

    SSL_memset(dev,0,sizeof(struct hbi_dev));
    SSL_memcpy(&(dev->devcfg),devcfg,sizeof(hbi_dev_cfg_t));
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Opening device %d, %lld with addr : 0x%x bus num %d\n",dev->devcfg.deviceId, dev->devcfg.dev_lock, dev->devcfg.dev_addr,dev->devcfg.bus_num);

    status = HBI_open(&(dev->hbi_handle),&(dev->devcfg));
    if(status !=  HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_open failed\n");
        kfree(dev);
        return HBI_STATUS_RESOURCE_ERR;
    }

    /* create a device file (for user space apps) */
    dev_minor = find_first_zero_bit(minors, VPROC_MAX_NUM_DEVS);
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "dev_minor:%ld \n",dev_minor);
    if (dev_minor < VPROC_MAX_NUM_DEVS)
    {
        dev->t_dev = MKDEV(MAJOR(hbi_lnx_drv_priv.dev_t), dev_minor);

        dev->device = device_create(hbi_lnx_drv_priv.dev_class,
                                    NULL,dev->t_dev,dev,
                                    "%s%d", HBI_DEV_NAME,dev->devcfg.deviceId);

        if(IS_ERR(dev->device))
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Device file creation failed %ld\n", PTR_ERR(dev->device));
            HBI_close(dev->hbi_handle);
            kfree(dev);
            dev=NULL;
            return HBI_STATUS_INTERNAL_ERR;
        }
        set_bit(dev_minor, minors);
    }
    else
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Device file creation failed\n");
        HBI_close(dev->hbi_handle);
        kfree(dev);
        dev=NULL;
        return HBI_STATUS_INTERNAL_ERR;
    }

    /* Add it to the list of devices maintained by driver */
    INIT_LIST_HEAD(&(dev->list));



    /* Add it to driver device list */
    list_add(&(dev->list),&(hbi_lnx_drv_priv.list));

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "list head 0x%px\n",list_entry(&(hbi_lnx_drv_priv.list),struct hbi_dev,list));

#if (HBI_ENABLE_PROCFS)
    /* create a proc entry for each device */
    SSL_memset(dir_name,0,sizeof(dir_name));
    sprintf(dir_name,"dev_%x%x",dev->devcfg.bus_num,dev->devcfg.dev_addr);

    dev->dev_proc_dir = proc_mkdir(dir_name,hbi_lnx_drv_priv.drv_proc_dir);
    for(i=HBI_DEV_PROC_ENTRY_FIRST;i<HBI_DEV_PROC_ENTRY_LAST;i++)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Creating Device 0x%px Proc Entry %s\n",dev,dev_proc_entry[i].name);

        dev->dev_proc_entry[i] = proc_create_data(dev_proc_entry[i].name,
                                                  dev_proc_entry[i].mode,
                                                  dev->dev_proc_dir,
                                                  //&(dev_proc_entry[i].ops),
                                                  (const struct proc_ops *)&(dev_proc_entry[i].ops),
                                                  (void *)dev);
    }
#endif /* HBI_ENABLE_PROCFS */

    *((struct hbi_dev **)device) = dev;


    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return HBI_STATUS_SUCCESS;
}

static hbi_status_t internal_hbi_close(struct hbi_dev *device)
{
    hbi_status_t      status = HBI_STATUS_SUCCESS;
#if (HBI_ENABLE_PROCFS)
    int i;
#endif

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(device == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device handle passed\n");
        return HBI_STATUS_BAD_HANDLE;
    }

    if((status = HBI_close(device->hbi_handle)) != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Failed to close device\n");
        return status;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Closing Device 0x%lx\n",(unsigned long)device);

    list_del(&(device->list));

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "list head 0x%lx\n",(unsigned long)list_entry(&(hbi_lnx_drv_priv.list),struct hbi_lnx_drv,list));

    if(device->device!=NULL)
    {
        device_destroy(hbi_lnx_drv_priv.dev_class, device->t_dev);
        clear_bit(MINOR(device->t_dev), minors);
    }

#if (HBI_ENABLE_PROCFS)
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Delete proc entries \n");
    for(i=HBI_DEV_PROC_ENTRY_FIRST;i<HBI_DEV_PROC_ENTRY_LAST;i++)
    {
        if(device->dev_proc_entry[i] != NULL)
        {
            PROC_REMOVE(device->dev_proc_entry[i]);
            device->dev_proc_entry[i] = NULL;
        }
    }

    if(device->dev_proc_dir != NULL)
    {
      PROC_REMOVE(device->dev_proc_dir);
      device->dev_proc_dir = NULL;
    }
#endif /* HBI_ENABLE_PROCFS */

    kfree(device);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return status;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
long  internal_hbi_drv_ioctl(
#else
int internal_hbi_drv_ioctl(struct inode *inode,
#endif
                         struct file *filp, unsigned int cmd, unsigned long pArgs)
{
    int ret=0;
    struct hbi_dev *dev=(struct hbi_dev *)(filp->private_data);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    switch(cmd)
    {
        case HBI_UPDATE:
        {
             int i = 0;
             VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_READ called\n");
             for (i=0; i<VPROC_MAX_NUM_DEVS; i++)
             {
                 hbi_dev_info[i].chip       = sdk_devices_info[i].chip;
                 hbi_dev_info[i].dev_addr   = sdk_devices_info[i].dev_addr;
                 hbi_dev_info[i].isboot     = sdk_devices_info[i].isboot;
                 hbi_dev_info[i].bus_num    = sdk_devices_info[i].bus_num;
                 hbi_dev_info[i].dev_lock   = sdk_devices_info[i].dev_lock;
                 hbi_dev_info[i].pFirmware  = sdk_devices_info[i].pFirmware;
                 hbi_dev_info[i].pConfig    = sdk_devices_info[i].pConfig;
                 hbi_dev_info[i].isImage_typeC = sdk_devices_info[i].imageType;

             }
             if (copy_to_user((hbi_dev_info_t *)pArgs,
			                hbi_dev_info,
			                VPROC_MAX_NUM_DEVS*sizeof(hbi_dev_info_t)));
             return 0;
        }
        case HBI_SLEEP:
        {
            hbi_lnx_drv_arb_arg_t args;
            args.status = HBI_sleep(dev->hbi_handle);

            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_drv_arb_arg_t));

			VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user %d, HBI_sleep = %d\n", ret, args.status);
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user failed\n");
                return -1;
            }
            return 0;
        }
        case HBI_WAKE:
        {
            hbi_lnx_drv_arb_arg_t args;
            args.status = HBI_wake(dev->hbi_handle);

            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_drv_arb_arg_t));

			VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user %d, HBI_sleep = %d\n", ret, args.status);
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user failed\n");
                return -1;
            }
            return 0;
        }
        case HBI_READ:
            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_READ called\n");
            return 0;
        case HBI_WRITE:
        {
            hbi_lnx_drv_rw_arg_t args;
             /* TODO: 256 some magical number took to avoid malloc n free
                  for small bytes read.can be changed or set to max limit
             */
            uint8_t buffer[256];
            uint8_t *tmp = NULL;
            int      bmalloced = FALSE;
            int i;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_WRITE called\n");

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL param passed\n");
                return -EINVAL;
            }

            ret = copy_from_user(&args,
                                (const void __user *)pArgs,
                                sizeof(hbi_lnx_drv_rw_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed!!\n");
                return -EAGAIN;
            }

 //           dev = (struct hbi_dev *)(args.handle);

            /* use static for read length up to 256 */
            tmp = buffer;
            if(args.len > sizeof(buffer))
            {
                tmp = NULL;
                tmp = kmalloc(args.len, GFP_KERNEL);
                if(tmp == NULL)
                {
                    args.status = HBI_STATUS_RESOURCE_ERR;
                    ret = copy_to_user((void __user*)pArgs,
                                       (void *)&args,
                                       sizeof(hbi_lnx_drv_rw_arg_t));
                    return -EFAULT;
                }
                bmalloced = TRUE;
            }

            if(cmd == HBI_READ)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Calling HBI_read reg 0x%x , length %lu\n",
                        args.reg,args.len);

                args.status = HBI_read((dev->hbi_handle),
                                       args.reg,
                                       tmp,
                                       (size_t)(args.len));

                if(args.status == HBI_STATUS_SUCCESS)
                {
                    /*copy read buffer */
                    args.len = copy_to_user(args.pData,tmp,args.len);
                }
            }
            else
            {
               VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Calling HBI_write reg 0x%x , length %lu\n",
                       args.reg,args.len);

                ret = copy_from_user((void *)tmp,
                                          (const void * __user)(args.pData),
                                          args.len);
               if(!ret)
               {
                  VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "copied %lu bytes ..\n",args.len);
                  for (i=0;i<args.len;i++)
                  {
                     VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "0x%x ",tmp[i]);
                  }

                  args.status = HBI_write(dev->hbi_handle,
                                         args.reg,
                                         tmp,
                                         args.len);
              }
              else
              {
                  VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
              }
            }

            if(bmalloced && tmp)
            {
                kfree(tmp);
                tmp=NULL;
            }
            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_drv_rw_arg_t));
            if(ret)
            {
               VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user failed\n");
               return -1;
            }
            return 0;
        }
        case HBI_LOAD_FW:
        {
            hbi_lnx_send_data_arg_t args;
            hbi_data_t              data;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_LOAD_FW called\n");

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL ioctl arg passed\n");
                return -EFAULT;
            }


            ret = copy_from_user(&args,
                                 (const void __user*)pArgs,
                                 sizeof(hbi_lnx_send_data_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
                return -EAGAIN;
            }

//            dev = (struct hbi_dev *)(args.handle);

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "copy from user buffer 0x%lx size %d\n",
                        (unsigned long)(args.data.pData),
                        (int)(args.data.size));

            if(sizeof(dev->hbi_buf.buf) < args.data.size)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "insufficient buffer\n");
                args.status = HBI_STATUS_RESOURCE_ERR;
                if(copy_to_user((void __user*)pArgs,
                              &args,
                              sizeof(hbi_lnx_send_data_arg_t)))
                {
                  VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user performed an incomplete copy\n");
                }
                return -ENOMEM;
            }

            //memcpy((dev->hbi_buf.buf),args.data.pData,args.data.size);
            if (copy_from_user(dev->hbi_buf.buf, args.data.pData, args.data.size)) {
                printk("Error: copy_from_user failed for firmware data\n");
                return -EFAULT;




            }

            data.pData = (dev->hbi_buf.buf);
            data.size = args.data.size;

            args.status = HBI_CMD(dev,HBI_CMD_LOAD_FWR_FROM_HOST,&data);



            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_send_data_arg_t));
            return ret;
        }
        case HBI_LOAD_FW_COMPLETE:
        {
            hbi_lnx_ldfw_done_arg_t args;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_LOAD_FW_COMPLETE called\n");

            ret = copy_from_user((void *)&args,
                               (const void* __user)pArgs,
                               sizeof(hbi_lnx_ldfw_done_arg_t));

//            dev = (struct hbi_dev *)(args.handle);

            args.status = HBI_CMD(dev,HBI_CMD_LOAD_FWR_COMPLETE,NULL);

            ret = copy_to_user((void * __user)pArgs,
                               (const void*)&args,
                               sizeof(hbi_lnx_ldfw_done_arg_t));

            return ret;
        }
        case HBI_START_FW:
        {
            hbi_lnx_start_fw_arg_t  args;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_START_FW called\n");

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL ioctl arg passed\n");
                return -EFAULT;
            }

            ret = copy_from_user(&args,
                                 (const void __user *)pArgs,
                                 sizeof(hbi_lnx_start_fw_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
                return -EAGAIN;
            }
//            dev = (struct hbi_dev *)(args.handle);
            args.status = HBI_CMD(dev,HBI_CMD_START_FWR,NULL);
            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_start_fw_arg_t));
            return ret;
        }
        case HBI_LOAD_CFG:
        {
            hbi_lnx_send_data_arg_t args;
            hbi_data_t              data;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_LOAD_CFG called\n");

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL ioctl arg passed\n");
                return -EFAULT;
            }


            ret = copy_from_user(&args,
                                 (const void __user*)pArgs,
                                 sizeof(hbi_lnx_send_data_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
                return -EAGAIN;
            }

//            dev = (struct hbi_dev *)(args.handle);

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "copy from user buffer 0x%lx size %d\n",
                        (unsigned long)(args.data.pData),
                        (int)(args.data.size));


            if(sizeof(dev->hbi_buf.buf) < args.data.size)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "insufficient buffer\n");
                args.status = HBI_STATUS_RESOURCE_ERR;
                if(copy_to_user((void __user*)pArgs,
                              &args,
                              sizeof(hbi_lnx_send_data_arg_t)))
                {
                  VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user performed an incomplete copy\n");
                }
                return -ENOMEM;
            }

             //memcpy((dev->hbi_buf.buf),args.data.pData,args.data.size);
            // 使用安全的 copy_from_user 函数
            if (copy_from_user(dev->hbi_buf.buf, args.data.pData, args.data.size)) {
                printk("Error: copy_from_user failed for firmware data\n");
                return -EFAULT;

            }

            data.pData = (dev->hbi_buf.buf);
            data.size = args.data.size;

            args.status = HBI_CMD(dev,HBI_CMD_LOAD_CFGREC_FROM_HOST,&data);



            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_send_data_arg_t));
            return ret;
        }
        case HBI_FLASH_SAVE_FWR_CFGREC:
        {
#ifdef FLASH_PRESENT
            hbi_lnx_flash_save_fwrcfg_arg_t  args;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_FLASH_SAVE_FWR_CFGREC called\n");

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL ioctl arg passed\n");
                return -EFAULT;
            }

            ret = copy_from_user(&args,
                                 (const void __user*)pArgs,
                                 sizeof(hbi_lnx_flash_save_fwrcfg_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
                return -EAGAIN;
            }

//            dev = (struct hbi_dev *)(args.handle);

            args.status = HBI_CMD(dev,
                                  HBI_CMD_SAVE_FWRCFG_TO_FLASH,
                                  &(args.image_num));

            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_flash_save_fwrcfg_arg_t));
#endif
            return ret;
        }
        case HBI_FLASH_SAVE_CFGREC:
        {
#ifdef FLASH_PRESENT
            hbi_lnx_flash_save_fwrcfg_arg_t  args;
            char image_num[2]={0};

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL ioctl arg passed\n");
                return -EFAULT;
            }

            ret = copy_from_user(&args,
                                 (const void __user*)pArgs,
                                 sizeof(hbi_lnx_flash_save_fwrcfg_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"copy_from_user failed\n");
                return -EAGAIN;
            }

            //dev = (struct hbi_dev *)(args.handle);
            image_num[0] = (args.image_num >> 8) & 0xFF;
            image_num[1] = args.image_num & 0xFF;

            args.status = HBI_CMD(dev,
                                  HBI_CMD_SAVE_CFG_TO_FLASH,
                                  image_num);

            ret = copy_to_user((void __user *)pArgs,
                                 &args,
                                 sizeof(hbi_lnx_flash_save_fwrcfg_arg_t));
#endif
            return ret;
        }
        case HBI_FLASH_LOAD_FWR_CFGREC:
        {
#ifdef FLASH_PRESENT
            hbi_lnx_flash_load_fwrcfg_arg_t args;
            char image_num[2]={0};

            VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "HBI_FLASH_LOAD_FWR_CFGREC called\n");

            if(!pArgs)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL ioctl arg passed\n");
                return -EFAULT;
            }

            ret = copy_from_user(&args,
                                 (const void __user*)pArgs,
                                 sizeof(hbi_lnx_flash_load_fwrcfg_arg_t));
            if(ret)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
                return -EAGAIN;
            }

//            dev = (struct hbi_dev *)(args.handle);

            image_num[0] = (args.image_num >> 8) & 0xFF;
            image_num[1] = args.image_num & 0xFF;

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Writing image num %d\n",*((int *)image_num));

            args.status = HBI_CMD(dev,
                                 HBI_CMD_LOAD_FWRCFG_FROM_FLASH,
                                 image_num);

            ret = copy_to_user((void __user *)pArgs,
                              &args,
                              sizeof(hbi_lnx_flash_load_fwrcfg_arg_t));
#endif
                return ret;
            }
            case HBI_FLASH_ERASE_FWRCFGREC:
            case HBI_FLASH_ERASE_WHOLE:
            {
#ifdef FLASH_PRESENT
                hbi_lnx_flash_erase_fwcfg_arg_t args;
                char image_num[2];
                if(!pArgs)
                {
                    VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL ioctl arg passed\n");
                    return -EFAULT;
                }

                ret = copy_from_user(&args,
                                    (const void __user*)pArgs,
                                    sizeof(hbi_lnx_flash_erase_fwcfg_arg_t));
                if(ret)
                {
                    VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed\n");
                    return -EAGAIN;
                }

//                dev = (struct hbi_dev *)(args.handle);

                image_num[0]=(args.image_num >> 8) & 0xFF;
                image_num[1] = (args.image_num) & 0xFF;
                if(cmd == HBI_FLASH_ERASE_FWRCFGREC)
                    args.status = HBI_CMD(dev,
                                          HBI_CMD_ERASE_FWRCFG_FROM_FLASH,
                                          image_num);
                else
                    args.status = HBI_CMD(dev,HBI_CMD_ERASE_WHOLE_FLASH,NULL);

                ret = copy_to_user((void __user*)pArgs,
                                    &args,
                                    sizeof(hbi_lnx_flash_erase_fwcfg_arg_t));
#endif
                return ret;
            }
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return ret;
}

static int hbi_io_open(struct inode *inode, struct file *filp)
{
    int status = -ENXIO;
    struct hbi_dev *dev;

    /* else parse through list of other devices*/
    list_for_each_entry(dev,&(hbi_lnx_drv_priv.list),list)
    {

        if (dev->t_dev == inode->i_rdev)
        {
            status = 0;
            break;
        }
    }

    if (status != 0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "status %d of hbi_io_open", status);
        return status;
    }

    module_usage_count++;
    filp->private_data = dev;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "device file opened - %d users\n", module_usage_count);
    return 0;
}

static int hbi_io_close(struct inode *inode, struct file *filp)
{
    struct hbi_dev *dev;

    module_usage_count--;
    if (!module_usage_count) {
        dev = filp->private_data;
        filp->private_data = NULL;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "device file closed - %d users\n", module_usage_count);
    return 0;
}


struct file_operations fops = {
    .open =         hbi_io_open,
    .release =      hbi_io_close,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
    .unlocked_ioctl = internal_hbi_drv_ioctl,
#else
    .ioctl = internal_hbi_drv_ioctl,
#endif
};


static inline int atoi(char *s,int size)
{
    uint32_t value=0;
    int i;
    for(i=0;i<size;i++)
    {
        if(s[i]>='0' && s[i]<='9')
        {
            value = (value * 10) + (s[i]-'0');
        }
    }
    return value;
}

static inline int atoh(char *s,int size)
{
    unsigned int val = 0;
    char c;
    unsigned char i = 0;
    for (i = 0; i< size; i++)
    {
        c = *s++;
        if (c >= '0' && c <= '9')
        {
            val = (val << 4) +( c & 0x0F );
            continue;
        }
        c &= 0xDF;
        if (c >= 'A' && c <= 'F')
        {
            val = (val << 4) + ((c & 0x07) + 9);
            continue;
        }
    }
    return val;
}
#if (HBI_ENABLE_PROCFS)

int itoa(unsigned int num,unsigned char *buf, int index)
{
    unsigned int  tmp;
    int k;
    int len=0;

    tmp = num;

    if(!num)
        len++;
    else
    {
        while(tmp)
        {
            len++;
            tmp=tmp/10;
        }
    }

    tmp = len;

    while(len)
    {
        k = (len+index)-1;
        if(num < 10)
            buf[k] = num+'0';
        else
            buf[k] = (num%10)+'0';
        num = num/10;
        len--;
    }

    index+=tmp;

    return index;
}

int htoa(unsigned int num,unsigned char *buf, int index)
{
    unsigned int tmp;
    int k;
    int len=0;

    tmp = num;

    if(!num)
        len++;
    else
    {
        while(tmp)
        {
            len++;
            tmp=tmp/16;
        }
    }

    tmp = len;
    while(len)
    {
        k=(len+index)-1;
        if((num & 0xf) >= 10)
            buf[k]= ((num & 0xf)-10) + 'A';
        else
            buf[k] = (num & 0xf) + '0';
        len--;
        num >>=4;
    }
    index += tmp;

    return index;
}

static hbi_status_t hbi_wr_bin_fw(struct hbi_dev *dev,
                                 unsigned char *buf,
                                 size_t size,
                                 size_t block_size)
{
   hbi_status_t         status = HBI_STATUS_SUCCESS;
   int                  i=0;
   size_t               chunk_len;
   hbi_data_t           fwrimg;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

   /* convert to bytes */
   chunk_len = block_size << 1;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Retrieved Block size of %zu\n",block_size);

   do
   {
     if(size  < chunk_len || !chunk_len)
     {
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Incomplete data read.requested %zu, read %zu\n",chunk_len,size);
         return HBI_STATUS_RESOURCE_ERR;
     }

     fwrimg.pData = &buf[i];
     fwrimg.size = chunk_len;

     status = HBI_set_command(dev->hbi_handle,HBI_CMD_LOAD_FWR_FROM_HOST, &fwrimg);
     if ((status != HBI_STATUS_SUCCESS &&  status != HBI_STATUS_OP_INCOMPLETE))
      {
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_write failed\n");
         return status;
      }
      i+=chunk_len;
   }while(i<size);
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "2- Conclude boot image loading....\n");

   status = HBI_set_command((dev->hbi_handle),HBI_CMD_LOAD_FWR_COMPLETE,NULL);
   if (status != HBI_STATUS_SUCCESS)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error %d:HBI_CMD_BOOT_COMPLETE failed!\n", status);
   }

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

   return status;
}



ssize_t hbi_proc_reg_rd_dump(struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    int i,j;

    unsigned int num;
    unsigned char kbuf[512];

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(hbi_rw.len)
    {
        if(size > hbi_rw.len)
            size = hbi_rw.len;

        for(i=0,j=0; ((i<size) && (j<(sizeof(kbuf)-1))); i++)
        {
            num=hbi_rw.buf[i];
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "num %d, j %d\n",num,j);
            j=htoa(num,kbuf,j);
            kbuf[j++] = '\t';
        }

        kbuf[j]='\n';

        if(copy_to_user((void __user*)buf,kbuf,j))
        {
            /* issue just a warning */
           VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_to_user failed with incomplete copy\n");
        }


        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Read %d bytes to user buf\n",j);

        for(i=0;i<=j;i++)
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "%c\n",kbuf[i]);

        *offset+=j;
        hbi_rw.len -= size;

        return j;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "return EOF\n");
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}
#if HBI_ENABLE_FWR_BIN
static hbi_status_t hbi_wr_bin_cfg(struct hbi_dev *dev,
                                 unsigned char *buf,
                                 size_t size,
                                 size_t block_size)
{
   hbi_status_t         status = HBI_STATUS_SUCCESS;
   int                  i=0;
   size_t               chunk_len;
   hbi_data_t           fwrimg;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

   /* convert to bytes */
   chunk_len = block_size << 1;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Retrieved Block size of %zu\n",block_size);

   do
   {
     if(size  < chunk_len || !chunk_len)
     {
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Incomplete data read.requested %zu, read %zu\n",chunk_len,size);
         return HBI_STATUS_RESOURCE_ERR;
     }

     fwrimg.pData = &buf[i];
     fwrimg.size = chunk_len;

     status = HBI_set_command(dev->hbi_handle,HBI_CMD_LOAD_CFGREC_FROM_HOST, &fwrimg);
     if ((status != HBI_STATUS_SUCCESS &&  status != HBI_STATUS_OP_INCOMPLETE))
      {
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_write failed\n");
         return status;
      }
      i+=chunk_len;
   }while(i<size);
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "2- Conclude boot config loading....\n");

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

   return status;
}


static hbi_status_t hbi_load_binimage_from_host(struct hbi_dev *dev, uint8_t *pArg)
{

    hbi_status_t status;
    static int total_img_len=1;
    static int block_size;
    static int hdr_len;
    hbi_data_t img;
    hbi_img_hdr_t hdr;
    const struct firmware *twfw;
    int ret;


    if(dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device or image size retrieved\n");
        return -1;
    }
    VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "request_firmware file to load %s \n",(char *)pArg);
    ret = request_firmware(&twfw, (char *)pArg, dev->device);

    if (ret < 0)
    {
       VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "request_firmware failed to load %s \n",(char *)pArg);
    }

    if ((twfw->size > sizeof(dev->hbi_buf.buf)) || (!twfw->size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Insufficient buffer to perform this operation\n");
        /* reset firmware buffer */
        SSL_memset(&(dev->hbi_buf),0,sizeof(dev->hbi_buf));
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Could not load complete data size of" \
        " Firmware Buffer is less than size of data\n");
        return -1;
    }

    SSL_memcpy(dev->hbi_buf.buf,(unsigned char *)twfw->data,twfw->size);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "image (size %zu)... \n", twfw->size);
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "image hdr data: 0x%02x, 0x%02x, 0x%02x, \n", dev->hbi_buf.buf[2], dev->hbi_buf.buf[3], dev->hbi_buf.buf[5]);


    dev->hbi_buf.len = twfw->size;
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "image (size %zu)... \n", twfw->size);


    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Getting Image header...\n");
    img.pData = dev->hbi_buf.buf;
    img.size = dev->hbi_buf.len;
    status = HBI_get_header(&img,&hdr);
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Image header len = %zu...\n", hdr.img_len);
    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error ! Invalid Image Header Found\n");
        return -1;
    }
    total_img_len = hdr.img_len;
    block_size = hdr.block_size;
    hdr_len = hdr.hdr_len;

    if(dev->hbi_buf.len < total_img_len)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Error: size of buffer %d, is less than total image len %d\n",dev->hbi_buf.len, total_img_len);
        return twfw->size;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Passing image of size 0x%x\n",total_img_len);

    /* skip rest of the header jump to payload */
    if (hdr.image_type == HBI_IMG_TYPE_FWR)
    {
        status = hbi_wr_bin_fw(dev,
        &(dev->hbi_buf.buf[hdr_len]),
        total_img_len,
        block_size);
    } else if(hdr.image_type == HBI_IMG_TYPE_CR)
    {
        status = hbi_wr_bin_cfg(dev,
        &(dev->hbi_buf.buf[hdr_len]),
        total_img_len,
        block_size);

    } else
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Invalid image type %d\n", hdr.image_type);
        status = HBI_STATUS_INVALID_ARG;
    }

    SSL_memset(&(dev->hbi_buf),0,sizeof(dev->hbi_buf));
    dev->hbi_buf.len =0;

    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "failed to load image to device\n");
        return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return status;
}
#endif

ssize_t hbi_proc_reg_rd(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
    struct hbi_dev *dev=NULL;
    uint8_t         val[256];
    uint32_t        len;
    uint32_t        reg;
    hbi_status_t      status;
    int             i=0;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    dev = (struct hbi_dev *)(DEVP(filp));
    if(dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "No device opened\n");
        return -1;
    }

    /*expected format dev_addr reg size */
    if(copy_from_user((void *)val,buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole user buffer\n");
        return -1;
    }

    /* Get register to access */
    while(val[i++] != ' ');
    if(i >= size)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "To read a register , give command "\
                        "'echo <register addr in hex> <number of bytes in" \
                        " dec(should be multiple of 2)>'\n");
        return -1;
    }

    reg = atoh(val,i-1);
    len = atoi(&val[i],size-i);

    if(len > sizeof(val))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Cannot read more than %zu bytes in one shot\n",sizeof(val));
        return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Got dev_addr 0x%x register 0x%x" \
                  "len to read 0x%x dev handle 0x%px\n",
                  dev->devcfg.dev_addr,reg,len, dev);


    SSL_memset(hbi_rw.buf,0,len);

    status = HBI_read((dev->hbi_handle),reg,hbi_rw.buf,len);
    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_read error\n");
        return -1;
    }
    hbi_rw.len = len;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

ssize_t hbi_proc_reg_wr(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
    struct hbi_dev *dev=NULL;
    uint8_t         val[256];
    uint32_t        data_len=0;
    reg_addr_t        reg;
    hbi_status_t      status;
    int              i=0;
    uint8_t          tmp[256];

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    dev = (struct hbi_dev *)(DEVP(filp));

    if(dev==NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Invalid Device\n");
        return -1;
    }

    /*expected format dev_addr:reg:val */
    if(copy_from_user((void *)val,buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole user buffer\n");
    }

    while(val[i++] != ' ');

    if(i >= size)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "To write register enter: echo <register addr in hex> <data in hex(should be multiple of 2)>\n");
        return -1;
    }

    reg = atoh(val,i-1);
    data_len=0;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Writing ... \n");

    while((val[i] != ' ') && (i <(size-1)) && (data_len < sizeof(tmp)))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "input %c %c\n",val[i],val[i+1]);
        tmp[data_len] = atoh(&val[i],2); /* convert two chars into a byte */
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "0x%x\n ",tmp[data_len]);
        data_len++;
        i+=2;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Got addr 0x%x,len %d reg 0x%x\n",dev->devcfg.dev_addr,data_len,reg);

    status = HBI_write((dev->hbi_handle),reg,tmp,data_len);

    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Write Failed\n");
        return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

/*getDeviceClientId() - check whether the device to be openend is valid
 *  Args:
 *         pDevCfg : pointer to the device bus number and the address on the bus
 * Return:
 *         deviceId (integer index number of the device in the list of available devices) if success, -1 if failure
 */

static int hbi_get_deviceid(uint8_t bus_num, uint8_t dev_addr) {
    /*Device ID out of range check*/
    int i = 0;
    for(i=0;i<VPROC_MAX_NUM_DEVS;i++)
    {
        if ((sdk_devices_info[i].dev_addr == dev_addr) &&
           (sdk_devices_info[i].bus_num == bus_num))
        {
            return i;
        }
    }
    return -1;
}

ssize_t hbi_proc_open_dev_wr(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
    hbi_dev_cfg_t     devcfg;
    uint8_t         val[16];
    uint8_t         bus_num[VPROC_MAX_NUM_DEVS]={'\0'};
    uint8_t         *tmp;
    struct hbi_dev *device;
    hbi_status_t      status;
    int             i;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    memset(val,0,sizeof(val));

    if(copy_from_user((void *)val,buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole user buffer\n");
    }

    tmp = strchr(val,':');
    if(tmp == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Enter device addr as bus_num:dev_addr \n");
        return -1;
    }
    for(i=0;i<(tmp-val);i++)
    {
        bus_num[i]=val[i];
    }

    devcfg.bus_num = atoh(bus_num,(tmp-val));
    devcfg.dev_addr = atoh(tmp,size-(tmp-val));
    devcfg.pDevName = NULL;
    devcfg.deviceId = hbi_get_deviceid(devcfg.bus_num, devcfg.dev_addr);
    if (devcfg.deviceId < 0)
        return -1;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "received dev_addr 0x%x bus number %d \n",
                  (unsigned int)(devcfg.dev_addr),devcfg.bus_num);

    status = internal_hbi_open((void **)&device,&devcfg);

    if(status == HBI_STATUS_SUCCESS)
    {
        filp->private_data = (void *)device;
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Opened device handle 0x%px\n", device);
    }
    else
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Failed to open device\n");

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

ssize_t hbi_proc_open_dev_rd(struct file *filp,  char __user *buf, size_t size, loff_t *offset)
{

    struct hbi_dev *dev;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Opened devices(addr:bus_num)\n");

    if(!list_empty(&(hbi_lnx_drv_priv.list)))
    {
        list_for_each_entry(dev,&(hbi_lnx_drv_priv.list),list)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "0x%x:%d\n",dev->devcfg.dev_addr,dev->devcfg.bus_num);
        }
    }
    else
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"No opened devices\n");
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}


ssize_t hbi_proc_close_dev_wr(struct file *filp,
                              const char __user *buf,
                              size_t size, loff_t *offset)
{
    uint8_t           val[16];
    uint32_t          dev_addr;
    hbi_status_t      status;
    struct hbi_dev    *device=NULL;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    SSL_memset(val,0,sizeof(val));

    if(copy_from_user((void *)val,buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole user buffer\n");
    }

    dev_addr = atoh(val,size);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Received address 0x%x to close\n",dev_addr);
    if(list_empty(&(hbi_lnx_drv_priv.list)))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "No more devices to close\n");
        return size;
    }

    /* Search if device with requested address is registered */
    list_for_each_entry(device,&(hbi_lnx_drv_priv.list),list)
    {
        if(device && (device->devcfg.dev_addr == dev_addr))
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Close dev_addr 0x%x device handle 0x%px\n",
                        dev_addr,
                        device);
            status = internal_hbi_close(device);
            device = NULL;
            return size;
        }
    }
#if 0
    /* do this for head entry */
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "received address 0x%x registered device handle address 0x%x\n",
               (unsigned int)dev_addr,(unsigned int)(device));

    if(device && (device->devcfg.dev_addr == dev_addr))
    {
        status = internal_hbi_close(device);
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "internal_hbi_close status %s\n",
                     (status == HBI_STATUS_SUCCESS) ? "OK!" : "Err!");
    }
#endif
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

ssize_t hbi_proc_wr_cfgrec(struct file *filp,
                           const char __user *buf,
                           size_t size, loff_t *offset)
{
#define APP_RST_WAIT 10000

    char                *kbuf;
    struct hbi_dev      *dev = DEVP(filp);
    int                 i=0;
    reg_addr_t          reg;
    uint32_t            val;
    user_buffer_t       tmp[2];
    char                c;
    hbi_status_t          status;
    int                  count=0;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Size of user buffer %zu\n", size);
    if(!size || dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Either size is NULL or no device opened\n");
        return -1;
    }

    kbuf = kmalloc(size,GFP_KERNEL);
    if(!kbuf)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Memory Allocation Failure\n");
    return -1;
    }

    if(copy_from_user(kbuf,buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole user buffer\n");
    }

    i=0;
    /* Reset size, it is assumed every new cfgrec loading will invalidate previous settings */
    dev->cfgrec.size=0;

    while(i<size)
    {
    if(kbuf[i] != ';')
    {
        if (sscanf(&kbuf[i], "%x %c %x", (unsigned int *)&reg, &c, (unsigned int *)&val) == 3)
        {
            /*
            TODO: this is probably slower approach to write every 2 bytes.
            We can change implementation to accumulate the data upto allowed limit and
            then write. This implementation to be revisit during performance evaluation.
            */
            /* Device bydefault operate in Big Endian so swap MSB first before writing a word */

            tmp[0]=(val>>8)& 0xFF;
            tmp[1]=val&0xFF;
            status = HBI_write((dev->hbi_handle),reg,(user_buffer_t *)tmp,2);
            if(status == HBI_STATUS_SUCCESS)
            {
                dev->cfgrec.size+=2;
            }
            else
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI write failed\n");
            }
        }
    }

    /* skip to next line */
    while(kbuf[i++] != '\n');
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Received CFGREC of size %d\n",dev->cfgrec.size);

    reg = ZL380xx_HOST_SW_FLAGS_REG;

    tmp[0] = ZL380xx_HOST_SW_FLAGS_APP_REBOOT>>8;
    tmp[1]=ZL380xx_HOST_SW_FLAGS_APP_REBOOT&0xFF;
    status = HBI_write((dev->hbi_handle),reg,tmp,2);

    tmp[0]=0;
    tmp[1]=0;

   if(status == HBI_STATUS_SUCCESS)
   {
        do{
        /* wait for reset to complete */
        status = HBI_read(dev->hbi_handle, reg,(user_buffer_t*) tmp, 2);
        }while((status == HBI_STATUS_SUCCESS) && (count++ < APP_RST_WAIT));
   }
    /* value of reg comes out to be 0xFFF8 on successful reset */
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "reset result 0x%x\n",*((unsigned int *)tmp));

    kfree(kbuf);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

ssize_t hbi_proc_load_fw(struct file *filp,
                        const char __user *buf, size_t size, loff_t *offset)
{
    struct hbi_dev    *dev = (struct hbi_dev *)(DEVP(filp));
    hbi_status_t status;
    static int first=1;
    static int total_img_len=1;
    static int block_size;
    static int hdr_len;
    hbi_data_t img;
    hbi_img_hdr_t hdr;
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter (size %zu)... \n",size);
    if(dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device or image size retrieved\n");
    return -1;
    }

    if(!size)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Either size is NULL or no device opened\n");
        return -1;
    }

    if((dev->hbi_buf.len + size) > sizeof(dev->hbi_buf.buf))
    {
        /* reset firmware buffer */
        memset(&(dev->hbi_buf),0,sizeof(dev->hbi_buf));
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Could not load complete data size of" \
        " Firmware Buffer is less than size of data\n");
        return -1;
    }

    /* this function ideally should be loading image on to device.
    however sometimes file write operation using cat command/linux shell
    gives data in chunk rather than one buffer with whole image.
    in such scenario we were getting checksum error after downloading to
    device was complete. We probably were missing out on information.
    So on safer note, this command accumulates complete data in kernel
    buffer and then written to device
    */
    if(copy_from_user(&(dev->hbi_buf.buf[dev->hbi_buf.len]),buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole user buffer\n");
        return -1;
    }

    dev->hbi_buf.len += size;
    if(first)
    {
        /* if this the first chunk received, parse header and
        get total image len */
        img.pData = dev->hbi_buf.buf;
        img.size = dev->hbi_buf.len;
        status = HBI_get_header(&img,&hdr);
        if(status != HBI_STATUS_SUCCESS)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error ! Invalid Image Header Found\n");
            return -1;
        }
        total_img_len = hdr.img_len;
        block_size = hdr.block_size;
        hdr_len = hdr.hdr_len;
        first = 0;
    }
    if(dev->hbi_buf.len < total_img_len)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "size of buffer %d\n",dev->hbi_buf.len);
        return size;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Passing fwr image of size 0x%x\n",total_img_len);

    /* skip rest of the header jump to payload */
    status = hbi_wr_bin_fw(dev,
    &(dev->hbi_buf.buf[hdr_len]),
    total_img_len,
    block_size);

    SSL_memset(&(dev->hbi_buf),0,sizeof(dev->hbi_buf));
    dev->hbi_buf.len =0;

    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "failed to load firmware to device\n");
        return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

ssize_t hbi_proc_start_fw(struct file *filp,  char __user *buf,
                           size_t size, loff_t *offset)
{
    struct hbi_dev   *pDev=(struct hbi_dev *)(DEVP(filp));
    hbi_status_t      status;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(pDev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device or image size retrieved\n");
        return -1;
    }

    status = HBI_set_command((pDev->hbi_handle),HBI_CMD_START_FWR,NULL);
    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "failed to load firmware to device\n");
        return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}

ssize_t hbi_proc_rd_cfgrec(struct file *filp,
                           char __user *buf,
                           size_t size, loff_t *offset)
{
    hbi_status_t    status;
    struct hbi_dev *dev = (struct hbi_dev *)(DEVP(filp));
    int            i=0;
    uint16_t       tmp;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "No device opened to read cfgrec\n");
        return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "; Addr, Data,\n");
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "; ----- -----\n");
    for(i=HBI_CFGREC_BASE;i<HBI_CFGREC_MAX_SIZE;i+=2)
    {
        status=HBI_read(dev->hbi_handle,i,(user_buffer_t *)&tmp,2);
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "0x%04X, 0x%04X\n",i,tmp);
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}

#ifdef FLASH_PRESENT
ssize_t hbi_proc_save_fwrcfgrec_to_flash(struct file *filp,  char __user *buf, size_t size, loff_t *offset)
{
    struct hbi_dev *dev = (struct hbi_dev*)(DEVP(filp));
    hbi_status_t   status;
    int            image_num=-1;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(dev==NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device handle\n");
        return -1;
    }

    status = HBI_set_command((dev->hbi_handle),
        HBI_CMD_SAVE_FWRCFG_TO_FLASH,
        &image_num);
    if(status == HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Assigned image number %d\n",image_num);
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}


ssize_t hbi_proc_load_fwrcfgrec_from_flash(struct file *filp,
                                          const char __user *buf,
                                          size_t size, loff_t *offset)
{
    struct hbi_dev *dev = (struct hbi_dev *)(DEVP(filp));
    hbi_status_t  status;
    char          image_num[2];
    int32_t       tmp;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device handle\n");
        return -1;
    }

    if(copy_from_user(&tmp,buf,size))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't copy whole data\n");
        return -1;
    }
    SSL_memset(image_num,0,sizeof(image_num));
    tmp = atoi((char *)&tmp,size);

    image_num[0]=tmp>>8;
    image_num[1]=tmp & 0xff;
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Writing image num %d tmp %d\n",*((unsigned int *)image_num),tmp);
    status = HBI_set_command((dev->hbi_handle),
                              HBI_CMD_LOAD_FWRCFG_FROM_FLASH,
                              (void *)image_num);

    status = HBI_set_command((dev->hbi_handle),HBI_CMD_START_FWR,NULL);
    if(status != HBI_STATUS_SUCCESS)
    {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"failed to load firmware to device\n");
      return -1;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}


ssize_t hbi_proc_erase_img_from_flash(struct file *filp,
                                    const char __user *buf,
                                    size_t size, loff_t *offset)
{
    struct hbi_dev *dev = (struct hbi_dev *)(DEVP(filp));

    hbi_status_t  status;
    char          image_num[2];
    int16_t       tmp;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(dev == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device handle\n");
        return -1;
    }

    if(copy_from_user(&tmp,buf,size))
    {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "copy_from_user failed. couldn't copy whole user arg\n");
      return -1;
    }

    SSL_memset(image_num,0,sizeof(image_num));

    tmp = atoi((char *)&tmp,size);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Erase image number %d\n",tmp);


    image_num[0]=tmp>>8;
    image_num[1]=tmp & 0xFF;

    status = HBI_set_command((dev->hbi_handle),
                              HBI_CMD_ERASE_FWRCFG_FROM_FLASH,
                              (void *)image_num);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return size;
}

ssize_t hbi_proc_erase_flash(struct file *filp,
                              char __user *buf, size_t size, loff_t *offset)
{
    struct hbi_dev *dev = (struct hbi_dev*)(DEVP(filp));
    hbi_status_t status;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(dev==NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "NULL device handle\n");
        return -1;
    }

    status = HBI_set_command((dev->hbi_handle),HBI_CMD_ERASE_WHOLE_FLASH,NULL);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}
#endif

#if 0
ssize_t hbi_proc_drv_init(struct file *filp,  char __user *buf, size_t size, loff_t *offset)
{
    hbi_status_t status = HBI_init(NULL);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(status != HBI_STATUS_SUCCESS)
    {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_init failed.Err 0x%x\n",status);
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}

ssize_t hbi_proc_drv_term(struct file *filp,  char __user *buf, size_t size, loff_t *offset)
{
    hbi_status_t status = HBI_term();

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    if(status != HBI_STATUS_SUCCESS)
    {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_term failed.Err 0x%x\n",status);
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}
#endif
static int procfs_init(void)
{
    int i;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    hbi_lnx_drv_priv.drv_proc_dir = proc_mkdir("hbi",NULL);

    if(hbi_lnx_drv_priv.drv_proc_dir == NULL )
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't create HBI Driver Proc Entry\n");
        return -1;
    }

    for(i=HBI_PROC_ENTRY_FIRST;i<HBI_PROC_ENTRY_LAST;i++)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Creating proc_entry name %s\n",drv_proc_entry[i].name);
        drv_proc_entry[i].proc_entry = proc_create(drv_proc_entry[i].name,
                                                   (S_IFREG|S_IRUGO|S_IWUGO),
                                                   hbi_lnx_drv_priv.drv_proc_dir,
                                                    (const struct proc_ops *)&(drv_proc_entry[i].ops));
        if(drv_proc_entry[i].proc_entry == NULL)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Couldn't create HBI Driver %s Proc Entry\n",
                        drv_proc_entry[i].name);
        }
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}

static int procfs_term(void)
{
    int i;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");

    for(i=HBI_PROC_ENTRY_FIRST;i<HBI_PROC_ENTRY_LAST;i++)
    {
        if(drv_proc_entry[i].proc_entry != NULL)
        {
            PROC_REMOVE(drv_proc_entry[i].proc_entry);
            drv_proc_entry[i].proc_entry = NULL;
        }
    }
    if(hbi_lnx_drv_priv.drv_proc_dir != NULL)
    {
         /* remove base dir */
         PROC_REMOVE(hbi_lnx_drv_priv.drv_proc_dir);
         hbi_lnx_drv_priv.drv_proc_dir = NULL;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}
#endif /* HBI_ENABLE_PROCFS */

//__init
int  hbi_drv_init(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
    int         ret;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");
    printk("Test Entering hbi_drv_init...\n");

	  printk("vproc_dbg_lvl = 0x%02X DEBUG_LEVEL = 0x%02X \n",vproc_dbg_lvl,DEBUG_LEVEL);

    SSL_memset(&hbi_lnx_drv_priv,0,sizeof(struct hbi_lnx_drv));

    /* initialise device list */
    INIT_LIST_HEAD(&(hbi_lnx_drv_priv.list));

    /* register HBI driver as character device driver  - Allocate driver instances
     * as per specified major number
     */
    ret = alloc_chrdev_region(&hbi_lnx_drv_priv.dev_t,
                              0,
                              VPROC_MAX_NUM_DEVS,
                              HBI_DEV_NAME);
    if(ret <0)
        return ret;

    hbi_lnx_drv_priv.cdev=cdev_alloc();
    if(hbi_lnx_drv_priv.cdev == NULL)
    {
        unregister_chrdev_region(hbi_lnx_drv_priv.dev_t,VPROC_MAX_NUM_DEVS);
        return -1;
    }

    cdev_init(hbi_lnx_drv_priv.cdev,&fops);

    ret = cdev_add(hbi_lnx_drv_priv.cdev,hbi_lnx_drv_priv.dev_t,VPROC_MAX_NUM_DEVS);
    if(ret<0)
    {
        unregister_chrdev_region(hbi_lnx_drv_priv.dev_t,VPROC_MAX_NUM_DEVS);
        cdev_del(hbi_lnx_drv_priv.cdev);
        return ret;
    }

#if (HBI_ENABLE_PROCFS)
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Init PROC FS\n");
    procfs_init();
#endif

    hbi_lnx_drv_priv.dev_class = class_create(THIS_MODULE,HBI_DEV_NAME);
    if(hbi_lnx_drv_priv.dev_class)
    {
        struct hbi_dev *dev=NULL;
        hbi_status_t status;
        int i;
        hbi_dev_cfg_t devcfg;

        HBI_init(NULL, pClient, pDeviceId);
        //HBI_init(NULL);

        for(i=0;i<VPROC_MAX_NUM_DEVS;i++)
        {
            devcfg.dev_addr = sdk_devices_info[i].dev_addr;
            devcfg.pDevName = NULL;
            devcfg.bus_num = sdk_devices_info[i].bus_num;
            devcfg.dev_lock = 0;
            devcfg.deviceId = i;
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "initing device,lock %d,%llu with addr : 0x%x bus num %d\n",devcfg.deviceId, devcfg.dev_lock, devcfg.dev_addr,devcfg.bus_num);

            status=internal_hbi_open((void **)&dev,&devcfg);
            if(status != HBI_STATUS_SUCCESS)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Unable to create a device files\n");
                hbi_drv_exit();
            }

            /*Load firmware and related config into the device if the boot firmware and config record to device is TRUE*/
            if (sdk_devices_info[i].isboot)
            {

                if (!sdk_devices_info[i].imageType) {
#if HBI_LOAD_FWR_STATIC
                    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Boot loading Vproc *.h image to device %d with addr : 0x%x bus num %d\n",devcfg.deviceId, devcfg.dev_addr,devcfg.bus_num);
                    status = hbi_load_staticimage_from_host(dev->hbi_handle, sdk_devices_info[i].pFirmware);
                    if(status != HBI_STATUS_SUCCESS)
                    {
                        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error %d, Config loading from host failed!!!\n", status);
                    }
#endif
#if HBI_LOAD_CFGREC_STATIC
                    status = hbi_load_staticimage_from_host(dev->hbi_handle, sdk_devices_info[i].pConfig);
#endif
                } else {
#if HBI_ENABLE_FWR_BIN
                    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Boot loading Vproc *.bin image to device %d with addr : 0x%x bus num %d\n",devcfg.deviceId, devcfg.dev_addr,devcfg.bus_num);
                    status = hbi_load_binimage_from_host(dev, sdk_devices_info[i].pFirmware);
                    if(status != HBI_STATUS_SUCCESS)
                    {
                        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error %d, Config loading from host failed!!!\n", status);
                    }
                    status = hbi_load_binimage_from_host(dev, sdk_devices_info[i].pConfig);
#endif
                }
                if(status != HBI_STATUS_SUCCESS)
                {
                    VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Error %d, Firmware loading from host failed!!!\n", status);
                }

                status = HBI_set_command(dev->hbi_handle, HBI_CMD_START_FWR, NULL);
                if(status != HBI_STATUS_SUCCESS)
                {
                    VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Starting Firmware failed!!!\n");
                }
            }
        }
        list_for_each_entry(dev,&(hbi_lnx_drv_priv.list),list)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Added 0x%px to list 0x%px\n", dev, list_entry(&(hbi_lnx_drv_priv.list),struct hbi_lnx_drv,list));
        }

    }
    else
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Unable to create a device class\n");
        unregister_chrdev_region(hbi_lnx_drv_priv.dev_t,VPROC_MAX_NUM_DEVS);
        cdev_del(hbi_lnx_drv_priv.cdev);
    }
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return 0;
}


void hbi_drv_exit(void)
{
    struct hbi_dev *dev,*next_dev;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Enter...\n");
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Check if any device is opened\n");


    if(!list_empty(&(hbi_lnx_drv_priv.list)))
    {
        list_for_each_entry_safe(dev,next_dev,&(hbi_lnx_drv_priv.list),list)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Closing Device Addr 0x%x:%d\n",
                           dev->devcfg.dev_addr,
                           dev->devcfg.bus_num);
            internal_hbi_close(dev);
        }
//        internal_hbi_close(hbi_lnx_drv_priv.dev);
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Calling HBI_term()\n");
    if(HBI_term() != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "HBI_term failed\n");
        return;
    }
#if (HBI_ENABLE_PROCFS)
    procfs_term();
#endif
	VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "hbi_drv_exit() function debug print 1\n");
    cdev_del(hbi_lnx_drv_priv.cdev);
//    device_destroy(hbi_lnx_drv_priv.dev_class,hbi_lnx_drv_priv.dev_t);
	VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "hbi_drv_exit() function debug print 2 hbi_lnx_drv_priv.dev_class= %p\n",hbi_lnx_drv_priv.dev_class);
	//Modified by Perry@20181013
	if(hbi_lnx_drv_priv.dev_class)
	{
    class_destroy(hbi_lnx_drv_priv.dev_class);
		hbi_lnx_drv_priv.dev_class=NULL;
	}
	VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "hbi_drv_exit() function debug print 3\n");
    unregister_chrdev_region(hbi_lnx_drv_priv.dev_t,VPROC_MAX_NUM_DEVS);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC, "Exit...\n");

    return ;
}

EXPORT_SYMBOL(HBI_init);
EXPORT_SYMBOL(HBI_term);
EXPORT_SYMBOL(HBI_open);
EXPORT_SYMBOL(HBI_close);
EXPORT_SYMBOL(HBI_read);
EXPORT_SYMBOL(HBI_write);
EXPORT_SYMBOL(HBI_reset);
EXPORT_SYMBOL(HBI_set_command);
EXPORT_SYMBOL(HBI_get_header);
EXPORT_SYMBOL(HBI_wake);
EXPORT_SYMBOL(HBI_sleep);

EXPORT_SYMBOL(hbi_drv_init);
EXPORT_SYMBOL(hbi_drv_exit);


//module_init(hbi_drv_init);
//module_exit(hbi_drv_exit);

//MODULE_AUTHOR("Jean Bony <jean.bony@microsemi.com>");
//MODULE_DESCRIPTION("Microsemi Timberwolf Voice Processor Driver");
//MODULE_LICENSE("GPL");
