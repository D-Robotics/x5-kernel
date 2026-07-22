/*
* hal_port.c - spi or I2C driver implmentation for HBI
*
* Hardware Abstraction Layer for Voice processor devices
* Every successful call would return 0 or
* a linux error code as defined in linux errno.h
*
* Copyright 2016 Microsemi Inc.
*
* This program is free software you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option)any later version.
*/

#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include "typedefs.h"
#include "ssl.h"
#include "hal.h"
#include "vproc_dbg.h"
#include "fwr_image_headers.h"

#undef HAL_DEBUG
static ssl_dev_info_t sdk_board_devices_info[] =
{
    {
        .chip       = 38063,    /*Microsemi chip number without the ZL*/
        .bus_num    = 5,        /*SPI or I2C bus number*/
        .dev_addr   = 0x45,     /*SPI chip select or I2C address*/
        .isboot     = FALSE,    /*set this TRUE if a device firmware has to be loaded at boot*/
        .pFirmware  = NULL,     /*a pointer to either the filename if in *.bin format or data array  if in c code format*/
        .pConfig    = NULL,     /*a pointer to either the filename if in *.bin format or data array  if in c code format*/
        .dev_lock   = 0,        /* lock to serialise device access */
        .imageType  = 1,        /*0: for static *.h, 1: for *.bin */
    },
};
int dev_id = 0;
static struct i2c_client devClient[VPROC_MAX_NUM_DEVS];

//=======================================================================================

/*hal_init() - This function is the first function call by the driver upon install
 *             any specific board/platform setup must be done by this funtion
 *  Args:
 *         none
 * Return:
 *          0 if success, a negative number if failure
 */
int hal_init(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
    int status = sdk_register_board_devices_info(sdk_board_devices_info);
    if (status < 0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"sdk_register_board_devices_info() failed error = %d\n", status);
    }
    devClient[dev_id++] = *pClient;
    dev_id = 0;
    return status;
}

int hal_term()
{
    //i2c_del_driver(&vproc_driver);
    dev_id = 0;
    return 0;
}


/*hal_open() - use this function to open one or multiple instances of the driver
 *  Args:
 *         pHandle : device driver handle, basically a refence to how to access the device
 *         pDevCfg : pointer to the device bus number and the address on the bus to open
 * Return:
 *         0 if success, a negative number if failure
 */
int hal_open(void **ppHandle,void *pDevCfg)
{
    tw_device_id_t deviceId;

    ssl_dev_cfg_t *pDev = (ssl_dev_cfg_t *)pDevCfg;
    struct i2c_adapter *pAdap=NULL;
    struct i2c_client  *pClient = NULL;
    struct i2c_board_info bi;
    if ((pDev->deviceId < 0) || (pDev->deviceId >= VPROC_MAX_NUM_DEVS)){
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid Device Id, device ID must be a value from 0 to %d \n", VPROC_MAX_NUM_DEVS-1);
        return -EINVAL;
    }

    if(pDev == NULL){
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid Device Cfg Reference\n");
        return -EINVAL;
    }
    deviceId = pDev->deviceId;
    pDev->bus_num = sdk_board_devices_info[deviceId].bus_num;
    pDev->dev_addr =sdk_board_devices_info[deviceId].dev_addr;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO, "Hal Opening device %d with addr : 0x%x bus num %d\n", deviceId, pDev->dev_addr, pDev->bus_num);

    pClient = &devClient[deviceId];
    /* get the controller driver through bus num */

    pAdap = i2c_get_adapter(pDev->bus_num);
    if(pAdap==NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid Bus Num %d \n",pDev->bus_num);
        return SSL_STATUS_INVALID_ARG;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"i2c adap name %s \n",pAdap->name);
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"i2c pDev->pDevName %s \n",pDev->pDevName);
    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"i2c pDev->dev_addr 0x%02X \n",pDev->dev_addr);

    pClient->addr = pDev->dev_addr;
    pClient->adapter = pAdap;
    if(pDev->pDevName != NULL)
    {
        strcpy(pClient->name,pDev->pDevName);
    }

    memset(&bi,0,sizeof(bi));
    if(pDev->pDevName != NULL)
    {
        strcpy(bi.type,pDev->pDevName);
    }
    bi.addr = pDev->dev_addr;
    #if 0
    pClient = i2c_new_device(pAdap,(struct i2c_board_info const *)&bi);
	//pClient = i2c_new_dummy(pAdap,0x45);
    if(pClient == NULL)
    {
     /* call failed either because address is invalid, valid but occupied
        or there is resource err. Just return code to try again later */
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"i2c device instantiation failed\n");
        return -EAGAIN;
    }
    #endif
   *((struct i2c_client **)ppHandle) =  pClient;
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Opened i2c device %d adapter:%s addr:0x%02X...\n", dev_id, pClient->adapter->name, pClient->addr);

    return 0;
}

/*hal_close() - Since multiple instance of the driver can be opened simultaneously
 *              use this function to close a particular instance of the driver
 *  Args:
 *         pHandle : device driver handle, basically a refence to how to access the device
 * Return:
 *         0 if success, a negative number if failure
 */

int hal_close(void *pHandle)
{
    if(pHandle == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL client handle passed\n");
        return -EINVAL;
    }

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                     "Unregistering client 0x%p\n", pHandle);

    /*Do not unregister the SPI if using device tree registration*/
#ifndef SUPPORT_LINUX_DEVICE_TREE_OF_MATCHING
    i2c_unregister_device((struct i2c_client *) pHandle);
#endif /*SUPPORT_LINUX_DEVICE_TREE_OF_MATCHING*/
    return 0;
}


/*hal_port_rw():  this function is used for both read and write accesses
 *  write: send data from the master to the slave device
 *  read:  master receives data from the slave device
 *  Args:
 *         pHandle : device driver handle, basically a refence to how to access the device
 *         pPortAccess: the access type, data to send and the buffer to receive the data
 * Return:
 *         0 if success, a negative number if failure
 */
int hal_port_rw(void *pHandle,void *pPortAccess)
{
    int                 ret=0;
    ssl_port_access_t   *pPort = (ssl_port_access_t *)pPortAccess;
    struct i2c_client *pClient = pHandle;
    struct i2c_msg msg[2];
    int                 msgnum=0;
    ssl_op_t          op_type;
#ifdef HAL_DEBUG
    int                  i;
#endif

    if(pHandle == NULL || pPort == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid Parameters\n");
        return -EINVAL;
    }

    op_type = pPort->op_type;
    memset(msg,0,sizeof(msg));
    if(op_type & SSL_OP_PORT_WR)
    {
        if(pPort->pSrc == NULL)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL src buffer passed\n");
            return -EINVAL;
        }
        msg[msgnum].addr = pClient->addr;
        msg[msgnum].flags = 0;
        msg[msgnum].buf = pPort->pSrc;
        msg[msgnum].len = pPort->nwrite;

#ifdef HAL_DEBUG
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"writing %d bytes..\n",pPort->nwrite);

        for(i=0;i<pPort->nwrite;i++)
        {
            printk("0x%x\t",((uint8_t *)(pPort->pSrc))[i]);
        }
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"\n");
#endif
        msgnum++;
    }

    if(op_type & SSL_OP_PORT_RD)
    {
        if(pPort->pDst == NULL)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL destination buffer passed\n");
            return -EINVAL;
        }
        msg[msgnum].addr = pClient->addr;
        msg[msgnum].flags = I2C_M_RD;
        msg[msgnum].buf = pPort->pDst;
        msg[msgnum].len = pPort->nread;
        msgnum++;
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"read %ld bytes..\n",pPort->nread);
    }
    ret = i2c_transfer(pClient->adapter,msg,msgnum);
    if(ret < 0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"failed with Error %d\n",ret);
    }
    #ifdef HAL_DEBUG
    if(!ret && (msgnum >=1))
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Received...\n");
        for(i=0;i<pPort->nread;i++)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"0x%x\t",((uint8_t *)(pPort->pDst))[i]);
        }
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"\n");
    }
    #endif
    return ret;
}
