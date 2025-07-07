/**
******************************************************************************
* @file    ms7210_drv_hd_tx.c
* @author  
* @version V1.0.0
* @date    15-Nov-2016
* @brief   hd tx module driver source file
* @history 
*
* Copyright (c) 2009 - 2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#include "ms7210_comm.h"
#include "ms7210_mpi.h"
#include "ms7210_drv_hd_tx_config.h"
#include "ms7210_drv_hd_tx.h"

#if 1
#define TPI_EDID_PRINT(X)
#else
#define TPI_EDID_PRINT
#endif

#define HD_DDC_set_speed      mculib_i2c_set_speed
#define HD_DDC_ReadByte       mculib_i2c_read_8bidx8bval
#define HD_DDC_WriteByte      mculib_i2c_write_8bidx8bval
// #define HD_DDC_ReadBytes      mculib_i2c_burstread_8bidx8bval
#define HD_DDC_ReadBytes      mculib_ddc_i2c_readbytes

#ifdef _PLATFORM_WINDOWS_
//because mTools no these APIs
#define HD_DDC_WriteBlank(u8_address, u8_index) 
#define HD_DDC_ReadBytes_ext(u8_address, u8_index, u8_length)
#else
#define HD_DDC_WriteBlank(u8_address, u8_index) mculib_i2c_write_blank(u8_address, u8_index)
#define HD_DDC_ReadBytes_ext(u8_address, u8_index, u8_length) mculib_i2c_burstread_8bidx8bval_ext(u8_address, u8_index, u8_length)
#endif

#if 0
static VOID _drv_hd_tx_pll_set_2x_tmds_clk(UINT16 u16_input_clk);
#endif

#define HD_TX_DIGITAL_IN_ENABLE (1)


static UINT8 g_u8_hd_tx_channel = 0;

#define REG_ADDRESS_OFST        ((UINT16)g_u8_hd_tx_channel * MS7210_HD_TX_CHN_REG_ADDRESS_OFST)

#define REG_CSC_ADDRESS_OFST    ((UINT16)g_u8_hd_tx_channel * MS7210_MISC_CSC_CHN_REG_ADDRESS_OFST)


VOID ms7210drv_hd_tx_set_channel(UINT8 u8_chn)
{
    g_u8_hd_tx_channel = u8_chn % (HD_TX_CHN3 + 1);
}

UINT8 ms7210drv_hd_tx_get_channel(VOID)
{
    return g_u8_hd_tx_channel;
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_phy_power_enable
*  Description:     hd tx module power on/off
*  Entry:           [in]b_enable: if true turn on 
*                                 else turn off
*               
*  Returned value:  None
*  Remark:
***************************************************************/ 
VOID ms7210drv_hd_tx_phy_power_enable(BOOL b_enable)
{
    //PLL power control
    HAL_ToggleBits(MS7210_HD_TX_PLL_POWER_REG + REG_ADDRESS_OFST, MSRT_BITS2_0, !b_enable);

    //PHY power control
    HAL_ToggleBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT3 | MSRT_BIT1, b_enable);
    HAL_ToggleBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT5, !b_enable);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_phy_output_enable
*  Description:     hd tx module output timing on/off
*  Entry:           [in]b_enable: if true turn on 
*                                 else turn off
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_phy_output_enable(BOOL b_enable)
{
    //20190103, enable: data -> clk; disable: clk -> data
    if (b_enable)
    {
        //output data drive control
        HAL_SetBits(MS7210_HD_TX_PHY_DATA_DRV_REG + REG_ADDRESS_OFST, MSRT_BITS2_0);

        //output clk drive control
        HAL_SetBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT2);   
    }
    else
    {
        //output clk drive control
        HAL_ClrBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT2);   
    
        //output data drive control
        HAL_ClrBits(MS7210_HD_TX_PHY_DATA_DRV_REG + REG_ADDRESS_OFST, MSRT_BITS2_0);
    }
    
}

VOID ms7210drv_hd_tx_phy_output_auto_ctrl(UINT8 u8_ctrl_mode, BOOL b_auto)
{    
    UINT8 u8_reg = 0;

    u8_reg |= (b_auto ? 0x80 : 0x00);
    u8_reg |= ((u8_ctrl_mode == 0) ? 0x00 : 0x40);

    HAL_ModBits(MS7210_HD_TX_PHY_DATA_DRV_REG + REG_ADDRESS_OFST, MSRT_BITS7_6, u8_reg);
}


VOID ms7210drv_hd_tx_phy_data_R200_enable(BOOL b_enable)
{
    HAL_ToggleBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT6, b_enable);
}

VOID ms7210drv_hd_tx_phy_clk_drive_config(UINT8 u8_main_pre, UINT8 u8_main_po)
{
    u8_main_pre &= 0x07;
    u8_main_po &= 0x0F;

    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PREC_2_REG + REG_ADDRESS_OFST, MSRT_BITS7_4, u8_main_pre << 4); //reg0x925
    
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_POC_2_REG + REG_ADDRESS_OFST, MSRT_BITS7_4, u8_main_po << 4);   //reg0x927
}

VOID ms7210drv_hd_tx_phy_data_drive_config(UINT8 u8_main_pre, UINT8 u8_main_po, UINT8 u8_post_po)
{
    u8_main_pre &= 0x07;
    u8_main_po &= 0x0F;
    u8_post_po &= 0x0F;

    HAL_WriteByte(MS7210_HD_TX_PHY_MAIN_PRE0_1_REG + REG_ADDRESS_OFST, (u8_main_pre << 4) | u8_main_pre); //reg0x924
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PREC_2_REG + REG_ADDRESS_OFST, MSRT_BITS3_0, u8_main_pre);          //reg0x925
    
    HAL_WriteByte(MS7210_HD_TX_PHY_MAIN_PO0_1_REG + REG_ADDRESS_OFST, (u8_main_po << 4) | u8_main_po);    //reg0x926
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_POC_2_REG + REG_ADDRESS_OFST, MSRT_BITS3_0, u8_main_po);            //reg0x927

    HAL_WriteByte(MS7210_HD_TX_PHY_POST_PO0_1_REG + REG_ADDRESS_OFST, (u8_post_po << 4) | u8_post_po);    //reg0x928
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PO2_REG + REG_ADDRESS_OFST, MSRT_BITS3_0, u8_post_po);              //reg0x929
}

VOID ms7210drv_hd_tx_phy_data_drive_enhance(BOOL b_enable)
{
    //register bit for AS5332A only 
    HAL_ToggleBits(MS7210_HD_TX_PHY_POST_PRE_REG + REG_ADDRESS_OFST, MSRT_BIT4, b_enable);
}


/***************************************************************
*  Function name:   ms7210drv_hd_tx_phy_init
*  Description:     hd tx phy module init
*  Entry:           [in]u16_tmds_clk: tmds clk,uint:10000Hz
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_phy_init(UINT16 u16_tmds_clk)
{
    UINT8 u8_main_po;
    UINT8 u8_post_po;
    
    //tmds_clk > 200MHz
    u8_main_po = (u16_tmds_clk > 20000) ? 9 : 4;
    u8_post_po = (u16_tmds_clk > 20000) ? 9 : 7;
    
    //PLL init, 20180312, use Hardware auto mode.
    HAL_ClrBits(MS7210_HD_TX_PLL_CFG_SEL_REG + REG_ADDRESS_OFST, MSRT_BITS2_0);
    HAL_SetBits(MS7210_HD_TX_PLL_CTRL1_REG + REG_ADDRESS_OFST, MSRT_BIT0);
    HAL_SetBits(MS7210_HD_TX_PLL_CTRL4_REG + REG_ADDRESS_OFST, MSRT_BIT3);
    //misc clk and reset
    HAL_SetBits(MS7210_HD_TX_MISC_ACLK_SEL_REG, MSRT_BITS7_4);
    HAL_SetBits(MS7210_HD_TX_MISC_RST_CTRL1_REG, MSRT_BITS7_4);
    
    //clk drive
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PREC_2_REG + REG_ADDRESS_OFST, MSRT_BITS6_4, (4) << 4); //clk main pre
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_POC_2_REG + REG_ADDRESS_OFST,  MSRT_BITS7_4, (2) << 4); //clk main po

    //data0 drive
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PRE0_1_REG + REG_ADDRESS_OFST, MSRT_BITS2_0, (4) << 0); //data main pre
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PO0_1_REG + REG_ADDRESS_OFST,  MSRT_BITS3_0, (u8_main_po) << 0); //data main po,  9
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PRE_REG   + REG_ADDRESS_OFST,  MSRT_BIT0,    (1) << 0); //data post pre
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PO0_1_REG + REG_ADDRESS_OFST,  MSRT_BITS3_0, (u8_post_po) << 0); //data post po, 9

    //data1 drive
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PRE0_1_REG + REG_ADDRESS_OFST, MSRT_BITS6_4, (4) << 4); //data main pre
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PO0_1_REG + REG_ADDRESS_OFST,  MSRT_BITS7_4, (u8_main_po) << 4); //data main po
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PRE_REG   + REG_ADDRESS_OFST,  MSRT_BIT1,    (1) << 1); //data post pre
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PO0_1_REG + REG_ADDRESS_OFST,  MSRT_BITS7_4, (u8_post_po) << 4); //data post po

    //data2 drive
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_PREC_2_REG + REG_ADDRESS_OFST, MSRT_BITS2_0, (4) << 0); //data main pre
    HAL_ModBits(MS7210_HD_TX_PHY_MAIN_POC_2_REG + REG_ADDRESS_OFST,  MSRT_BITS3_0, (u8_main_po) << 0); //data main po
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PRE_REG   + REG_ADDRESS_OFST,  MSRT_BIT2,    (1) << 2); //data post pre
    HAL_ModBits(MS7210_HD_TX_PHY_POST_PO2_REG   + REG_ADDRESS_OFST,  MSRT_BITS3_0, (u8_post_po) << 0); //data post po

    HAL_WriteByte(MS7210_HD_TX_PLL_POWER_REG + REG_ADDRESS_OFST, 0x20);
    if ((u16_tmds_clk / 100) > 100)
    {
        HAL_SetBits(MS7210_HD_TX_PLL_CTRL6_REG + REG_ADDRESS_OFST, MSRT_BIT2);
        HAL_SetBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT6);
        HAL_WriteByte(MS7210_HD_TX_PHY_MAIN_PO0_1_REG + REG_ADDRESS_OFST, 0xdd);
        HAL_WriteByte(MS7210_HD_TX_PHY_MAIN_POC_2_REG + REG_ADDRESS_OFST, 0x0d);
        HAL_WriteByte(MS7210_HD_TX_PHY_POST_PO0_1_REG + REG_ADDRESS_OFST, 0x88);
        HAL_WriteByte(MS7210_HD_TX_PHY_POST_PO2_REG + REG_ADDRESS_OFST, 0x08);
    }
    else
    {
        HAL_ClrBits(MS7210_HD_TX_PLL_CTRL6_REG + REG_ADDRESS_OFST, MSRT_BIT2);
        HAL_ClrBits(MS7210_HD_TX_PHY_POWER_REG + REG_ADDRESS_OFST, MSRT_BIT6);
        HAL_WriteByte(MS7210_HD_TX_PHY_MAIN_PO0_1_REG + REG_ADDRESS_OFST, 0x11);
        HAL_WriteByte(MS7210_HD_TX_PHY_MAIN_POC_2_REG + REG_ADDRESS_OFST, 0x01);
        HAL_WriteByte(MS7210_HD_TX_PHY_POST_PO0_1_REG + REG_ADDRESS_OFST, 0x11);
        HAL_WriteByte(MS7210_HD_TX_PHY_POST_PO2_REG + REG_ADDRESS_OFST, 0x01);
    }

    //HBR audio mode clk config, spdif div clk, expected value�� The bigger the better.But do not greater than 350MHz
    HAL_ModBits(MS7210_HD_TX_PLL_CTRL5_REG   + REG_ADDRESS_OFST,  MSRT_BITS1_0, 0x2);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_phy_set_clk
*  Description:     hd tx phy module set video clk
*  Entry:           [in]u16_clk: uint 10000Hz 
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_phy_set_clk(UINT16 u16_clk)
{
    _UNUSED_(u16_clk);

    HAL_WriteByte(MS7210_HD_TX_PLL_TRIG_REG + REG_ADDRESS_OFST, MSRT_BIT0);
    //delay > 100us for PLLV clk stable
    Delay_ms(10);
}

VOID ms7210drv_hd_tx_phy_set_clk_ratio(UINT8 u8_ratio)
{
    switch (u8_ratio)
    {
    case 0:
        HAL_ClrBits(MS7210_HD_TX_PLL_CTRL11_REG, MSRT_BIT6 | MSRT_BIT2);
        break;
    case 1:   // x2 clk
        HAL_ModBits(MS7210_HD_TX_PLL_CTRL11_REG, MSRT_BIT6 | MSRT_BIT2, MSRT_BIT2);
        break;
    case 2:   // 1/2 clk
        HAL_ModBits(MS7210_HD_TX_PLL_CTRL11_REG, MSRT_BIT6 | MSRT_BIT2, MSRT_BIT6);
        break;
    }
}

BOOL ms7210drv_hd_tx_pll_lock_status(VOID)
{
    return (HAL_ReadByte(MS7210_HD_TX_PLL_CAL_RESULT1) & MSRT_BIT7) >> 7;
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_video_mute_enable
*  Description:     hd tx shell module video mute
*  Entry:           [in]b_enable, if true mute screen else normal video
*
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_video_mute_enable(BOOL b_enable)
{
    HAL_ToggleBits(MS7210_HD_TX_SHELL_AVMUTE_REG + REG_ADDRESS_OFST, MSRT_BIT1, b_enable);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_audio_mute_enable
*  Description:     hd tx shell module audio mute
*  Entry:           [in]b_enable, if true mute audio else normal audio
*
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_audio_mute_enable(BOOL b_enable)
{
    HAL_ToggleBits(MS7210_HD_TX_SHELL_AVMUTE_REG + REG_ADDRESS_OFST, MSRT_BIT2, b_enable);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_hd_out
*  Description:     hd tx shell module set hd out
*  Entry:           [in]b_hd_out: FALSE = dvi out;  TRUE = hd out��
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_hd_out(BOOL b_hd_out)
{
    HAL_ToggleBits(MS7210_HD_TX_SHELL_DVI_REG + REG_ADDRESS_OFST, MSRT_BIT0, !b_hd_out);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_clk_repeat
*  Description:     hd tx shell module set out clk repeat times
*  Entry:           [in]u8_times: enum refer to HD_CLK_RPT_E
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_clk_repeat(UINT8 u8_times)
{
    if (u8_times > HD_X10CLK) u8_times = HD_X1CLK;

    HAL_ModBits(MS7210_HD_TX_SHELL_MODE_REG + REG_ADDRESS_OFST, MSRT_BITS5_4, u8_times << 4);

    //if use deep color old method(reg0x505[0] = 0), same as CS5288A, need to config below register
    //tx video write enable divider
    //HAL_WriteByte(MS7210_HD_TX_MISC_AV_CTRL2_REG, u8_times);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_color_space
*  Description:     hd tx shell module video color space
*  Entry:           [in]u8_cs: enum refer to HD_CS_E
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_color_space(UINT8 u8_cs)
{
    HAL_ModBits(MS7210_HD_TX_SHELL_MODE_REG + REG_ADDRESS_OFST, MSRT_BITS7_6, u8_cs << 6);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_color_depth
*  Description:     hd tx shell module video color depth. 10bits, 12bits, 16bits no support in ms7210
*  Entry:           [in]u8_depth: enum refer to HD_COLOR_DEPTH_E
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_color_depth(UINT8 u8_depth)
{
    HAL_ToggleBits(MS7210_HD_TX_SHELL_MODE_REG + REG_ADDRESS_OFST, MSRT_BIT2, u8_depth > 0);
    HAL_ModBits(MS7210_HD_TX_SHELL_MODE_REG + REG_ADDRESS_OFST, MSRT_BITS1_0, u8_depth);
}

VOID ms7210drv_hd_tx_shell_set_audio_cbyte_from_channel(UINT8 u8_tx_chn)
{
    //status mux, don't need delay to read cbyte status.
    HAL_ModBits(MS7210_HD_TX_MISC_AUD_CTRL, MSRT_BITS5_4, u8_tx_chn << 4);
}

UINT8 ms7210drv_hd_tx_shell_get_audio_cbyte_status(VOID)
{
    return (HAL_ReadByte(MS7210_HD_TX_MISC_C_BYTE0) & MSRT_BIT1) ? TRUE : FALSE;
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_audio_mode
*  Description:     hd tx shell module set audio mode. HBR, DSD mode no support in ms7210
*  Entry:           [in]u8_audio_mode: enum refer to HD_AUDIO_MODE_E
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_audio_mode(UINT8 u8_audio_mode)
{
    switch (u8_audio_mode)
    {
    case HD_AUD_MODE_AUDIO_SAMPLE:
        //HAL_ClrBits(MS7210_HD_TX_MISC_ACLK_SEL_REG, MSRT_BITS1_0);                  // I2S clk
        HAL_ClrBits(MS7210_HD_TX_AUDIO_CFG_REG + REG_ADDRESS_OFST, MSRT_BIT0);      // I2S Audio data selection.
        //HAL_ClrBits(MS7210_HD_TX_MISC_AV_CTRL_REG, MSRT_BITS5_4);  //tx_audio_sel[1:0]
        break;
    case HD_AUD_MODE_HBR:
        HAL_SetBits(MS7210_HD_TX_MISC_RST_CTRL1_REG, MSRT_BIT1); //spdif_cdr_sw_rstb
        //HAL_SetBits(MS7210_HD_TX_MISC_ACLK_SEL_REG, MSRT_BITS1_0);                  // SPDIF clk
        HAL_SetBits(MS7210_HD_TX_AUDIO_CFG_REG + REG_ADDRESS_OFST, MSRT_BIT0);      // SPDIF Audio data selection.  
        //HAL_ClrBits(MS7210_HD_TX_MISC_AV_CTRL_REG, MSRT_BITS5_4);
        break;
    case HD_AUD_MODE_DSD:
        //20191225, mistake for 
        HAL_SetBits(MS7210_HD_TX_MISC_RST_CTRL1_REG, MSRT_BIT0); //dsd_sw_rstb
        //HAL_ModBits(MS7210_HD_TX_MISC_ACLK_SEL_REG, MSRT_BITS1_0, MSRT_BIT0);       // dsd clk
        HAL_ClrBits(MS7210_HD_TX_AUDIO_CFG_REG + REG_ADDRESS_OFST, MSRT_BIT0);      // I2S Audio data selection.
        //HAL_ModBits(MS7210_HD_TX_MISC_AV_CTRL_REG, MSRT_BITS5_4, MSRT_BIT4);
        break;
    case HD_AUD_MODE_DST:
        //HAL_ModBits(MS7210_HD_TX_MISC_ACLK_SEL_REG, MSRT_BITS1_0, MSRT_BIT0);       // dsd clk
        HAL_SetBits(MS7210_HD_TX_AUDIO_CFG_REG + REG_ADDRESS_OFST, MSRT_BIT0);      // SPDIF Audio data selection.  
        //HAL_ModBits(MS7210_HD_TX_MISC_AV_CTRL_REG, MSRT_BITS5_4, MSRT_BIT5);
        break;

    default:
        break;
    }
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_audio_rate
*  Description:     hd tx shell module set audio I2S rate
*  Entry:           [in]u8_audio_rate: enum refer to HD_AUDIO_RATE_E
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_audio_rate(UINT8 u8_audio_rate)
{
    switch (u8_audio_rate)
    {       
    //case HD_AUD_RATE_44K1:
    //    HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT5);
    //    break;
        
    case HD_AUD_RATE_96K:
        HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT1);
        break;
        
    case HD_AUD_RATE_32K:
        HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT6);
        break;
        
    case HD_AUD_RATE_88K2:
        HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT4);
        break;
        
    case HD_AUD_RATE_176K4: 
        HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT3);
        break;
        
    case HD_AUD_RATE_192K:
        HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT0);
        break;        

    //20190704, 44.1K, N value 7007 can't use. fixed same to 48K N value.
    case HD_AUD_RATE_44K1:
    case HD_AUD_RATE_48K:
    default:
        HAL_ModBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BITS6_0, MSRT_BIT2);
        break; 
    }
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_audio_bits
*  Description:     hd tx shell module set audio bit length
*  Entry:           [in]u8_audio_bits: enum refer to HD_AUDIO_LENGTH_E
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_audio_bits(UINT8 u8_audio_bits)
{
    #if MS7210_AUDIO_SAMPLE_PACKET_192BIT_BYPASS_ENABLE
    HAL_ModBits(MS7210_HD_TX_AUDIO_CFG_I2S_REG + REG_ADDRESS_OFST, MSRT_BITS5_4, HD_AUD_LENGTH_24BITS ? 0x00 : MSRT_BIT4);
    #else
    HAL_ModBits(MS7210_HD_TX_AUDIO_CFG_I2S_REG + REG_ADDRESS_OFST, MSRT_BITS5_4, u8_audio_bits ? 0x00 : MSRT_BIT4);
    #endif
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_audio_channels
*  Description:     hd tx shell module set audio channels
*  Entry:           [in]u8_audio_channels: enum refer to HD_AUDIO_CHN_E
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_audio_channels(UINT8 u8_audio_channels)
{
    if (u8_audio_channels <= HD_AUD_2CH)
    {
        u8_audio_channels = 0x01;
    }
    else if (u8_audio_channels <= HD_AUD_4CH)
    {
        u8_audio_channels = 0x03;
    }
    else if (u8_audio_channels <= HD_AUD_6CH)
    {
        u8_audio_channels = 0x07;
    }
    else
    {
        u8_audio_channels = 0x0f;
    }
    
    HAL_ModBits(MS7210_HD_TX_AUDIO_CH_EN_REG + REG_ADDRESS_OFST, MSRT_BITS3_0, u8_audio_channels);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_set_audio_lrck
*  Description:     hd tx shell module set audio lrck
*  Entry:           [in]b_left: if true left channel in, else right channel in
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_set_audio_lrck(BOOL b_left)
{
    HAL_ModBits(MS7210_HD_TX_AUDIO_CFG_I2S_REG + REG_ADDRESS_OFST, MSRT_BITS1_0, b_left ? MSRT_BIT1 : 0x00);
}

static UINT8 arrInfoFrameBuffer[0x20];

VOID ms7210drv_hd_tx_shell_set_video_infoframe(HD_CONFIG_T *pt_hd_tx)
{
    UINT8 i;
    UINT8 u8tmp = 0;

    HAL_ClrBits(MS7210_HD_TX_SHELL_CTRL_REG + REG_ADDRESS_OFST, MSRT_BIT6); // "AVI packet is enabled, active is high"
    memset(arrInfoFrameBuffer, 0, 0x0E);

    u8tmp = pt_hd_tx->u8_color_space;
    arrInfoFrameBuffer[1] = (u8tmp << 5) & MSRT_BITS6_5; 

    arrInfoFrameBuffer[1] |= 0x10;     //Active Format Information Present, Active
    
    // 2013-10-18, Change the SCAN INFO @ CKL's request. 
    // it should Overscan in a CE format and underscan in a IT format.
    arrInfoFrameBuffer[1] |= (pt_hd_tx->u8_scan_info & 0x03);

    if (pt_hd_tx->u8_colorimetry == HD_COLORIMETRY_601)
    {
        arrInfoFrameBuffer[2] = MSRT_BIT6;
    }
    else if ((pt_hd_tx->u8_colorimetry == HD_COLORIMETRY_709) || (pt_hd_tx->u8_colorimetry == HD_COLORIMETRY_1120))
    {
        arrInfoFrameBuffer[2] = MSRT_BIT7;
    }
    else if (pt_hd_tx->u8_colorimetry == HD_COLORIMETRY_XVYCC601)
    {
        arrInfoFrameBuffer[2] = MSRT_BITS7_6;
        arrInfoFrameBuffer[3] &= ~MSRT_BITS6_4;
    }
    else if (pt_hd_tx->u8_colorimetry == HD_COLORIMETRY_XVYCC709)
    {
        arrInfoFrameBuffer[2] = MSRT_BITS7_6;
        arrInfoFrameBuffer[3] = MSRT_BIT4;
    }
    else
    {
        arrInfoFrameBuffer[2] &= ~MSRT_BITS7_6;
        arrInfoFrameBuffer[3] &= ~MSRT_BITS6_4; 
    }

    //others set to 0
    if (pt_hd_tx->u8_aspect_ratio == HD_4X3)
    {
        arrInfoFrameBuffer[2] |= 0x10;
    }
    else if (pt_hd_tx->u8_aspect_ratio == HD_16X9)
    {
        arrInfoFrameBuffer[2] |= 0x20;
    }

    //R3...R0, default to 0x08
    arrInfoFrameBuffer[2] |= 0x08; // Same as coded frame aspect ratio.

    // According to HD1.3 spec. VESA mode should be set to 0
    if (pt_hd_tx->u8_vic >= 64) //VESA
    {
        arrInfoFrameBuffer[4] = 0;    // 0 for vesa modes.
    }
    else
    {
        arrInfoFrameBuffer[4] = pt_hd_tx->u8_vic;
    }
    arrInfoFrameBuffer[5] = pt_hd_tx->u8_clk_rpt;

    //Calculate the check-sum
    arrInfoFrameBuffer[0] = 0x82 + 0x02 + 0x0D;    
    for (i = 1; i < 0x0E; i ++)
    {
        arrInfoFrameBuffer[0] += arrInfoFrameBuffer[i];
    }
    arrInfoFrameBuffer[0] = 0x100 - arrInfoFrameBuffer[0];
    // Write data into hd shell.
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_TYPE_REG + REG_ADDRESS_OFST, 0x82);
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_VER_REG + REG_ADDRESS_OFST, 0x02);
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_LEN_REG + REG_ADDRESS_OFST, 0x0D);

    for (i = 0; i < 0x0E; i ++)
    {
        HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_PACK_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[i]);
    }
    HAL_SetBits(MS7210_HD_TX_SHELL_CTRL_REG + REG_ADDRESS_OFST, MSRT_BIT6);
}

VOID ms7210drv_hd_tx_shell_set_audio_infoframe(HD_CONFIG_T *pt_hd_tx)
{
    UINT8 i;
    // 
    HAL_ClrBits(MS7210_HD_TX_SHELL_CTRL_REG + REG_ADDRESS_OFST, MSRT_BIT5);
    
    memset(arrInfoFrameBuffer, 0, 0x0E);
    arrInfoFrameBuffer[0] = 0x84;
    arrInfoFrameBuffer[1] = 0x01;
    arrInfoFrameBuffer[2] = 0x0A;
    arrInfoFrameBuffer[3] = 0x84 + 0x01 + 0x0A;
    // 20140917 fixed bug for audio inforframe channel config
    arrInfoFrameBuffer[4] = pt_hd_tx->u8_audio_channels;
    arrInfoFrameBuffer[5] = 0;
    arrInfoFrameBuffer[7] = pt_hd_tx->u8_audio_speaker_locations;
    // Calculate the checksum
    for (i = 4; i < 0x0E; i ++)
    {
        arrInfoFrameBuffer[3] += arrInfoFrameBuffer[i];
    }
    arrInfoFrameBuffer[3] = 0x100 - arrInfoFrameBuffer[3];
    // Write packet info.
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_TYPE_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[0]);
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_VER_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[1]);
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_LEN_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[2]);
    
    for (i = 3; i < 0x0E; i ++)
    {
        HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_PACK_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[i]);
    }
    // open the 'cfg_audio_en_o", tell the hw infoframe is ready.
    HAL_SetBits(MS7210_HD_TX_SHELL_CTRL_REG + REG_ADDRESS_OFST, MSRT_BIT5);
}

VOID ms7210drv_hd_tx_shell_set_vendor_specific_infoframe(HD_CONFIG_T *pt_hd_tx)
{
    UINT8 i;
    // 
    HAL_ClrBits(MS7210_HD_TX_SHELL_CFG_REG + REG_ADDRESS_OFST, MSRT_BIT0);
    
    memset(arrInfoFrameBuffer, 0, 0x20);
    arrInfoFrameBuffer[0] = 0x81;
    arrInfoFrameBuffer[1] = 0x01;
    arrInfoFrameBuffer[2] = 0x1B;
    arrInfoFrameBuffer[3] = 0x81 + 0x01 + 0x1B;
    //24bit IEEE
    arrInfoFrameBuffer[4] = 0x03;
    arrInfoFrameBuffer[5] = 0x0C;
    arrInfoFrameBuffer[6] = 0x00;
    //HD_VIDEO_FORMAT
    arrInfoFrameBuffer[7] = pt_hd_tx->u8_video_format << 5;
    //HD_VIC
    arrInfoFrameBuffer[8] = pt_hd_tx->u8_4Kx2K_vic;
    
    // Calculate the checksum
    for (i = 4; i < 0x1F; i ++)
    {
        arrInfoFrameBuffer[3] += arrInfoFrameBuffer[i];
    }
    arrInfoFrameBuffer[3] = 0x100 - arrInfoFrameBuffer[3];
    
    // Write packet info.
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_TYPE_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[0]);
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_VER_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[1]);
    HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_LEN_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[2]);
    
    for (i = 3; i < 0x1F; i ++)
    {
        HAL_WriteByte(MS7210_HD_TX_SHELL_INFO_PACK_REG + REG_ADDRESS_OFST, arrInfoFrameBuffer[i]);
    }
    //
    HAL_SetBits(MS7210_HD_TX_SHELL_CFG_REG + REG_ADDRESS_OFST, MSRT_BIT0);
}

VOID ms7210drv_hd_tx_shell_set_gcp_packet_avmute(BOOL b_mute)
{
    HAL_ToggleBits(MS7210_HD_TX_SHELL_CTRL_REG + REG_ADDRESS_OFST, MSRT_BIT7, b_mute);
}


#define MASK_HD_VIDEO_CLK (0x01 << g_u8_hd_tx_channel)
#define MASK_HD_AUDIO_CLK (0x10 << g_u8_hd_tx_channel)

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_init
*  Description:     hd tx shell module init
*  Entry:           [in]None 
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_init(VOID)
{
    //20180328, resolve SPDIF format not correct issue.
    HAL_SetBits(MS7210_HD_TX_SHELL_VOLUME_CFG0_REG + REG_ADDRESS_OFST, MSRT_BIT0);

    #if (!HD_TX_DIGITAL_IN_ENABLE)
    //hd shell use new design
    HAL_SetBits(MS7210_HD_TX_SHELL_DC_DBG + REG_ADDRESS_OFST, MSRT_BIT0);
    #endif
    
    //video and audio clk enable, 
    //20170412, add video clk for mistable
    HAL_SetBits(MS7210_HD_TX_MISC_HD_CLK_REG, MASK_HD_VIDEO_CLK | MASK_HD_AUDIO_CLK);
    
    //audio
    #if HD_TX_DIGITAL_IN_ENABLE
    HAL_ClrBits(MS7210_HD_TX_MISC_AUD_CTRL, MSRT_BITS3_0);
    #endif
    //HAL_ClrBits(MS7210_HD_TX_MISC_HD_CTS_CLK_REG, MSRT_BITS2_1);  // CTS_SEL, register default
    //ms7210drv_hd_tx_shell_set_audio_lrck(TRUE); //register default

    //video and audio release reset
    //HAL_SetBits(MS7210_HD_TX_MISC_HD_RST_REG, MASK_HD_VIDEO_CLK | MASK_HD_AUDIO_CLK);

    //20140918 set audio enable
    HAL_SetBits(MS7210_HD_TX_AUDIO_RATE_REG + REG_ADDRESS_OFST, MSRT_BIT7);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_reset_enable
*  Description:     hd tx shell module reset
*  Entry:           [in]None 
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_shell_reset_enable(BOOL b_en)
{
    //video and audio reset
    HAL_ToggleBits(MS7210_HD_TX_MISC_HD_RST_REG, MASK_HD_VIDEO_CLK | MASK_HD_AUDIO_CLK, !b_en);
}

//#define ms7210drv_hd_tx_clk_sel(u8_sel)   HAL_ModBits(MS7210_MISC_CLK_CTRL4, MSRT_BITS3_2, u8_sel << 2);
VOID ms7210drv_hd_tx_clk_sel(UINT8 u8_sel)
{
    switch (u8_sel)
    {
    case 0: // clk from rx
        HAL_ModBits(MS7210_MISC_CLK_CTRL4, MSRT_BITS3_2, (0x01) << 2);
        break;
    case 1: // clk form dvin
        HAL_ModBits(MS7210_MISC_CLK_CTRL4, MSRT_BITS3_2 | MSRT_BIT0, ((0x02) << 2) | 0x01);
        HAL_ModBits(MS7210_HD_TX_PLL_CTRL1_REG + REG_ADDRESS_OFST, MSRT_BITS4_3, (0x00) << 3);
        break;
    }
}

VOID ms7210drv_hd_tx_audio_fs_update(UINT8 u8_audio_rate)
{   
    ms7210drv_hd_tx_shell_set_audio_rate(u8_audio_rate);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_audio_config
*  Description:     hd tx audio config
*  Entry:           [in]pt_hd_tx: struct refer to HD_CONFIG_T
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_audio_config(HD_CONFIG_T *pt_hd_tx)
{
    //audio reset
    HAL_SetBits(MS7210_HD_TX_SHELL_SWRST_REG + REG_ADDRESS_OFST, MSRT_BITS2_1);
    
    //
    //ms7210drv_hd_tx_shell_set_audio_mode(pt_hd_tx->u8_audio_mode);
    ms7210drv_hd_tx_shell_set_audio_rate(pt_hd_tx->u8_audio_rate);
    ms7210drv_hd_tx_shell_set_audio_bits(pt_hd_tx->u8_audio_bits);
    ms7210drv_hd_tx_shell_set_audio_channels(pt_hd_tx->u8_audio_channels);

    //20190819, infoframe update before audio sample packet be send
    ms7210drv_hd_tx_shell_set_audio_infoframe(pt_hd_tx);
    
    //audio release reset, reset audio fifo r/w, hd clk domain
    HAL_ClrBits(MS7210_HD_TX_SHELL_SWRST_REG + REG_ADDRESS_OFST, MSRT_BIT2);
    Delay_us(100);
    HAL_ClrBits(MS7210_HD_TX_SHELL_SWRST_REG + REG_ADDRESS_OFST, MSRT_BIT1);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_config
*  Description:     hd tx config
*  Entry:           [in]pt_hd_tx: struct refer to HD_CONFIG_T
*                      
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_phy_config(UINT16 u16_video_clk)
{
    //disable output
    //ms7210drv_hd_tx_phy_output_enable(FALSE);
    //sel dvin clk
#if HD_TX_DIGITAL_IN_ENABLE
    ms7210drv_hd_tx_clk_sel(0x01);
#endif
    
    //PHY
    ms7210drv_hd_tx_phy_init(u16_video_clk);
    ms7210drv_hd_tx_phy_power_enable(TRUE); //
    //delay > 100us for PLLV power stable
    Delay_ms(10);
    ms7210drv_hd_tx_phy_set_clk(u16_video_clk);
}

VOID ms7210drv_hd_tx_shell_config(HD_CONFIG_T *pt_hd_tx)
{
    //SHELL
    ms7210drv_hd_tx_shell_reset_enable(TRUE);
    ms7210drv_hd_tx_shell_init();
    ms7210drv_hd_tx_shell_set_hd_out(pt_hd_tx->u8_hd_flag);
#if HD_TX_DIGITAL_IN_ENABLE //20180606, HD spliter project do not need to set this APIs.
    ms7210drv_hd_tx_shell_set_clk_repeat(pt_hd_tx->u8_clk_rpt);
#endif

    //20190702, if input is YUV422 and deep color mode, color space must set to RGB
    if (pt_hd_tx->u8_color_space == HD_YCBCR422 && pt_hd_tx->u8_color_depth != HD_COLOR_DEPTH_8BIT)
    {
        ms7210drv_hd_tx_shell_set_color_space(HD_RGB);
    }
    else
    {
        ms7210drv_hd_tx_shell_set_color_space(pt_hd_tx->u8_color_space);
    }

    ms7210drv_hd_tx_shell_set_color_depth(pt_hd_tx->u8_color_depth);
    //
    //ms7210drv_hd_tx_shell_set_audio_mode(pt_hd_tx->u8_audio_mode);
    ms7210drv_hd_tx_shell_set_audio_rate(pt_hd_tx->u8_audio_rate);
    ms7210drv_hd_tx_shell_set_audio_bits(pt_hd_tx->u8_audio_bits);
    ms7210drv_hd_tx_shell_set_audio_channels(pt_hd_tx->u8_audio_channels);
    //
    ms7210drv_hd_tx_shell_reset_enable(FALSE);
    //
    ms7210drv_hd_tx_shell_set_video_infoframe(pt_hd_tx);
    ms7210drv_hd_tx_shell_set_audio_infoframe(pt_hd_tx);
    if (pt_hd_tx->u8_video_format)
    {
        ms7210drv_hd_tx_shell_set_vendor_specific_infoframe(pt_hd_tx);
    }
    
    //enable output
    //ms7210drv_hd_tx_shell_video_mute_enable(FALSE);
    //ms7210drv_hd_tx_shell_audio_mute_enable(FALSE);
    //ms7210drv_hd_tx_phy_output_enable(TRUE);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_phy_power_down
*  Description:     hd tx module power down
*  Entry:           [in]None
*               
*  Returned value:  None
*  Remark:
***************************************************************/ 
VOID ms7210drv_hd_tx_phy_power_down(VOID)
{
    ms7210drv_hd_tx_phy_power_enable(FALSE);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_shell_hpd
*  Description:     hd tx hot plug detection
*  Entry:           [in]
*               
*  Returned value:  if hd cable plug in return true, else return false
*  Remark:
***************************************************************/ 
BOOL ms7210drv_hd_tx_shell_hpd(VOID)
{
    #if (1)
    return (HAL_ReadByte(MS7210_HD_TX_SHELL_STATUS_REG + REG_ADDRESS_OFST) & MSRT_BIT0) ? TRUE : FALSE;
    #else
    
    UINT16 u16_addr;
    UINT8 u8_chn = g_u8_hd_tx_channel;

    if (u8_chn == 1)
    {
        u8_chn = 3;
    }
    else if (u8_chn == 3)
    {
        u8_chn = 1;
    }
    u16_addr = (UINT16)u8_chn * MS7210_HD_TX_CHN_REG_ADDRESS_OFST;
    
    return (HAL_ReadByte(MS7210_HD_TX_SHELL_STATUS_REG + u16_addr) & MSRT_BIT0);
    #endif
}

BOOL ms7210drv_hd_tx_shell_timing_stable(VOID)
{
    return (HAL_ReadByte(MS7210_HD_TX_SHELL_CTS_STABLE_REG + REG_ADDRESS_OFST) & MSRT_BIT4);
}


#if (1)
#define MASK_HD_DDC_DISABLE (0x01 << g_u8_hd_tx_channel)
#else

UINT8 _bug_fixed_MASK_HD_DDC_DISABLE(VOID)
{
    UINT8 u8_chn = g_u8_hd_tx_channel;

    if (u8_chn == 1)
    {
        u8_chn = 3;
    }
    else if (u8_chn == 3)
    {
        u8_chn = 1;
    }

    return (0x01 << u8_chn);
}

#define MASK_HD_DDC_DISABLE _bug_fixed_MASK_HD_DDC_DISABLE()

#endif

BOOL ms7210drv_hd_tx_ddc_is_busy(VOID)
{
    UINT8 u8_tx_chn;
    UINT8 u8_val;
    UINT8 u8_mask = 0x3;
    
    u8_val = HAL_ReadByte(MS7210_HD_TX_MISC_DDC_DEBUG_REG) & 0x0f;

    //check whether is AS5330A.
    //if is AS5330A, force return FALSE.
    if (u8_val != 0x09)
    {
        return FALSE;
    }
    
    u8_tx_chn = g_u8_hd_tx_channel;
    
    u8_val = HAL_ReadByte(MS7210_HD_TX_MISC_DDC_RO_REG);

    u8_mask <<= (u8_tx_chn * 2);

    return ((u8_val & u8_mask) != u8_mask);
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_ddc_enable
*  Description:     hd tx module DDC enable
*  Entry:           [in]b_enable, if true enable ddc, else disable
*               
*  Returned value:  None
*  Remark:
***************************************************************/ 
VOID ms7210drv_hd_tx_ddc_enable(BOOL b_enable)
{
    if (b_enable)
    {
        HD_DDC_set_speed(0); // Default to 20 kbit/s
        // Prepare the GPIO
        HAL_WriteByte(MS7210_HD_TX_MISC_DDC_CTRL_REG, 0xFF);   
        HAL_WriteByte(MS7210_HD_TX_MISC_DDC_ENABLE_REG, (~MASK_HD_DDC_DISABLE) & 0x0F);
    }
    else
    {
        //20190108, do not use setbits, because maybe read error data for hd tx sink.
        HAL_WriteByte(MS7210_HD_TX_MISC_DDC_ENABLE_REG, 0x0F);
        //20190108, optimize, default set to 0xff.
        //HAL_WriteByte(MS7210_HD_TX_MISC_DDC_CTRL_REG, 0x11);
        HD_DDC_set_speed(1); // Default to 100 kbit/s
    }
}

//
#if MS7210_HD_TX_ENCRYPT

#define ENCRYPT_RX_SLAVE_ADDR      0x74
#define REG_ENCRYPT_RX_BKSV0       0x00
#define REG_ENCRYPT_RX_AKSV0       0x10
#define REG_ENCRYPT_RX_AN0         0x18
#define REG_ENCRYPT_RX_RI0         0x08
#define REG_ENCRYPT_RX_RI1         0x09

#define AKSV_SIZE               5
#define NUM_OF_ONES_IN_KSV      20
#define ENCRYPT_BYTE_SIZE          0x08
#define TX_KEY_LEN              280

BOOL ms7210drv_hd_tx_encrypt_get_bksv_from_rx(UINT8 *p_data)
{
    UINT8 NumOfOnes = 0;
    UINT8 i, j;
    UINT8 u8_temp;
    
    ms7210drv_hd_tx_ddc_enable(TRUE);
    HD_DDC_ReadBytes(ENCRYPT_RX_SLAVE_ADDR, REG_ENCRYPT_RX_BKSV0, AKSV_SIZE, p_data);
    
    for (i = 0; i < AKSV_SIZE; i ++)
    {
        u8_temp = p_data[i];
        //MS7210_LOG1("bksv = ", u8_temp);
        for (j = 0; j < ENCRYPT_BYTE_SIZE; j++)
        {
            if (u8_temp & 0x01)
            {
                NumOfOnes++;
            }
            u8_temp >>= 1;
        }
    }
    ms7210drv_hd_tx_ddc_enable(FALSE);
    
    if (NumOfOnes != NUM_OF_ONES_IN_KSV)
    {
        return FALSE;
    }

    //MS7210_LOG("bksv ok.");
    
    return TRUE;
}

VOID ms7210drv_hd_tx_encrypt_set_bksv_to_tx(UINT8 *p_data)
{
    UINT8 i;
    
    for (i = 0; i < AKSV_SIZE; i ++)
    {
        HAL_WriteByte(MS7210_HD_TX_ENCRYPT_BKSV0_REG + REG_ADDRESS_OFST + i, p_data[i]);
    }
    
    //HPD is OK, TX/RX is connected
    //HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG + REG_ADDRESS_OFST, 0x02);
    HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CFG1_REG + REG_ADDRESS_OFST, 0x03);
    //HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG+ REG_ADDRESS_OFST, 0x42);
    HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG + REG_ADDRESS_OFST, 0x4a);   
}

//AN is random number
// static UINT8 __CODE ENCRYPT_AN_DATA[] =
static UINT8 ENCRYPT_AN_DATA[] =
{
    0x4d,
    0xdb,
    0xd1,
    0x91,
    0xc9,
    0x0d,
    0x0e,
    0xe7,
};

VOID ms7210drv_hd_tx_encrypt_set_an(VOID)
{
    UINT8 i;

    for (i = 0; i < 8; i ++)
    {
        HAL_WriteByte(MS7210_HD_TX_ENCRYPT_AN0_REG + REG_ADDRESS_OFST + i, ENCRYPT_AN_DATA[i]);
    }

    ms7210drv_hd_tx_ddc_enable(TRUE);
    for (i = 0; i < 8; i ++)
    {
        HD_DDC_WriteByte(ENCRYPT_RX_SLAVE_ADDR, REG_ENCRYPT_RX_AN0 + i, ENCRYPT_AN_DATA[i]);
    }
    ms7210drv_hd_tx_ddc_enable(FALSE);
}

VOID ms7210drv_hd_tx_encrypt_set_aksv_to_rx(UINT8 *p_u8_ksv)
{
    UINT8 i;
    
    ms7210drv_hd_tx_ddc_enable(TRUE);
    for (i = 0; i < AKSV_SIZE; i ++)
    {
        HD_DDC_WriteByte(ENCRYPT_RX_SLAVE_ADDR, REG_ENCRYPT_RX_AKSV0 + i, p_u8_ksv[i]);
    }
    ms7210drv_hd_tx_ddc_enable(FALSE);
}

VOID ms7210drv_hd_tx_encrypt_set_key_to_tx(UINT8 *p_u8_key)
{
    UINT16 i;
    
    for (i = 0; i < TX_KEY_LEN; i ++)
    {
        HAL_WriteByte(MS7210_HD_TX_ENCRYPT_KEY_REG + REG_ADDRESS_OFST, p_u8_key[i]);
    }
}

/***************************************************************
*  Function name:   ms7210drv_hd_tx_encrypt_enable
*  Description:     hd encrypt enable
*  Entry:           [in]b_enable, if true enable encrypt, else disable
*               
*  Returned value:  None
*  Remark:
***************************************************************/
VOID ms7210drv_hd_tx_encrypt_enable(BOOL b_enable)
{
    if (b_enable)
    {
        //HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG + REG_ADDRESS_OFST, 0x4a);
        //delay >= 100us for wait m0/ks/r0 is calculated 
        //Delay_us(100);
        //MS7210_LOG1("KM calc done = ", HAL_ReadByte(MS7210_HD_TX_ENCRYPT_STATUS_REG + REG_ADDRESS_OFST) & MSRT_BIT2);
        HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG + REG_ADDRESS_OFST, 0x0b);
    }
    else
    {
        HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG + REG_ADDRESS_OFST, 0x04);
        HAL_WriteByte(MS7210_HD_TX_ENCRYPT_CONTROL_REG + REG_ADDRESS_OFST, 0x00);
    }
}   

/***************************************************************
*  Function name:   ms7210drv_hd_tx_encrypt_init
*  Description:     hd encrypt init
*  Entry:           [in]p_u8_key, 280 bytes hd tx key
*                       p_u8_ksv, 5 bytes hd tx ksv
*
*  Returned value:  if encrypt init success return true, else return false
*  Remark:
***************************************************************/
BOOL ms7210drv_hd_tx_encrypt_init(UINT8 *p_u8_key, UINT8 *p_u8_ksv)
{
    UINT8 u8_arr_aksv_buf[AKSV_SIZE];

    if (ms7210drv_hd_tx_ddc_is_busy())
    {
        return FALSE;
    }
    
    if (ms7210drv_hd_tx_encrypt_get_bksv_from_rx(u8_arr_aksv_buf))
    {
        ms7210drv_hd_tx_encrypt_set_bksv_to_tx(u8_arr_aksv_buf);
        ms7210drv_hd_tx_encrypt_set_an();
        ms7210drv_hd_tx_encrypt_set_aksv_to_rx(p_u8_ksv);
        ms7210drv_hd_tx_encrypt_set_key_to_tx(p_u8_key);

        return TRUE;
    }
    
    return FALSE;
}

UINT8 ms7210drv_hd_tx_encrypt_get_status(HD_ENCRYPT_RI *pt)
{
    UINT8 i;
    
    for (i = 0; i < 3; i ++)
    {
        pt->TX_Ri0 = HAL_ReadByte(MS7210_HD_TX_ENCRYPT_RI0_REG + REG_ADDRESS_OFST);
        pt->TX_Ri1 = HAL_ReadByte(MS7210_HD_TX_ENCRYPT_RI1_REG + REG_ADDRESS_OFST);
        ms7210drv_hd_tx_ddc_enable(TRUE);
        pt->RX_Ri0 = HD_DDC_ReadByte(ENCRYPT_RX_SLAVE_ADDR, REG_ENCRYPT_RX_RI0);
        pt->RX_Ri1 = HD_DDC_ReadByte(ENCRYPT_RX_SLAVE_ADDR, REG_ENCRYPT_RX_RI1);
        ms7210drv_hd_tx_ddc_enable(FALSE);
        //MS7210_LOG1("TX_Ri0 = ", pt->TX_Ri0);
        //MS7210_LOG1("TX_Ri1 = ", pt->TX_Ri1);
        //MS7210_LOG1("RX_Ri0 = ", pt->RX_Ri0);
        //MS7210_LOG1("RX_Ri1 = ", pt->RX_Ri1);
        if (pt->TX_Ri0 == pt->RX_Ri0 && pt->TX_Ri1 == pt->RX_Ri1)
        {
            //MS7210_LOG("ENCRYPT success.");
            return 0x01;
        }
        else
        {
            Delay_ms(1); //20180607, resolve for new design.
        }
    }

    //MS7210_LOG("ENCRYPT failed.");
    return 0x00;
}

#endif // MS7210_HD_TX_ENCRYPT


//
// 
#if MS7210_HD_TX_EDID

// Generic Masks
//====================================================
#define LOW_BYTE                        (0x00FF)

#define LOW_NIBBLE                      (0x0F)
#define HI_NIBBLE                       (0xF0)

#define MSBIT                           (0x80)
#define LSBIT                           (0x01)

#define BIT_0                           (0x01)
#define BIT_1                           (0x02)
#define BIT_2                           (0x04)
#define BIT_3                           (0x08)
#define BIT_4                           (0x10)
#define BIT_5                           (0x20)
#define BIT_6                           (0x40)
#define BIT_7                           (0x80)

#define TWO_LSBITS                      (0x03)
#define THREE_LSBITS                    (0x07)
#define FOUR_LSBITS                     (0x0F)
#define FIVE_LSBITS                     (0x1F)
#define SEVEN_LSBITS                    (0x7F)
#define TWO_MSBITS                      (0xC0)
#define EIGHT_BITS                      (0xFF)
#define EDID_BYTE_SIZE                  (0x08)
#define BITS_1_0                        (0x03)
#define BITS_2_1                        (0x06)
#define BITS_2_1_0                      (0x0C)
#define BITS_4_3_2                      (0x1C)  
#define BITS_5_4                        (0x30)
#define BITS_5_4_3                      (0x38)
#define BITS_6_5                        (0x60)
#define BITS_6_5_4                      (0x70)
#define BITS_7_6                        (0xC0)

//--------------------------------------------------------------------
// EDID Constants Definition
//--------------------------------------------------------------------
#define EDID_BLOCK_0_OFFSET             (0x00)
#define EDID_BLOCK_1_OFFSET             (0x80)

#define EDID_BLOCK_SIZE                 (128)
#define EDID_HDR_NO_OF_FF               (0x06)
#define NUM_OF_EXTEN_ADDR               (0x7E)

#define EDID_TAG_ADDR                   (0x00)
#define EDID_REV_ADDR                   (0x01)
#define EDID_TAG_IDX                    (0x02)
#define LONG_DESCR_PTR_IDX              (0x02)
#define MISC_SUPPORT_IDX                (0x03)

#define ESTABLISHED_TIMING_INDEX        (35)      // Offset of Established Timing in EDID block
#define NUM_OF_STANDARD_TIMINGS         (8)
#define STANDARD_TIMING_OFFSET          (38)
#define LONG_DESCR_LEN                  (18)
#define NUM_OF_DETAILED_DESCRIPTORS     (4)

#define DETAILED_TIMING_OFFSET          (0x36)


// Offsets within a Long Descriptors Block
//====================================================
#define PIX_CLK_OFFSET                  (0)
#define H_ACTIVE_OFFSET                 (2)
#define H_BLANKING_OFFSET               (3)
#define V_ACTIVE_OFFSET                 (5)
#define V_BLANKING_OFFSET               (6)
#define H_SYNC_OFFSET                   (8)
#define H_SYNC_PW_OFFSET                (9)
#define V_SYNC_OFFSET                   (10)
#define V_SYNC_PW_OFFSET                (10)
#define H_IMAGE_SIZE_OFFSET             (12)
#define V_IMAGE_SIZE_OFFSET             (13)
#define H_BORDER_OFFSET                 (15)
#define V_BORDER_OFFSET                 (16)
#define FLAGS_OFFSET                    (17)

#define AR16_10                         (0)
#define AR4_3                           (1)
#define AR5_4                           (2)
#define AR16_9                          (3)


// Data Block Tag Codes
#define AUDIO_D_BLOCK                   (0x01)
#define VIDEO_D_BLOCK                   (0x02)
#define VENDOR_SPEC_D_BLOCK             (0x03)
#define SPKR_ALLOC_D_BLOCK              (0x04)
#define VESA_DTC_D_BLOCK                (0x05)
#define USE_EXTENDED_TAG                (0x07)


// Extended Data Block Tag Codes
//====================================================
#define VIDEO_CAPABILITY_D_BLOCK        (0x00)
#define VENDOR_SPEC_VIDEO_D_BLOCK       (0x01)
#define COLORIMETRY_D_BLOCK             (0x05)
#define CEA_MISC_AUDIO_FIELDS           (0x10)
#define VENDOR_SPEC_AUDIO_D_BLOCK       (0x11)

#define HD_SIGNATURE_LEN              (0x03)

#define CEC_PHYS_ADDR_LEN               (0x02)
#define EDID_EXTENSION_TAG              (0x02)
#define EDID_REV_THREE                  (0x03)
#define EDID_DATA_START                 (0x04)

#define EDID_BLOCK_0                    (0x00)
#define EDID_BLOCK_2_3                  (0x01) //



// Checking EDID header only
static BOOL EDID_CheckHeader(UINT8* pHeader)
{
    UINT8 i = 0;
    if ((pHeader[0] != 0) || (pHeader[7] != 0))
    {
        return FALSE;
    }

    for (i = 1; i <= 6; i ++)
    {
        if (pHeader[i] != 0xFF)    // Must be 0xFF.
            return FALSE;
    }
    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: ParseDetailedTiming()
// Function Description: Parse the detailed timing section of EDID Block 0 and
//                  print their decoded meaning to the screen.
//
// Accepts: Pointer to the 128 byte array where the data read from EDID Block0 is stored.
//              Offset to the beginning of the Detailed Timing Descriptor data.
//
//              Block indicator to distinguish between block #0 and blocks #2, #3
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
UINT8 ParseDetailedTiming (UINT8 *Data, UINT8 DetailedTimingOffset, UINT8 Block, HD_EDID_FLAG_T *pt_edid)
{
    UINT8 TmpByte;
    UINT8 i;
    UINT16 TmpWord;
    UINT16 u16PerferHact;
    UINT16 u16PerferVact;
    UINT16 u16PerferPixelClk;

    TmpWord = Data[DetailedTimingOffset + PIX_CLK_OFFSET] +
                256 * Data[DetailedTimingOffset + PIX_CLK_OFFSET + 1];

    if (TmpWord == 0x00)            // 18 byte partition is used as either for Monitor Name or for Monitor Range Limits or it is unused
    {
        if (Block == EDID_BLOCK_0)      // if called from Block #0 and first 2 bytes are 0 => either Monitor Name or for Monitor Range Limits
        {
            if (Data[DetailedTimingOffset + 3] == 0xFC) // these 13 bytes are ASCII coded monitor name
            {
                TPI_EDID_PRINT(MS7210_PRINTF(("Monitor Name: ")));

                for (i = 0; i < 13; i++)
                {
                    TPI_EDID_PRINT(MS7210_PRINTF("%c", Data[DetailedTimingOffset + 5 + i])); // Display monitor name on SiIMon
                }
                TPI_EDID_PRINT(MS7210_PRINTF("\n"));
            }
            else if (Data[DetailedTimingOffset + 3] == 0xFD) // these 13 bytes contain Monitor Range limits, binary coded
            {
                TPI_EDID_PRINT(MS7210_PRINTF(("Monitor Range Limits:\n\n")));

                i = 0;
                TPI_EDID_PRINT(MS7210_PRINTF("Min Vertical Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++])); //
                TPI_EDID_PRINT(MS7210_PRINTF("Max Vertical Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++])); //
                TPI_EDID_PRINT(MS7210_PRINTF("Min Horizontal Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++])); //
                TPI_EDID_PRINT(MS7210_PRINTF("Max Horizontal Rate in Hz: %d\n", (int) Data[DetailedTimingOffset + 5 + i++])); //
                TPI_EDID_PRINT(MS7210_PRINTF("Max Supported pixel clock rate in MHz/10: %d\n", (int) Data[DetailedTimingOffset + 5 + i++])); //
                TPI_EDID_PRINT(MS7210_PRINTF("Tag for secondary timing formula (00h=not used): %d\n", (int) Data[DetailedTimingOffset + 5 + i++])); //
                TPI_EDID_PRINT(MS7210_PRINTF("Min Vertical Rate in Hz %d\n", (int) Data[DetailedTimingOffset + 5 + i])); //
                TPI_EDID_PRINT(MS7210_PRINTF("\n"));
            }
        }
        else if (Block == EDID_BLOCK_2_3)                          // if called from block #2 or #3 and first 2 bytes are 0x00 (padding) then this
        {                                                                                          // descriptor partition is not used and parsing should be stopped
            TPI_EDID_PRINT(MS7210_PRINTF("No More Detailed descriptors in this block\n"));
            TPI_EDID_PRINT(MS7210_PRINTF("\n"));
            return FALSE;
        }
    }
    else                                            // first 2 bytes are not 0 => this is a detailed timing descriptor from either block
    {
        if((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x36))
        {
            TPI_EDID_PRINT(MS7210_PRINTF("\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 1:\n"));
            TPI_EDID_PRINT(MS7210_PRINTF("===========================================================\n\n"));
        }
        else if((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x48))
        {
            TPI_EDID_PRINT(MS7210_PRINTF("\n\n\nParse Results, EDID Block #0, Detailed Descriptor Number 2:\n"));
            TPI_EDID_PRINT(MS7210_PRINTF("===========================================================\n\n"));
        }

        TPI_EDID_PRINT(MS7210_PRINTF("Pixel Clock (MHz * 100): %d\n", (int)TmpWord));
        u16PerferPixelClk = TmpWord;

        TmpWord = Data[DetailedTimingOffset + H_ACTIVE_OFFSET] +
                    256 * ((Data[DetailedTimingOffset + H_ACTIVE_OFFSET + 2] >> 4) & FOUR_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Horizontal Active Pixels: %d\n", (int)TmpWord));
        u16PerferHact = TmpWord;

        TmpWord = Data[DetailedTimingOffset + H_BLANKING_OFFSET] +
                    256 * (Data[DetailedTimingOffset + H_BLANKING_OFFSET + 1] & FOUR_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Horizontal Blanking (Pixels): %d\n", (int)TmpWord));

        TmpWord = (Data[DetailedTimingOffset + V_ACTIVE_OFFSET] )+
                    256 * ((Data[DetailedTimingOffset + (V_ACTIVE_OFFSET) + 2] >> 4) & FOUR_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Vertical Active (Lines): %d\n", (int)TmpWord));
        u16PerferVact = TmpWord;

        TmpWord = Data[DetailedTimingOffset + V_BLANKING_OFFSET] +
                    256 * (Data[DetailedTimingOffset + V_BLANKING_OFFSET + 1] & LOW_NIBBLE);
        TPI_EDID_PRINT(MS7210_PRINTF("Vertical Blanking (Lines): %d\n", (int)TmpWord));

        TmpWord = Data[DetailedTimingOffset + H_SYNC_OFFSET] +
                    256 * ((Data[DetailedTimingOffset + (H_SYNC_OFFSET + 3)] >> 6) & TWO_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Horizontal Sync Offset (Pixels): %d\n", (int)TmpWord));

        TmpWord = Data[DetailedTimingOffset + H_SYNC_PW_OFFSET] +
                    256 * ((Data[DetailedTimingOffset + (H_SYNC_PW_OFFSET + 2)] >> 4) & TWO_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Horizontal Sync Pulse Width (Pixels): %d\n", (int)TmpWord));

        TmpWord = (Data[DetailedTimingOffset + V_SYNC_OFFSET] >> 4) & FOUR_LSBITS +
                    256 * ((Data[DetailedTimingOffset + (V_SYNC_OFFSET + 1)] >> 2) & TWO_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Vertical Sync Offset (Lines): %d\n", (int)TmpWord));

        TmpWord = (Data[DetailedTimingOffset + V_SYNC_PW_OFFSET]) & FOUR_LSBITS +
                    256 * (Data[DetailedTimingOffset + (V_SYNC_PW_OFFSET + 1)] & TWO_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Vertical Sync Pulse Width (Lines): %d\n", (int)TmpWord));

        TmpWord = Data[DetailedTimingOffset + H_IMAGE_SIZE_OFFSET] +
                    256 * (((Data[DetailedTimingOffset + (H_IMAGE_SIZE_OFFSET + 2)]) >> 4) & FOUR_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Horizontal Image Size (mm): %d\n", (int)TmpWord));

        TmpWord = Data[DetailedTimingOffset + V_IMAGE_SIZE_OFFSET] +
                    256 * (Data[DetailedTimingOffset + (V_IMAGE_SIZE_OFFSET + 1)] & FOUR_LSBITS);
        TPI_EDID_PRINT(MS7210_PRINTF("Vertical Image Size (mm): %d\n", (int)TmpWord));

        TmpByte = Data[DetailedTimingOffset + H_BORDER_OFFSET];
        TPI_EDID_PRINT(MS7210_PRINTF("Horizontal Border (Pixels): %d\n", (int)TmpByte));

        TmpByte = Data[DetailedTimingOffset + V_BORDER_OFFSET];
        TPI_EDID_PRINT(MS7210_PRINTF("Vertical Border (Lines): %d\n", (int)TmpByte));

        TmpByte = Data[DetailedTimingOffset + FLAGS_OFFSET];
        if (TmpByte & BIT_7)
            TPI_EDID_PRINT(MS7210_PRINTF("Interlaced\n"));
        else
            TPI_EDID_PRINT(MS7210_PRINTF("Non-Interlaced\n"));

        if (!(TmpByte & BIT_5) && !(TmpByte & BIT_6))
            TPI_EDID_PRINT(MS7210_PRINTF("Normal Display, No Stereo\n"));
        else
            TPI_EDID_PRINT(MS7210_PRINTF("Refer to VESA E-EDID Release A, Revision 1, table 3.17\n"));

        if (!(TmpByte & BIT_3) && !(TmpByte & BIT_4))
            TPI_EDID_PRINT(MS7210_PRINTF("Analog Composite\n"));
        if ((TmpByte & BIT_3) && !(TmpByte & BIT_4))
            TPI_EDID_PRINT(MS7210_PRINTF("Bipolar Analog Composite\n"));
        else if (!(TmpByte & BIT_3) && (TmpByte & BIT_4))
            TPI_EDID_PRINT(MS7210_PRINTF("Digital Composite\n"));

        else if ((TmpByte & BIT_3) && (TmpByte & BIT_4))
            TPI_EDID_PRINT(MS7210_PRINTF("Digital Separate\n"));

      TPI_EDID_PRINT(MS7210_PRINTF("\n"));

      if ((Block == EDID_BLOCK_0) && (DetailedTimingOffset == 0x36))
      {
          //MS7210_LOG2("u16PerferPixelClk = ", u16PerferPixelClk);
          //MS7210_LOG2("u16PerferHact = ", u16PerferHact);
          //MS7210_LOG2("u16PerferVact = ", u16PerferVact);
          pt_edid->u16_preferred_pixel_clk = u16PerferPixelClk;
          pt_edid->u32_preferred_timing = (UINT32)u16PerferHact * u16PerferVact;
      }
    }
    
    return TRUE;
}

//------------------------------------------------------------------------------
// Function Name: ParseBlock_0_TimingDescripors()
// Function Description: Parse EDID Block 0 timing descriptors per EEDID 1.3
//                  standard. printf() values to screen.
//
// Accepts: Pointer to the 128 byte array where the data read from EDID Block0 is stored.
// Returns: none
// Globals: EDID data
//------------------------------------------------------------------------------
VOID ParseBlock_0_TimingDescripors (UINT8 *Data, HD_EDID_FLAG_T *pt_edid)
{
        UINT8 i;
        UINT8 Offset;

        //ParseEstablishedTiming(Data);
        //ParseStandardTiming(Data);

        for (i = 0; i < NUM_OF_DETAILED_DESCRIPTORS; i++)
        {
            Offset = DETAILED_TIMING_OFFSET + (LONG_DESCR_LEN * i);
            ParseDetailedTiming(Data, Offset, EDID_BLOCK_0, pt_edid);
        }
}

static VOID EDID_Parse_HD_D_block(UINT8 *p_u8_edid_buf, UINT8 u8_d_block_len, HD_EDID_FLAG_T *pt_edid)
{
    UINT8 i;
    UINT32 u32maxVideoTiming;
    UINT8 u8vic;

    i = 0;
    while (i < u8_d_block_len)        // each SVD is 1 byte long
    {
        u32maxVideoTiming = 0;
        ++i;
        u8vic = p_u8_edid_buf[i] & 0x7f;
        if (u8vic < 86)
        {
            continue;
        }
        else if (u8vic < 93)   //2560 x 1080p: 86 ~ 92
        {
            u32maxVideoTiming = 2560UL * 1080;
        }
        else if (u8vic < 98)   //3840 x 2160p: 93 ~ 97
        {
            u32maxVideoTiming = 3840UL * 2160;
        }
        else if (u8vic < 103)  //4096 x 2160p: 98 ~ 102
        {
            u32maxVideoTiming = 4096UL * 2160;
        }
        else if (u8vic < 108)  //3840 x 2160p: 103 ~ 107
        {
            u32maxVideoTiming = 3840UL * 2160;
        }

        pt_edid->u32_max_video_block_timing = (u32maxVideoTiming > pt_edid->u32_max_video_block_timing) ? u32maxVideoTiming : pt_edid->u32_max_video_block_timing;
    }
}

//0x00: no 4K timing, BIT4: 4096x2160 SMPTE
static UINT8 EDID_Parse_HD_VSDB_4K_VIC(UINT8 *p_u8_hd_vic_buf, UINT8 u8_hd_vic_len)
{
    UINT8 i;
    UINT8 u8_flag = 0;

    //HD_VIC = 1: 4Kx2K 29.97, 30Hz
    //HD_VIC = 2: 4Kx2K 25Hz
    //HD_VIC = 3: 4Kx2K 23.98, 24Hz
    //HD_VIC = 4: 4Kx2K 24Hz, SMPTE
    
    for (i = 0; i < u8_hd_vic_len; i ++)
    {
        if (p_u8_hd_vic_buf[i] > 0 && p_u8_hd_vic_buf[i] < 4)
        {
            u8_flag |= 0x01;
        }
        else if (p_u8_hd_vic_buf[i] == 4)
        {
            u8_flag |= 0x10;
        }
    }

    return u8_flag;
}

static VOID EDID_Parse_HD_VSDB_4K_timing(UINT8 *p_u8_edid_buf, UINT8 u8_VSDB_len, HD_EDID_FLAG_T *pt_edid)
{
    //UINT8 i;
    UINT8 u8_offset;
    UINT8 u8_hd_vic_len;
    UINT8 u8_flag;
    UINT32 u32maxVideoTiming;
    
    if (u8_VSDB_len >= 0x7) //HD VSDB length
    {
        pt_edid->u8_max_tmds_clk = p_u8_edid_buf[7];
    }
    
    //HD VIC 
    if (u8_VSDB_len >= 8)
    {
        if ((p_u8_edid_buf[8] & 0xC0) == 0x00)
        {
            u8_offset = 10;
        }
        else if ((p_u8_edid_buf[8] & 0xC0) == 0xC0)
        {
            u8_offset = 14;
        }
        else
        {
            u8_offset = 12;
        }
        
        if (u8_VSDB_len >= u8_offset)
        {
            u8_hd_vic_len = p_u8_edid_buf[u8_offset] >> 5; //HD VIC length
            if (u8_VSDB_len >= (u8_offset + u8_hd_vic_len))
            {
                //MS7210_LOG("HD_VIC 4K Parse.");
                u8_flag = EDID_Parse_HD_VSDB_4K_VIC(&p_u8_edid_buf[u8_offset + 1], u8_hd_vic_len);

                u32maxVideoTiming = 0;
                
                if (u8_flag & 0x10)
                {
                    u32maxVideoTiming = 4096UL * 2160;
                }
                else if (u8_flag & 0x01)
                {
                    u32maxVideoTiming = 3840UL * 2160;
                }

                pt_edid->u32_max_video_block_timing = (u32maxVideoTiming > pt_edid->u32_max_video_block_timing) ? u32maxVideoTiming : pt_edid->u32_max_video_block_timing;
            }
        }
        
    }
}

// Support EDID 1.3 only. Pls refer to HD 1.3a specification. EDID block1 128 data
static BOOL EDID_Parse861Extensions(UINT8 *p_u8_edid_buf, HD_EDID_FLAG_T *pt_edid)
{
    BOOL bSucc = FALSE;
    UINT8 u8tagCode;
    UINT8 u8tagLength;
    
    UINT8 u8dataIdx;
    UINT8 u8longDescriptor = 0;
    UINT8 *p_u8_buf = p_u8_edid_buf;
    
    // 02 03
    if (p_u8_buf[EDID_TAG_ADDR] != EDID_EXTENSION_TAG)
    {
        return TRUE; //20170331, update according to si9022 hd tx sdk.
    }
    if (p_u8_buf[EDID_REV_ADDR] != EDID_REV_THREE) //20170330, other version set to DVI device
    {
        return TRUE;    
    }
    
    // Support the misc supported-features.
    //20181122, only bit4 and bit5 defined for color space.
    pt_edid->u8_color_space  = p_u8_buf[MISC_SUPPORT_IDX] & 0x30;
    
    u8dataIdx = 0;
    u8longDescriptor = p_u8_buf[LONG_DESCR_PTR_IDX];
    while (u8dataIdx < u8longDescriptor)
    {
        p_u8_buf = &p_u8_edid_buf[(EDID_DATA_START + u8dataIdx) % EDID_BLOCK_SIZE];
        
        // $NOTE:
        // BIT: 7    6    5   4   3   2   1   0
        //      | Tag Code| Length of block   |
        //  
        u8tagCode   = (p_u8_buf[0] >> 5) & 0x07;
        u8tagLength = (p_u8_buf[0]) & 0x1F;
        
        if ((u8dataIdx + u8tagLength) > u8longDescriptor)
        {
            return TRUE;
        }
        
        switch (u8tagCode)
        {
        case VIDEO_D_BLOCK:        
            EDID_Parse_HD_D_block(p_u8_buf, u8tagLength, pt_edid);
            break;
            
        case AUDIO_D_BLOCK:        
        case SPKR_ALLOC_D_BLOCK:
        case VESA_DTC_D_BLOCK:
            break;

        case USE_EXTENDED_TAG:           
            switch (p_u8_buf[1])
            {
                case VIDEO_CAPABILITY_D_BLOCK:
                case VENDOR_SPEC_VIDEO_D_BLOCK:
                case COLORIMETRY_D_BLOCK:
                case CEA_MISC_AUDIO_FIELDS:
                case VENDOR_SPEC_AUDIO_D_BLOCK:
                    break;
                default:
                    pt_edid->u8_hd_2_0_flag = 1;
            }
            break;
            
        case VENDOR_SPEC_D_BLOCK:
            if ((p_u8_buf[1] == 0x03) && (p_u8_buf[2] == 0x0C) && (p_u8_buf[3] == 0x00))
            {
                pt_edid->u8_hd_sink = 0x01;    // Yes, it is HD sink
                bSucc = TRUE;//return TRUE;
            }
            else
            {
                pt_edid->u8_hd_2_0_flag = 1;
            }
            
            EDID_Parse_HD_VSDB_4K_timing(p_u8_buf, u8tagLength, pt_edid);
            break;               
        }  
        u8dataIdx += u8tagLength;    // Increase the size.
        u8dataIdx ++;
    } 
    return bSucc;   
}

BOOL _drv_edid_checksum(UINT8 *p_u8_edid_buf)
{
    UINT8 i;
    UINT8 checksum = 0;
    
    for (i = 0; i < 128; i ++)
    {
        checksum += p_u8_edid_buf[i];
    }

    return (checksum == 0x00);
}

BOOL ms7210drv_hd_tx_parse_edid(UINT8 *p_u8_edid_buf, HD_EDID_FLAG_T *pt_edid)
{   
    UINT8 i = 0;
    UINT8 u8_block_max = 0;
    BOOL bSucc = TRUE;
    
	printk("ms7210drv_hd_tx_parse_edid\n");

    // 
    pt_edid->u8_hd_sink = FALSE;  //default DVI sink
    pt_edid->u8_color_space = 0;    
    pt_edid->u8_edid_total_blocks = 0;
    //
    pt_edid->u16_preferred_pixel_clk = 0xFFFF;
    pt_edid->u32_preferred_timing = 0xFFFFFFFFUL;
    pt_edid->u8_max_tmds_clk = 0;
    pt_edid->u32_max_video_block_timing = 0;
    pt_edid->u8_hd_2_0_flag = 0;

    //Reset variable
    memset(p_u8_edid_buf, 0, 256);

    if (ms7210drv_hd_tx_ddc_is_busy())
    {
        printk("tx_ddc_busy.");
        bSucc = FALSE;
        goto NEXT;
    }

    ms7210drv_hd_tx_ddc_enable(TRUE); 
    
    HD_DDC_ReadBytes(0xA0, 0, 8, p_u8_edid_buf);     
        
    if (! EDID_CheckHeader(p_u8_edid_buf))
    {
        MS7210_LOG("EDID header failed.");
        bSucc = FALSE;
        goto NEXT;
    }

    //
    HD_DDC_ReadBytes(0xA0, EDID_BLOCK_0_OFFSET, EDID_BLOCK_SIZE, p_u8_edid_buf);
    if (!_drv_edid_checksum(p_u8_edid_buf))
    {
        MS7210_LOG("EDID block0 failed.");
        bSucc = FALSE;
        goto NEXT;
    }
    
    pt_edid->u8_edid_total_blocks = 1;
    u8_block_max = p_u8_edid_buf[NUM_OF_EXTEN_ADDR] % 4;

    //
    ParseBlock_0_TimingDescripors(p_u8_edid_buf, pt_edid);

    // Read more one byte for extension flag.
    // Checking if the EDID extension
    if (u8_block_max != 0)    
    {
        pt_edid->u8_edid_total_blocks = 2;
        HD_DDC_ReadBytes(0xA0, EDID_BLOCK_1_OFFSET, EDID_BLOCK_SIZE, &p_u8_edid_buf[128]);
        if (!_drv_edid_checksum(&p_u8_edid_buf[128]))
        {
            MS7210_LOG("EDID block1 failed.");
            bSucc = FALSE;
            goto NEXT;
        }
        bSucc = EDID_Parse861Extensions(&p_u8_edid_buf[128], pt_edid);
    }

    if (u8_block_max >= 2)
    {
        MS7210_LOG1("u8_block_max = ", u8_block_max);
        for (i = 2; i <= u8_block_max; i ++)
        {
            HD_DDC_WriteBlank(0x60, i / 2);
            if ((i % 2)==0)
            {
                HD_DDC_ReadBytes_ext(0xA0, 0, 128);
            }
            else
            {
                HD_DDC_ReadBytes_ext(0xA0, 128, 128);
            }
        }
    }
    
NEXT:
    if (!bSucc)
    {
        pt_edid->u8_hd_sink = TRUE;   //default HD sink
        pt_edid->u8_color_space = 0;
        pt_edid->u8_edid_total_blocks = 0;
        //
        pt_edid->u16_preferred_pixel_clk = 0xFFFF;
        pt_edid->u32_preferred_timing = 0xFFFFFFFFUL;
        pt_edid->u8_max_tmds_clk = 0;
        pt_edid->u32_max_video_block_timing = 0;
        pt_edid->u8_hd_2_0_flag = 0;
    }
    
    ms7210drv_hd_tx_ddc_enable(FALSE);
    return bSucc;
}
#endif //#if MS7210_HD_TX_EDID
