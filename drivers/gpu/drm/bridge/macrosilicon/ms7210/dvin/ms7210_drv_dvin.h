/**
******************************************************************************
* @file    ms7210_drv_misc.h
* @author  
* @version V1.0.0
* @date    15-Nov-2016
* @brief   misc module driver declare
* @history 
*
* Copyright (c) 2009 - 2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#ifndef __MACROSILICON_MS7210_DRV_DVOUT_H__
#define __MACROSILICON_MS7210_DRV_DVOUT_H__


#ifdef __cplusplus
extern "C" {
#endif

MS7210_DRV_API BOOL ms7210drv_dvin_mode_config(DVIN_CONFIG_T *t_dvin_config);

MS7210_DRV_API VOID ms7210drv_dvin_timing_detect(DVIN_TIMING_DET_T *t_dvin_det);

MS7210_DRV_API BOOL ms7210drv_dvin_timing_config(DVIN_CONFIG_T *t_dvin_config, VIDEOTIMING_T *t_output_timing, UINT8 *u8_rpt);

MS7210_DRV_API VOID ms7210drv_dvin_data_swap_all(VOID);

MS7210_DRV_API VOID ms7210drv_dvin_data_swap_rb_channel(VOID);

MS7210_DRV_API VOID ms7210drv_dvin_data_swap_yc_channel(VOID);

MS7210_DRV_API VOID ms7210drv_dvin_clk_reset_release(BOOL b_release);

MS7210_DRV_API VOID ms7210drv_dvin_pa_adjust(BOOL b_invert, UINT8 u8_delay);

//MS7210_DRV_API VOID ms7210drv_dvin_clk_sel(UINT8 u8_clk_rpt, UINT16 u16_video_clk);

#ifdef __cplusplus
}
#endif

#endif //__MACROSILICON_MS7210_DVOUT_H__
