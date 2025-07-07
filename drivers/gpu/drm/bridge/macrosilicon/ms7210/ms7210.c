/**
******************************************************************************
* @file    ms7210.c
* @author  
* @version V1.0.0
* @date    24-Nov-2020
* @brief   ms7210 SDK Library interfaces source file
* @history    
*
* Copyright (c) 2009-2020, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#include "ms7210_comm.h"
#include "ms7210_drv_hd_tx.h"
#include "ms7210_drv_misc.h"
#include "ms7210_drv_dvin.h"
#include "ms7210_mpi.h"
#include "ms7210.h"
#include "autobuild.h" 
/****************************************/

/****************************************/
// #ifdef  MS_AUTO_BUILD_VERSION
// #define MS7210_SDK_VERSION  MS_AUTO_BUILD_VERSION
// #else
// #define MS7210_SDK_VERSION __DATE__" "__TIME__
// #endif
// static __CODE CHAR g_sdk_version[] = {MS7210_SDK_VERSION};

// static __CODE UINT8 g_u8_chip_addr[] = { 0xB2, 0x56 };
// static __CODE UINT8 g_u8_chip_addr_num = sizeof(g_u8_chip_addr) / sizeof(UINT8);
/****************************************/

/****************************************/
CHAR* ms7210_sdk_version(VOID)
{
    // return (CHAR*)g_sdk_version;
	return NULL;
}

void ms7210_set_i2c_adapter(struct i2c_client *_i2c_main)
{
	mculib_set_i2c_adapter(_i2c_main);
}

BOOL ms7210_chip_connect_detect(void)
{
#if 0
    UINT8 i;
    for (i = 0; i < (u8_chip_addr ? 1 : g_u8_chip_addr_num); i++)
    {
        HAL_SetChipAddr(u8_chip_addr ? u8_chip_addr : g_u8_chip_addr[i]);
        if (ms7210drv_misc_package_sel_get() == 0x02)
        {
            if (ms7210drv_misc_chipisvalid())
            {
                return TRUE;
            }
        }
    }
    return FALSE;
#else
	if (ms7210drv_misc_chipisvalid())
	{
		printk("%s(): ms7210 is valid\n", __FUNCTION__);
		return TRUE;
	}
	printk("%s(): Error ms7210 is not valid\n", __FUNCTION__);
	return FALSE;
#endif
}

VOID ms7210_dvin_init(DVIN_CONFIG_T *t_dvin_config, UINT8 u8_spdif_in)
{
    ms7210drv_misc_rc_freq_set();
    ms7210drv_csc_config_input((DVIN_CS_MODE_E)t_dvin_config->u8_cs_mode);
    if (ms7210drv_dvin_mode_config(t_dvin_config))
    {
        ms7210drv_hd_tx_phy_set_clk_ratio(1);
    }
    ms7210drv_misc_dig_pads_pull_set(2);
    ms7210drv_misc_audio_pad_in_spdif(u8_spdif_in);
    ms7210drv_hd_tx_shell_set_audio_mode(u8_spdif_in ? 1 : 0);
    ms7210drv_misc_freqm_pclk_enable();
}

VOID ms7210_dvin_data_swap(UINT8 u8_swap_mode)
{
    switch (u8_swap_mode)
    {
    case 0:
        ms7210drv_dvin_data_swap_all();
        break;

    case 1:
        ms7210drv_dvin_data_swap_rb_channel();
        break;

    case 2:
        ms7210drv_dvin_data_swap_yc_channel();
        break;
    }
}

VOID ms7210_dvin_phase_adjust(BOOL b_invert, UINT8 u8_delay)
{
    ms7210drv_dvin_pa_adjust(b_invert, u8_delay);
}

BOOL ms7210_dvin_timing_get(DVIN_TIMING_DET_T *t_dvin_det)
{
    memset(t_dvin_det, 0, sizeof(DVIN_TIMING_DET_T));
    t_dvin_det->u16_pixclk = ms7210drv_misc_freqm_pclk_get();
    if (t_dvin_det->u16_pixclk > 500)
    {
        if (ms7210drv_hd_tx_pll_lock_status())
        {
            ms7210drv_dvin_timing_detect(t_dvin_det);
        }
        else
        {
            ms7210drv_hd_tx_phy_config(t_dvin_det->u16_pixclk);
        }
    }
    if ((t_dvin_det->u16_hactive < 50) || (t_dvin_det->u16_vtotal < 50) || (t_dvin_det->u16_htotal < t_dvin_det->u16_hactive))
    {
        return FALSE;
    }
    return TRUE;
}

VOID ms7210_dvin_timing_config(DVIN_CONFIG_T *t_dvin_config, VIDEOTIMING_T *ptTiming, HD_CONFIG_T *pt_hd_tx)
{
    ms7210drv_hd_tx_phy_set_clk_ratio((UINT8)ms7210drv_dvin_timing_config(t_dvin_config, ptTiming, &pt_hd_tx->u8_clk_rpt));
    pt_hd_tx->u16_video_clk = ptTiming->u16_pixclk;
}

VOID ms7210_dvin_video_config(BOOL b_config)
{
    ms7210drv_dvin_clk_reset_release(b_config);
}

BOOL ms7210_hdtx_hpd_detect(VOID)
{
    return ms7210drv_hd_tx_shell_hpd();
}

BOOL ms7210_hdtx_edid_get(UINT8 *u8_edid)
{
    BOOL b_succ = FALSE;
    HD_EDID_FLAG_T pt_edid;
    
    if (u8_edid != NULL)
    {
        b_succ = ms7210drv_hd_tx_parse_edid(u8_edid, &pt_edid);
    }

    return b_succ;
}

BOOL ms7210_hdtx_input_timing_stable_get(VOID)
{
    return ms7210drv_hd_tx_shell_timing_stable();
}

VOID ms7210_hdtx_output_config(HD_CONFIG_T *pt_hd_tx)
{
    ms7210drv_hd_tx_phy_output_enable(FALSE);
    ms7210drv_hd_tx_encrypt_enable(FALSE);
    ms7210drv_hd_tx_shell_set_gcp_packet_avmute(FALSE);

    ms7210drv_csc_config_output((HD_CS_E)pt_hd_tx->u8_color_space);

    ms7210drv_hd_tx_phy_config(pt_hd_tx->u16_video_clk);
    ms7210drv_hd_tx_shell_config(pt_hd_tx);

	ms7210drv_hd_tx_audio_config(pt_hd_tx);

    ms7210drv_hd_tx_shell_video_mute_enable(FALSE);
    ms7210drv_hd_tx_shell_audio_mute_enable(FALSE);
    ms7210drv_hd_tx_phy_output_enable(TRUE);
}

VOID ms7210_hdtx_shutdown_output(VOID)
{
    ms7210drv_hd_tx_phy_output_enable(FALSE);
    ms7210drv_hd_tx_phy_power_down();
    //
    ms7210drv_hd_tx_encrypt_enable(FALSE);
    //_hd_tx_encrypt_param_default(u8_output_chn);
}

