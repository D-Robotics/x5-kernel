/*
* hbi_tw.c  -- Internal implementation for specific device family. Used by
*              master hbi.c
*
* Copyright 2016 Microsemi Inc.
*/

#include "typedefs.h"
#include "chip.h"
#include "vproc_dbg.h"
#include "ssl.h"
#include "hbi.h"
#include "hbi_prv.h"

#undef ENABLE_SAVE_CFGREC_TO_FLASH

#define TW_RST_WAIT_PERIOD      5000
#define TW_RST_WAIT_CNT         10000
#define PAGE_255                0xFF
#define ZL380xx_HBI_TP_FRAME_HDR_MIN_LEN 2

/*TWOLF MACROS-------------------------------*/
/* local defines */
#define ZL380xx_HBI_TP_FRAME_HDR_MAX_LEN       4
#define HOST_CMD_CMD_IN_PROGRESS               0xFFFF  /*wait command is in progress */
#define TOTAL_FWR_DATA_WORD_PER_LINE           24
#define TOTAL_FWR_DATA_BYTE_PER_LINE           128
#define TWOLF_MBCMDREG_SPINWAIT                10000
#define ZL380xx_RESET_IN_PROGRESS              0x0001
#define MAX_HBI_BYTES_PER_ACCESS               256
/* The same driver write function is used for both writing to register
 * and also to load the firmware and config into the device
 * key to differentiate between the actual command and the data
 */
#define HBI_ACCESS_TYPE_WR_CMD   1  /*to specify that the access is write command*/
#define HBI_ACCESS_TYPE_LOAD_FWR 3 /*to specify that the access is to load firmware*/

#define HBI_PAGED_READ(offset,length) \
    ((uint16_t)(((uint16_t)(offset) << 8) | (length)))
#define HBI_DIRECT_READ(offset,length) \
    ((uint16_t)(0x8000 | ((uint16_t)(offset) << 8) | (length)))
#define HBI_PAGED_WRITE(offset,length) \
    ((uint16_t)(HBI_PAGED_READ(offset,length) | 0x0080))
#define HBI_DIRECT_WRITE(offset,length) \
    ((uint16_t)(HBI_DIRECT_READ(offset,length) | 0x0080))
#define HBI_GLOBAL_DIRECT_WRITE(offset,length) \
    ((uint16_t)(0xFC00 | ((offset) << 4) | (length)))
#define HBI_CONFIGURE(pinConfig) \
    ((uint16_t)(0xFD00 | (pinConfig)))
#define HBI_SELECT_PAGE(page) \
    ((uint16_t)(0xFE00 | (page)))
#define HBI_NO_OP \
    ((uint16_t)0xFFFF)

/* Macro to check for HBI status okay or not */
#define CHK_STATUS(status)  \
   if (status != HBI_STATUS_SUCCESS) { \
           VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"ERROR %d: \n", status); \
           return status; \
       }

/* Macro that helps identifying host and device endian compatibility */
#if (HOST_ENDIAN_LITTLE)
#define MATCH_ENDIAN(devEndian) (devEndian == HBI_DEV_ENDIAN_LITTLE)
#else
#define MATCH_ENDIAN(devEndian) (devEndian == HBI_DEV_ENDIAN_BIG)
#endif

/* Macro that reformat values read/written to device in device format */
#define HBI_VAL(dev,val)  \
   (!MATCH_ENDIAN(((struct vproc_dev *)dev)->endianness) ? \
   (((val & 0xFF) << 8) | (val >>8)) : val)

/* TW firmware bin image header description */
/* header field width in bytes */
#define VER_WIDTH          1
#define FORMAT_WIDTH       1
#define OPN_WIDTH          2
#define BLOCK_SIZE_WIDTH   2
#define TOTAL_LEN_WIDTH    4
#define RESERVE_LEN_WIDTH  2
/* unused right now */
#define FWR_CHKSUM_LEN     1
#define IMG_HDR_LEN    \
   (VER_WIDTH + FORMAT_WIDTH +  \
   OPN_WIDTH + BLOCK_SIZE_WIDTH + TOTAL_LEN_WIDTH + RESERVE_LEN_WIDTH)

/* field index */
#define VER_INDX        0
#define FORMAT_INDX     (VER_INDX+VER_WIDTH)
#define FWR_OPN_INDX    (FORMAT_INDX+FORMAT_WIDTH)
#define BLOCK_SIZE_INDX (FWR_OPN_INDX + OPN_WIDTH)
#define TOTAL_LEN_INDX  (BLOCK_SIZE_INDX + BLOCK_SIZE_WIDTH)

/* Image Version Info */
#define IMG_VERSION_MAJOR_SHIFT 6
#define IMG_VERSION_MINOR_SHIFT 4

/* image type fields */
#define IMG_HDR_TYPE_SHIFT    6
#define IMG_HDR_ENDIAN_SHIFT  5

struct tw_hdr
{
    uint8_t cmd[4];
    size_t   cmdlen;
};


 /* tw_mbox_acquire(): use this function to
  *                    check whether the host or the device owns the mailbox right
  * \param[in]    dev - device pointer
  *
  * Return: error code
  *
  */
 static inline hbi_status_t tw_mbox_acquire(struct vproc_dev *pDev)
 {
     hbi_status_t status =0;
     uint16_t  i=0;
     uint16_t  temp = 0x0BAD;
    /*Check whether the host owns the command register*/
     for (i = 0; i < TWOLF_MBCMDREG_SPINWAIT; i++)
     {
        status = internal_hbi_read(pDev,
                                    ZL380xx_HOST_SW_FLAGS_REG,
                                    (user_buffer_t *)&temp,
                                    sizeof(temp));
        CHK_STATUS(status);

        temp = HBI_VAL(pDev,temp);
        if (!(temp & ZL380xx_HOST_SW_FLAGS_HOST_CMD))
            break;
        SSL_delay(10);
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                        "cmdbox =0x%x timeout count = %d: \n",
                        temp, i);
     }

     if ((i>= TWOLF_MBCMDREG_SPINWAIT) && (temp & ZL380xx_HOST_SW_FLAGS_HOST_CMD))
     {
         return HBI_STATUS_RESOURCE_ERR;
     }

     return HBI_STATUS_SUCCESS;
 }

 /* tw_cmdresult_check(): Checks the status register to know result of command
 *                        issued through tw_wr_cmdreg()
 * \param[in]      device pointer
 *
 * Return: result value as read from T-Wolf status register
 *
 */
static inline int tw_cmdresult_check(struct vproc_dev *pDev)
{
    hbi_status_t status = HBI_STATUS_SUCCESS;
    uint16_t     buf;

    status = internal_hbi_read(pDev,
                              ZL380xx_HOST_CMD_PARAM_RESULT_REG,
                              (user_buffer_t *)&buf,sizeof(buf));
    CHK_STATUS(status);

    if (buf !=0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                        "Command ...Resultcode = 0x%04x\n", buf);
    }
    return (int)buf;
}
 /* tw_cmdreg_acquire(): use this function to
  *                      check whether the last command is completed
  * \param[in]         device pointer
  *
  * Return: error code
  */
 static inline hbi_status_t tw_cmdreg_acquire(struct vproc_dev *pDev)
{
   hbi_status_t status =0;
   uint16_t   i=0;
   uint16_t   temp = 0x0BAD;

   for (i = 0; i < TWOLF_MBCMDREG_SPINWAIT; i++)
   {
      status = internal_hbi_read(pDev,
                                 ZL380xx_HOST_CMD_REG,
                                 (user_buffer_t *)&temp,sizeof(temp));
      CHK_STATUS(status);

      if (temp == HOST_CMD_IDLE)
      break;

      SSL_delay(10); /*wait*/
   }

   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
            "cmdReg =0x%x timeout count = %d: \n", temp, i);

   if ((i>= TWOLF_MBCMDREG_SPINWAIT) && (temp != HOST_CMD_IDLE))
      return HBI_STATUS_RESOURCE_ERR;

   return HBI_STATUS_SUCCESS;
}


 static hbi_status_t tw_wr_cmdreg(struct vproc_dev *pDev,uint16_t cmd)
{
    hbi_status_t status = HBI_STATUS_SUCCESS;

    /*
        Writing a host command into Command register and processing it
        is a 3-step process
        1. check if there's any command in process by monitoring sw flag regs.
            if yes, wait for it to compelete
        2. if no command in progress, then write current command in to command
            register and issue notice to firmware that a host command is written
        3. wait for current command to complete
    */

    /* check whether there's any ongoing command */
    status = tw_mbox_acquire(pDev);
    CHK_STATUS(status);

    /* write the command into the Host Command register*/
    cmd = HBI_VAL(pDev,cmd);
    status = internal_hbi_write(pDev,
                                 ZL380xx_HOST_CMD_REG,
                                 (user_buffer_t *)&cmd,sizeof(cmd));
    CHK_STATUS(status);

    /* Issue "Host Command Written" notice to firmware */
    cmd = HBI_VAL(pDev,ZL380xx_HOST_SW_FLAGS_HOST_CMD);
    status = internal_hbi_write(pDev,
                                 ZL380xx_HOST_SW_FLAGS_REG,
                                (user_buffer_t *)&cmd,sizeof(cmd));
    CHK_STATUS(status);

    return tw_cmdreg_acquire(pDev);
}


 /* tw_reset(): use this function to reset the device.
  *  \param[in]      devp - pointer to device specific structure
  *  \param[in]      mode - reset mode
  *
  * Return:  type error code (0 = success, else= fail)
  */
 static inline hbi_status_t tw_reset(struct vproc_dev *pDev,int mode)
 {
    hbi_status_t status = 0;
    uint16_t     val=0;
    reg_addr_t   reg=0;
    int32_t      count=0;

    switch(mode)
    {
        case RST_HARDWARE_RAM:
           /*hard reset*/
            reg = ZL380xx_CLK_STATUS_REG;
            val = HBI_VAL(pDev,
                         (ZL380xx_CLK_STATUS_HWRST | ZL380xx_CLK_STATUS_RST));

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"val 0x%x\n",val);

            status = internal_hbi_write(pDev,reg,(user_buffer_t *)&val,2);
            CHK_STATUS(status);

           /* As per TW documentation, A RESET bit of Page 255 base LO register
             should set on asserting POR/RST_N and be polled to determine if
             reset is complete or not.However seems this is not getting set by
             FW and this bit always remain 0 giving false information that reset
             is complete.
             Thus, we had to rely on time delay of 2secs (based on hit n trial)
             on every hardware reset to ensure its complete before returning
             function.*/

            SSL_delay(TW_RST_WAIT_PERIOD);
            return HBI_STATUS_SUCCESS;

        case RST_HARDWARE_ROM:
             /*power on reset*/

            reg = ZL380xx_CLK_STATUS_REG;
            val = HBI_VAL(pDev,
                          (ZL380xx_CLK_STATUS_HWRST | ZL380xx_CLK_STATUS_POR));

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"val 0x%x\n",val);

            status = internal_hbi_write(pDev,reg,(user_buffer_t *)&val,2);
            CHK_STATUS(status);

            SSL_delay(TW_RST_WAIT_PERIOD);

#ifdef VPROC_DEV_INT_MODE_OD
            /* if host is using Open Drain interrupt send this */
            status = internal_hbi_set_attrib(pDev, HBI_ATTRIB_CONFIG_INT,NULL);
            if(status != HBI_STATUS_SUCCESS)
            {
               VPROC_DBG_PRINT(VPROC_DBG_LVL_WARN,"CONFIG_INT failed\n");
            }
#endif
            return HBI_STATUS_SUCCESS;

        case RST_AEC:
            /*AEC method*/

            reg = ZL380xx_AEC_CTRL0_REG;
            status = internal_hbi_read(pDev,reg,(user_buffer_t *)&val,2);
            CHK_STATUS(status);

            val |= HBI_VAL(pDev,ZL380xx_AEC_RST);
            count = 0;

            status = internal_hbi_write(pDev,reg,(user_buffer_t *)&val,2);
            CHK_STATUS(status);

            do{
                status = internal_hbi_read(pDev,reg,(user_buffer_t *)&val,2);
            }while((status == HBI_STATUS_SUCCESS) &&
                   (HBI_VAL(pDev,val) & ZL380xx_AEC_RST) &&
                   (count++ < TW_RST_WAIT_CNT));

            if (status != HBI_STATUS_SUCCESS) {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"ERROR %d: \n", status);
            }
            else if(count >= TW_RST_WAIT_CNT)
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Timeout waiting on AEC Reset to complete!\n");
                status = HBI_STATUS_INTERNAL_ERR;
            }
            return status;

        case RST_SOFTWARE:
            /*soft reset*/

            reg = ZL380xx_HOST_SW_FLAGS_REG;
            val = HBI_VAL(pDev,ZL380xx_HOST_SW_FLAGS_APP_REBOOT);

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"val 0x%x\n",val);

            status = internal_hbi_write(pDev,reg,(user_buffer_t *)&val,2);
            if (status != HBI_STATUS_SUCCESS) {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"ERROR %d: \n", status);
                return status;
            }

            /* wait for reset to complete */
            count=0;
            do{
            status = internal_hbi_read(pDev,reg,(user_buffer_t *)&val,2);
            }while( (status == HBI_STATUS_SUCCESS) &&
                    ((HBI_VAL(pDev,val)) & 1) &&
                    (count++ < TW_RST_WAIT_CNT));

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                           " Soft Reset Complete.Status 0x%x val 0x%x,count %d\n",
                           status,val,count);

            if(((HBI_VAL(pDev,val)) & 1) && (count >= TW_RST_WAIT_CNT))
            {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                              "Timeout waiting on Reset Complete\n");
                status = HBI_STATUS_INTERNAL_ERR;
            }
            return status;

        case RST_TO_BOOT:
            /*reset to bootrom mode*/

            reg = ZL380xx_CLK_STATUS_REG;
            val = HBI_VAL(pDev,ZL380xx_CLK_STATUS_HWRST);

            status = internal_hbi_write(pDev,reg,(user_buffer_t *)&val,2); /*go to boot rom mode*/
            CHK_STATUS(status);

            /*required for the reset to cmplete. */
            SSL_delay(50);

            VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                           "Check if device entered into boot rom\n");

            /*check whether the device has gone into boot mode as ordered*/
            status = internal_hbi_read(pDev,
                                       ZL380xx_HOST_CMD_PARAM_RESULT_REG,
                                      (user_buffer_t *) &val,
                                       2);
            if (status != HBI_STATUS_SUCCESS) {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"HBI Read Failed \n");
                return HBI_STATUS_INTERNAL_ERR;
            }

            if ((val != 0xD3D3)) {
                VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                               "ERROR: HBI is not accessible, cmdResultCheck = 0x%04x\n",
                               val);
                status =  HBI_STATUS_INTERNAL_ERR;
            }
        return status;

        default:
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid reset type\n");
            return HBI_STATUS_INVALID_ARG;
    }

    return status;
}
 /* tw_chk_dev_boot_rom_mode() - Check the current mode of device if its in App mode or Boot Rom mode
  * \param[in]      instancep - pointer to user instance
  *
  * Return : 0 on success else error code
  */
 static inline int32_t tw_chk_dev_boot_rom_mode(struct vproc_dev *pDev)
 {
    uint16_t      val1 = 0;
    hbi_status_t  status;

     /* read the app running status bit from reg 0x28 "Currently Loaded Firmware Reg" */
     status = internal_hbi_read(pDev,
                                ZL380xx_CUR_LOADED_FW_IMG_REG,
                               (user_buffer_t *) &val1,sizeof(val1));
     CHK_STATUS(status);

     if(HBI_VAL(pDev,val1) & ZL380xx_CUR_FW_APP_RUNNING)
         return 0;

     return 1;
 }

 /* tw_boot_conclude() - Performs necessary action post boot image loading
  * \param[in]  dev - pointer to device
  *
  * Return -
 */
static inline hbi_status_t tw_boot_conclude(struct vproc_dev *pDev)
{
    hbi_status_t status = HBI_STATUS_SUCCESS;
    ZL380xx_HMI_RESPONSE hmi_response;

    status = tw_wr_cmdreg(pDev,HOST_CMD_HOST_LOAD_CMP); /*loading complete*/
    CHK_STATUS(status);

    /*check whether the device has gone into boot mode as ordered*/
    hmi_response = tw_cmdresult_check(pDev);
    switch(HBI_VAL(pDev,hmi_response))
    {
        case HMI_RESP_INCOMPAT_APP:
            return HBI_STATUS_INCOMPAT_APP;
       case HMI_RESP_SUCCESS:
        return HBI_STATUS_SUCCESS;
    }
    return HBI_STATUS_COMMAND_ERR;
}

static inline hbi_status_t tw_wr_transport_cmd(struct vproc_dev *pDev,
                                             uint16_t cmdword)
{
    ssl_status_t         ssl_status;
    size_t               num_bytes_write;

    /* command word is always sent in Big Endian Byte Order */
#if 0
    #if HOST_ENDIAN_LITTLE
    cmdword = (((cmdword & 0xFF) << 8) | (cmdword >>8));
    #endif


#endif

    num_bytes_write= sizeof(cmdword);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Writing cmdword 0x%x\n",cmdword);
    cmdword = HBI_VAL(pDev,cmdword);
    HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);
    ssl_status = SSL_port_write(pDev->port_handle,(void *)&cmdword,
                                 &num_bytes_write);
    HBI_UNLOCK(pDev->port_lock);

    return ((ssl_status == SSL_STATUS_OK) ?
            HBI_STATUS_SUCCESS : HBI_STATUS_INTERNAL_ERR);
}

 /* tw_init_check_flash() -Checks if there is a flash on board and initialize it
  * \param[in]          instancep - pointer to user
  *
  * \retval ::0 success or device error code
  *
  */
static inline hbi_status_t  tw_init_check_flash(struct vproc_dev *pDev)
{
#if defined(FLASH_PRESENT)
    ZL380xx_HMI_RESPONSE hmi_response;
    hbi_status_t status;

    /*Check and initialize flash and */
    status = tw_wr_cmdreg(pDev,HOST_CMD_HOST_FLASH_INIT);
    CHK_STATUS(status);

    hmi_response = tw_cmdresult_check(pDev);

    switch(HBI_VAL(pDev,hmi_response))
    {
        case HMI_RESP_FLASH_INIT_OK:
            return HBI_STATUS_SUCCESS;
        case HMI_RESP_FLASH_INIT_NO_DEV:
            return HBI_STATUS_NO_FLASH_PRESENT;
        default:
            return HBI_STATUS_INTERNAL_ERR;
    }
#else
    return HBI_STATUS_NO_FLASH_PRESENT;

#endif
}

static hbi_status_t tw_save_cfg_to_flash(struct vproc_dev *pDev,void *pCmdArgs)
{
#if defined(FLASH_PRESENT)
   hbi_status_t            status = HBI_STATUS_SUCCESS;
   uint16_t                image_number = *((int16_t *)pCmdArgs);
   ZL380xx_HMI_RESPONSE    hmi_response;
   uint16_t                val;
   uint16_t                cmd;
   user_buffer_t           buf[2] ={0};

   if (image_number <= 0) {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid image number %d \n", image_number);
      return HBI_STATUS_INVALID_ARG;
   }

   status = internal_hbi_read(pDev,
                                 ZL380xx_FWR_COUNT_REG,
                                 (user_buffer_t *)&val,2);
   CHK_STATUS(status);

   val = HBI_VAL(pDev,val);
   image_number = HBI_VAL(pDev,image_number);

   buf[0]  = image_number >> 8;
   buf[1]  = image_number & 0xFF;

   if (image_number > val) {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"ERROR: Invalid image number - Number of images on flash = %d, it must be >= passed image number, %d\n", val, image_number);
      return HBI_STATUS_INVALID_ARG;
   }

  VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"INFO: image number - Number of images on flash = %d must be >= passed image number, %d, %d\n", val, buf[0], buf[1]);

   val=0;

   status = internal_hbi_write(pDev,
                              ZL380xx_CFG_REC_CHKSUM_REG,
                              (user_buffer_t *)&val,2);
   if (status != HBI_STATUS_SUCCESS) {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"ERROR %d: \n", status);
     return status;
   }


   status = internal_hbi_write(pDev,
                              ZL380xx_HOST_CMD_PARAM_RESULT_REG, buf, 2);
   CHK_STATUS(status);

   /* check whether there's any ongoing command */
   status = tw_mbox_acquire(pDev);
   CHK_STATUS(status);

   if(tw_chk_dev_boot_rom_mode(pDev))
   {
          VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                     "Using the boot mode cfg loading\n");
     status = tw_init_check_flash(pDev);
     CHK_STATUS(status);

     cmd = HBI_VAL(pDev,HOST_CMD_IMG_CFG_SAVE);

     status = internal_hbi_write(pDev,
                                 ZL380xx_HOST_CMD_REG,
                                 (user_buffer_t *)&cmd,sizeof(cmd));
     CHK_STATUS(status);

     cmd = HBI_VAL(pDev,ZL380xx_HOST_SW_FLAGS_HOST_CMD);
   }
   else
   {

     VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                     "Using the program mode cfg loading\n");
     cmd = HBI_VAL(pDev,HOST_CMD_APP_SAVE_CFG_TO_FLASH);

     status = internal_hbi_write(pDev,
                                 ZL380xx_HOST_CMD_REG,
                                 (user_buffer_t *)&cmd,sizeof(cmd));
     CHK_STATUS(status);

     cmd = HBI_VAL(pDev, ZL380xx_HOST_SW_FLAGS_APP_HOST_CMD);
   }

   status = internal_hbi_write(pDev,
                              ZL380xx_HOST_SW_FLAGS_REG,
                              (user_buffer_t *)&cmd,sizeof(cmd));
   CHK_STATUS(status);

   hmi_response = tw_cmdresult_check(pDev);
   hmi_response = HBI_VAL(pDev,hmi_response);

   if(hmi_response != HMI_RESP_SUCCESS)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                     "Command failed with response 0x%x\n",
                     hmi_response);
     return HBI_STATUS_COMMAND_ERR;
   }
   return status;
#endif
   return HBI_STATUS_NO_FLASH_PRESENT;
}

static hbi_status_t tw_sleep(struct vproc_dev *pDev)
{
   hbi_status_t           status = HBI_STATUS_SUCCESS;
   uint16_t               cmd;

   /* check whether there's any ongoing command */
   status = tw_mbox_acquire(pDev);
   CHK_STATUS(status);

   /* To check: whats the behavior is system is in BOOT ROM mode ?*/
   cmd = HBI_VAL(pDev,HOST_CMD_APP_SLEEP);
   status = internal_hbi_write(pDev,
                              ZL380xx_HOST_CMD_REG,
                              (user_buffer_t *)&cmd,sizeof(cmd));
   CHK_STATUS(status);

   cmd = HBI_VAL(pDev, ZL380xx_HOST_SW_FLAGS_APP_HOST_CMD);
   status = internal_hbi_write(pDev,
                              ZL380xx_HOST_SW_FLAGS_REG,
                              (user_buffer_t *)&cmd,sizeof(cmd));
   CHK_STATUS(status);

   return status;
}

static hbi_status_t tw_load_fwrcfg_from_flash(struct vproc_dev *pDev,
                                             void *pCmdArgs)
{
#if defined(FLASH_PRESENT)
   hbi_status_t            status = HBI_STATUS_SUCCESS;
   int16_t                 image_number = *((int16_t *)pCmdArgs);
   int32_t                  ret;
   ZL380xx_HMI_RESPONSE    hmi_response;

   if (image_number <= 0) {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid image number\n");
      return HBI_STATUS_INVALID_ARG;
   }

   if( (ret = tw_chk_dev_boot_rom_mode(pDev)) <0)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Err\n");
      return HBI_STATUS_INTERNAL_ERR;
   }

   if (!ret)
   {
      /* put device in boot rom mode */
      status = tw_reset(pDev,RST_TO_BOOT);
      CHK_STATUS(status);
   }

   status = tw_init_check_flash(pDev);
   CHK_STATUS(status);

   status = internal_hbi_write(pDev,
                              ZL380xx_HOST_CMD_PARAM_RESULT_REG,
                              (user_buffer_t *)&image_number,
                              sizeof(image_number));
   CHK_STATUS(status);

   status = tw_wr_cmdreg(pDev,HOST_CMD_IMG_CFG_LOAD);
   CHK_STATUS(status);

   hmi_response = tw_cmdresult_check(pDev);
   hmi_response = HBI_VAL(pDev,hmi_response);
   if(hmi_response != HMI_RESP_SUCCESS)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                        "Command failed with response 0x%x\n",
                        hmi_response);
      return HBI_STATUS_COMMAND_ERR;
   }
   return status;
#else
   return HBI_STATUS_NO_FLASH_PRESENT;
#endif
}

/* tw_save_fw_image_to_flash(): Saves current loaded both the config record
 *                              and the firmware in memory to flash.
 * \param[in]  instancep - pointer to user instance
 * \param[out] valp      - pointer to a value updated with image number
 *                         allocated to present firmware image.
 *
 * \retval ::0 success or device error code and updates
 *             valp with loaded fwr image number as on flash

 * NOTE:
 *      Every firmware load command will save next to existing images and will
 *      carry a unique image number which is generated internally by boot rom
 *      code and an sequential number as per documentation
 *      (See appendix C.2 Page 229 in ZL38040_Firmware_Manual.pdf Ver 1.1.0).
 *
 *      Another alternative way to use an APP ID passed by user to this routine
 *      which can be put in the fw header and later be matched by looping and
 *      reading through all of the present FW images headers.
 *      However, that seems to be bit cumbersome process. It would have
 *      been easier if every LOAD_FWR_TO_FLASH command could write back
 *      corresponding image number in host parameter/result register on success.
 *      Then user would have known image number of just loaded mage and
 *      bookkeeping would have been easier.
 */

static inline hbi_status_t tw_save_fwrcfg_to_flash(struct vproc_dev *pDev,
                                                   void *pVal)
{
#if defined(FLASH_PRESENT)
    hbi_status_t            status;
    uint16_t                num_fwr_images=0;
    ZL380xx_HMI_RESPONSE    hmi_response;
    int32_t                 ret;
    uint16_t                val;

    if( (ret = tw_chk_dev_boot_rom_mode(pDev)) <0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Err\n");
        return HBI_STATUS_INTERNAL_ERR;
    }

    if (!ret)
    {
        /* put device in boot rom mode */
        status = tw_reset(pDev,RST_TO_BOOT);
        CHK_STATUS(status);
    }

    status = tw_init_check_flash(pDev);
    CHK_STATUS(status);

    val=0;
    status = internal_hbi_write(pDev,
                                 ZL380xx_CFG_REC_CHKSUM_REG,
                                 (user_buffer_t *)&val,2);
    if (status != HBI_STATUS_SUCCESS) {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"ERROR %d: \n", status);
        return status;
    }

    /*save the image to flash*/
    status = tw_wr_cmdreg(pDev,HOST_CMD_IMG_CFG_SAVE);
    CHK_STATUS(status);

    hmi_response = tw_cmdresult_check(pDev);
    hmi_response = HBI_VAL(pDev,hmi_response);
    if (hmi_response != HMI_RESP_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Command Result 0x%x \n", hmi_response);
        if(HBI_VAL(pDev, hmi_response) == HMI_RESP_FLASH_FULL)
        {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                           "Please erase flash to free up space\n");
            return HBI_STATUS_FLASH_FULL;
        }
        return HBI_STATUS_COMMAND_ERR;
    }
    if(pVal)
    {
      status = internal_hbi_read(pDev,
                                 ZL380xx_FWR_COUNT_REG,
                                 (user_buffer_t *)&num_fwr_images,2);
      CHK_STATUS(status);

      *(int *)pVal = HBI_VAL(pDev,num_fwr_images);
    }
    return status;
#else
    return HBI_STATUS_NO_FLASH_PRESENT;
#endif
}

/* tw_erase_flash() - Erases whole flash
 * \param[in] instancep - user specific instance
 *
 * \retval ::0 success or error code
 *
 */
static inline hbi_status_t tw_erase_flash(struct vproc_dev *pDev)
{
#if defined(FLASH_PRESENT)
   hbi_status_t            status =0;
   ZL380xx_HMI_RESPONSE    hmi_response;
   uint16_t                val = 0;
   int32_t                 ret;

   if( (ret = tw_chk_dev_boot_rom_mode(pDev)) <0)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Err\n");
     return HBI_STATUS_INTERNAL_ERR;
   }

   if (!ret)
   {
      /* put device in boot rom mode */
      status = tw_reset(pDev,RST_TO_BOOT);
      CHK_STATUS(status);
   }

   /* if there is a flash on board initialize it */
   status = tw_init_check_flash(pDev);
   CHK_STATUS(status);

   /* erase all config/fwr */
   val = HBI_VAL(pDev, 0xAA55);
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"val 0x%x\n",val);

   status = internal_hbi_write(pDev,
                             ZL380xx_HOST_CMD_PARAM_RESULT_REG,
                             (user_buffer_t *)&val,
                             sizeof(val));
   CHK_STATUS(status);

   /* erase firmware */
   status = tw_wr_cmdreg(pDev,HOST_CMD_ERASE_FLASH_INIT);
   CHK_STATUS(status);

   hmi_response = tw_cmdresult_check(pDev);

   switch(HBI_VAL(pDev,hmi_response))
   {
     case HMI_RESP_FLASH_INIT_OK:
         return HBI_STATUS_SUCCESS;
     case HMI_RESP_BAD_IMAGE:
         return HBI_STATUS_BAD_IMAGE;
     case HMI_RESP_INCOMPAT_APP:
         return HBI_STATUS_INCOMPAT_APP;
     case HMI_RESP_NO_FLASH_PRESENT:
         return HBI_STATUS_NO_FLASH_PRESENT;
     default:
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Command response 0x%x\n",hmi_response);
         return HBI_STATUS_COMMAND_ERR;
}
#else
   return  HBI_STATUS_NO_FLASH_PRESENT;
#endif
}

/* tw_start_fwr_from_ram() - starts the firmware execution loaded into RAM
 * \param[in] instancep - pointer to user specific function
 *
 * \retval ::0 success or error code
 *
 */
static inline hbi_status_t  tw_start_fwr_from_ram(struct vproc_dev *pDev)
{
    int32_t  ret;
    hbi_status_t status = HBI_STATUS_SUCCESS;
    ZL380xx_HMI_RESPONSE    hmi_response;

    if( (ret = tw_chk_dev_boot_rom_mode(pDev)) <0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Err\n");
        return HBI_STATUS_INTERNAL_ERR;
    }

    if (!ret)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Device should already be in boot mode state\n");
        return HBI_STATUS_INVALID_STATE;
    }

    status = tw_wr_cmdreg(pDev,HOST_CMD_FWR_GO);
    CHK_STATUS(status);

    hmi_response = tw_cmdresult_check(pDev);
    hmi_response = HBI_VAL(pDev,hmi_response);

    if(hmi_response != HMI_RESP_SUCCESS)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                        "Command failed with response 0x%x\n",hmi_response);
        return HBI_STATUS_COMMAND_ERR;
    }
    return status;
}

/* tw_erase_fwrcfg_from_flash() - Erases a specific firmware image and related
 *                                config from flash
 * \param[in] instancep - pointer to user instance
 * \param[in] image_number - image number to be erased (1-14)
 *
 * \retval ::0 success or device error code
 *
 */
static inline hbi_status_t tw_erase_fwrcfg_from_flash(struct vproc_dev *pDev,
                                                     void *pCmdArgs)
{
#if defined(FLASH_PRESENT)
    hbi_status_t              status = 0;
    int16_t                 image_number;
    ZL380xx_HMI_RESPONSE    hmi_response;
    int32_t                 ret;

    if(pCmdArgs == NULL)
    {
        return HBI_STATUS_INVALID_ARG;
    }

    image_number = *((int16_t *)pCmdArgs);
    if (image_number <= 0) {
        return HBI_STATUS_INVALID_ARG;
    }

    if( (ret = tw_chk_dev_boot_rom_mode(pDev)) <0)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Err\n");
        return HBI_STATUS_INTERNAL_ERR;
    }

    if (!ret)
    {
        /* put device in boot rom mode */
        status = tw_reset(pDev,RST_TO_BOOT);
        CHK_STATUS(status);
    }

     /*if there is a flash on board initialize it*/
    status = tw_init_check_flash(pDev);
    CHK_STATUS(status);

    status = internal_hbi_write(pDev,
                                 ZL380xx_HOST_CMD_PARAM_RESULT_REG,
                                 (user_buffer_t *)&image_number,
                                 sizeof(image_number));
    CHK_STATUS(status);

    status  = tw_wr_cmdreg(pDev,HOST_CMD_IMG_CFG_ERASE);
    CHK_STATUS(status);

    hmi_response = tw_cmdresult_check(pDev);
    switch(HBI_VAL(pDev, hmi_response))
    {
        case HMI_RESP_SUCCESS:
            return HBI_STATUS_SUCCESS;
        case HMI_RESP_BAD_IMAGE:
            return HBI_STATUS_BAD_IMAGE;
        case HMI_RESP_INCOMPAT_APP:
            return HBI_STATUS_INCOMPAT_APP;
        case HMI_RESP_NO_FLASH_PRESENT:
            return HBI_STATUS_NO_FLASH_PRESENT;
        default:
            return HBI_STATUS_COMMAND_ERR;
    }
#else
    return HBI_STATUS_NO_FLASH_PRESENT;
#endif
}

/* tw_boot_write() - Upload a firmware image
 *  \param[in]       dev - device pointer
 *                   cmdargs - pointer to tw_data structure
 *
 * Return: See Description below
 *
 * Description:
 *  User will call this function by passing a buffer containing binary
 *  firmware image.
 *
 *  please note its user responsibility to keep track of how much data
 *  already written.
 *  function will return SUCCESS on a successful write operation
 *
 */
static hbi_status_t tw_boot_write(struct vproc_dev *pDev,void *pCmdArgs)
{
   hbi_status_t        status = HBI_STATUS_SUCCESS;
   ssl_status_t        ssl_status;
   unsigned char       *pFwrData;
   size_t               len;
   int32_t              ret;

   if(pCmdArgs == NULL)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Null Argument Passed\n");
     return HBI_STATUS_INVALID_ARG;
   }

   pFwrData = (unsigned char *)(((hbi_data_t *)pCmdArgs)->pData);
   len = ((hbi_data_t *)pCmdArgs)->size;
   if(pFwrData == NULL)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid buffer pointer\n");
      return HBI_STATUS_INVALID_ARG;
   }

   if((ret = tw_chk_dev_boot_rom_mode(pDev)) <0)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Err\n");
      return HBI_STATUS_INTERNAL_ERR;
   }

   if (!ret)
   {
     /* put device in boot rom mode */
     status = tw_reset(pDev,RST_TO_BOOT);
     CHK_STATUS(status);
   }

    HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);
    ssl_status = SSL_port_write(pDev->port_handle,pFwrData,&len);
    HBI_UNLOCK(pDev->port_lock);
    if(ssl_status != SSL_STATUS_OK)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"SSL_port_write failed\n");
        return HBI_STATUS_INTERNAL_ERR;
    }

   return status;
}

/* tw_boot_write() - Upload a firmware image
 *  \param[in]       dev - device pointer
 *                   cmdargs - pointer to tw_data structure
 *
 * Return: See Description below
 *
 * Description:
 *  User will call this function by passing a buffer containing binary
 *  firmware image.
 *
 *  please note its user responsibility to keep track of how much data
 *  already written.
 *  function will return SUCCESS on a successful write operation
 *
 */
static hbi_status_t tw_config_write(struct vproc_dev *pDev,void *pCmdArgs)
{
   hbi_status_t        status = HBI_STATUS_SUCCESS;
   ssl_status_t        ssl_status;
   unsigned char       *pCfgData;
   size_t               len;


   if(pCmdArgs == NULL)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR," pCmdArgs: Null Argument Passed\n");
     return HBI_STATUS_INVALID_ARG;
   }

   pCfgData = (unsigned char *)(((hbi_data_t *)pCmdArgs)->pData);
   len = ((hbi_data_t *)pCmdArgs)->size;
   if(pCfgData == NULL)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"pCfgData: Invalid buffer pointer\n");
      return HBI_STATUS_INVALID_ARG;
   }

   HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);
   ssl_status = SSL_port_write(pDev->port_handle,pCfgData,&len);
   HBI_UNLOCK(pDev->port_lock);
   if(ssl_status != SSL_STATUS_OK)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"SSL_port_write failed\n");
        return HBI_STATUS_INTERNAL_ERR;
    }

   return status;
}
/*
    Description: this function makes transport frame header for command
                 to be sent over HBI
*/
static inline void tw_hbi_tp_frame_hdr(struct vproc_dev *pDev,
                                       reg_addr_t        addr,
                                       bool              read,
                                       size_t            size,
                                       struct tw_hdr *pHbiHdr)
{
   uint8_t         page = addr >> 8;
   uint8_t         offset = (addr & 0xFF) >> 1;
   uint8_t         num_words = (size >> 1)-1;
   uint16_t        val=0;
   int32_t         i=0;

   pHbiHdr->cmdlen = 0;

   if(page)
   {
     if(page != 0xFF)
         page -= 1;

     val = HBI_SELECT_PAGE(page);

     i=0;
     pHbiHdr->cmd[i++] = val >> 8;
     pHbiHdr->cmd[i++] = val & 0xFF;

     if(read)
     {
         val = HBI_PAGED_READ(offset, num_words);
     }
     else
     {
         val = HBI_PAGED_WRITE(offset, num_words);
     }

     pHbiHdr->cmd[i++] = val >> 8;
     pHbiHdr->cmd[i++] = val & 0xFF;
   }
   else
   {
     /*Direct page access. Make read or write command*/
     if(read)
         val = HBI_DIRECT_READ(offset, num_words);
     else
         val = HBI_DIRECT_WRITE(offset,num_words);

      i=0;

      pHbiHdr->cmd[i++] = val >> 8;
      pHbiHdr->cmd[i++] = val & 0xFF;
   }

   pHbiHdr->cmdlen = i;

   return;
}


hbi_status_t internal_hbi_set_attrib(struct vproc_dev *pDev,
                                   hbi_attrib_t attrib,
                                   void *pVal)
{
   uint32_t value=0;
   hbi_status_t status;

   switch(attrib)
   {
      case HBI_ATTRIB_DEV_ENDIAN:
         if(pVal == NULL)
         {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Null pointer passed\n");
            return HBI_STATUS_INVALID_ARG;
         }
         value = HBI_CONFIG_IF_CMD;
         if(*((hbi_dev_endian_t *)pVal) & HBI_DEV_ENDIAN_LITTLE)
         {
            value |= HBI_CONFIG_IF_ENDIAN_LITTLE;
         }
         return tw_wr_transport_cmd(pDev,value);

      case HBI_ATTRIB_SLEEP:

         if(pVal == NULL)
         {
            VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Null pointer passed\n");
            return HBI_STATUS_INVALID_ARG;
         }
         if(*((uint32_t *)pVal))
         {
            return tw_sleep(pDev);
         }
         else
         {
            status = tw_wr_transport_cmd(pDev,(HBI_CONFIG_IF_CMD | HBI_CONFIG_WAKE));
			VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"wake status %d\n", status);
			SSL_delay(10);
            status = tw_wr_transport_cmd(pDev,(HBI_CONFIG_IF_CMD));
			VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"wake0 status %d\n", status);

#ifdef VPROC_DEV_INT_MODE_OD
            if(status == HBI_STATUS_SUCCESS)
            {
               status = tw_wr_transport_cmd(pDev,
                                          (HBI_CONFIG_IF_CMD | HBI_CONFIG_INT_MODE_OD));
               if(status != HBI_STATUS_SUCCESS)
               {
                 VPROC_DBG_PRINT(VPROC_DBG_LVL_WARN,"HBI_CONFIG_INT failed\n");
               }
            }
#endif
            return status;
      }
      case HBI_ATTRIB_CONFIG_INT:
         value = (HBI_CONFIG_IF_CMD | HBI_CONFIG_INT_MODE_OD);
         return tw_wr_transport_cmd(pDev,value);
      default:
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Attrib not supported\n");
         return HBI_STATUS_INVALID_ARG;
   }
   return HBI_STATUS_SUCCESS;
}

hbi_status_t internal_hbi_read(struct vproc_dev *pDev,
                                reg_addr_t reg_addr,
                                user_buffer_t *pData,
                                size_t size)
{
    ssl_status_t      ssl_status;
    ssl_port_access_t port_access;
    struct tw_hdr     hbi_hdr;
    int               i;
    user_buffer_t buf[MAX_HBI_BYTES_PER_ACCESS];

    SSL_memset(&port_access,0,sizeof(ssl_port_access_t));

    tw_hbi_tp_frame_hdr(pDev,reg_addr,1,size,&hbi_hdr);

    port_access.pDst = (void *)buf;
    port_access.nread = size;
    port_access.pSrc = &(hbi_hdr.cmd);
    port_access.nwrite = hbi_hdr.cmdlen;
    port_access.op_type = SSL_OP_PORT_RW;

    HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);
    ssl_status = SSL_port_rw(pDev->port_handle,&port_access);
    HBI_UNLOCK(pDev->port_lock);

    CHK_SSL_STATUS(ssl_status);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Wrote\n");
    for(i=0;i<port_access.nwrite;i++)
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                        "0x%x\t",((uint8_t *)port_access.pSrc)[i]);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"Received\n");
    for(i=0;i<size;i++)
    {

        *(pData + i) = ((user_buffer_t *)port_access.pDst)[i];
        VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"0x%x\t",((uint8_t*)pData)[i]);
    }

    return HBI_STATUS_SUCCESS;
}

hbi_status_t internal_hbi_write(struct vproc_dev *pDev,
                                 reg_addr_t reg_addr,
                                 user_buffer_t *pData,
                                 size_t size)
{
   ssl_status_t    ssl_status;
   struct tw_hdr   hbi_hdr;
   size_t        num_bytes_wr=0;
   int i=0;


   if(size > ZL380xx_MAX_ACCESS_SIZE_IN_BYTES)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                     "Size exceed.Received %d," \
                     "Maximum limit to transfer data is %d\n",
                     (int32_t)size,(int32_t)ZL380xx_MAX_ACCESS_SIZE_IN_BYTES);
     return HBI_STATUS_INVALID_ARG;
   }

   tw_hbi_tp_frame_hdr(pDev,reg_addr,0,size,&hbi_hdr);

   for(i=0;i<hbi_hdr.cmdlen;i++)
     VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"0x%x\t",hbi_hdr.cmd[i]);

   for(i=0;i<size;i++)
     VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,"0x%x\t",((uint8_t*)pData)[i]);

   SSL_memcpy(pDev->buffer,hbi_hdr.cmd,hbi_hdr.cmdlen);

   SSL_memcpy(pDev->buffer+hbi_hdr.cmdlen,pData,size);

   num_bytes_wr = hbi_hdr.cmdlen+size;

   HBI_LOCK(pDev->port_lock,SSL_WAIT_FOREVER);
   ssl_status = SSL_port_write(pDev->port_handle,pDev->buffer,&num_bytes_wr);
   HBI_UNLOCK(pDev->port_lock);
   if(ssl_status == SSL_STATUS_OP_INCOMPLETE)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                     "Number of bytes written %d\n",(int)num_bytes_wr);
      return HBI_STATUS_OP_INCOMPLETE;
   }

   CHK_SSL_STATUS(ssl_status);

   return HBI_STATUS_SUCCESS;

}

hbi_status_t internal_hbi_set_command(struct vproc_dev *pDev,
                                       hbi_cmd_t       cmd,
                                       void           *pCmdArgs)
{
    hbi_status_t status;
    switch(cmd)
    {
        case HBI_CMD_LOAD_FWRCFG_FROM_FLASH:
            status = tw_load_fwrcfg_from_flash(pDev,pCmdArgs);
        break;
        case HBI_CMD_LOAD_CFGREC_FROM_HOST:
            status = tw_config_write(pDev,pCmdArgs);
        break;
        case HBI_CMD_SAVE_FWRCFG_TO_FLASH:
            status = tw_save_fwrcfg_to_flash(pDev,pCmdArgs);
        break;
        case HBI_CMD_SAVE_CFG_TO_FLASH:
            status = tw_save_cfg_to_flash(pDev,pCmdArgs);
        break;
        case HBI_CMD_ERASE_FWRCFG_FROM_FLASH:
            status = tw_erase_fwrcfg_from_flash(pDev,pCmdArgs);
        break;
        case HBI_CMD_ERASE_WHOLE_FLASH:
            status = tw_erase_flash(pDev);
        break;
        case HBI_CMD_LOAD_FWR_FROM_HOST:
            status = tw_boot_write(pDev,pCmdArgs);
        break;
        case HBI_CMD_LOAD_FWR_COMPLETE:
            status = tw_boot_conclude(pDev);
        break;
        case HBI_CMD_START_FWR:
            status = tw_start_fwr_from_ram(pDev);
        break;
        default:
           VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Command not supported\n");
           status = HBI_STATUS_INVALID_ARG;
    }
    return status;
}

hbi_status_t internal_hbi_reset(struct vproc_dev *pDev,
                                 hbi_rst_mode_t reset_mode)
{
    switch(reset_mode)
    {
        case HBI_RST_POR:
            reset_mode = RST_HARDWARE_ROM;
        break;
        default:
            return HBI_STATUS_INVALID_ARG;
    }
    return tw_reset(pDev,reset_mode);
}

hbi_status_t internal_hbi_get_hdr(hbi_data_t *pImg,hbi_img_hdr_t *pHdr)
{


   if(pImg == NULL || (pImg->size < IMG_HDR_LEN) || pHdr==NULL)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid arguments passed\n:");
      return HBI_STATUS_INVALID_ARG;
   }

   pHdr->major_ver = pImg->pData[VER_INDX] >> IMG_VERSION_MAJOR_SHIFT;
   pHdr->minor_ver = pImg->pData[VER_INDX] >> IMG_VERSION_MINOR_SHIFT;
   pHdr->image_type = pImg->pData[FORMAT_INDX] >> IMG_HDR_TYPE_SHIFT;

   pHdr->endianness = pImg->pData[FORMAT_INDX] >> IMG_HDR_ENDIAN_SHIFT;
   pHdr->fwr_code   = (pImg->pData[FWR_OPN_INDX] << 8) | pImg->pData[FWR_OPN_INDX+1];

   pHdr->block_size = (pImg->pData[BLOCK_SIZE_INDX] << 8) | pImg->pData[BLOCK_SIZE_INDX+1];

   pHdr->img_len = pImg->pData[TOTAL_LEN_INDX] << 24;
   pHdr->img_len |= pImg->pData[TOTAL_LEN_INDX+1] << 16;
   pHdr->img_len |= pImg->pData[TOTAL_LEN_INDX+2] <<8;
   pHdr->img_len |= pImg->pData[TOTAL_LEN_INDX+3];

   pHdr->hdr_len = IMG_HDR_LEN;
   #if 0
   VPROC_DBG_PRINT(VPROC_DBG_LVL_INFO,
                  "image_type %d, major %d minor %d, block size 0x%08lx," \
                  "total len %z, fwr code %ld, endian %d\n",
                  pHdr->image_type,
                  pHdr->major_ver,
                  pHdr->minor_ver,
                  pHdr->block_size,
                  pHdr->img_len,
                  pHdr->fwr_code,
                  pHdr->endianness);
   #endif
   return HBI_STATUS_SUCCESS;
}

hbi_status_t internal_hbi_open(struct vproc_dev *pDev,
                               hbi_dev_cfg_t *pDevCfg)
{
   hbi_status_t status;

#if (VPROC_DEV_ENDIAN_LITTLE)
  pDev->endianness = HBI_DEV_ENDIAN_LITTLE;
#endif

   /* at last configure device to requested endianness */
   /* TODO: this would need modification when it will be tested with little
     We need a support to switch back to Big endian as well */
   status = internal_hbi_set_attrib(pDev, HBI_ATTRIB_DEV_ENDIAN,&(pDev->endianness));
   if(status != HBI_STATUS_SUCCESS)
   {
     /* Dont return failure let user decide what to do */
     VPROC_DBG_PRINT(VPROC_DBG_LVL_WARN,
                     "Failed to set Endianness of device with error %d\n", status);
     status = HBI_STATUS_SUCCESS;
   }

#ifdef VPROC_DEV_INT_MODE_OD
   /* if host is using Open Drain interrupt send this */
   status = internal_hbi_set_attrib(pDev, HBI_ATTRIB_CONFIG_INT,NULL);
   if(status != HBI_STATUS_SUCCESS)
   {
      /* Dont return failure let user decide what to do */
      VPROC_DBG_PRINT(VPROC_DBG_LVL_WARN,
                    "Failed to set Interrupt Mode of device\n");
      status = HBI_STATUS_SUCCESS;
   }
#endif
   return status;
}
