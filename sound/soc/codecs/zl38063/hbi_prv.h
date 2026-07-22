/*
* hbi_prv.h  - Internal header Used by  master hbi.c
*
* Copyright 2016 Microsemi Inc.
*/

#ifndef __HBI_PRV_H__
#define __HBI_PRV_H__



#ifndef HBI_MAX_INST_PER_DEV
#define HBI_MAX_INST_PER_DEV 1
#endif

#define HBI_MAX_INSTANCES (HBI_MAX_INST_PER_DEV * VPROC_MAX_NUM_DEVS)


#define HBI_LOCK(lock, wait) \
   if(lock) SSL_lock(lock,wait); \


#define HBI_UNLOCK(lock) \
   if(lock) SSL_unlock(lock);

#define CHK_SSL_STATUS(status)  \
   if(status != SSL_STATUS_OK) { \
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ALL, \
                     "SSL call returned Err 0x%x\n", \
                     status); \
      return (HBI_STATUS_INTERNAL_ERR); \
   }

typedef enum
{
    HBI_DEV_ENDIAN_BIG,
    HBI_DEV_ENDIAN_LITTLE
}hbi_dev_endian_t;

typedef enum
{
    HBI_ATTRIB_DEV_ENDIAN,
    HBI_ATTRIB_SLEEP,
    HBI_ATTRIB_CONFIG_INT
}hbi_attrib_t;

/* structure specific to device */
struct  vproc_dev{
    uint32_t                endianness; /* endian device is operating */
    hbi_dev_cfg_t           dev_cfg;    /* structure containing user pass device configuration */
    ssl_port_handle_t       port_handle;/* reference handle of port */
    ssl_lock_handle_t       port_lock;  /* lock to serialise device access calls */
    uint32_t                instances[HBI_MAX_INST_PER_DEV]; /* list of users on this device */
    uint32_t                num_cur_instances; /* total number of users of device */
    unsigned char           buffer[HBI_BUFFER_SIZE];
    uint32_t                inuse;
};

hbi_status_t internal_hbi_open(struct vproc_dev *pDev,
                               hbi_dev_cfg_t *pDevCfg);

hbi_status_t internal_hbi_set_attrib(struct vproc_dev *pDev,
                                   hbi_attrib_t attrib,
                                   void *pVal);
hbi_status_t internal_hbi_set_command(struct vproc_dev *pDev,
                                    hbi_cmd_t cmd,
                                    void *pCmdArgs);
hbi_status_t internal_hbi_read(struct vproc_dev *pDev,
                                reg_addr_t reg_addr,
                                user_buffer_t *pData,
                                size_t size);
hbi_status_t internal_hbi_write(struct vproc_dev *pDev,
                                 reg_addr_t reg_addr,
                                 user_buffer_t *pData,
                                 size_t size);
hbi_status_t internal_hbi_reset(struct vproc_dev *pDev,
                                 hbi_rst_mode_t reset_mode);

hbi_status_t internal_hbi_get_hdr(hbi_data_t *,hbi_img_hdr_t *);


#endif
