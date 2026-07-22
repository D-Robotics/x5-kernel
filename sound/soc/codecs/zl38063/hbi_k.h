/*
* hbi_k.h - header file for hbi kernel space driver
*
* Copyright (c) 2016, Microsemi Corporation
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "hbi.h"
#define HBI_LNX_DRV_MAGIC 'q'

#define HBI_DEV_NAME "hbi"


typedef struct hbi_lnx_drv_rw_arg
{
    hbi_handle_t handle;
    hbi_status_t status;
    reg_addr_t   reg;
    user_buffer_t  *pData;
    size_t      len;
}hbi_lnx_drv_rw_arg_t;

typedef struct hbi_lnx_send_data_arg{
    hbi_handle_t handle;
    hbi_status_t status;
    hbi_data_t   data;
}hbi_lnx_send_data_arg_t;


typedef struct{
    hbi_handle_t handle;
    hbi_status_t status;
    int32_t    image_num;
}hbi_lnx_flash_save_fwrcfg_arg_t;

typedef struct{
    hbi_handle_t handle;
    hbi_status_t status;
    int32_t    image_num;
}hbi_lnx_flash_load_fwrcfg_arg_t;

typedef struct {
    hbi_handle_t handle;
    hbi_status_t status;
    int32_t   image_num;
}hbi_lnx_flash_erase_fwcfg_arg_t;

typedef struct {
   hbi_init_cfg_t *pCfg;
   hbi_status_t status;
}hbi_lnx_drv_cfg_t;

typedef struct {
   hbi_dev_cfg_t *pDevCfg;
   hbi_handle_t  *pHandle;
   hbi_status_t   status;
}hbi_lnx_open_arg_t;

typedef struct {
   hbi_handle_t handle;
   hbi_status_t status;
}hbi_lnx_close_arg_t;

typedef struct {
   hbi_handle_t handle;
   hbi_status_t status;
}hbi_lnx_drv_status_arg_t;

typedef struct {
   hbi_handle_t handle;
   hbi_status_t status;
}hbi_lnx_drv_arb_arg_t;


typedef hbi_lnx_drv_status_arg_t hbi_lnx_start_fw_arg_t;
typedef hbi_lnx_drv_status_arg_t hbi_lnx_drv_term_arg_t;
typedef hbi_lnx_drv_status_arg_t hbi_lnx_ldfw_done_arg_t;



#define HBI_OPEN        _IOWR(HBI_LNX_DRV_MAGIC,1,hbi_lnx_open_arg_t)
#define HBI_CLOSE       _IOWR(HBI_LNX_DRV_MAGIC,2,hbi_lnx_close_arg_t)
#define HBI_WRITE       _IOWR(HBI_LNX_DRV_MAGIC,3,hbi_lnx_drv_rw_arg_t)
#define HBI_READ        _IOWR(HBI_LNX_DRV_MAGIC,4,hbi_lnx_drv_rw_arg_t)
#define HBI_INIT       _IOWR(HBI_LNX_DRV_MAGIC,5,hbi_lnx_drv_cfg_t)
#define HBI_TERM       _IOWR(HBI_LNX_DRV_MAGIC,6,hbi_lnx_drv_term_arg_t)

/* TODO: We have alternative approach to combine few IOCTL and make them one Ex.
    HBI_FW_LOAD_SAVE_TO_FLASH -> this would contain loading firmware to device memory and save to flash
    HBI_FW_LOAD_RUN - > Loads and start execution from RAM
    HBI_FW_LOAD_FROM_FLASH_RUN -> Read from flash and run
    HBI_FW_LOAD_SAVE_TO_FLASH_RUN -> loads firmware, save  to flash and run (if possible)
    */

/* ioctl called when loading firmware from host.*/
#define HBI_LOAD_FW     _IOWR(HBI_LNX_DRV_MAGIC,7,hbi_lnx_send_data_arg_t)

/* Load FW complete into VPROC device memory
   (should be called after HBI_LOAD_FW or HBI_FLASH_LOAD_FWR_CFGREC) */
#define HBI_LOAD_FW_COMPLETE _IOWR(HBI_LNX_DRV_MAGIC,8,hbi_lnx_ldfw_done_arg_t)

/* start firmware execution i.e. loaded into VPROC device memory
   (should be called after HBI_LOAD_FW or HBI_FLASH_LOAD_FWR_CFGREC) */
#define HBI_START_FW    _IOWR(HBI_LNX_DRV_MAGIC,9,hbi_lnx_start_fw_arg_t)

/* Following ioctl are defined based in FLASH_PRESENT macro */

/* ioctl to save loaded firmware into RAM to flash.
   should be called after HBI_LOAD_FW*/
#define HBI_FLASH_SAVE_FWR_CFGREC  _IOWR(HBI_LNX_DRV_MAGIC,10,hbi_lnx_flash_save_fwrcfg_arg_t)

/* ioctl to read specified firmware image from flash.
   can be called anytime */
#define HBI_FLASH_LOAD_FWR_CFGREC  _IOWR(HBI_LNX_DRV_MAGIC,11,hbi_lnx_flash_load_fwrcfg_arg_t)

/* ioctl to erase specific firmware image from flash. */
#define HBI_FLASH_ERASE_FWRCFGREC  _IOWR(HBI_LNX_DRV_MAGIC,12,hbi_lnx_flash_erase_fwcfg_arg_t)

/* ioctl to erase complete firmware image from flash. */
#define HBI_FLASH_ERASE_WHOLE      _IOWR(HBI_LNX_DRV_MAGIC,13,hbi_lnx_flash_erase_fwcfg_arg_t)

#define HBI_UPDATE                 _IOWR(HBI_LNX_DRV_MAGIC,14,hbi_dev_info_t)

/* ioctl called when loading config from host.*/
#define HBI_LOAD_CFG     _IOWR(HBI_LNX_DRV_MAGIC,15,hbi_lnx_send_data_arg_t)
#define HBI_FLASH_SAVE_CFGREC  _IOWR(HBI_LNX_DRV_MAGIC,16,hbi_lnx_flash_save_fwrcfg_arg_t)

#define HBI_SLEEP     _IOWR(HBI_LNX_DRV_MAGIC,17,hbi_lnx_drv_arb_arg_t)
#define HBI_WAKE      _IOWR(HBI_LNX_DRV_MAGIC,18,hbi_lnx_drv_arb_arg_t)
