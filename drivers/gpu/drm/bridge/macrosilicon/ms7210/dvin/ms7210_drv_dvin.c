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
#include "ms7210_drv_dvin.h"


#define MS7210_MISC_REG_BASE        (0x0000)
#define REG_MISC_DVIN_RESET         (MS7210_MISC_REG_BASE + 0x09)
#define REG_MISC_DVIN_ENABLE        (MS7210_MISC_REG_BASE + 0x16)
#define REG_MISC_DIG_CLK_SEL        (MS7210_MISC_REG_BASE + 0xC0)

/* DVIN register address map */
#define MS7210_DVIN_REG_BASE        (0x1200)
#define REG_DVIN_SRC_SEL            (MS7210_DVIN_REG_BASE + 0x00)
#define REG_DVIN_SWAP               (MS7210_DVIN_REG_BASE + 0x01)
#define REG_DVIN_MODE               (MS7210_DVIN_REG_BASE + 0x02)
#define REG_DVIN_SYNCIN_FLIP        (MS7210_DVIN_REG_BASE + 0x06)
#define REG_DVIN_HVDET              (MS7210_DVIN_REG_BASE + 0x07)
#define REG_DVIN_HVDET_RD0          (MS7210_DVIN_REG_BASE + 0x08)
#define REG_DVIN_HVDET_RD1          (MS7210_DVIN_REG_BASE + 0x09)
#define REG_DVIN_HVDET_RD2          (MS7210_DVIN_REG_BASE + 0x0A)
#define REG_DVIN_SYNCOUT_FLIP       (MS7210_DVIN_REG_BASE + 0x0B)
#define REG_DVIN_HT_L               (MS7210_DVIN_REG_BASE + 0x0C)
#define REG_DVIN_HT_H               (MS7210_DVIN_REG_BASE + 0x0D)
#define REG_DVIN_VT_L               (MS7210_DVIN_REG_BASE + 0x0E)
#define REG_DVIN_VT_H               (MS7210_DVIN_REG_BASE + 0x0F)
#define REG_DVIN_PXL_SHIFT_L        (MS7210_DVIN_REG_BASE + 0x10)
#define REG_DVIN_PXL_SHIFT_H        (MS7210_DVIN_REG_BASE + 0x11)
#define REG_DVIN_LN_SHIFT_L         (MS7210_DVIN_REG_BASE + 0x12)
#define REG_DVIN_LN_SHIFT_H         (MS7210_DVIN_REG_BASE + 0x13)
#define REG_DVIN_HSW_L              (MS7210_DVIN_REG_BASE + 0x14)
#define REG_DVIN_HSW_H              (MS7210_DVIN_REG_BASE + 0x15)
#define REG_DVIN_VSW_L              (MS7210_DVIN_REG_BASE + 0x16)
#define REG_DVIN_VSW_H              (MS7210_DVIN_REG_BASE + 0x17)
#define REG_DVIN_HDE_ST_L           (MS7210_DVIN_REG_BASE + 0x18)
#define REG_DVIN_HDE_ST_H           (MS7210_DVIN_REG_BASE + 0x19)
#define REG_DVIN_HDE_SP_L           (MS7210_DVIN_REG_BASE + 0x1A)
#define REG_DVIN_HDE_SP_H           (MS7210_DVIN_REG_BASE + 0x1B)
#define REG_DVIN_VDE_OST_L          (MS7210_DVIN_REG_BASE + 0x1C)
#define REG_DVIN_VDE_OST_H          (MS7210_DVIN_REG_BASE + 0x1D)
#define REG_DVIN_VDE_OSP_L          (MS7210_DVIN_REG_BASE + 0x1E)
#define REG_DVIN_VDE_OSP_H          (MS7210_DVIN_REG_BASE + 0x1F)
#define REG_DVIN_VDE_EST_L          (MS7210_DVIN_REG_BASE + 0x20)
#define REG_DVIN_VDE_EST_H          (MS7210_DVIN_REG_BASE + 0x21)
#define REG_DVIN_VDE_ESP_L          (MS7210_DVIN_REG_BASE + 0x22)
#define REG_DVIN_VDE_ESP_H          (MS7210_DVIN_REG_BASE + 0x23)
#define REG_DVIN_RD_TRIG            (MS7210_DVIN_REG_BASE + 0x37)
#define REG_DVIN_BT1004             (MS7210_DVIN_REG_BASE + 0x62)
#define REG_DVIN_12B444             (MS7210_DVIN_REG_BASE + 0x63)
#define REG_DVIN_IO_MAP             (MS7210_DVIN_REG_BASE + 0x64)
#define REG_DVIN_DEONLY             (MS7210_DVIN_REG_BASE + 0x65)

/* PA register address map */
#define MS7210_PA_REG_BASE          (0x1280)
#define REG_PA_S                    (MS7210_PA_REG_BASE + 0x01)


BOOL ms7210drv_dvin_mode_config(DVIN_CONFIG_T *t_dvin_config)
{
    UINT8 u8_clk_ratio = 0;

    // dvin_mode_sel
    switch (t_dvin_config->u8_cs_mode)
    {
    case DVIN_CS_MODE_RGB:
    case DVIN_CS_MODE_YUV444:
        HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x00);
        t_dvin_config->u8_sq_mode = DVIN_SQ_MODE_SEQ;
        break;
    case DVIN_CS_MODE_YUV422:
        switch (t_dvin_config->u8_bw_mode)
        {
        case DVIN_BW_MODE_16_20_24BIT:
            //HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x01);   //16
            //HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x02);   //20
            HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x03);   //24
            break;
        case DVIN_BW_MODE_8_10_12BIT:
            //HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x04);   //8
            //HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x05);   //10
            HAL_ModBits(REG_DVIN_MODE, MSRT_BITS2_0, 0x06);   //12
            break;
        }
        break;
    }

    // dvin_data_sel
    HAL_ToggleBits(REG_DVIN_SWAP, MSRT_BIT4, !t_dvin_config->u8_sq_mode);

    // dvin_ddr_mode
    switch (t_dvin_config->u8_dr_mode)
    {
    case DVIN_DR_MODE_SDR:
        // sdr mode
        HAL_ClrBits(REG_DVIN_MODE, MSRT_BIT3);
        // clk div2
        u8_clk_ratio = (t_dvin_config->u8_bw_mode == DVIN_BW_MODE_8_10_12BIT) ? 2 : 0;
        break;
    case DVIN_DR_MODE_DDR:
        if ((t_dvin_config->u8_cs_mode != DVIN_CS_MODE_YUV422) && (t_dvin_config->u8_bw_mode == DVIN_BW_MODE_8_10_12BIT))
        {
            // dvin_12b444
            HAL_SetBits(REG_DVIN_12B444, MSRT_BIT0);
        }
        else
        {
            // ddr mode
            HAL_SetBits(REG_DVIN_MODE, MSRT_BIT3);
            if ((t_dvin_config->u8_cs_mode == DVIN_CS_MODE_YUV422) && (t_dvin_config->u8_bw_mode == DVIN_BW_MODE_16_20_24BIT))
            {
                // dvin_ddr_edge_inv
                HAL_SetBits(REG_DVIN_MODE, MSRT_BIT4);
            }
        }
        // clk x2
        u8_clk_ratio = (t_dvin_config->u8_bw_mode == DVIN_BW_MODE_8_10_12BIT) ? 0 : 1;
        break;
    }
    HAL_ModBits(REG_MISC_DIG_CLK_SEL, MSRT_BITS1_0, u8_clk_ratio);

    // dvin_sync_fmt
    // dvin_ccir_cut_en, dvin_bt1120_mode, dvin_bt656_mode
    switch (t_dvin_config->u8_sy_mode)
    {
    case DVIN_SY_MODE_HSVSDE:
        HAL_ModBits(REG_DVIN_SRC_SEL, MSRT_BITS3_2, 0x04);
        HAL_ModBits(REG_DVIN_MODE, MSRT_BITS7_5, 0x00);
        break;
    case DVIN_SY_MODE_HSVS:
        HAL_ModBits(REG_DVIN_SRC_SEL, MSRT_BITS3_2, 0x00);
        HAL_ModBits(REG_DVIN_MODE, MSRT_BITS7_5, 0x00);
        break;
    case DVIN_SY_MODE_VSDE:
    case DVIN_SY_MODE_DEONLY:
        HAL_ModBits(REG_DVIN_SRC_SEL, MSRT_BITS3_2, 0x08);
        HAL_ModBits(REG_DVIN_MODE, MSRT_BITS7_5, 0x00);
        break;
    case DVIN_SY_MODE_EMBEDDED:
    case DVIN_SY_MODE_BTAT1004:
        HAL_ModBits(REG_DVIN_SRC_SEL, MSRT_BITS3_2, 0x0c);
        HAL_ModBits(REG_DVIN_MODE, MSRT_BITS7_5, 0xa0);
        break;
    case DVIN_SY_MODE_2XEMBEDDED:
        HAL_ModBits(REG_DVIN_SRC_SEL, MSRT_BITS3_2, 0x0c);
        HAL_ModBits(REG_DVIN_MODE, MSRT_BITS7_5, ((t_dvin_config->u8_bw_mode == DVIN_BW_MODE_8_10_12BIT) &&
            (t_dvin_config->u8_dr_mode == DVIN_DR_MODE_DDR) && (t_dvin_config->u8_sy_mode == DVIN_SY_MODE_2XEMBEDDED)) ? 0xa0 : 0xe0);
        break;
    }
    // r_de_only_mode
    HAL_ToggleBits(REG_DVIN_DEONLY, MSRT_BIT0, t_dvin_config->u8_sy_mode == DVIN_SY_MODE_DEONLY);
    // dvin_bt_1004_mode
    HAL_ToggleBits(REG_DVIN_BT1004, MSRT_BIT0, (t_dvin_config->u8_sy_mode == DVIN_SY_MODE_BTAT1004) ||
        ((t_dvin_config->u8_cs_mode == DVIN_CS_MODE_YUV422) && (t_dvin_config->u8_bw_mode == DVIN_BW_MODE_8_10_12BIT)
        && (t_dvin_config->u8_dr_mode == DVIN_DR_MODE_DDR) && (t_dvin_config->u8_sy_mode == DVIN_SY_MODE_EMBEDDED)));
    // dvin_deregen_en
    HAL_ToggleBits(REG_DVIN_SYNCOUT_FLIP, MSRT_BIT0, t_dvin_config->u8_sy_mode != DVIN_SY_MODE_HSVSDE);

    return (u8_clk_ratio == 1);
}

typedef enum _E_DVIN_TM_DET_
{
    DVIN_TM_DET_HTOTAL,
    DVIN_TM_DET_VTOTAL,
    DVIN_TM_DET_HACTIVE,
    DVIN_TM_DET_HSTART,
    DVIN_TM_DET_VSTART
}DVIN_TM_DET_E;

UINT32 _drv_dvin_timing_detect(DVIN_TM_DET_E e_det_index)
{
    HAL_WriteByte(REG_DVIN_HVDET, ((UINT8)e_det_index << 4) | MSRT_BIT0);
    HAL_WriteByte(REG_DVIN_RD_TRIG, MSRT_BIT0);
    return HAL_ReadWord(REG_DVIN_HVDET_RD0) | ((UINT32)HAL_ReadByte(REG_DVIN_HVDET_RD2) << 16);
}

VOID ms7210drv_dvin_timing_detect(DVIN_TIMING_DET_T *t_dvin_det)
{
    t_dvin_det->u16_htotal = _drv_dvin_timing_detect(DVIN_TM_DET_HTOTAL);
    if (t_dvin_det->u16_htotal)
    {
        t_dvin_det->u16_vtotal = _drv_dvin_timing_detect(DVIN_TM_DET_VTOTAL) / t_dvin_det->u16_htotal;
    }
    t_dvin_det->u16_hactive = _drv_dvin_timing_detect(DVIN_TM_DET_HACTIVE);
}

BOOL ms7210drv_dvin_timing_config(DVIN_CONFIG_T *t_dvin_config, VIDEOTIMING_T *t_output_timing, UINT8 *u8_rpt)
{
    BOOL b_need_x2_clk = FALSE;
    VIDEOTIMING_T st_output_timing;
    UINT16 u16_pixel_shift = 3, u16_line_shift = 2;

    if (HAL_ReadByte(REG_MISC_DIG_CLK_SEL) & MSRT_BIT0)
    {
        b_need_x2_clk = TRUE;
        t_output_timing->u16_htotal /= *u8_rpt + 1;
        t_output_timing->u16_hsyncwidth /= *u8_rpt + 1;
        t_output_timing->u16_hoffset /= *u8_rpt + 1;
        t_output_timing->u16_hactive /= *u8_rpt + 1;
        t_output_timing->u16_pixclk /= *u8_rpt + 1;
        *u8_rpt = 0;
    }
    st_output_timing = *t_output_timing;

    // timing regeneration
    if (*u8_rpt)
    {
        st_output_timing.u16_htotal /= *u8_rpt + 1;
        st_output_timing.u16_hsyncwidth /= *u8_rpt + 1;
        st_output_timing.u16_hoffset /= *u8_rpt + 1;
        st_output_timing.u16_hactive /= *u8_rpt + 1;
        st_output_timing.u16_pixclk /= *u8_rpt + 1;
        *u8_rpt = 1;
        b_need_x2_clk = TRUE;
    }
    if ((t_dvin_config->u8_bw_mode == DVIN_BW_MODE_8_10_12BIT) && (t_dvin_config->u8_dr_mode == DVIN_DR_MODE_SDR))
    {
        st_output_timing.u16_htotal *= 2;
        st_output_timing.u16_hsyncwidth *= 2;
        st_output_timing.u16_hoffset *= 2;
        st_output_timing.u16_hactive *= 2;
    }

    switch (t_dvin_config->u8_sy_mode)
    {
    case DVIN_SY_MODE_HSVS:
        break;
    case DVIN_SY_MODE_HSVSDE:
    case DVIN_SY_MODE_VSDE:
        u16_pixel_shift += st_output_timing.u16_hoffset;
        break;
    case DVIN_SY_MODE_DEONLY:
        u16_pixel_shift += st_output_timing.u16_hoffset;
        u16_line_shift += st_output_timing.u16_voffset;
        break;
    case DVIN_SY_MODE_EMBEDDED:
        u16_pixel_shift += st_output_timing.u16_hoffset + st_output_timing.u16_hactive;
        u16_line_shift += st_output_timing.u16_voffset + st_output_timing.u16_vactive / ((st_output_timing.u8_polarity & 0x01) ? 1 : 2);
        break;
    case DVIN_SY_MODE_2XEMBEDDED:
        u16_pixel_shift += st_output_timing.u16_hoffset + st_output_timing.u16_hactive + 4;
        u16_line_shift += st_output_timing.u16_voffset + st_output_timing.u16_vactive / ((st_output_timing.u8_polarity & 0x01) ? 1 : 2);
        break;
    case DVIN_SY_MODE_BTAT1004:
        u16_pixel_shift += st_output_timing.u16_hoffset + st_output_timing.u16_hactive - 2;
        u16_line_shift += st_output_timing.u16_voffset + st_output_timing.u16_vactive / ((st_output_timing.u8_polarity & 0x01) ? 1 : 2);
        break;
    }
    HAL_WriteWord(REG_DVIN_HT_L, st_output_timing.u16_htotal);
    HAL_WriteWord(REG_DVIN_VT_L, st_output_timing.u16_vtotal);
    HAL_WriteWord(REG_DVIN_PXL_SHIFT_L, u16_pixel_shift);
    HAL_WriteWord(REG_DVIN_LN_SHIFT_L, u16_line_shift);
    HAL_WriteWord(REG_DVIN_HSW_L, st_output_timing.u16_hsyncwidth);
    HAL_WriteWord(REG_DVIN_VSW_L, st_output_timing.u16_vsyncwidth);
    HAL_WriteWord(REG_DVIN_HDE_ST_L, st_output_timing.u16_hoffset);
    HAL_WriteWord(REG_DVIN_HDE_SP_L, st_output_timing.u16_hoffset + st_output_timing.u16_hactive);
    HAL_WriteWord(REG_DVIN_VDE_OST_L, st_output_timing.u16_voffset);
    HAL_WriteWord(REG_DVIN_VDE_OSP_L, st_output_timing.u16_voffset + st_output_timing.u16_vactive / ((st_output_timing.u8_polarity & 0x01) ? 1 : 2));
    HAL_WriteWord(REG_DVIN_VDE_EST_L, st_output_timing.u16_voffset + st_output_timing.u16_vtotal / 2 + 1);
    HAL_WriteWord(REG_DVIN_VDE_ESP_L, st_output_timing.u16_voffset + st_output_timing.u16_vtotal / 2 + st_output_timing.u16_vactive / 2 + 1);
    HAL_ToggleBits(REG_DVIN_SYNCIN_FLIP, MSRT_BIT0, (~st_output_timing.u8_polarity) & MSRT_BIT1);
    HAL_ToggleBits(REG_DVIN_SYNCIN_FLIP, MSRT_BIT1, (~st_output_timing.u8_polarity) & MSRT_BIT2);
    HAL_ToggleBits(REG_DVIN_SYNCOUT_FLIP, MSRT_BIT4, (~st_output_timing.u8_polarity) & MSRT_BIT1);
    HAL_ToggleBits(REG_DVIN_SYNCOUT_FLIP, MSRT_BIT5, (~st_output_timing.u8_polarity) & MSRT_BIT2);
    HAL_ToggleBits(REG_DVIN_SYNCOUT_FLIP, MSRT_BIT2, st_output_timing.u8_polarity & MSRT_BIT0);

    return b_need_x2_clk;
}

VOID ms7210drv_dvin_data_swap_all(VOID)
{
    UINT8 u8_swap = HAL_ReadByte(REG_DVIN_SWAP) & MSRT_BIT7;
    HAL_ToggleBits(REG_DVIN_SWAP, MSRT_BIT7, !u8_swap);
}

VOID ms7210drv_dvin_data_swap_rb_channel(VOID)
{
    UINT8 u8_swap = HAL_ReadByte(REG_DVIN_SWAP) & MSRT_BIT4;
    HAL_ToggleBits(REG_DVIN_SWAP, MSRT_BIT4, !u8_swap);
}

VOID ms7210drv_dvin_data_swap_yc_channel(VOID)
{
    UINT8 u8_swap = HAL_ReadByte(REG_DVIN_IO_MAP) & MSRT_BIT1;
    HAL_ToggleBits(REG_DVIN_IO_MAP, MSRT_BIT1, !u8_swap);
}

VOID ms7210drv_dvin_pa_adjust(BOOL b_invert, UINT8 u8_delay)
{
    HAL_ModBits(REG_PA_S, MSRT_BITS2_0, ((UINT8)b_invert << 2) | (u8_delay % 4));
}

VOID ms7210drv_dvin_clk_reset_release(BOOL b_release)
{
    if (b_release)
    {
        HAL_SetBits(REG_MISC_DVIN_ENABLE, MSRT_BIT2);
        HAL_SetBits(REG_MISC_DVIN_RESET, MSRT_BIT0);
    }
    else
    {
        HAL_ClrBits(REG_MISC_DVIN_RESET, MSRT_BIT0);
        HAL_ClrBits(REG_MISC_DVIN_ENABLE, MSRT_BIT2);
    }
}

#if 0
VOID ms7210drv_dvout_clk_sel(UINT8 u8_clk_rpt, UINT16 u16_video_clk)
{
    HAL_ModBits(REG_DIG_CLK_SEL0, MSRT_BITS7_4, u8_clk_rpt << 4);
    HAL_ModBits(REG_DIG_CLK_SEL1, MSRT_BITS3_0, ((u16_video_clk <= 5000) ? 2 : 1) * (u8_clk_rpt + 1) - 1);
}
#endif
