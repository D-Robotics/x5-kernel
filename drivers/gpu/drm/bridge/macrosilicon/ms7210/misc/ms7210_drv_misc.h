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
#ifndef __MACROSILICON_MS7210_DRV_MISC_H__
#define __MACROSILICON_MS7210_DRV_MISC_H__


#ifdef __cplusplus
extern "C" {
#endif

MS7210_DRV_API UINT8 ms7210drv_misc_package_sel_get(VOID);

/***************************************************************
*  Function name:   ms7210drv_misc_chipisvalid
*  Description:     check ms7210 chip is valid or not
*  Entry:           NULL             
*  Returned value:  BOOL (valid is true)
*  Remark: 
***************************************************************/
MS7210_DRV_API BOOL ms7210drv_misc_chipisvalid(VOID);

MS7210_DRV_API VOID ms7210drv_misc_rc_freq_set(VOID);

MS7210_DRV_API VOID ms7210drv_misc_freqm_pclk_enable(VOID);
MS7210_DRV_API UINT16 ms7210drv_misc_freqm_pclk_get(VOID);


MS7210_DRV_API VOID ms7210drv_misc_audio_pad_in_spdif(UINT8 u8_spdif_in);

/***************************************************************
*  Function name:   ms7210drv_misc_audio_i2s_mclk_div
*  Description:     I2S mclk divider, default 128fs
*  Entry:           [in]u8_div
*                       0: div 1
*                       1: div 2
*                       2: div 4
*                       3: div 8
*
*  Returned value:  None
*  Remark: 
***************************************************************/
MS7210_DRV_API VOID ms7210drv_misc_audio_mclk_div(UINT8 u8_div);

/***************************************************************
*  Function name:   ms7210drv_misc_dig_pads_pull_set
*  Description:     dig pads(vdata+hs+vs+de+vclk) pull status set
*  Entry:           [in]u8_pull_status
*                       0: floating
*                       1: pull up
*                       2: pull down
*
*  Returned value:  None
*  Remark: 
***************************************************************/
MS7210_DRV_API VOID ms7210drv_misc_dig_pads_pull_set(UINT8 u8_pull_status);

MS7210_DRV_API VOID ms7210drv_csc_config_input(DVIN_CS_MODE_E e_cs);
MS7210_DRV_API VOID ms7210drv_csc_config_output(HD_CS_E e_cs);


#ifdef __cplusplus
}
#endif

#endif //__MACROSILICON_MS7210_DRV_MISC_H__
