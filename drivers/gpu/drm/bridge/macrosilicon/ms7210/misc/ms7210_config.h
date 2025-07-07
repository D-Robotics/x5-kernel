/**
******************************************************************************
* @file    ms7210_config.h
* @author  
* @version V1.0.0
* @date    15-Nov-2014
* @brief   header files
*
* Copyright (c) 2009-2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#ifndef __MACROSILICON_MS7210_CONFIG_H__
#define __MACROSILICON_MS7210_CONFIG_H__

//#include "ms7210_app_config.h"


#ifndef MS7210_EXT_XTAL
#define MS7210_EXT_XTAL             (27000000UL) //uint Hz
#endif

#ifndef MS7210_USE_I2CBUS
#define MS7210_USE_I2CBUS           (1)
#endif

#ifndef MS7210_I2C_ADDR
#define MS7210_I2C_ADDR             (0xB2)
#endif

#ifndef MS7210_HD_RX_EDID_ENABLE
#define MS7210_HD_RX_EDID_ENABLE  (1)
#endif

#ifndef MS7210_HD_TX_ENCRYPT
#define MS7210_HD_TX_ENCRYPT         (1)
#endif

#ifndef MS7210_HD_TX_ENCRYPT_METHOD
#define MS7210_HD_TX_ENCRYPT_METHOD  (0)
#endif

#ifndef MS7210_HD_TX_EDID
#define MS7210_HD_TX_EDID         (1)
#endif

#ifndef MS7210_HD_RX_INT_ENABLE
#define MS7210_HD_RX_INT_ENABLE   (1)
#endif

#ifndef MS7210_RXPLL_METHOD
#define MS7210_RXPLL_METHOD     (1) //new method
#endif


//RC_25M config
#ifndef MS7210_RC_CTRL1             
#define MS7210_RC_CTRL1             (0x81) 
#endif

#ifndef MS7210_RC_CTRL2
#define MS7210_RC_CTRL2             (0x3B)     //24M RC
#endif


#ifndef MS7210_HD_RX_TMDS_OVERLOAD_PROTECT_ENABLE
#define MS7210_HD_RX_TMDS_OVERLOAD_PROTECT_ENABLE     (0) 
#endif


#ifndef MS7210_AUDIO_SAMPLE_PACKET_192BIT_BYPASS_ENABLE
#define MS7210_AUDIO_SAMPLE_PACKET_192BIT_BYPASS_ENABLE     (0)
#endif


////////////////////////////////////////////////////////////////////
#ifndef MS7210_FPGA_VERIFY
#define MS7210_FPGA_VERIFY          (0)
#endif

#ifndef MS7210_EXT_APIS
#define MS7210_EXT_APIS             (0)     //external drive
#endif

#ifndef MS7210_DEBUG_LOG
#define MS7210_DEBUG_LOG            (0)      //use uart trace log
#endif

#endif  // __MACROSILICON_MS7210_CONFIG_H__
