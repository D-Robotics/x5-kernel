/**
******************************************************************************
* @file    ms7210_drv_misc.c
* @author  
* @version V1.0.0
* @date    15-Nov-2016
* @brief   misc module driver source file
* @history 
*
* Copyright (c) 2009 - 2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#include "ms7210_comm.h"
#include "ms7210_mpi.h"
#include "ms7210_drv_misc.h"


/* TOP MISC register address map */
#define MS7210_MISC_REG_BASE                     (0x0000)
#define REG_CHIPID0_REG                          (MS7210_MISC_REG_BASE + 0x0000)  
#define REG_CHIPID1_REG                          (MS7210_MISC_REG_BASE + 0x0001)
#define REG_CHIPID2_REG                          (MS7210_MISC_REG_BASE + 0x0002)
#define REG_CLK_CTRL1                            (MS7210_MISC_REG_BASE + 0x0004)
#define REG_CLK_CTRL5                            (MS7210_MISC_REG_BASE + 0x0008)
#define REG_RX_PLL_SEL                           (MS7210_MISC_REG_BASE + 0x000c)
#define REG_MISC_FREQ_CTRL                       (MS7210_MISC_REG_BASE + 0x0019)
#define REG_MISC_FREQ_OUT_L                      (MS7210_MISC_REG_BASE + 0x001e)
#define REG_RC_CTRL1                             (MS7210_MISC_REG_BASE + 0x0060)
#define REG_RC_CTRL2                             (MS7210_MISC_REG_BASE + 0x0061)
#define REG_BDOP_REG                             (MS7210_MISC_REG_BASE + 0x00a3)
#define REG_DIG_CLK_SEL                          (MS7210_MISC_REG_BASE + 0x00c1)
#define REG_PINMUX                               (MS7210_MISC_REG_BASE + 0x00a5)
#define REG_IO_CTRL3                             (MS7210_MISC_REG_BASE + 0x0079)

/* CSC register address map */
#define MS7210_CSC_REG_BASE                      (0x0080)
#define REG_CSC_CTRL1                            (MS7210_CSC_REG_BASE + 0x00)  

/* hd rx controller register address map */
#define MS7210_AUPLL_REGBASE                     (0x0400)
#define REG_AUPLL_PWR                            (MS7210_AUPLL_REGBASE + 0x000)
#define REG_AUPLL_M                              (MS7210_AUPLL_REGBASE + 0x001)
#define REG_AUPLL_FN_L                           (MS7210_AUPLL_REGBASE + 0x002)
#define REG_AUPLL_FN_M                           (MS7210_AUPLL_REGBASE + 0x003)
#define REG_AUPLL_FN_H                           (MS7210_AUPLL_REGBASE + 0x004)
#define REG_AUPLL_CTRL1                          (MS7210_AUPLL_REGBASE + 0x005)
#define REG_AUPLL_CTRL2                          (MS7210_AUPLL_REGBASE + 0x006)
#define REG_AUPLL_POS_DVI                        (MS7210_AUPLL_REGBASE + 0x007)
#define REG_AUPLL_TEST_CTRL                      (MS7210_AUPLL_REGBASE + 0x008)
#define REG_AUPLL_CFG_CTRL                       (MS7210_AUPLL_REGBASE + 0x009)
#define REG_AUPLL_CFG_COEF                       (MS7210_AUPLL_REGBASE + 0x00A)
#define REG_AUPLL_UPD_TIMER_L                    (MS7210_AUPLL_REGBASE + 0x00B)
#define REG_AUPLL_UPD_TIMER_M                    (MS7210_AUPLL_REGBASE + 0x00C)
#define REG_AUPLL_UPD_TIMER_H                    (MS7210_AUPLL_REGBASE + 0x00D)
#define REG_AUPLL_FIFO_TH1_L                     (MS7210_AUPLL_REGBASE + 0x00E)
#define REG_AUPLL_FIFO_TH1_H                     (MS7210_AUPLL_REGBASE + 0x00F)
#define REG_AUPLL_FIFO_TH2_L                     (MS7210_AUPLL_REGBASE + 0x010)
#define REG_AUPLL_FIFO_TH2_H                     (MS7210_AUPLL_REGBASE + 0x011)
#define REG_AUPLL_FIFO_TH3_L                     (MS7210_AUPLL_REGBASE + 0x012)
#define REG_AUPLL_FIFO_TH3_H                     (MS7210_AUPLL_REGBASE + 0x013)
#define REG_AUPLL_FIFO_TH4_L                     (MS7210_AUPLL_REGBASE + 0x014)
#define REG_AUPLL_FIFO_TH4_H                     (MS7210_AUPLL_REGBASE + 0x015)
#define REG_AUPLL_FIFO_TH5_L                     (MS7210_AUPLL_REGBASE + 0x016)
#define REG_AUPLL_FIFO_TH5_H                     (MS7210_AUPLL_REGBASE + 0x017)
#define REG_AUPLL_FIFO_TH6_L                     (MS7210_AUPLL_REGBASE + 0x018)
#define REG_AUPLL_FIFO_TH6_H                     (MS7210_AUPLL_REGBASE + 0x019)
#define REG_AUPLL_FIFO_TH7_L                     (MS7210_AUPLL_REGBASE + 0x01A)
#define REG_AUPLL_FIFO_TH7_H                     (MS7210_AUPLL_REGBASE + 0x01B)
#define REG_AUPLL_FREQ_INC_STEP4                 (MS7210_AUPLL_REGBASE + 0x01C)
#define REG_AUPLL_FREQ_INC_STEP3                 (MS7210_AUPLL_REGBASE + 0x01D)
#define REG_AUPLL_FREQ_INC_STEP2                 (MS7210_AUPLL_REGBASE + 0x01E)
#define REG_AUPLL_FREQ_INC_STEP1                 (MS7210_AUPLL_REGBASE + 0x01F)
#define REG_AUPLL_FREQ_DEC_STEP1                 (MS7210_AUPLL_REGBASE + 0x020)
#define REG_AUPLL_FREQ_DEC_STEP2                 (MS7210_AUPLL_REGBASE + 0x021)
#define REG_AUPLL_FREQ_DEC_STEP3                 (MS7210_AUPLL_REGBASE + 0x022)
#define REG_AUPLL_FREQ_DEC_STEP4                 (MS7210_AUPLL_REGBASE + 0x023)
#define REG_AUPLL_M_VAL                          (MS7210_AUPLL_REGBASE + 0x024)
#define REG_AUPLL_FN_VAL_L                       (MS7210_AUPLL_REGBASE + 0x025)
#define REG_AUPLL_FN_VAL_M                       (MS7210_AUPLL_REGBASE + 0x026)
#define REG_AUPLL_FN_VAL_H                       (MS7210_AUPLL_REGBASE + 0x027)
#define REG_AUPLL_LOCK                           (MS7210_AUPLL_REGBASE + 0x028)
#define REG_AUPLL_FREQ_RANGE                     (MS7210_AUPLL_REGBASE + 0x029)
#define REG_AUPLL_FIFO_DIFF_TH                   (MS7210_AUPLL_REGBASE + 0x02A)


#define MS7210_CHIP_ID0         (0xa2)
#define MS7210_CHIP_ID1         (0x20)
#define MS7210_CHIP_ID2         (0x0a)


UINT8 ms7210drv_misc_package_sel_get(VOID)
{
    UINT8 u8_package_sel = (HAL_ReadByte(REG_BDOP_REG) & MSRT_BITS3_2) >> 2;
    return u8_package_sel;
}

BOOL ms7210drv_misc_chipisvalid(VOID)
{
    UINT8 u8_chipid;
    
    u8_chipid = HAL_ReadByte(REG_CHIPID0_REG);
    if (u8_chipid != MS7210_CHIP_ID0)
    {
		printk("%s: chip id 0 reg[%04x] val[%02x] should be [%02x]\n", __FUNCTION__, REG_CHIPID0_REG, u8_chipid, MS7210_CHIP_ID0);
        return FALSE;
    }
	printk("%s(): chip id 0 = %02x\n", __FUNCTION__, u8_chipid);

	u8_chipid = HAL_ReadByte(REG_CHIPID1_REG);
	if (u8_chipid != MS7210_CHIP_ID1)
    {
		printk("%s: chip id 1 reg[%04x] val[%02x] should be [%02x]\n", __FUNCTION__, REG_CHIPID1_REG, u8_chipid, MS7210_CHIP_ID1);
        return FALSE;
    }
	printk("%s(): chip id 1 = %02x\n", __FUNCTION__, u8_chipid);

	u8_chipid = HAL_ReadByte(REG_CHIPID2_REG);
	if (u8_chipid != MS7210_CHIP_ID2)
    {
		printk("%s: chip id 2 reg[%04x] val[%02x] should be [%02x]\n", __FUNCTION__, REG_CHIPID2_REG, u8_chipid, MS7210_CHIP_ID2);
        return FALSE;
    }
	printk("%s(): chip id 2 = %02x\n", __FUNCTION__, u8_chipid);

    return TRUE;
}

VOID ms7210drv_misc_rc_freq_set(VOID)
{
    // RC25M = 27M
    HAL_WriteByte(REG_RC_CTRL1, 0x81);
    HAL_WriteByte(REG_RC_CTRL2, 0x54);
}

VOID ms7210drv_misc_freqm_pclk_enable(VOID)
{
    HAL_WriteByte(REG_MISC_FREQ_CTRL, (3 << 1) | MSRT_BIT0);
}

UINT16 ms7210drv_misc_freqm_pclk_get(VOID)
{
    UINT16 u16_freq_tmp = 0;
    u16_freq_tmp = HAL_ReadWord(REG_MISC_FREQ_OUT_L);
    return (UINT32)u16_freq_tmp * (MS7210_EXT_XTAL / 10000) / 4096;
}

VOID ms7210drv_misc_audio_pad_in_spdif(UINT8 u8_spdif_in)
{
    //u8_spdif_in: 0 = i2s, 1 = spdif & mclk, 2 = spdif
    if (u8_spdif_in == 2)
    {
        //aupll sel xtal
        HAL_SetBits(REG_RX_PLL_SEL, MSRT_BITS3_2);
        HAL_SetBits(REG_AUPLL_CTRL2, MSRT_BIT4);
        //audio clk enable
        HAL_SetBits(REG_CLK_CTRL1, MSRT_BIT0);
        //aupll
        //power down
        HAL_ClrBits(REG_AUPLL_PWR, MSRT_BIT1);
        HAL_SetBits(REG_AUPLL_PWR, MSRT_BIT0);
        //M
        HAL_WriteByte(REG_AUPLL_M, 0x1c);
        //spdiv div
        HAL_ModBits(REG_AUPLL_CFG_CTRL, MSRT_BITS7_6, MSRT_BIT6);
        //power on
        HAL_ClrBits(REG_AUPLL_PWR, MSRT_BIT0);
        Delay_us(100);
        HAL_SetBits(REG_AUPLL_PWR, MSRT_BIT1);
        //clk sel cdr
        HAL_SetBits(REG_CLK_CTRL5, MSRT_BITS1_0);
    }
    else
    {
        //clk sel pad
        HAL_ClrBits(REG_CLK_CTRL5, MSRT_BITS1_0);
        if (u8_spdif_in)
        {
            HAL_SetBits(REG_CLK_CTRL5, MSRT_BIT2);
        }
    }
    HAL_ModBits(REG_PINMUX, MSRT_BITS4_3, u8_spdif_in << 3);
}

VOID ms7210drv_misc_audio_mclk_div(UINT8 u8_div)
{
    HAL_ModBits(REG_DIG_CLK_SEL, MSRT_BITS7_6, u8_div << 6);
}

VOID ms7210drv_misc_dig_pads_pull_set(UINT8 u8_pull_status)
{
    //0=floating 1=pull up 2=pull down
    HAL_ModBits(REG_IO_CTRL3, MSRT_BITS5_4, u8_pull_status << 4);
}

VOID ms7210drv_csc_config_input(DVIN_CS_MODE_E e_cs)
{
    HAL_ModBits(REG_CSC_CTRL1, MSRT_BITS1_0, 0x10 | (UINT8)e_cs);
}

VOID ms7210drv_csc_config_output(HD_CS_E e_cs)
{
    UINT8 u8_cs = 3;   //0:RGB; 1:YUV444; 2:YUV422; 3:bypass
    switch (e_cs)
    {
        case HD_RGB: u8_cs = 0; break;
        case HD_YCBCR444: u8_cs = 1; break;
        case HD_YCBCR422: u8_cs = 2; break;
        default: break;
    }
    HAL_ModBits(REG_CSC_CTRL1, MSRT_BITS3_2, u8_cs << 2);
}

