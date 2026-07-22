/*
* ssl.c - System Service Layer implementation for Raspberry Pi
*
*
* Copyright 2016 Microsemi Inc.
*
* This program is free software you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option)any later version.
*/

#include <linux/i2c.h>
#include "typedefs.h"
#include "ssl.h"
#include "hal.h"
#include "vproc_dbg.h"

/* Variable for current debug level set in the system.*/
VPROC_DBG_LVL vproc_dbg_lvl = DEBUG_LEVEL;
ssl_dev_info_t sdk_devices_info[VPROC_MAX_NUM_DEVS];

/* strcture defining devices opened by ssl */
struct ssl_dev
{
    void *pClient; /* pointer to device instance */
    bool inuse;   /* flag indicating entry in use */
};

/* structure defining driver */
struct ssl_drv{
    struct ssl_dev      dev[VPROC_MAX_NUM_DEVS]; /* number of devices
                                                   managed by driver */
    bool                initialised;  /* flag indicating driver is initialised */
};

/* variable keeping driver level information */
static struct ssl_drv ssl_drv_priv;

/* Macro to check for port handle validity */
#define CHK_PORT_HANDLE_VALIDITY(handle) \
{ \
    for(i=0;i<VPROC_MAX_NUM_DEVS;i++) \
    { \
        if((&(ssl_drv_priv.dev[i]) == handle) && (ssl_drv_priv.dev[i].inuse)) \
            break; \
    } \
    if( i >= VPROC_MAX_NUM_DEVS) \
        return SSL_STATUS_BAD_HANDLE; \
}

/* Macro to check if SSL driver intialised or not */
#define CHK_SSL_INITIALIZED(val) \
   if (!val) { \
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Driver not initialised\n"); \
      return SSL_STATUS_NOT_INIT; \
   }


ssl_status_t SSL_init(ssl_drv_cfg_t *pCfg, struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
    ssl_status_t status=SSL_STATUS_OK;
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

    /* if driver is already initialised, do nothing. return okay.
      Please note, we are not maintaining initialization count of the
      driver here. Once initialized by any layer, will be returned okay
      to subsequent calls regardless of where it is made from.
    */
    if(ssl_drv_priv.initialised == TRUE)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"SSL Driver already initialised\n");
        return SSL_STATUS_OK;
    }
    SSL_memset(&ssl_drv_priv,0,sizeof(struct ssl_drv));
    if(!hal_init(pClient, pDeviceId))
    {
        ssl_drv_priv.initialised = TRUE;
    }
    else
        status = SSL_STATUS_INTERNAL_ERR;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");
    return status;
}

/* Do not make any change to the definition of this function
 * Its implementation is already complete
 */
ssl_status_t sdk_register_board_devices_info(ssl_dev_info_t *pDevicesInfo)
{
     int i=0;
     for (i=0; i<VPROC_MAX_NUM_DEVS; i++)
     {
         if(pDevicesInfo->isboot) {
             if ((pDevicesInfo[i].pFirmware == NULL) || (pDevicesInfo[i].pConfig == NULL))
             {
                 VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                         "ERROR: driver device info structure is not properly initialized!!!\n");
                 return SSL_STATUS_RESOURCE_ERR;
             }
         }
         sdk_devices_info[i].chip       = pDevicesInfo[i].chip;
         sdk_devices_info[i].dev_addr   = pDevicesInfo[i].dev_addr;
         sdk_devices_info[i].isboot     = pDevicesInfo[i].isboot;
         sdk_devices_info[i].bus_num    = pDevicesInfo[i].bus_num;
         sdk_devices_info[i].dev_lock   = pDevicesInfo[i].dev_lock;
         sdk_devices_info[i].pFirmware  = pDevicesInfo[i].pFirmware;
         sdk_devices_info[i].pConfig    = pDevicesInfo[i].pConfig;
         sdk_devices_info[i].imageType  = pDevicesInfo[i].imageType;

         VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"sdk_devices_info[%d].chip = %d\n", i, sdk_devices_info[i].chip);
         VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"sdk_devices_info[%d].dev_addr = %d\n", i, sdk_devices_info[i].dev_addr);
         VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"sdk_devices_info[%d].isboot = %d\n", i, sdk_devices_info[i].isboot);
         VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"sdk_devices_info[%d].bus_num = %d\n", i, sdk_devices_info[i].bus_num);
         VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"sdk_devices_info[%d].dev_lock = %d\n", i, (int)sdk_devices_info[i].dev_lock);
     }
     return SSL_STATUS_OK;
}


ssl_status_t SSL_port_open(ssl_port_handle_t *pHandle,ssl_dev_cfg_t *pDevCfg)
{
   ssl_status_t status=SSL_STATUS_OK;
   int32_t      i;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

   /* Check if driver is initialised */
   CHK_SSL_INITIALIZED(ssl_drv_priv.initialised);

   /* Check for bad parameters */
   if(pHandle == NULL || pDevCfg == NULL)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL parameter pointer passed\n");
      return SSL_STATUS_INVALID_ARG;
   }

   /* Check if port can be opened */
   for(i=0;i<VPROC_MAX_NUM_DEVS;i++)
   {
     if(!(ssl_drv_priv.dev[i].inuse))
         break;
   }

   if(i >= VPROC_MAX_NUM_DEVS)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                     "Cannot open device. Limit exceed Maximum Allowed." \
                    " Please refer to platform file for maximum allowed port.\n");
     return SSL_STATUS_RESOURCE_ERR;
   }

   /* open device port */
   if(hal_open(&(ssl_drv_priv.dev[i].pClient),pDevCfg)<0)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"HAL Port open failed\n");
     return  SSL_STATUS_INTERNAL_ERR;
   }

   ssl_drv_priv.dev[i].inuse = TRUE;
   *pHandle = (ssl_port_handle_t)&(ssl_drv_priv.dev[i]);

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");
   return status;
}

ssl_status_t SSL_port_close(ssl_port_handle_t handle)
{
   ssl_status_t   status=SSL_STATUS_OK;
   int8_t         i;
   struct ssl_dev *pDev = (struct ssl_dev *)handle;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

   CHK_SSL_INITIALIZED(ssl_drv_priv.initialised);
   CHK_PORT_HANDLE_VALIDITY(pDev);

   if(hal_close(pDev->pClient)<0)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"HAL Port close failed\n");
     return SSL_STATUS_INTERNAL_ERR;
   }
   pDev->inuse = FALSE;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

   return status;
}

ssl_status_t SSL_term(void)
{
   ssl_status_t  status=SSL_STATUS_OK;
   int32_t        i;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

   if(ssl_drv_priv.initialised != TRUE)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"SSL Driver not initialised \n");
      return SSL_STATUS_OK;
   }

   /* Check if any of the resource is in use. if so, return error with message.
      This also means that if there are driver has been initialised multiple
      times as long as layer hold any resource it cannot be terminated */
   for(i=0;i<VPROC_MAX_NUM_DEVS;i++)
   {
     if(ssl_drv_priv.dev[i].inuse)
     {
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                        "Can't terminate driver. please close all ports\n");
         return SSL_STATUS_FAILED;
     }
   }

   if(hal_term()<0)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"hal_term() failed\n");
      status = SSL_STATUS_INTERNAL_ERR;
   }

   ssl_drv_priv.initialised = FALSE;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

   return status;
}

ssl_status_t SSL_port_rw(ssl_port_handle_t handle,ssl_port_access_t *pPortAccess)
{
   ssl_status_t   status=SSL_STATUS_OK;
   struct ssl_dev *pDev = (struct ssl_dev *)handle;
   int32_t        i;
   size_t         num_bytes_read=0;
   size_t         num_bytes_write=0;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

   CHK_SSL_INITIALIZED(ssl_drv_priv.initialised);
   CHK_PORT_HANDLE_VALIDITY(pDev);

   if(pPortAccess == NULL)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL port access info passed\n");
      return SSL_STATUS_INVALID_ARG;
   }

   num_bytes_read  = pPortAccess->nread;
   num_bytes_write = pPortAccess->nwrite;

   if(hal_port_rw(pDev->pClient,pPortAccess) <0)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"HAL Port read/write failed.\n");
     return SSL_STATUS_INTERNAL_ERR;
   }

   if(((pPortAccess->op_type & SSL_OP_PORT_RD) &&
       (pPortAccess->nread < num_bytes_read))  ||
       ((pPortAccess->op_type & SSL_OP_PORT_WR) &&
       (pPortAccess->nwrite < num_bytes_write)))
   {
     status = SSL_STATUS_OP_INCOMPLETE;
   }

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

   return status;
}

ssl_status_t SSL_port_write(ssl_port_handle_t handle,void *pSrc, size_t *pSize)
{
   ssl_status_t      status=SSL_STATUS_OK;
   struct ssl_dev    *pDev = (struct ssl_dev *)handle;
   ssl_port_access_t port_access;
   int               i;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter.....\n");

   CHK_SSL_INITIALIZED(ssl_drv_priv.initialised);
   CHK_PORT_HANDLE_VALIDITY(pDev);

   if(pSrc == NULL)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL param passed.\n");
      return SSL_STATUS_INVALID_ARG;
   }

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"4\n");

   port_access.pDst = NULL;
   port_access.pSrc = pSrc;
   port_access.nread = 0;
   port_access.nwrite = *pSize;
   port_access.op_type = SSL_OP_PORT_WR;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                  "Writing to client 0x%p\n",
                  (pDev->pClient));

   if(hal_port_rw(pDev->pClient,&port_access) < 0)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"HAL port write failed.n");
     return SSL_STATUS_INTERNAL_ERR;
   }

   if((port_access.op_type & SSL_OP_PORT_WR) && (port_access.nwrite < *pSize))
   {
      status = SSL_STATUS_OP_INCOMPLETE;
      *pSize = port_access.nwrite;
   }

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

   return status;
}
