/**
******************************************************************************
* @file    ms7210_typedef.h
* @author  
* @version V1.0.0
* @date    24-Nov-2020
* @brief   Definitions for typedefs.
*
* Copyright (c) 2009-2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#ifndef __MACROSILICON_MS7210_COMMON_TYPEDEF_H__
#define __MACROSILICON_MS7210_COMMON_TYPEDEF_H__

#include <linux/types.h>

/*
** Global typedefs.
*/
#ifndef MS_GLOBAL_DEFINE
#define MS_GLOBAL_DEFINE

#ifndef NULL
#define NULL ((void*)0)
#endif

// For ARM platform
#if defined (_PLATFORM_ARM_)
#define  __CODE const
#define  __XDATA
#define  __DATA
#define __IDATA
#define  __NEAR
#define  __IO volatile


typedef _Bool BOOL;

#elif defined (__STD_GCC__)
#define  __CODE const
#define  __XDATA
#define  __DATA
#define __IDATA
#define  __NEAR
#define  __IO volatile


typedef _Bool BOOL;

#elif defined (_PLATFORM_WINDOWS_)
#define  __CODE
#define  __XDATA
#define  __DATA
#define __IDATA
#define  __NEAR
#define  __IO

#elif defined (__KEIL_C__)
#define __CODE code
#define __XDATA xdata
#define __DATA data
#define __IDATA idata
#define __NEAR
#define __IO volatile

//bool bype
typedef bit BOOL;

#elif defined (__CSMC__)
#define __CODE const
#define __XDATA
#define __DATA 
#define __IDATA 
#define __NEAR @near
#define __IO volatile

//bool bype
typedef _Bool BOOL;
#elif defined (_IAR_)
#define __CODE const
#define __XDATA
#define __DATA 
#define __IDATA 
#define __NEAR @near
#define __IO volatile

//bool bype
typedef _Bool BOOL;
#endif // end of compiler platform define 


//unsigned integer type
typedef unsigned char UINT8;
typedef char          CHAR;
typedef unsigned short UINT16;

//signed integer type
typedef signed char INT8;
typedef signed short INT16;

//32bit type
#if defined (_PLATFORM_ARM_) || defined (_PLATFORM_WINDOWS_)
typedef unsigned int UINT32;
typedef signed int INT32;
#else
typedef unsigned int UINT32;
typedef signed int INT32;
#endif

#define VOID void

// #define FALSE 0
// #define TRUE  1

#define DISABLE 0
#define ENABLE  1

#define LOW     0
#define HIGH    1

#define OFF     0
#define ON      1

typedef bool BOOL;


// Helper macros.
#define _UNUSED_(arg)     ((arg) = (arg))

#ifndef _countof
#define _countof(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))
#endif

// #ifndef max
// #define max(a, b)   (((a)>(b))?(a):(b)) 
// #endif

// #ifndef min
// #define min(a, b)   (((a)<(b))?(a):(b))
// #endif



/*
* generic mask macro definitions
*/
#define MSRT_BIT0                   (0x01)
#define MSRT_BIT1                   (0x02)
#define MSRT_BIT2                   (0x04)
#define MSRT_BIT3                   (0x08)
#define MSRT_BIT4                   (0x10)
#define MSRT_BIT5                   (0x20)
#define MSRT_BIT6                   (0x40)
#define MSRT_BIT7                   (0x80)
    
#define MSRT_MSB8BITS               MSRT_BIT7
#define MSRT_LSB                    MSRT_BIT0
    
// Bit7 ~ Bit0
#define MSRT_BITS7_6                (0xC0)
#define MSRT_BITS7_5                (0xE0)
#define MSRT_BITS7_4                (0xF0)
#define MSRT_BITS7_3                (0xF8)
#define MSRT_BITS7_2                (0xFC)
#define MSRT_BITS7_1                (0xFE)
#define MSRT_BITS7_0                (0xff)

#define MSRT_BITS6_5                (0x60)
#define MSRT_BITS6_4                (0x70)
#define MSRT_BITS6_2                (0x7c)
#define MSRT_BITS6_1                (0x7e)
#define MSRT_BITS6_0                (0x7f)

#define MSRT_BITS5_4                (0x30)
#define MSRT_BITS5_3                (0x38)
#define MSRT_BITS5_2                (0x3c)
#define MSRT_BITS5_0                (0x3f)

#define MSRT_BITS4_3                (0x18)
#define MSRT_BITS4_2                (0x1c)
#define MSRT_BITS4_1                (0x1e)
#define MSRT_BITS4_0                (0x1f)

#define MSRT_BITS3_2                (0x0C)
#define MSRT_BITS3_1                (0x0E)
#define MSRT_BITS3_0                (0x0F)

#define MSRT_BITS2_1                (0x06)
#define MSRT_BITS2_0                (0x07)

#define MSRT_BITS1_0                (0x03)

#endif


// 20121207, for video data type 
#ifndef MS_TIMING_DEFINE
#define MS_TIMING_DEFINE

typedef struct _T_MS_VIDEO_SIZE_
{
    UINT16 u16_h;
    UINT16 u16_v;
} VIDEOSIZE_T;

typedef struct _T_MS7210_VIDEO_TIMING_
{
    UINT8           u8_polarity;
    UINT16          u16_htotal;
    UINT16          u16_vtotal;
    UINT16          u16_hactive;
    UINT16          u16_vactive;
    UINT16          u16_pixclk;     /*10000hz*/
    UINT16          u16_vfreq;      /*0.01hz*/
    UINT16          u16_hoffset;    /* h sync start to h active*/
    UINT16          u16_voffset;    /* v sync start to v active*/
    UINT16          u16_hsyncwidth;
    UINT16          u16_vsyncwidth;
} VIDEOTIMING_T;

typedef enum _E_SYNC_POLARITY_
{
    ProgrVNegHNeg = 0x01,
    ProgrVNegHPos = 0x03,
    ProgrVPosHNeg = 0x05,
    ProgrVPosHPos = 0x07,

    InterVNegHNeg = 0x00,
    InterVNegHPos = 0x02,
    InterVPosHNeg = 0x04,
    InterVPosHPos = 0x06
}SYNCPOLARITY_E;

typedef struct _T_WIN_BORDER_
{
    INT16 top;
    INT16 bottom;
    INT16 left;
    INT16 right;
} WINBORDER_T;

#endif


//
//HD video
#ifndef MS_HD_DEFINE
#define MS_HD_DEFINE

typedef enum _E_HD_VIDEO_CLK_REPEAT_
{
    HD_X1CLK      = 0x00,
    HD_X2CLK      = 0x01,
    HD_X3CLK      = 0x02,
    HD_X4CLK      = 0x03,
    HD_X5CLK      = 0x04,
    HD_X6CLK      = 0x05,
    HD_X7CLK      = 0x06,
    HD_X8CLK      = 0x07,
    HD_X9CLK      = 0x08,
    HD_X10CLK     = 0x09
}HD_CLK_RPT_E;

typedef enum _E_HD_VIDEO_ASPECT_RATIO_
{
    HD_4X3     = 0x01,
    HD_16X9    = 0x02
}HD_ASPECT_RATIO_E;


typedef enum _E_HD_VIDEO_SCAN_INFO_
{
    HD_OVERSCAN     = 0x01,    //television type
    HD_UNDERSCAN    = 0x02     //computer type
}HD_SCAN_INFO_E;

typedef enum _E_HD_COLOR_SPACE_
{
    HD_RGB        = 0x00,
    HD_YCBCR422   = 0x01,
    HD_YCBCR444   = 0x02,
    HD_YUV420     = 0x03
}HD_CS_E;

typedef enum _E_HD_COLOR_DEPTH_
{
    HD_COLOR_DEPTH_8BIT    = 0x00,
    HD_COLOR_DEPTH_10BIT   = 0x01,
    HD_COLOR_DEPTH_12BIT   = 0x02,
    HD_COLOR_DEPTH_16BIT   = 0x03
}HD_COLOR_DEPTH_E;

typedef enum _E_HD_COLORIMETRY_
{
    HD_COLORIMETRY_601    = 0x00,
    HD_COLORIMETRY_709    = 0x01,
    HD_COLORIMETRY_656    = 0x02,
    HD_COLORIMETRY_1120   = 0x03,
    HD_COLORIMETRY_SMPTE  = 0x04,
    HD_COLORIMETRY_XVYCC601 = 0x05,
    HD_COLORIMETRY_XVYCC709 = 0x06
}HD_COLORIMETRY_E;

//HD vendor specific
typedef enum _E_HD_VIDEO_FORMAT_
{
    HD_NO_ADD_FORMAT,
    HD_4Kx2K_FORMAT,
    HD_3D_FORMAT
}HD_VIDEO_FORMAT_E;

typedef enum _E_HD_4Kx2K_VIC_
{
    HD_4Kx2K_30HZ = 0x01,
    HD_4Kx2K_25HZ,
    HD_4Kx2K_24HZ,
    HD_4Kx2K_24HZ_SMPTE
}HD_4Kx2K_VIC_E;

typedef enum _E_HD_3D_STRUCTURE_
{
    HD_FRAME_PACKING,
    HD_FIELD_ALTERNATIVE,
    HD_LINE_ALTERNATIVE,
    HD_SIDE_BY_SIDE_FULL,
    L_DEPTH,
    L_DEPTH_GRAPHICS,
    SIDE_BY_SIDE_HALF = 8
}HD_3D_STRUCTURE_E;

//HD audio
typedef enum _E_HD_AUDIO_MODE_
{
    HD_AUD_MODE_AUDIO_SAMPLE  = 0x00,
    HD_AUD_MODE_HBR           = 0x01,
    HD_AUD_MODE_DSD           = 0x02,
    HD_AUD_MODE_DST           = 0x03
}HD_AUDIO_MODE_E;

typedef enum _E_HD_AUDIO_I2S_RATE_
{
    HD_AUD_RATE_44K1  = 0x00,
    HD_AUD_RATE_48K   = 0x02,
    HD_AUD_RATE_32K   = 0x03,
    HD_AUD_RATE_88K2  = 0x08,
    HD_AUD_RATE_96K   = 0x0A,
    HD_AUD_RATE_176K4 = 0x0C,
    HD_AUD_RATE_192K  = 0x0E
}HD_AUDIO_RATE_E;

typedef enum _E_HD_AUDIO_LENGTH_
{
    HD_AUD_LENGTH_16BITS    = 0x00,
    HD_AUD_LENGTH_20BITS    = 0x01,
    HD_AUD_LENGTH_24BITS    = 0x02
}HD_AUDIO_LENGTH_E;

typedef enum _E_HD_AUDIO_CHANNEL_
{
    HD_AUD_2CH    = 0x01,
    HD_AUD_3CH    = 0x02,
    HD_AUD_4CH    = 0x03,
    HD_AUD_5CH    = 0x04,
    HD_AUD_6CH    = 0x05,
    HD_AUD_7CH    = 0x06,
    HD_AUD_8CH    = 0x07
}HD_AUDIO_CHN_E;


typedef struct _T_HD_CONFIG_PARA_
{   
    UINT8  u8_hd_flag;          // FALSE = dvi out;  TRUE = hd out
    UINT8  u8_vic;                // reference to CEA-861 VIC
    UINT16 u16_video_clk;         // TMDS video clk, uint 10000Hz
    UINT8  u8_clk_rpt;            // enum refer to HD_CLK_RPT_E. X2CLK = 480i/576i, others = X1CLK
    UINT8  u8_scan_info;          // enum refer to HD_SCAN_INFO_E
    UINT8  u8_aspect_ratio;       // enum refer to HD_ASPECT_RATIO_E
    UINT8  u8_color_space;        // enum refer to HD_CS_E
    UINT8  u8_color_depth;        // enum refer to HD_COLOR_DEPTH_E
    UINT8  u8_colorimetry;        // enum refer to HD_COLORIMETRY_E. IT601 = 480i/576i/480p/576p, ohters = IT709
    //
    UINT8  u8_video_format;       // enum refer to HD_VIDEO_FORMAT_E
    UINT8  u8_4Kx2K_vic;          // enum refer to HD1.4 extented resolution transmission
    UINT8  u8_3D_structure;       // enum refer to HD_3D_STRUCTURE_E
    //
    UINT8  u8_audio_mode;         // enum refer to HD_AUDIO_MODE_E
    UINT8  u8_audio_rate;         // enum refer to HD_AUDIO_RATE_E
    UINT8  u8_audio_bits;         // enum refer to HD_AUDIO_LENGTH_E
    UINT8  u8_audio_channels;     // enum refer to HD_AUDIO_CHN_E
    UINT8  u8_audio_speaker_locations;  // 0~255, refer to CEA-861 audio infoframe, BYTE4
}HD_CONFIG_T;
#endif


#ifndef MS_HDTX_DEFINE
#define MS_HDTX_DEFINE

//HD TX module define

//HD TX channel
typedef enum _E_HD_TX_CHANNEL_
{
    HD_TX_CHN0      = 0x00,
    HD_TX_CHN1      = 0x01,
    HD_TX_CHN2      = 0x02,
    HD_TX_CHN3      = 0x03
}HD_CHANNEL_E;

typedef struct _T_HD_ENCRYPT_RI_
{   
    UINT8 TX_Ri0;
    UINT8 TX_Ri1;
    UINT8 RX_Ri0;
    UINT8 RX_Ri1;
}HD_ENCRYPT_RI;

//HD EDID
typedef struct _T_HD_EDID_FLAG_
{   
    UINT8    u8_hd_sink;              //1 = HD sink, 0 = dvi
    UINT8    u8_color_space;            //color space support flag, flag 1 valid. BIT5: YCBCR444 flag; BIT4: YCBCR422 flag.(RGB must be support)
    //
    UINT8    u8_edid_total_blocks;      //block numbers, 128bytes in one block
    UINT16   u16_preferred_pixel_clk;   //EDID Preferred pixel clock rate, u16_preferred_pixel_clk * 10000Hz, ERROR code is 0xFFFF
    UINT32   u32_preferred_timing;      //EDID Preferred Timing (Hact*Vact)
    UINT8    u8_max_tmds_clk;           //HD VSDB max tmds clock, u8_max_tmds_clk * 5 Mhz
    UINT32   u32_max_video_block_timing;//EDID max video block timing (Hact*Vact)
    UINT8    u8_hd_2_0_flag;          //1 = HD 2.0
}HD_EDID_FLAG_T;
#endif


//dvin
#ifndef MS_DVIN_DEFINE
#define MS_DVIN_DEFINE

typedef struct T_DVIN_CONFIG
{
    UINT8 u8_cs_mode;  //refer to DVIN_CS_MODE_E
    UINT8 u8_bw_mode;  //refer to DVIN_BW_MODE_E
    UINT8 u8_sq_mode;  //refer to DVIN_SQ_MODE_E
    UINT8 u8_dr_mode;  //refer to DVIN_DR_MODE_E
    UINT8 u8_sy_mode;  //refer to DVIN_SY_MODE_E
}DVIN_CONFIG_T;

typedef enum _E_DVIN_CS_MODE_
{
    DVIN_CS_MODE_RGB,
    DVIN_CS_MODE_YUV444,
    DVIN_CS_MODE_YUV422
}DVIN_CS_MODE_E;

typedef enum _E_DVIN_BW_MODE_
{
    DVIN_BW_MODE_16_20_24BIT,
    DVIN_BW_MODE_8_10_12BIT
}DVIN_BW_MODE_E;

typedef enum _E_DVIN_SQ_MODE_
{
    DVIN_SQ_MODE_NONSEQ,
    DVIN_SQ_MODE_SEQ
}DVIN_SQ_MODE_E;

typedef enum _E_DVIN_DR_MODE_
{
    DVIN_DR_MODE_SDR,
    DVIN_DR_MODE_DDR
}DVIN_DR_MODE_E;

typedef enum _E_DVIN_SY_MODE_
{
    DVIN_SY_MODE_HSVSDE,      // 8/16/24-bit BT601
    DVIN_SY_MODE_HSVS,
    DVIN_SY_MODE_VSDE,        // non suport interlace mode
    DVIN_SY_MODE_DEONLY,
    DVIN_SY_MODE_EMBEDDED,    // 16-bit BT1120 or 8bit BT656
    DVIN_SY_MODE_2XEMBEDDED,  // 8-bit BT1120
    DVIN_SY_MODE_BTAT1004     // 16-bit BTA-T1004
}DVIN_SY_MODE_E;

typedef struct T_DVIN_TIMING_DET
{
    UINT16 u16_htotal;
    UINT16 u16_vtotal;
    UINT16 u16_hactive;
    UINT16 u16_pixclk;     /*10000hz*/
}DVIN_TIMING_DET_T;

#endif

#endif  // __MACROSILICON_MS7210_COMMON_TYPEDEF_H__
