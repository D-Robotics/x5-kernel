/*
* hbi.c  --  Host Bus Interface Implementation
*
* Copyright 2016 Microsemi Inc.
*/

#include <linux/i2c.h>

#include <typedefs.h>
#include <chip.h>
#include <ssl.h>
#include <hbi.h>
#include <hbi_prv.h>
#include <vproc_dbg.h>

extern int zl380xx_reset_gpio_high(void);
extern int zl380xx_reset_gpio_low(void);

/*
    Each device and user correlation is depicted like this:
     -----------      ------------        ------------
    |          |     |           |       |           |
    |instance0 |     | instance 1|.......|instance n |
    |          |     |           |       |           |
    |-----------     ------------         -----------
      /|\               /|\                   /|\
       |                 |                     |
      \|/               \|/                   \|/
     -- ---------------------------------------------
    |             Device                            |
    |                                               |
     -----------------------------------------------

*/

/*specific to each user on device */
struct vproc_inst
{
    struct vproc_dev        *pDev;      /* pointer to device user is using */
    bool                     inuse; /* flag indicate if current item in list free or occupied */
};

struct hbi_drv_priv
{
    struct vproc_dev   dev_list[VPROC_MAX_NUM_DEVS]; /* maximum number of devices supported by HBI driver */
    struct vproc_inst  inst_list[HBI_MAX_INSTANCES]; /* maximum number of users supported by HBI driver (for multiple apps support of single or multiple devices) */
    hbi_init_cfg_t     cfg;
    bool               bInitialized;  /* flag to indicate if driver is
                                         initialised or not */
    uint32_t           num_opened_dev;  /* number of currently opened devices */
    uint32_t           num_instances;   /* number of current users of driver */
    uint32_t           init_count;
};

static struct hbi_drv_priv hbi_priv;

#define CHK_IF_INITED(val)  \
   if( val != TRUE){ \
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "Driver not intialised\n"); \
         return HBI_STATUS_NOT_INIT; \
    }
/*
   Since handle is an index to inst_list array free slot which can be
   greater than or less than num_cur_instances.
*/
#define CHK_VALID_HANDLE(handle)  \
   if((handle >= HBI_MAX_INSTANCES) || \
      (hbi_priv.inst_list[handle].inuse == FALSE)) { \
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid Handle\n"); \
        return HBI_STATUS_BAD_HANDLE; \
    }

#define CHK_NULL_ARG(arg)  \
   if( arg == NULL){ \
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL parameter passed\n"); \
      return HBI_STATUS_INVALID_ARG; \
   }

#define HBI_INST_INVALID_ID -1


hbi_status_t HBI_init(hbi_init_cfg_t *pCfg, struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
   hbi_status_t  status = HBI_STATUS_SUCCESS;
   ssl_status_t  ssl_status;

   if(hbi_priv.bInitialized)
   {
     HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER)
     hbi_priv.init_count++;
     HBI_UNLOCK(hbi_priv.cfg.lock);

     VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"HBI already initialized\n");
     return HBI_STATUS_SUCCESS;
   }

   VPROC_DBG_SET_LVL(DEBUG_LEVEL);

   SSL_memset(&hbi_priv,0,sizeof(struct hbi_drv_priv));

   ssl_status = SSL_init(NULL, pClient, pDeviceId);
   CHK_SSL_STATUS(ssl_status);

   if(pCfg!=NULL)
      hbi_priv.cfg.lock = pCfg->lock;

   hbi_priv.bInitialized = TRUE;
   hbi_priv.init_count++;
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"HBI initialized %d, opened count %d\n", hbi_priv.bInitialized, hbi_priv.init_count);
   return status;
}

hbi_status_t HBI_open(hbi_handle_t *pHandle, hbi_dev_cfg_t *pDevCfg)
{
    ssl_status_t         ssl_status;
    ssl_dev_cfg_t        ssl_devcfg;
    int                  i;
    struct vproc_dev    *pDev;
    struct vproc_inst   *pInst;
    hbi_status_t         status=HBI_STATUS_SUCCESS;
    uint8_t              inst_index;
    uint8_t              dev_index;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter ...\n");
    zl380xx_reset_gpio_high();

//#ifdef INIT_TERM_AUTO
#if 0
    status = HBI_init(NULL);
    if (status != HBI_STATUS_SUCCESS)
    {
        return status;
    }
#endif
    CHK_IF_INITED(hbi_priv.bInitialized);
    CHK_NULL_ARG(pHandle);
    CHK_NULL_ARG(pDevCfg);

    /* check if instance limit been reached */
    if(hbi_priv.num_instances >= HBI_MAX_INSTANCES)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                        "Number of instance limit reached on driver\n");
        return HBI_STATUS_RESOURCE_ERR;
    }

    HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);

    /* find out free slot to store user info */
    for(i=0;i<HBI_MAX_INSTANCES;i++)
    {
        if(hbi_priv.inst_list[i].inuse == FALSE)
            break;
    }

    pInst = &(hbi_priv.inst_list[i]);
    inst_index = i;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"inst_index %d\n",i);

    /* check if unit already opened */
    for(i=0;i<VPROC_MAX_NUM_DEVS;i++)
    {
        if((hbi_priv.dev_list[i].dev_cfg.deviceId == pDevCfg->deviceId) &&
           (hbi_priv.dev_list[i].port_handle != (ssl_port_handle_t)NULL))
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Unit already opened\n");
            pDev = &(hbi_priv.dev_list[i]);
            HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);

            /* check if any more users can be opened on device */
            if(pDev->num_cur_instances >= HBI_MAX_INST_PER_DEV)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                               "device is already opened." \
                               "No more users can be opened on device\n");
                HBI_UNLOCK(pDev->port_lock);
                HBI_UNLOCK(hbi_priv.cfg.lock);
                return HBI_STATUS_RESOURCE_ERR;
            }
            HBI_UNLOCK(pDev->port_lock);
            break;
        }
    }
    if(i >= VPROC_MAX_NUM_DEVS)
    {
        /* if device isn't already opened,
            check if there's room to open a new device */
        if(hbi_priv.num_opened_dev >= VPROC_MAX_NUM_DEVS)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                           "Maximum limit reached.Cannot open new device\n");
            HBI_UNLOCK(hbi_priv.cfg.lock);
            return HBI_STATUS_RESOURCE_ERR;
        }

        /* find free device slot */
        for(i=0;i<VPROC_MAX_NUM_DEVS;i++)
        {
            if(hbi_priv.dev_list[i].inuse == FALSE)
                break;
        }

        pDev = &(hbi_priv.dev_list[i]);
        dev_index = i;

        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"dev_index %d\n",i);

        /* open an HBI to new device */
        SSL_memset(&ssl_devcfg,0,sizeof(ssl_dev_cfg_t));

        ssl_devcfg.bus_num = pDevCfg->bus_num;
        ssl_devcfg.dev_addr = pDevCfg->dev_addr;
        ssl_devcfg.deviceId = pDevCfg->deviceId;

        if(pDevCfg->pDevName != NULL)
        {
            ssl_devcfg.pDevName = pDevCfg->pDevName;
        }

        ssl_status = SSL_port_open(&(pDev->port_handle),&ssl_devcfg);
        if(ssl_status != SSL_STATUS_OK)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                           "SSL_port_open failed.Err 0x%x\n",
                           ssl_status);
            HBI_UNLOCK(hbi_priv.cfg.lock);
            return HBI_STATUS_INTERNAL_ERR;
        }

        if(pDevCfg->dev_lock)
         pDev->port_lock = pDevCfg->dev_lock;

        SSL_memcpy(&(pDev->dev_cfg),pDevCfg,sizeof(hbi_dev_cfg_t));
        hbi_priv.num_opened_dev++;
        hbi_priv.dev_list[dev_index].inuse = TRUE;

        /* Call device specific open function */
        status = internal_hbi_open(pDev,pDevCfg);

    }

    /* update device info */
    pDev->instances[pDev->num_cur_instances] = inst_index;
    pDev->num_cur_instances++;

    /* update user */
    pInst->pDev = pDev;
    pInst->inuse = TRUE;

    /*update driver global information */
    hbi_priv.num_instances++;

    *pHandle = (hbi_handle_t) inst_index;
    HBI_UNLOCK(hbi_priv.cfg.lock);
    return status;
}

hbi_status_t HBI_close(hbi_handle_t handle)
{
    ssl_status_t          ssl_status;
    hbi_status_t          status = HBI_STATUS_SUCCESS;
    hbi_handle_t        inst_index = handle;
    struct vproc_inst   *pInst;
    struct vproc_dev    *pDev;
    int                 i;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter (handle 0x%x)\n",handle);

    zl380xx_reset_gpio_low();

    CHK_IF_INITED(hbi_priv.bInitialized);
    CHK_VALID_HANDLE(inst_index);

    HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);

    pInst = &(hbi_priv.inst_list[inst_index]);

    pDev = pInst->pDev;

    /* check if this is the only user of device, then close the device */
    if(pDev->num_cur_instances == 1)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Close the device now\n");

        HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);
        ssl_status = SSL_port_close(pDev->port_handle);
        HBI_UNLOCK(pDev->port_lock);

        if(ssl_status != SSL_STATUS_OK)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"SSL_port_close failed.Err 0x%x\n",ssl_status);
            HBI_UNLOCK(hbi_priv.cfg.lock);
            return HBI_STATUS_INTERNAL_ERR;
        }

        pDev->port_handle = 0;
        pDev->dev_cfg.pDevName = NULL;
        pDev->inuse = FALSE;
        hbi_priv.num_opened_dev--;
    }

    pInst->pDev = NULL;
    pInst->inuse = FALSE;

    for(i=0;i<pDev->num_cur_instances;i++)
    {
        if(inst_index == pDev->instances[i])
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                           "Mark current user invalid on device\n");
            pDev->instances[i] = HBI_INST_INVALID_ID;
            break;
        }
    }

    pDev->num_cur_instances--;
    hbi_priv.num_instances--;

    HBI_UNLOCK(hbi_priv.cfg.lock);
#ifdef INIT_TERM_AUTO
    status = HBI_term();
    if(status != HBI_STATUS_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR, "driver term error\n");
        return status;
    }
#endif
    return status;
}

hbi_status_t HBI_term(void)
{
   ssl_status_t ssl_status;

   if(hbi_priv.bInitialized == FALSE)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"HBI driver not intiatialised\n");
      return HBI_STATUS_SUCCESS;
   }

   HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
   /* if there're multiple user of driver, just decrement count and return
   */
   if(hbi_priv.init_count>1)
   {
      hbi_priv.init_count--;
      HBI_UNLOCK(hbi_priv.cfg.lock);
      return HBI_STATUS_SUCCESS;
   }

   /* check if there's any user still using it */
   if(hbi_priv.num_instances || hbi_priv.num_opened_dev)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                     "driver in use. please free up all resouces\n");
     HBI_UNLOCK(hbi_priv.cfg.lock);
     return HBI_STATUS_INVALID_STATE;
   }

   ssl_status = SSL_term();
   HBI_UNLOCK(hbi_priv.cfg.lock);
   CHK_SSL_STATUS(ssl_status);

   hbi_priv.init_count--;
   hbi_priv.bInitialized=FALSE;

   return HBI_STATUS_SUCCESS;
}

hbi_status_t HBI_read(hbi_handle_t handle, reg_addr_t reg, user_buffer_t *pOutput,
                    size_t length)
{
   hbi_handle_t inst_index = handle;
   struct vproc_dev *pDev;
   hbi_status_t status;

   CHK_IF_INITED(hbi_priv.bInitialized);
   CHK_NULL_ARG(pOutput);
   CHK_VALID_HANDLE(inst_index);

   HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
   pDev = hbi_priv.inst_list[inst_index].pDev;
   HBI_UNLOCK(hbi_priv.cfg.lock);

   status = internal_hbi_read(pDev,reg,pOutput,length);

   return status;
}

hbi_status_t HBI_write(hbi_handle_t handle, reg_addr_t reg, user_buffer_t *pInput,
                        size_t length)
{
   hbi_status_t status = HBI_STATUS_SUCCESS;
   hbi_handle_t inst_index = handle;
   struct vproc_dev *pDev;

   CHK_IF_INITED(hbi_priv.bInitialized);
   CHK_NULL_ARG(pInput);
   CHK_VALID_HANDLE(inst_index);

   HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
   pDev = hbi_priv.inst_list[inst_index].pDev;
   HBI_UNLOCK(hbi_priv.cfg.lock);

   status = internal_hbi_write(pDev,reg,pInput,length);

   return status;
}

hbi_status_t HBI_set_command(hbi_handle_t handle,hbi_cmd_t cmd,void *pCmdArgs)
{
    hbi_status_t status=HBI_STATUS_SUCCESS;
    hbi_handle_t inst_indx = handle;
    struct vproc_dev *pDev;

    CHK_IF_INITED(hbi_priv.bInitialized);
    CHK_VALID_HANDLE(inst_indx);

    HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
    pDev = hbi_priv.inst_list[inst_indx].pDev;
    HBI_UNLOCK(hbi_priv.cfg.lock);

    status = internal_hbi_set_command(pDev,cmd,pCmdArgs);

    return status;
}

hbi_status_t HBI_reset(hbi_handle_t handle, hbi_rst_mode_t mode)
{
    hbi_status_t status = HBI_STATUS_SUCCESS;
    hbi_handle_t inst_indx = handle;
    struct vproc_dev *pDev;

    CHK_IF_INITED(hbi_priv.bInitialized);
    CHK_VALID_HANDLE(inst_indx);

    HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
    pDev = hbi_priv.inst_list[inst_indx].pDev;
    HBI_UNLOCK(hbi_priv.cfg.lock);

    status = internal_hbi_reset(pDev,mode);

    return status;


}

hbi_status_t HBI_sleep(hbi_handle_t handle)
{
   hbi_status_t status;
   struct vproc_dev *pDev;
   int val;

   CHK_IF_INITED(hbi_priv.bInitialized);
   CHK_VALID_HANDLE(handle);

   HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
   pDev = hbi_priv.inst_list[handle].pDev;
   HBI_UNLOCK(hbi_priv.cfg.lock);

   val = 1;
   status = internal_hbi_set_attrib(pDev,HBI_ATTRIB_SLEEP,&val);

   return status;
}

hbi_status_t HBI_wake(hbi_handle_t handle)
{
   hbi_status_t status;
   struct vproc_dev *pDev;
   int val=0;

   CHK_IF_INITED(hbi_priv.bInitialized);
   CHK_VALID_HANDLE(handle);

   HBI_LOCK(hbi_priv.cfg.lock,SSL_WAIT_FOREVER);
   pDev = hbi_priv.inst_list[handle].pDev;
   HBI_UNLOCK(hbi_priv.cfg.lock);

   val=0;
   status = internal_hbi_set_attrib(pDev,HBI_ATTRIB_SLEEP,&val);

   return status;
}

hbi_status_t HBI_get_header(hbi_data_t *pImg, hbi_img_hdr_t *pHdr)
{
   hbi_status_t status;
   status = internal_hbi_get_hdr(pImg,pHdr);
   return status;
}
