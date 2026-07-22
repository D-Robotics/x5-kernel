/*
* zl380_tw.h  --  Header file for defining common register defines across
*                 Timberwolf device variants
*
* Copyright 2016 Microsemi Inc.
*/

#ifndef __ZL380xx_TW_H__
#define __ZL380xx_TW_H__



#define ZL380xx_MAX_ACCESS_SIZE_IN_BYTES       256 /*128 16-bit words*/
#define ZL380xx_CFGREC_MAX_SIZE                 0xE00
#define ZL380xx_CFG_REC_BASE                    0x200
#define ZL380xx_MAX_NUM_FWR_IMAGES              14 /* Maximum number of images that can co-exist on flash */


typedef enum
{
    /* List of Host commands operated in BOOT ROM Mode */
    HOST_CMD_IDLE,                           /*idle/ operation complete*/
    HOST_CMD_NO_OP,                          /*no-op*/
    HOST_CMD_IMG_CFG_LOAD,                   /*load firmware and CR from flash*/
    HOST_CMD_IMG_LOAD,                       /*load firmware only from flash*/
    HOST_CMD_IMG_CFG_SAVE,                   /*save a firmware and CR to flash*/
    HOST_CMD_IMG_CFG_ERASE,                  /*erase a firmware and CR in flash*/
    HOST_CMD_CFG_LOAD,                       /*Load CR from flash*/
    HOST_CMD_CFG_SAVE,                       /*save CR to flash*/
    HOST_CMD_FWR_GO,                         /*start/restart firmware (GO)*/
    HOST_CMD_ERASE_FLASH_INIT,
    HOST_CMD_HOST_LOAD_CMP   = 0x000D,         /* Host Application Load Complete*/
    HOST_CMD_HOST_FLASH_INIT = 0x000B,        /* Host Application flash discovery*/
    HOST_CMD_FWR_STOP        = 0x8000,        /* Stop firmware */
    /* App mode commands */
    HOST_CMD_APP_SAVE_CFG_TO_FLASH = 0x8002,  /* save config record to flash */
    HOST_CMD_APP_SLEEP = 0x8005               /* put device to sleep mode */
}ZL380xx_HOST_CMD;

typedef enum
{
    HMI_RESP_SUCCESS=0,
    HMI_RESP_BAD_IMAGE,
    HMI_RESP_CKSUM_FAIL,
    HMI_RESP_FLASH_FULL,
    HMI_RESP_CONF_REC_MISMATCH,
    HMI_RESP_INVALID_FLASH_HEAD,
    HMI_RESP_NO_FLASH_PRESENT,
    HMI_RESP_FLASH_FAILURE,
    HMI_RESP_COMMAND_ERROR,
    HMI_RESP_NO_CONFIG_RECORD,
    HMI_RESP_INV_CMD_APP_IS_RUNNING,
    HMI_RESP_INCOMPAT_APP,
    HMI_RESP_FLASH_INIT_NO_DEV = 0x0000,
    HMI_RESP_FLASH_INIT_UNRECOG_DEV = 0x8000,
    HMI_RESP_FLASH_INIT_OK = 0x6000,
    HMI_RESP_FLASH_INIT_BAD_CHKSUM_DEV = 0x0001
}ZL380xx_HMI_RESPONSE;

typedef enum
{
  RST_HARDWARE_RAM,
  RST_HARDWARE_ROM,
  RST_SOFTWARE,
  RST_AEC,
  RST_TO_BOOT
}ZL380xx_RST_MODE;
/* Sample rate enum for TW device */
typedef enum
{
    ZL380xx_FR_NONE=0,
    ZL380xx_FR_8KHZ,
    ZL380xx_FR_16KHZ,
    ZL380xx_FR_24KHZ,
    ZL380xx_FR_441KHZ=5,
    ZL380xx_FR_48KHZ,
}ZL380xx_FR;

typedef struct tw_data
{
    uint8_t *data;
    size_t   len;
}TW_DATA;

/* typedefine for device register addressing type (addressing mode specific to
   device family)
*/
typedef uint16_t reg_addr_t;

/*------------------------------------------------------*/
/*TWOLF REGisters defintion */
#define ZL380xx_HOST_SW_FLAGS_REG                 0x0006
 #define ZL380xx_HOST_SW_FLAGS_HOST_CMD_SHIFT     0
 #define ZL380xx_HOST_SW_FLAGS_HOST_CMD           (1<<ZL380xx_HOST_SW_FLAGS_HOST_CMD_SHIFT)
 #define ZL380xx_HOST_SW_FLAGS_APP_REBOOT_SHIFT   1
 #define ZL380xx_HOST_SW_FLAGS_APP_REBOOT         (1<<ZL380xx_HOST_SW_FLAGS_APP_REBOOT_SHIFT)
 #define ZL380xx_HOST_SW_FLAGS_APP_HOST_CMD_SHIFT 2
 #define ZL380xx_HOST_SW_FLAGS_APP_HOST_CMD       (1<<ZL380xx_HOST_SW_FLAGS_APP_HOST_CMD_SHIFT)

#define ZL380xx_PAGE_255_CHKSUM_HI_REG            0x0008
#define ZL380xx_PAGE_255_CHKSUM_LO_REG            0x000A
#define ZL380xx_PAGE_255_BASE_HI_REG              0x000C
#define ZL380xx_PAGE_255_BASE_LO_REG              0x000E

#define ZL380xx_CLK_STATUS_REG                    0x014   /*Clock status register*/
 #define ZL380xx_CLK_STATUS_HWRST_SHIFT           0
 #define ZL380xx_CLK_STATUS_HWRST                 (1<<ZL380xx_CLK_STATUS_HWRST_SHIFT)
 #define ZL380xx_CLK_STATUS_RST_SHIFT             2
 #define ZL380xx_CLK_STATUS_RST                   (1<<ZL380xx_CLK_STATUS_RST_SHIFT)
 #define ZL380xx_CLK_STATUS_POR_SHIFT             3
 #define ZL380xx_CLK_STATUS_POR                   (1<<ZL380xx_CLK_STATUS_POR_SHIFT)

#define ZL380xx_FWR_COUNT_REG                     0x0026 /*Fwr on flash count register*/
#define ZL380xx_CUR_LOADED_FW_IMG_REG            0x0028
 #define ZL380xx_CUR_FW_APP_RUNNING               (1<< 15)
 #define ZL380xx_CUR_FW_IMG_NUM_SHIFT             0
 #define ZL380xx_CUR_FW_IMG_NUM_MASK              0xF

#define ZL380xx_SYSSTAT_REG                         0x066
#define ZL380xx_HOST_CMD_PARAM_RESULT_REG           0x0034 /*Host Command Param/Result register*/
#define HOST_FWR_EXEC_REG                           0x012C /*Fwr EXEC register*/
#define ZL380xx_HOST_CMD_REG                        0x0032 /*Host Command register*/
#define ZL380xx_CFG_REC_SIZE_REG                    0x01F0
#define ZL380xx_CFG_REC_CHKSUM_REG                  0x01F2
#define HOST_SW_FLAGS_CMD_NORST                     0x0004

#define HBI_CONFIG_IF_CMD           0xFD00
 #define HBI_CONFIG_ENDIAN_SHIFT     0
 #define HBI_CONFIG_IF_ENDIAN_LITTLE (1<<HBI_CONFIG_ENDIAN_SHIFT)
 #define HBI_CONFIG_INT_MODE_SHIFT    1
 #define HBI_CONFIG_INT_MODE_OD      (1<<HBI_CONFIG_INT_MODE_SHIFT)
 #define HBI_CONFIG_WAKE_SHIFT       7
 #define HBI_CONFIG_WAKE             (1<< HBI_CONFIG_WAKE_SHIFT)

#define ZL380xx_POWER_CFG_REG                        0x0206
#define ZL380xx_POWER_I2C_SHIFT                      (1<<13)

/* I2S Port Config Regs */
#define ZL380xx_TDMA_CFG_REG                         0x0260
 #define ZL380xx_TDM_CFG_FS_ALIGN_SHIFT              0
 #define ZL380xx_TDM_CFG_FS_ALIGN_LEFT               (1<<ZL380xx_TDM_CFG_FS_ALIGN_SHIFT)
 #define ZL380xx_TDM_CFG_FS_POL_SHIFT                2
 #define ZL380xx_TDM_CFG_FS_POL_LOW                  (1<<ZL380xx_TDM_CFG_FS_POL_SHIFT)
 #define ZL380xx_TDM_CFG_ENCODE_SHIFT                15
 #define ZL380xx_TDM_CFG_ENCODE_I2S                  (1<<ZL380xx_TDM_CFG_ENCODE_SHIFT)
 #define ZL380xx_TDM_CFG_ENCODE_PCM                 ~(1<<ZL380xx_TDM_CFG_ENCODE_SHIFT)

#define ZL380xx_TDMA_CLK_CFG_REG                     0x262
 #define ZL380xx_TDM_CLK_FSRATE_SHIFT                 0
 #define ZL380xx_TDM_CLK_FSRATE_MASK                  0x0007
 #define ZL380xx_TDM_CLK_PCLKRATE_SHIFT               4
 #define ZL380xx_TDM_CLK_PCLKRATE_MASK                0x7FF0
 #define ZL380xx_TDM_CLK_MASTER_SHIFT                 15
 #define ZL380xx_TDM_CLK_MASTER_SHIFT                 15
 #define ZL380xx_TDM_CLK_MASTER                      (1<<ZL380xx_TDM_CLK_MASTER_SHIFT)
 #define ZL380xx_TDM_CLK_SLAVE                      ~(1<<ZL380xx_TDM_CLK_MASTER_SHIFT)

#define ZL380xx_TDMA_CHANNEL1_CFG_REG                0x268
#define ZL380xx_TDMA_CHANNEL2_CFG_REG                0x26A
#define ZL380xx_TDMB_CHANNEL1_CFG_REG                0x280
 #define ZL380xx_TDM_CHANNEL_CFG_ENCODE_SHIFT         8
#define ZL380xx_TDMB_CHANNEL2_CFG_REG                0x282

#define ZL380xx_TDMB_CFG_REG                         0x278
#define ZL380xx_TDMB_CLK_CFG_REG                     0x27A


#define ZL380xx_XROSS_PT_AUD_OUTPUT_PATH_EN_REG    0x0202
 #define ZL380xx_XROSS_PT_AUD_DAC1EN_SHIFT          0
 #define ZL380xx_XROSS_PT_AUD_DAC1EN                (1<<ZL380xx_XROSS_PT_AUD_DAC1EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_DAC2EN_SHIFT          1
 #define ZL380xx_XROSS_PT_AUD_DAC2EN                (1<<ZL380xx_XROSS_PT_AUD_DAC2EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_I2S1L_EN_SHIFT        2
 #define ZL380xx_XROSS_PT_AUD_I2S1L_EN              (1<<ZL380xx_XROSS_PT_AUD_I2S1L_EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_I2S1R_EN_SHIFT        3
 #define ZL380xx_XROSS_PT_AUD_I2S1R_EN              (1<<ZL380xx_XROSS_PT_AUD_I2S1R_EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_I2S2L_EN_SHIFT        6
 #define ZL380xx_XROSS_PT_AUD_I2S2L_EN              (1<<ZL380xx_XROSS_PT_AUD_I2S2L_EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_I2S2R_EN_SHIFT        7
 #define ZL380xx_XROSS_PT_AUD_I2S2R_EN              (1<<ZL380xx_XROSS_PT_AUD_I2S2R_EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_SIN_EN_SHIFT          10
 #define ZL380xx_XROSS_PT_AUD_SIN_EN                (1<<ZL380xx_XROSS_PT_AUD_SIN_EN_SHIFT)
 #define ZL380xx_XROSS_PT_AUD_RIN_EN_SHIFT          11
 #define ZL380xx_XROSS_PT_AUD_RIN_EN                (1<<ZL380xx_XROSS_PT_AUD_RIN_EN_SHIFT)

#define ZL380xx_CP_DAC1_SRC_REG                    0x210
 #define ZL380xx_CP_AUD_SRCA_SHIFT                  0
 #define ZL380xx_CP_AUD_SRCA_MASK                   (0xFF << ZL380xx_CP_AUD_SRCA_SHIFT)
 #define ZL380xx_CP_AUD_SRCB_SHIFT                  8
 #define ZL380xx_CP_AUD_SRCB_MASK                   (0xFF << ZL380xx_CP_AUD_SRCB_SHIFT)

#define ZL380xx_CP_DAC2_SRC_REG                    0x212
#define ZL380xx_CP_I2S1L_SRC_REG                   0x214
#define ZL380xx_CP_I2S1R_SRC_REG                   0x216
#define ZL380xx_CP_I2S2L_SRC_REG                   0x21C
#define ZL380xx_CP_I2S2R_SRC_REG                   0x21E
#define ZL380xx_CP_AECSIN_SRC_REG                  0x224
#define ZL380xx_CP_AECRIN_SRC_REG                  0x226
#define ZL380xx_CP_DAC1_GAIN_REG                   0x0238
#define ZL380xx_CP_DAC2_GAIN_REG                   0x023A
#define ZL380xx_CP_I2S1L_GAIN_REG                  0x023C
#define ZL380xx_CP_I2S1R_GAIN_REG                  0x023E
#define ZL380xx_CP_I2S2L_GAIN_REG                  0x0244
#define ZL380xx_CP_I2S2R_GAIN_REG                  0x0246

#define ZL380xx_CP_AEC_SIN_GAIN_REG                0x024C
#define ZL380xx_CP_AEC_RIN_GAIN_REG                0x024E
 #define ZL380xx_CP_GAINA_SHIFT                     0
 #define ZL380xx_CP_GAINA_MASK                      (0xFF << ZL380xx_CP_GAINA_SHIFT)
 #define ZL380xx_CP_GAINB_SHIFT                     8
 #define ZL380xx_CP_GAINB_MASK                      (0xFF << ZL380xx_CP_GAINB_SHIFT)



#define ZL380xx_MICEN_CFG_REG                     0x02B0
 #define ZL380xx_MICEN_MIC1_SHIFT                  0
 #define ZL380xx_MICEN_MIC1                        (1<<ZL380xx_MICEN_MIC1_SHIFT)
 #define ZL380xx_MICEN_MIC2_SHIFT                  1
 #define ZL380xx_MICEN_MIC2                        (1<<ZL380xx_MICEN_MIC2_SHIFT)
 #define ZL380xx_MICEN_MIC3_SHIFT                  2
 #define ZL380xx_MICEN_MIC3                        (1<<ZL380xx_MICEN_MIC3_SHIFT)

#define ZL380xx_DIG_MIC_GAIN_REG                  0x02B2

/* AEC module register bits */
#define ZL380xx_AEC_CTRL0_REG                       0x300
 #define ZL380xx_AEC_RST_SHIFT                       0
 #define ZL380xx_AEC_RST                             (1<<ZL380xx_AEC_RST_SHIFT)
 #define ZL380xx_AEC_MBYPASS_SHIFT                   1
 #define ZL380xx_AEC_MBYPASS                         (1<<ZL380xx_AEC_MBYPASS_SHIFT)
 #define ZL380xx_AEC_RIN_EQ_SHIFT                    2
 #define ZL380xx_AEC_RIN_EQ_DISABLE                  (1<<ZL380xx_AEC_RIN_EQ_SHIFT)
 #define ZL380xx_AEC_BYPASS_SHIFT                    4
 #define ZL380xx_AEC_BYPASS                          (1<<ZL380xx_AEC_BYPASS_SHIFT)
 #define ZL380xx_AEC_AUDENHBYPASS_SHIFT              5
 #define ZL380xx_AEC_AUDENHBYPASS                    (1<<ZL380xx_AEC_AUDENHBYPASS_SHIFT)
 #define ZL380xx_AEC_MUTE_ROUT_SHIFT                  7
 #define ZL380xx_AEC_MUTE_ROUT                       (1<<ZL380xx_AEC_MUTE_ROUT_SHIFT)
 #define ZL380xx_AEC_MUTE_SOUT_SHIFT                  8
 #define ZL380xx_AEC_MUTE_SOUT                       (1<<ZL380xx_AEC_MUTE_SOUT_SHIFT)
 #define ZL380xx_AEC_RIN_HPF_SHIFT                    9
 #define ZL380xx_AEC_RIN_HPF_DISABLE                 (1<<ZL380xx_AEC_RIN_HPF_SHIFT)
 #define ZL380xx_AEC_SIN_HPF_SHIFT                    10
 #define ZL380xx_AEC_SIN_HPF_DISABLE                 (1<<ZL380xx_AEC_SIN_HPF_SHIFT)
 #define ZL380xx_AEC_HD_SHIFT                         11
 #define ZL380xx_AEC_HD_DISABLE                      (1<<ZL380xx_AEC_HD_SHIFT)
 #define ZL380xx_AEC_AGCDIS_SHIFT                     12
 #define ZL380xx_AEC_AGC_DISABLE                     (1<<ZL380xx_AEC_AGCDIS_SHIFT)
 #define ZL380xx_AEC_NBD_SHIFT                        13
 #define ZL380xx_AEC_NBD_DISABLE                     (1<<ZL380xx_AEC_NBD_SHIFT)
 #define ZL380xx_AEC_SWITCHED_ATTEN_SHIFT             14
 #define ZL380xx_AEC_SWITCHED_ATTEN_EN               (1<<ZL380xx_AEC_SWITCHED_ATTEN_SHIFT)

#define ZL380xx_ROUT_GAIN_CTRL_REG               0x030A
 #define ZL380xx_ROUT_GAIN_CTRL_UGAIN_SHIFT       0
 #define ZL380xx_ROUT_GAIN_CTRL_UGAIN_MASK        (0x7F << ZL380xx_ROUT_GAIN_CTRL_UGAIN_SHIFT)

#define ZL380xx_SOUT_GAIN_CTRL_REG               0x030C
 #define ZL380xx_SIN_GAIN_PAD_SHIFT               0
 #define ZL380xx_SIN_GAIN_PAD_MASK                (0xFF<<ZL380xx_SIN_GAIN_PAD_SHIFT)
#define ZL380xx_SOUT_GAIN_PAD_SHIFT              8
#define ZL380xx_SOUT_GAIN_PAD_MASK               (0xF << ZL380xx_SOUT_GAIN_PAD_SHIFT)

/* Automatic Gain Control Regs */
#define AUD_AGC_LVL_REG                           0x045B

#define AUD_SOUT_HI_SGNL_THRESHOLD_REG            0x05F1 /* automatically adjusts Sout gain */

#define AUD_ALC_CONTROL_REG                       0x05F3

#define AUD_USER_GAIN_CTRL_REG                    0x046B
 #define AUD_USER_ROUT_GAIN_SHIFT                  0
 #define AUD_USER_XROUT_GAIN_ADJUST_SHIFT          7

#define AUD_SEC_CHAN_USER_GAIN_CTRL_REG           0x046A
 #define AUD_SEC_CHAN_RX_PATH_GAIN_SHIFT           0
 #define AUD_SEC_CHAN_SEND_PATH_GAIN_SHIFT         4

#endif
