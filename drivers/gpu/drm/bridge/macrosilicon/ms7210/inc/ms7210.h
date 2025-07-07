/**
******************************************************************************
* @file    ms7210.h
* @author  
* @version V1.0.0
* @date    24-Nov-2020
* @brief   ms7210 SDK Library interfaces declare
* @history    
*
* Copyright (c) 2009-2020, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#ifndef __MACROSILICON_MS7210_H__
#define __MACROSILICON_MS7210_H__

#include "ms7210_comm.h"


#ifdef __cplusplus
extern "C" {
#endif

void ms7210_set_i2c_adapter(struct i2c_client *_i2c_main);

/***************************************************************
*  Function name:     ms7210_sdk_version
*  Description:       get the SDK version
*  Input parameters:  None
*  Output parameters: None
*  Returned value:    version string
***************************************************************/
CHAR* ms7210_sdk_version(VOID);

/***************************************************************
*  Function name:     ms7210_chip_connect_detect
*  Description:       get the chip connect status
*  Input parameters:  u8_chip_addr: 0 = auto check chip addr, others = set i2c slave addr
*  Output parameters: None
*  Returned value:    connect status, 0: disconnect 1: connect
***************************************************************/
BOOL ms7210_chip_connect_detect(void);

/***************************************************************
*  Function name:     ms7210_dvin_mode_config
*  Description:       config dvin mode, only config when system init
*  Input parameters:  t_dvin_config: dvin mode select
*                     u8_spdif_in: 0 = i2s, 1 = spdif with mclk, 2 = spdif
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_dvin_init(DVIN_CONFIG_T *t_dvin_config, UINT8 u8_spdif_in);

/***************************************************************
*  Function name:     ms7210_dvin_data_swap
*  Description:       set data pin swap mode
*  Input parameters:  u8_swap_mode: 0 = D0~D23 swap, 1 = rb swap for rgb/yuv444,
*                                   2 = yc swap for sequential 16-bit yuv422
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_dvin_data_swap(UINT8 u8_swap_mode);

/***************************************************************
*  Function name:     ms7210_dvin_phase_adjust
*  Description:       output phase adjust
*  Input parameters:  b_invert: output clk invert
*                     u8_delay: 1ns of one step, max 3ns
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_dvin_phase_adjust(BOOL b_invert, UINT8 u8_delay);

/***************************************************************
*  Function name:     ms7210_dvin_timing_get
*  Description:       regeneration hs&vs&de, must do when input is not hs+vs+de
*  Input parameters:  t_dvin_det: use for timing regeneration coeficient
*  Output parameters: None
*  Returned value:    None
***************************************************************/
BOOL ms7210_dvin_timing_get(DVIN_TIMING_DET_T *t_dvin_det);

/***************************************************************
*  Function name:     ms7210_dvin_timing_config
*  Description:       regeneration hs&vs&de, must do when input is not hs+vs+de
*  Input parameters:  t_dvin_config: use for timing regeneration coeficient
*                     ptTiming: use for timing regeneration
*                     pt_hd_tx: may change output repeat times
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_dvin_timing_config(DVIN_CONFIG_T *t_dvin_config, VIDEOTIMING_T *ptTiming, HD_CONFIG_T *pt_hd_tx);

/***************************************************************
*  Function name:     ms7210_dvin_video_config
*  Description:       enable/disable video pad input
*  Input parameters:  b_config: 1 = enable output, 0 = disable output
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_dvin_video_config(BOOL b_config);

/***************************************************************
*  Function name:     ms7210_hdtx_hpd_detect
*  Description:       get tx hpd status
*  Input parameters:  None
*  Output parameters: None
*  Returned value:    tx hpd status
***************************************************************/
BOOL ms7210_hdtx_hpd_detect(VOID);

/***************************************************************
*  Function name:     ms7210_hdtx_edid_get
*  Description:       get tx edid
*  Input parameters:  None
*  Output parameters: u8_edid: tx edid
*  Returned value:    tx edid get success or not
***************************************************************/
BOOL ms7210_hdtx_edid_get(UINT8 *u8_edid);

/***************************************************************
*  Function name:     ms7210_hdtx_input_timing_stable_get
*  Description:       get tx timing status
*  Input parameters:  None
*  Output parameters: None
*  Returned value:    tx timing stable status
***************************************************************/
BOOL ms7210_hdtx_input_timing_stable_get(VOID);

/***************************************************************
*  Function name:     ms7210_hdtx_output_config
*  Description:       set tx output infoframe
*  Input parameters:  pt_hd_tx
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_hdtx_output_config(HD_CONFIG_T *pt_hd_tx);

/***************************************************************
*  Function name:     ms7210_hdtx_shutdown_output
*  Description:       set tx output shutdown
*  Input parameters:  None
*  Output parameters: None
*  Returned value:    None
***************************************************************/
VOID ms7210_hdtx_shutdown_output(VOID);

#ifdef __cplusplus
}
#endif

#endif  //__MACROSILICON_MS7210_H__
