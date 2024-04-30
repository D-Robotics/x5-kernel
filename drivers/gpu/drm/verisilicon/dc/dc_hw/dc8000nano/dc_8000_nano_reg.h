/****************************************************************************
 *
 *    Copyright (c) 2005 - 2022 by Vivante Corp.  All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Vivante Corporation. This is proprietary information owned by
 *    Vivante Corporation. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Vivante Corporation.
 *
 *****************************************************************************/

#ifndef __gcregDisplay_h__
#define __gcregDisplay_h__

/*******************************************************************************
**                          ~~~~~~~~~~~~~~~~~~~~~~~~                          **
**                          Module DisplayController                          **
**                          ~~~~~~~~~~~~~~~~~~~~~~~~                          **
*******************************************************************************/

/* Register gcregDcChipRev **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Chip Revision Register.  Shows the revision for the chip in BCD.  This     **
** register has no set reset  value. It varies with the implementation. READ  **
** ONLY.                                                                      */

#define GCREG_DC_CHIP_REV_Address 0x02000

/* Revision. The reset value for this field is subject to change and may be   **
** ignored during test.                                                       */
#define GCREG_DC_CHIP_REV_REV 31 : 0

/* Register gcregDcChipDate **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Chip Date Register.  Shows the release date for the IP in YYYYMMDD (year,  **
** month. day) format.  This register has no set reset value.  It varies with **
** the implementation. READ ONLY.                                             */

#define GCREG_DC_CHIP_DATE_Address 0x02004

/* Date. The reset value for this field changes per release and may be        **
** ignored during test. You can read the accurate value from the RTL file     **
** because the actual value may be later than the document release time.      */
#define GCREG_DC_CHIP_DATE_DATE 31 : 0

/* Register gcregDcChipPatchRev **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Patch Revision Register.  Patch revision level for the chip. READ ONLY. */

#define GCREG_DC_CHIP_PATCH_REV_Address 0x02008

/* Patch revision.  The reset value for this field changes per release and    **
** may be ignored during test. You can read the accurate value from the RTL   **
** file because the actual value may be later than the document release time. */
#define GCREG_DC_CHIP_PATCH_REV_PATCH_REV 31 : 0

/* Register gcregDcProductId **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Product Identification Register. READ ONLY.  */

#define GCREG_DC_PRODUCT_ID_Address 0x0200C

/* Product ID.  Bits 3:0 contain Product GRADE_LEVEL. 1 indicates level is    **
** Nano.  Bits 23:4 contain Product NUM, product number. None for DCNano;     **
** 8000 for DC8000 Series.  Bits 27:24 contain Product TYPE. 2 indicates      **
** Display Controller.                                                        */
#define GCREG_DC_PRODUCT_ID_PRODUCT_ID 31 : 0

/* Register gcregDcCustomerId **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Shows the ID for the customer. READ ONLY. */

#define GCREG_DC_CUSTOMER_ID_Address 0x02010

/* Customer Id. */
#define GCREG_DC_CUSTOMER_ID_ID 31 : 0

/* Register gcregDcChipId **
** ~~~~~~~~~~~~~~~~~~~~~~ */

/* Shows the ID for the chip in BCD.  This register has no set reset value.   **
** It varies with the implementation. READ_ONLY.                              */

#define GCREG_DC_CHIP_ID_Address 0x02014

/* Id. */
#define GCREG_DC_CHIP_ID_ID 31 : 0

/* Register gcregDcChipTime **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Shows the release time for the IP in HHMMSS00 (hour,  minutes, second, 00) **
** format.  This register  has no set reset value. It varies with the         **
** implementation.                                                            */

#define GCREG_DC_CHIP_TIME_Address 0x02018

/* Time. */
#define GCREG_DC_CHIP_TIME_TIME 31 : 0

/* Register gcregDcChipInfo **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Chip information */

#define GCREG_DC_CHIP_INFO_Address 0x0201C

/* Chip information */
#define GCREG_DC_CHIP_INFO_CHIP_INFO 31 : 0

/* Register gcregDcEcoId **
** ~~~~~~~~~~~~~~~~~~~~~ */

/* Shows the ID for the chip ECO. READ ONLY. */

#define GCREG_DC_ECO_ID_Address 0x02020

/* ECO Id. */
#define GCREG_DC_ECO_ID_ID 31 : 0

/* Register gcregDcReservedId (6 in total) **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DC_RESERVED_ID_Address 0x02040

#define GCREG_DC_RESERVED_ID_ID 31 : 0

/* Register gcregFrameBufferConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Frame Buffer Configuration Register.  Frame Buffer and Timing control.     **
** NOTE: This register is double buffered.                                    */

#define GCREG_FRAME_BUFFER_CONFIG_Address 0x02024

/* The format of the frame buffer.  None means there is no  frame buffer and  **
** the display controller will not produce  any output. NOTE: This field is   **
** double buffered.                                                           */
#define GCREG_FRAME_BUFFER_CONFIG_FORMAT	  2 : 0
#define GCREG_FRAME_BUFFER_CONFIG_FORMAT_NONE	  0x0
#define GCREG_FRAME_BUFFER_CONFIG_FORMAT_A4R4G4B4 0x1
#define GCREG_FRAME_BUFFER_CONFIG_FORMAT_A1R5G5B5 0x2
#define GCREG_FRAME_BUFFER_CONFIG_FORMAT_R5G6B5	  0x3
#define GCREG_FRAME_BUFFER_CONFIG_FORMAT_A8R8G8B8 0x4

/* Enable this layer or disable this layer. NOTE: This field is double        **
** buffered.                                                                  */
#define GCREG_FRAME_BUFFER_CONFIG_ENABLE	  3 : 3
#define GCREG_FRAME_BUFFER_CONFIG_ENABLE_DISABLED 0x0
#define GCREG_FRAME_BUFFER_CONFIG_ENABLE_ENABLED  0x1

/* Enable or disable clear. NOTE: This field is double buffered. */
#define GCREG_FRAME_BUFFER_CONFIG_CLEAR_EN	    5 : 5
#define GCREG_FRAME_BUFFER_CONFIG_CLEAR_EN_DISABLED 0x0
#define GCREG_FRAME_BUFFER_CONFIG_CLEAR_EN_ENABLED  0x1

/* When SwitchPanel of panel 0 is enabled, output channel 0 will show the     **
** image of panel 1,not panel 0's.                                            **
** When SwitchPanel of panel 0 is disabeld,output channel 0 will show its     **
** original image which is from panel 0.                                      **
** When SwitchPanel of panel 1 is enabled, output channel 1 will show the     **
** image of panel 0,not panel 1's.                                            **
** When SwitchPanle of panel 1 is disabled,output channel 1 will show the     **
** original image which is from panel 1.                                      **
**                                                                            **
**                            disabled(normal)            enabled(switch)     **
** output channel 0            panel 0                            panel 1     **
** output channel 1            panel 1                            panel 0     **
** NOTE: This field is double buffered.                                       */
#define GCREG_FRAME_BUFFER_CONFIG_SWITCHPANEL	       9 : 9
#define GCREG_FRAME_BUFFER_CONFIG_SWITCHPANEL_DISABLED 0x0
#define GCREG_FRAME_BUFFER_CONFIG_SWITCHPANEL_ENABLED  0x1

/* Enable or disable color key. NOTE: This field is double buffered. */
#define GCREG_FRAME_BUFFER_CONFIG_COLOR_KEY_EN		10 : 10
#define GCREG_FRAME_BUFFER_CONFIG_COLOR_KEY_EN_DISABLED 0x0
#define GCREG_FRAME_BUFFER_CONFIG_COLOR_KEY_EN_ENABLED	0x1

/* NOTE: This field is double buffered. */
#define GCREG_FRAME_BUFFER_CONFIG_SWIZZLE      18 : 17
#define GCREG_FRAME_BUFFER_CONFIG_SWIZZLE_ARGB 0x0
#define GCREG_FRAME_BUFFER_CONFIG_SWIZZLE_RGBA 0x1
#define GCREG_FRAME_BUFFER_CONFIG_SWIZZLE_ABGR 0x2
#define GCREG_FRAME_BUFFER_CONFIG_SWIZZLE_BGRA 0x3

/* NOTE: This field is double buffered. */
#define GCREG_FRAME_BUFFER_CONFIG_UV_SWIZZLE 19 : 19

/* Register gcregFrameBufferAddress **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Frame Buffer Base Address Register.  Starting address of the frame buffer. **
**  NOTE: This register is double buffered.                                   */

#define GCREG_FRAME_BUFFER_ADDRESS_Address 0x02028

/* 32 bit address. */
#define GCREG_FRAME_BUFFER_ADDRESS_ADDRESS 31 : 0

/* Register gcregFrameBufferStride **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Frame Buffer Stride Register.  Stride of the frame buffer in bytes.  NOTE: **
** This register is double buffered.                                          */

#define GCREG_FRAME_BUFFER_STRIDE_Address 0x0202C

/* Number of bytes from start of one line to next line.  NOTE: This field is  **
** double buffered.                                                           */
#define GCREG_FRAME_BUFFER_STRIDE_STRIDE 16 : 0

/* Register gcregFrameBufferOrigin **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Frame Buffer Pixel Origin Register.  Pixel origin inside the frame buffer  **
** for the top-left panel  pixel. This register should be set to 0.  NOTE:    **
** This register is double buffered.                                          */

#define GCREG_FRAME_BUFFER_ORIGIN_Address 0x02030

/* X origin of frame buffer for panning.  NOTE: This field is double          **
** buffered.                                                                  */
#define GCREG_FRAME_BUFFER_ORIGIN_X 10 : 0

/* Y origin of frame buffer for panning.  NOTE: This field is double          **
** buffered.                                                                  */
#define GCREG_FRAME_BUFFER_ORIGIN_Y 26 : 16

/* Register gcregDcTileInCfg **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Tile Input Configuration Register.  Tile input configuration.  NOTE: This  **
** register is double buffered.                                               */

#define GCREG_DC_TILE_IN_CFG_Address 0x02034

/* Tile input data format 0 means non-tile input.  NOTE: This field is double **
** buffered.                                                                  */
#define GCREG_DC_TILE_IN_CFG_TILE_FORMAT	  1 : 0
#define GCREG_DC_TILE_IN_CFG_TILE_FORMAT_NONE	  0x0
#define GCREG_DC_TILE_IN_CFG_TILE_FORMAT_ARGB8888 0x1
#define GCREG_DC_TILE_IN_CFG_TILE_FORMAT_YUY2	  0x2
#define GCREG_DC_TILE_IN_CFG_TILE_FORMAT_NV12	  0x3

/* YUV standard select.  NOTE: This field is double buffered.  */
#define GCREG_DC_TILE_IN_CFG_YUV_STANDARD	2 : 2
#define GCREG_DC_TILE_IN_CFG_YUV_STANDARD_BT601 0x0
#define GCREG_DC_TILE_IN_CFG_YUV_STANDARD_BT709 0x1

/* Register gcregDcTileUvFrameBufferAdr **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Frame Buffer Tiled UV Base Address Register.  UV frame buffer address when **
** tile input.  NOTE: This register is double buffered.                       */

#define GCREG_DC_TILE_UV_FRAME_BUFFER_ADR_Address 0x02038

/* UV frame buffer address when tile input NOTE: This field is double         **
** buffered.                                                                  */
#define GCREG_DC_TILE_UV_FRAME_BUFFER_ADR_ADDRESS 31 : 0

/* Register gcregDcTileUvFrameBufferStr **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Frame Buffer Tiled UV Stride Register.  UV frame buffer stride when tile   **
** input  NOTE: This register is double buffered.                             */

#define GCREG_DC_TILE_UV_FRAME_BUFFER_STR_Address 0x0203C

/* UV frame buffer stride when tile input NOTE: This field is double          **
** buffered.                                                                  */
#define GCREG_DC_TILE_UV_FRAME_BUFFER_STR_STRIDE 15 : 0

/* Register gcregFrameBufferBackground **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Framebuffer background color.  NOTE: This register is double buffered.  */

#define GCREG_FRAME_BUFFER_BACKGROUND_Address 0x02058

/* NOTE: This field is double buffered.  */
#define GCREG_FRAME_BUFFER_BACKGROUND_BLUE 7 : 0

/* NOTE: This field is double buffered.  */
#define GCREG_FRAME_BUFFER_BACKGROUND_GREEN 15 : 8

/* NOTE: This field is double buffered.  */
#define GCREG_FRAME_BUFFER_BACKGROUND_RED 23 : 16

/* NOTE: This field is double buffered.  */
#define GCREG_FRAME_BUFFER_BACKGROUND_ALPHA 31 : 24

/* Register gcregFrameBufferColorKey **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Framebuffer Color Key Start Address Register.  Start of color key range of **
** framebuffer.  NOTE: This register is double buffered.                      */

#define GCREG_FRAME_BUFFER_COLOR_KEY_Address 0x0205C

/* 8 bit blue color. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_ALPHA 31 : 24

/* Register gcregFrameBufferColorKeyHigh **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Framebuffer Color Key End Address Register.  End of color key range of     **
** framebuffer.  NOTE: This register is double buffered.                      */

#define GCREG_FRAME_BUFFER_COLOR_KEY_HIGH_Address 0x02060

/* 8 bit blue color. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_HIGH_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_HIGH_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_HIGH_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_FRAME_BUFFER_COLOR_KEY_HIGH_ALPHA 31 : 24

/* Register gcregFrameBufferClearValue **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Framebuffer Clear Value Register.  Clear value used when                   **
** dcregFrameBufferConfig.Clear  is enabled, format is A8R8G8B8.  NOTE: This  **
** register is double buffered.                                               */

#define GCREG_FRAME_BUFFER_CLEAR_VALUE_Address 0x02064

/* 8 bit blue color. */
#define GCREG_FRAME_BUFFER_CLEAR_VALUE_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_FRAME_BUFFER_CLEAR_VALUE_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_FRAME_BUFFER_CLEAR_VALUE_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_FRAME_BUFFER_CLEAR_VALUE_ALPHA 31 : 24

/* Register gcregVideoTL **
** ~~~~~~~~~~~~~~~~~~~~~ */

/* Top left coordinate of panel pixel where the video should start.  Be aware **
** there is no panning inside the video. NOTE: This register is double        **
** buffered.                                                                  */

#define GCREG_VIDEO_TL_Address 0x02068

/* Left boundary of video window. NOTE: This field is double buffered.  */
#define GCREG_VIDEO_TL_X 11 : 0

/* Top boundary of video window. NOTE: This field is double buffered.  */
#define GCREG_VIDEO_TL_Y 27 : 16

/* Register gcregFrameBufferSize **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* video size information. NOTE: This register is double buffered. */

#define GCREG_FRAME_BUFFER_SIZE_Address 0x0206C

/* video width. NOTE: This field is double buffered.  */
#define GCREG_FRAME_BUFFER_SIZE_WIDTH 11 : 0

/* video height. NOTE: This field is double buffered.  */
#define GCREG_FRAME_BUFFER_SIZE_HEIGHT 27 : 16

/* Register gcregVideoGlobalAlpha **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Global alpha for the video.  NOTE: This register is double buffered.  */

#define GCREG_VIDEO_GLOBAL_ALPHA_Address 0x02070

/* Source alpha for video. NOTE: This field is double buffered.  */
#define GCREG_VIDEO_GLOBAL_ALPHA_SRC_ALPHA 7 : 0

/* Destination alpha for video.  NOTE: This field is double buffered.  */
#define GCREG_VIDEO_GLOBAL_ALPHA_DST_ALPHA 15 : 8

/* Register gcregBlendStackOrder **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Set the video, overlay0, overlay1 order for blend. The 1st is the lowest   **
** layer, the 2nd is the middle layer, the 3rd is the highest layer. NOTE:    **
** This register is double buffered.                                          */

#define GCREG_BLEND_STACK_ORDER_Address 0x02074

/* NOTE: This field is double buffered.  */
#define GCREG_BLEND_STACK_ORDER_ORDER			      2 : 0
#define GCREG_BLEND_STACK_ORDER_ORDER_VIDEO_OVERLAY0_OVERLAY1 0x0
#define GCREG_BLEND_STACK_ORDER_ORDER_VIDEO_OVERLAY1_OVERLAY0 0x1
#define GCREG_BLEND_STACK_ORDER_ORDER_OVERLAY0_VIDEO_OVERLAY1 0x2
#define GCREG_BLEND_STACK_ORDER_ORDER_OVERLAY0_OVERLAY1_VIDEO 0x3
#define GCREG_BLEND_STACK_ORDER_ORDER_OVERLAY1_VIDEO_OVERLAY0 0x4
#define GCREG_BLEND_STACK_ORDER_ORDER_OVERLAY1_OVERLAY0_VIDEO 0x5

/* Register gcregVideoAlphaBlendConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Alpha Blending Configuration Register.  NOTE: This register is double      **
** buffered.                                                                  */

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_Address 0x02078

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_ALPHA_BLEND	    0 : 0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_ALPHA_BLEND_DISABLED 0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_ALPHA_BLEND_ENABLED  0x1

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_ALPHA_MODE	       1 : 1
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_ALPHA_MODE_NORMAL   0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_ALPHA_MODE_INVERSED 0x1

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE	    4 : 3
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE_NORMAL 0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE_GLOBAL 0x1
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE_SCALED 0x2

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE	  7 : 6
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_ZERO	  0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_ONE	  0x1
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_NORMAL	  0x2
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_INVERSED 0x3

/* Src Blending factor is calculated from Src alpha.  */
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_ALPHA_FACTOR		 8 : 8
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_ALPHA_FACTOR_DISABLED 0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_SRC_ALPHA_FACTOR_ENABLED	 0x1

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_ALPHA_MODE	       9 : 9
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_ALPHA_MODE_NORMAL   0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_ALPHA_MODE_INVERSED 0x1

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE	    11 : 10
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE_NORMAL 0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE_GLOBAL 0x1
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE_SCALED 0x2

#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE	  14 : 13
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_ZERO	  0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_ONE	  0x1
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_NORMAL	  0x2
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_INVERSED 0x3

/* Destination blending factor is calculated from Dst alpha.  */
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_ALPHA_FACTOR		 15 : 15
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_ALPHA_FACTOR_DISABLED 0x0
#define GCREG_VIDEO_ALPHA_BLEND_CONFIG_DST_ALPHA_FACTOR_ENABLED	 0x1

/* Register gcregOverlayConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Configuration Register.  Overlay control.  NOTE: This register is  **
** double buffered.                                                           */

#define GCREG_OVERLAY_CONFIG_Address 0x0207C

/* The format of the overlay. None means there is no  overlay. NOTE: This     **
** field is double buffered.                                                  */
#define GCREG_OVERLAY_CONFIG_FORMAT	     2 : 0
#define GCREG_OVERLAY_CONFIG_FORMAT_NONE     0x0
#define GCREG_OVERLAY_CONFIG_FORMAT_A4R4G4B4 0x1
#define GCREG_OVERLAY_CONFIG_FORMAT_A1R5G5B5 0x2
#define GCREG_OVERLAY_CONFIG_FORMAT_R5G6B5   0x3
#define GCREG_OVERLAY_CONFIG_FORMAT_A8R8G8B8 0x4

/* Enable this overlay layer.  NOTE: This field is double buffered.  */
#define GCREG_OVERLAY_CONFIG_ENABLE	    3 : 3
#define GCREG_OVERLAY_CONFIG_ENABLE_DISABLE 0x0
#define GCREG_OVERLAY_CONFIG_ENABLE_ENABLE  0x1

/* Enable or disable clear. NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG_CLEAR_EN	       5 : 5
#define GCREG_OVERLAY_CONFIG_CLEAR_EN_DISABLED 0x0
#define GCREG_OVERLAY_CONFIG_CLEAR_EN_ENABLED  0x1

/* NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG_SWIZZLE	  18 : 17
#define GCREG_OVERLAY_CONFIG_SWIZZLE_ARGB 0x0
#define GCREG_OVERLAY_CONFIG_SWIZZLE_RGBA 0x1
#define GCREG_OVERLAY_CONFIG_SWIZZLE_ABGR 0x2
#define GCREG_OVERLAY_CONFIG_SWIZZLE_BGRA 0x3

/* NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG_UV_SWIZZLE 19 : 19

/* Enable or disable color key. NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG_COLOR_KEY_EN	   20 : 20
#define GCREG_OVERLAY_CONFIG_COLOR_KEY_EN_DISABLED 0x0
#define GCREG_OVERLAY_CONFIG_COLOR_KEY_EN_ENABLED  0x1

/* Register gcregOverlayAddress **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Starting address of the overlay. NOTE: This register is double buffered. */

#define GCREG_OVERLAY_ADDRESS_Address 0x02080

/* 32 bit address. */
#define GCREG_OVERLAY_ADDRESS_ADDRESS 31 : 0

/* Register gcregOverlayStride **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Stride of the overlay in bytes. NOTE: This register is double buffered. */

#define GCREG_OVERLAY_STRIDE_Address 0x02084

/* Number of bytes from start of one line to next line. */
#define GCREG_OVERLAY_STRIDE_STRIDE 16 : 0

/* Register gcregDcOverlayTileInCfg **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Tile Input Configuration Register.  Tile input configuration.  NOTE: This  **
** register is double buffered.                                               */

#define GCREG_DC_OVERLAY_TILE_IN_CFG_Address 0x02088

/* Tile input data format 0 means non-tile input  NOTE: This field is double  **
** buffered.                                                                  */
#define GCREG_DC_OVERLAY_TILE_IN_CFG_TILE_FORMAT	  1 : 0
#define GCREG_DC_OVERLAY_TILE_IN_CFG_TILE_FORMAT_NONE	  0x0
#define GCREG_DC_OVERLAY_TILE_IN_CFG_TILE_FORMAT_ARGB8888 0x1
#define GCREG_DC_OVERLAY_TILE_IN_CFG_TILE_FORMAT_YUY2	  0x2
#define GCREG_DC_OVERLAY_TILE_IN_CFG_TILE_FORMAT_NV12	  0x3

/* YUV standard select. NOTE: This field is double buffered. */
#define GCREG_DC_OVERLAY_TILE_IN_CFG_YUV_STANDARD	2 : 2
#define GCREG_DC_OVERLAY_TILE_IN_CFG_YUV_STANDARD_BT601 0x0
#define GCREG_DC_OVERLAY_TILE_IN_CFG_YUV_STANDARD_BT709 0x1

/* Register gcregDcTileUvOverlayAdr **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Starting address of the overlay UV. NOTE: This register is double          **
** buffered.                                                                  */

#define GCREG_DC_TILE_UV_OVERLAY_ADR_Address 0x0208C

/* 32 bit address. */
#define GCREG_DC_TILE_UV_OVERLAY_ADR_ADDRESS 31 : 0

/* Register gcregDcTileUvOverlayStr **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Stride of the overlay UV in bytes. NOTE: This register is double buffered. */

#define GCREG_DC_TILE_UV_OVERLAY_STR_Address 0x02090

/* Number of bytes from start of one line to next line. NOTE: This field is   **
** double buffered.                                                           */
#define GCREG_DC_TILE_UV_OVERLAY_STR_STRIDE 15 : 0

/* Register gcregOverlayTL **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Top left coordinate of panel pixel where the overlay should start.  Be     **
** aware there is no panning inside the overlay. NOTE: This register is       **
** double buffered.                                                           */

#define GCREG_OVERLAY_TL_Address 0x02094

/* Left boundary of overlay window. */
#define GCREG_OVERLAY_TL_X 11 : 0

/* Top boundary of overlay window. */
#define GCREG_OVERLAY_TL_Y 27 : 16

/* Register gcregOverlaySize **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* overlay size information. NOTE: This register is double buffered. */

#define GCREG_OVERLAY_SIZE_Address 0x02098

/* overlay width. */
#define GCREG_OVERLAY_SIZE_WIDTH 11 : 0

/* overlay height. */
#define GCREG_OVERLAY_SIZE_HEIGHT 27 : 16

/* Register gcregOverlayColorKey **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Color Key Start Address Register.  Start of color key range for    **
** overlay.  NOTE: This register is double buffered.                          */

#define GCREG_OVERLAY_COLOR_KEY_Address 0x0209C

/* 8 bit blue color. */
#define GCREG_OVERLAY_COLOR_KEY_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_OVERLAY_COLOR_KEY_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_OVERLAY_COLOR_KEY_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_OVERLAY_COLOR_KEY_ALPHA 31 : 24

/* Register gcregOverlayColorKeyHigh **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Color Key End Address Register.  End of color key range for        **
** overlay.  NOTE: This register is double buffered.                          */

#define GCREG_OVERLAY_COLOR_KEY_HIGH_Address 0x020A0

/* 8 bit blue color. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH_ALPHA 31 : 24

/* Register gcregOverlayAlphaBlendConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Alpha Blending Configuration Register.  NOTE: This register is double      **
** buffered.                                                                  */

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_Address 0x020A4

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_ALPHA_BLEND	      0 : 0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_ALPHA_BLEND_DISABLED 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_ALPHA_BLEND_ENABLED  0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_ALPHA_MODE		 1 : 1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_ALPHA_MODE_NORMAL	 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_ALPHA_MODE_INVERSED 0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE	      4 : 3
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE_NORMAL 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE_GLOBAL 0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_GLOBAL_ALPHA_MODE_SCALED 0x2

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE	    7 : 6
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_ZERO	    0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_ONE	    0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_NORMAL   0x2
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_BLENDING_MODE_INVERSED 0x3

/* Src Blending factor is calculated from Src alpha.  */
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_ALPHA_FACTOR	   8 : 8
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_ALPHA_FACTOR_DISABLED 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_SRC_ALPHA_FACTOR_ENABLED  0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_ALPHA_MODE		 9 : 9
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_ALPHA_MODE_NORMAL	 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_ALPHA_MODE_INVERSED 0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE	      11 : 10
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE_NORMAL 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE_GLOBAL 0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_GLOBAL_ALPHA_MODE_SCALED 0x2

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE	    14 : 13
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_ZERO	    0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_ONE	    0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_NORMAL   0x2
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_BLENDING_MODE_INVERSED 0x3

/* Destination blending factor is calculated from Dst alpha.  */
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_ALPHA_FACTOR	   15 : 15
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_ALPHA_FACTOR_DISABLED 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG_DST_ALPHA_FACTOR_ENABLED  0x1

/* Register gcregOverlayGlobalAlpha **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay global alpha value.  NOTE: This register is double buffered.  */

#define GCREG_OVERLAY_GLOBAL_ALPHA_Address 0x020A8

#define GCREG_OVERLAY_GLOBAL_ALPHA_SRC_ALPHA 7 : 0

#define GCREG_OVERLAY_GLOBAL_ALPHA_DST_ALPHA 15 : 8

/* Register gcregOverlayClearValue **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Clear Value Register.  Clear value used when                       **
** dcregOverlayConfig.Clear  is enabled, Format is A8R8G8B8.  NOTE: This      **
** register is double buffered.                                               */

#define GCREG_OVERLAY_CLEAR_VALUE_Address 0x020AC

/* 8 bit blue color. */
#define GCREG_OVERLAY_CLEAR_VALUE_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_OVERLAY_CLEAR_VALUE_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_OVERLAY_CLEAR_VALUE_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_OVERLAY_CLEAR_VALUE_ALPHA 31 : 24

/* Register gcregOverlayConfig1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Configuration Register.  Overlay control.  NOTE: This register is  **
** double buffered.                                                           */

#define GCREG_OVERLAY_CONFIG1_Address 0x020B0

/* The format of the overlay.  None means there is no  overlay. NOTE: This    **
** field is double buffered.                                                  */
#define GCREG_OVERLAY_CONFIG1_FORMAT	      2 : 0
#define GCREG_OVERLAY_CONFIG1_FORMAT_NONE     0x0
#define GCREG_OVERLAY_CONFIG1_FORMAT_A4R4G4B4 0x1
#define GCREG_OVERLAY_CONFIG1_FORMAT_A1R5G5B5 0x2
#define GCREG_OVERLAY_CONFIG1_FORMAT_R5G6B5   0x3
#define GCREG_OVERLAY_CONFIG1_FORMAT_A8R8G8B8 0x4

/* Enable this overlay layer.  NOTE: This field is double buffered.  */
#define GCREG_OVERLAY_CONFIG1_ENABLE	     3 : 3
#define GCREG_OVERLAY_CONFIG1_ENABLE_DISABLE 0x0
#define GCREG_OVERLAY_CONFIG1_ENABLE_ENABLE  0x1

/* Enable or disable clear. NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG1_CLEAR_EN		5 : 5
#define GCREG_OVERLAY_CONFIG1_CLEAR_EN_DISABLED 0x0
#define GCREG_OVERLAY_CONFIG1_CLEAR_EN_ENABLED	0x1

/* NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG1_SWIZZLE	   18 : 17
#define GCREG_OVERLAY_CONFIG1_SWIZZLE_ARGB 0x0
#define GCREG_OVERLAY_CONFIG1_SWIZZLE_RGBA 0x1
#define GCREG_OVERLAY_CONFIG1_SWIZZLE_ABGR 0x2
#define GCREG_OVERLAY_CONFIG1_SWIZZLE_BGRA 0x3

/* Enable or disable color key. NOTE: This field is double buffered. */
#define GCREG_OVERLAY_CONFIG1_COLOR_KEY_EN	    20 : 20
#define GCREG_OVERLAY_CONFIG1_COLOR_KEY_EN_DISABLED 0x0
#define GCREG_OVERLAY_CONFIG1_COLOR_KEY_EN_ENABLED  0x1

/* Register gcregOverlayAddress1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Starting address of the overlay. NOTE: This register is double buffered. */

#define GCREG_OVERLAY_ADDRESS1_Address 0x020B4

/* 32 bit address. */
#define GCREG_OVERLAY_ADDRESS1_ADDRESS 31 : 0

/* Register gcregOverlayStride1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Stride of the overlay in bytes. NOTE: This register is double buffered. */

#define GCREG_OVERLAY_STRIDE1_Address 0x020B8

/* Number of bytes from start of one line to next line. */
#define GCREG_OVERLAY_STRIDE1_STRIDE 16 : 0

/* Register gcregOverlayTL1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Top left coordinate of panel pixel where the overlay should start.  Be     **
** aware there is no panning inside the overlay. NOTE: This register is       **
** double buffered.                                                           */

#define GCREG_OVERLAY_TL1_Address 0x020BC

/* Left boundary of overlay window. */
#define GCREG_OVERLAY_TL1_X 11 : 0

/* Top boundary of overlay window. */
#define GCREG_OVERLAY_TL1_Y 27 : 16

/* Register gcregOverlaySize1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* overlay1 size information. NOTE: This register is double buffered. */

#define GCREG_OVERLAY_SIZE1_Address 0x020C0

/* overlay1 width. */
#define GCREG_OVERLAY_SIZE1_WIDTH 11 : 0

/* overlay1 height. */
#define GCREG_OVERLAY_SIZE1_HEIGHT 27 : 16

/* Register gcregOverlayColorKey1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Color Key Start Address Register.  Start of color key range for    **
** overlay.  NOTE: This register is double buffered.                          */

#define GCREG_OVERLAY_COLOR_KEY1_Address 0x020C4

/* 8 bit blue color. */
#define GCREG_OVERLAY_COLOR_KEY1_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_OVERLAY_COLOR_KEY1_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_OVERLAY_COLOR_KEY1_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_OVERLAY_COLOR_KEY1_ALPHA 31 : 24

/* Register gcregOverlayColorKeyHigh1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Color Key End Address Register.  End of color key range for        **
** overlay.  NOTE: This register is double buffered.                          */

#define GCREG_OVERLAY_COLOR_KEY_HIGH1_Address 0x020C8

/* 8 bit blue color. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH1_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH1_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH1_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_OVERLAY_COLOR_KEY_HIGH1_ALPHA 31 : 24

/* Register gcregOverlayAlphaBlendConfig1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Alpha Blending Configuration Register.  NOTE: This register is double      **
** buffered.                                                                  */

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_Address 0x020CC

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_ALPHA_BLEND	       0 : 0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_ALPHA_BLEND_DISABLED 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_ALPHA_BLEND_ENABLED  0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_ALPHA_MODE	  1 : 1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_ALPHA_MODE_NORMAL	  0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_ALPHA_MODE_INVERSED 0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_GLOBAL_ALPHA_MODE	       4 : 3
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_GLOBAL_ALPHA_MODE_NORMAL 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_GLOBAL_ALPHA_MODE_GLOBAL 0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_GLOBAL_ALPHA_MODE_SCALED 0x2

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_BLENDING_MODE	     7 : 6
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_BLENDING_MODE_ZERO     0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_BLENDING_MODE_ONE	     0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_BLENDING_MODE_NORMAL   0x2
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_BLENDING_MODE_INVERSED 0x3

/* Src Blending factor is calculated from Src alpha.  */
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_ALPHA_FACTOR	    8 : 8
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_ALPHA_FACTOR_DISABLED 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_SRC_ALPHA_FACTOR_ENABLED  0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_ALPHA_MODE	  9 : 9
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_ALPHA_MODE_NORMAL	  0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_ALPHA_MODE_INVERSED 0x1

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_GLOBAL_ALPHA_MODE	       11 : 10
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_GLOBAL_ALPHA_MODE_NORMAL 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_GLOBAL_ALPHA_MODE_GLOBAL 0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_GLOBAL_ALPHA_MODE_SCALED 0x2

#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_BLENDING_MODE	     14 : 13
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_BLENDING_MODE_ZERO     0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_BLENDING_MODE_ONE	     0x1
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_BLENDING_MODE_NORMAL   0x2
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_BLENDING_MODE_INVERSED 0x3

/* Destination blending factor is calculated from Dst alpha.  */
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_ALPHA_FACTOR	    15 : 15
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_ALPHA_FACTOR_DISABLED 0x0
#define GCREG_OVERLAY_ALPHA_BLEND_CONFIG1_DST_ALPHA_FACTOR_ENABLED  0x1

/* Register gcregOverlayGlobalAlpha1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay global alpha value.  NOTE: This register is double buffered.  */

#define GCREG_OVERLAY_GLOBAL_ALPHA1_Address 0x020D0

#define GCREG_OVERLAY_GLOBAL_ALPHA1_SRC_ALPHA 7 : 0

#define GCREG_OVERLAY_GLOBAL_ALPHA1_DST_ALPHA 15 : 8

/* Register gcregOverlayClearValue1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Overlay Clear Value Register.  Clear value used when                       **
** dcregOverlayConfig.Clear  is enabled, Format is A8R8G8B8.  NOTE: This      **
** register is double buffered.                                               */

#define GCREG_OVERLAY_CLEAR_VALUE1_Address 0x020D4

/* 8 bit blue color. */
#define GCREG_OVERLAY_CLEAR_VALUE1_BLUE 7 : 0

/* 8 bit green color. */
#define GCREG_OVERLAY_CLEAR_VALUE1_GREEN 15 : 8

/* 8 bit red color. */
#define GCREG_OVERLAY_CLEAR_VALUE1_RED 23 : 16

/* 8 bit alpha value. */
#define GCREG_OVERLAY_CLEAR_VALUE1_ALPHA 31 : 24

/* Register gcregPanelDestAddress **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Panel Out Destination Start Address Register.  Starting address of the     **
** panel out path destination buffer.  It is used for debugging.  NOTE: This  **
** register is double buffered.                                               */

#define GCREG_PANEL_DEST_ADDRESS_Address 0x020D8

/* 32 bit address. */
#define GCREG_PANEL_DEST_ADDRESS_ADDRESS 31 : 0

/* Register gcregDestStride **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Destination Stride Register.  Stride of the destination buffer in bytes.    **
** NOTE: This register is double buffered.                                    */

#define GCREG_DEST_STRIDE_Address 0x020DC

/* Number of bytes from the start of one line to the next line.  */
#define GCREG_DEST_STRIDE_STRIDE 17 : 0

/* Register gcregDisplayDitherTableLow **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Dither Threshold Table Register.  */

#define GCREG_DISPLAY_DITHER_TABLE_LOW_Address 0x020E0

/* Dither threshold value for x,y=0,0. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y0_X0 3 : 0

/* Dither threshold value for x,y=1,0. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y0_X1 7 : 4

/* Dither threshold value for x,y=2,0. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y0_X2 11 : 8

/* Dither threshold value for x,y=3,0. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y0_X3 15 : 12

/* Dither threshold value for x,y=0,1. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y1_X0 19 : 16

/* Dither threshold value for x,y=1,1. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y1_X1 23 : 20

/* Dither threshold value for x,y=2,1. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y1_X2 27 : 24

/* Dither threshold value for x,y=3,1. */
#define GCREG_DISPLAY_DITHER_TABLE_LOW_Y1_X3 31 : 28

/* Register gcregDisplayDitherTableHigh **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Address 0x020E4

/* Dither threshold value for x,y=0,2. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y2_X0 3 : 0

/* Dither threshold value for x,y=1,2. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y2_X1 7 : 4

/* Dither threshold value for x,y=2,2. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y2_X2 11 : 8

/* Dither threshold value for x,y=3,2. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y2_X3 15 : 12

/* Dither threshold value for x,y=0,3. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y3_X0 19 : 16

/* Dither threshold value for x,y=1,3. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y3_X1 23 : 20

/* Dither threshold value for x,y=2,3. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y3_X2 27 : 24

/* Dither threshold value for x,y=3,3. */
#define GCREG_DISPLAY_DITHER_TABLE_HIGH_Y3_X3 31 : 28

/* Register gcregPanelConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Panel Configuration Register.  */

#define GCREG_PANEL_CONFIG_Address 0x020E8

/* Data Enable enabled/disabled.  */
#define GCREG_PANEL_CONFIG_DE	       0 : 0
#define GCREG_PANEL_CONFIG_DE_DISABLED 0x0
#define GCREG_PANEL_CONFIG_DE_ENABLED  0x1

/* Data Enable polarity. */
#define GCREG_PANEL_CONFIG_DE_POLARITY		1 : 1
#define GCREG_PANEL_CONFIG_DE_POLARITY_POSITIVE 0x0
#define GCREG_PANEL_CONFIG_DE_POLARITY_NEGATIVE 0x1

/* Data enabled/disabled.  */
#define GCREG_PANEL_CONFIG_DATA_ENABLE		4 : 4
#define GCREG_PANEL_CONFIG_DATA_ENABLE_DISABLED 0x0
#define GCREG_PANEL_CONFIG_DATA_ENABLE_ENABLED	0x1

/* Data polarity. */
#define GCREG_PANEL_CONFIG_DATA_POLARITY	  5 : 5
#define GCREG_PANEL_CONFIG_DATA_POLARITY_POSITIVE 0x0
#define GCREG_PANEL_CONFIG_DATA_POLARITY_NEGATIVE 0x1

/* Clock enable/disable. */
#define GCREG_PANEL_CONFIG_CLOCK	  8 : 8
#define GCREG_PANEL_CONFIG_CLOCK_DISABLED 0x0
#define GCREG_PANEL_CONFIG_CLOCK_ENABLED  0x1

/* Clock polarity. */
#define GCREG_PANEL_CONFIG_CLOCK_POLARITY	   9 : 9
#define GCREG_PANEL_CONFIG_CLOCK_POLARITY_POSITIVE 0x0
#define GCREG_PANEL_CONFIG_CLOCK_POLARITY_NEGATIVE 0x1

/* Power enable/disable. */
#define GCREG_PANEL_CONFIG_POWER	  12 : 12
#define GCREG_PANEL_CONFIG_POWER_DISABLED 0x0
#define GCREG_PANEL_CONFIG_POWER_ENABLED  0x1

/* Power polarity. */
#define GCREG_PANEL_CONFIG_POWER_POLARITY	   13 : 13
#define GCREG_PANEL_CONFIG_POWER_POLARITY_POSITIVE 0x0
#define GCREG_PANEL_CONFIG_POWER_POLARITY_NEGATIVE 0x1

/* Backlight enabled/disabled. */
#define GCREG_PANEL_CONFIG_BACKLIGHT	      16 : 16
#define GCREG_PANEL_CONFIG_BACKLIGHT_DISABLED 0x0
#define GCREG_PANEL_CONFIG_BACKLIGHT_ENABLED  0x1

/* Backlight polarity. */
#define GCREG_PANEL_CONFIG_BACKLIGHT_POLARITY	       17 : 17
#define GCREG_PANEL_CONFIG_BACKLIGHT_POLARITY_POSITIVE 0x0
#define GCREG_PANEL_CONFIG_BACKLIGHT_POLARITY_NEGATIVE 0x1

/* Register gcregPanelControl **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Panel control Register. */

#define GCREG_PANEL_CONTROL_Address 0x020EC

/* The valid field defines whether we can copy a new set of registers at the  **
** next VBLANK or not.  This ensures a frame will always start with a valid   **
** working set if this register is programmed last, which reduces the need    **
** for SW to wait for the start of a VBLANK signal in order to ensure all     **
** states are loaded before the next VBLANK.                                  */
#define GCREG_PANEL_CONTROL_VALID	  0 : 0
#define GCREG_PANEL_CONTROL_VALID_PENDING 0x0
#define GCREG_PANEL_CONTROL_VALID_WORKING 0x1

/* Interface of DPI */
#define GCREG_PANEL_CONTROL_BACK_PRESSURE_DISABLE     1 : 1
#define GCREG_PANEL_CONTROL_BACK_PRESSURE_DISABLE_NO  0x0
#define GCREG_PANEL_CONTROL_BACK_PRESSURE_DISABLE_YES 0x1

/* Register gcregPanelFunction **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Panel function Register. NOTE: This register is double buffered. */

#define GCREG_PANEL_FUNCTION_Address 0x020F0

/* When Output is enabled, pixels will be displayed.  When Output is          **
** disabled, all pixels will be black.  This allows a panel to have correct   **
** timing but without any pixels. NOTE: This field is double buffered.        */
#define GCREG_PANEL_FUNCTION_OUTPUT	     0 : 0
#define GCREG_PANEL_FUNCTION_OUTPUT_DISABLED 0x0
#define GCREG_PANEL_FUNCTION_OUTPUT_ENABLED  0x1

/* When Gamma is enabled, the R, G, and B channels will be routed through the **
** Gamma LUT to perform gamma correction. NOTE: This field is double          **
** buffered.                                                                  */
#define GCREG_PANEL_FUNCTION_GAMMA	    1 : 1
#define GCREG_PANEL_FUNCTION_GAMMA_DISABLED 0x0
#define GCREG_PANEL_FUNCTION_GAMMA_ENABLED  0x1

/* Enabling dithering allows R8G8B8 modes to show better on  panels with less **
** bits-per-pixel. NOTE: This field is double buffered.                       */
#define GCREG_PANEL_FUNCTION_DITHER	     2 : 2
#define GCREG_PANEL_FUNCTION_DITHER_DISABLED 0x0
#define GCREG_PANEL_FUNCTION_DITHER_ENABLED  0x1

/* Register gcregPanelWorking **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Register to trigger Display Controller. */

#define GCREG_PANEL_WORKING_Address 0x020F4

/* Writing a one in this register will force a reset of the display           **
** controller.  All registers will be copied to the working set and counters  **
** will be reset to the end of HSYNC and VSYNC.                               */
#define GCREG_PANEL_WORKING_WORKING	    0 : 0
#define GCREG_PANEL_WORKING_WORKING_WORKING 0x1

/* Register gcregPanelState **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Register representing the Display Controller status. */

#define GCREG_PANEL_STATE_Address 0x020F8

/* When the frame buffer address gets written to, this bit gets set to one.   **
** It will be reset to zero at the start of the next VBLANK when the register **
** gets copied into the working set.                                          */
#define GCREG_PANEL_STATE_FLIP_IN_PROGRESS     0 : 0
#define GCREG_PANEL_STATE_FLIP_IN_PROGRESS_NO  0x0
#define GCREG_PANEL_STATE_FLIP_IN_PROGRESS_YES 0x1

/* When the display FIFO underflows, this bit gets set to one.  Reading this  **
** register will reset it back to zero.                                       */
#define GCREG_PANEL_STATE_VIDEO_UNDER_FLOW     1 : 1
#define GCREG_PANEL_STATE_VIDEO_UNDER_FLOW_NO  0x0
#define GCREG_PANEL_STATE_VIDEO_UNDER_FLOW_YES 0x1

/* When the overlay FIFO underflows, this bit gets set to  one. Reading this  **
** register will reset it back to zero.                                       */
#define GCREG_PANEL_STATE_OVERLAY_UNDER_FLOW	 2 : 2
#define GCREG_PANEL_STATE_OVERLAY_UNDER_FLOW_NO	 0x0
#define GCREG_PANEL_STATE_OVERLAY_UNDER_FLOW_YES 0x1

/* When the overlay FIFO underflows, this bit gets set to  one. Reading this  **
** register will reset it back to zero.                                       */
#define GCREG_PANEL_STATE_OVERLAY_UNDER_FLOW1	  3 : 3
#define GCREG_PANEL_STATE_OVERLAY_UNDER_FLOW1_NO  0x0
#define GCREG_PANEL_STATE_OVERLAY_UNDER_FLOW1_YES 0x1

/* Register gcregPanelTiming **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Panel Timing Register.  Timing for hardware panel sequencing.  */

#define GCREG_PANEL_TIMING_Address 0x020FC

/* Number of VSYNCsto wait after power has been enabled. */
#define GCREG_PANEL_TIMING_POWER_ENABLE 3 : 0

/* Number of VSYNCs to wait after backlight has been enabled. */
#define GCREG_PANEL_TIMING_BACKLIGHT_ENABLE 7 : 4

/* Number of VSYNCs to wait after clock has been enabled. */
#define GCREG_PANEL_TIMING_CLOCK_ENABLE 11 : 8

/* Number of VSYNCs to wait after data has been enabled. */
#define GCREG_PANEL_TIMING_DATA_ENABLE 15 : 12

/* Number of VSYNCs to wait after data has been disabled. */
#define GCREG_PANEL_TIMING_DATA_DISABLE 19 : 16

/* Number of VSYNCs to wait after clock has been disabled. */
#define GCREG_PANEL_TIMING_CLOCK_DISABLE 23 : 20

/* Number of VSYNCs to wait after backlight has been disabled. */
#define GCREG_PANEL_TIMING_BACKLIGHT_DISABLE 27 : 24

/* Number of VSYNCs to wait after power has been disabled. */
#define GCREG_PANEL_TIMING_POWER_DISABLE 31 : 28

/* Register gcregHDisplay **
** ~~~~~~~~~~~~~~~~~~~~~~ */

/* Horizontal Display Total and Visible Pixel Count Register.  Horizontal     **
** Total and Display End counters.  NOTE: This register is double buffered.   */

#define GCREG_HDISPLAY_Address 0x02100

/* Number of visible horizontal pixels. NOTE: This field is double buffered. */
#define GCREG_HDISPLAY_DISPLAY_END 12 : 0

/* Total number of horizontal pixels. NOTE: This field is double buffered. */
#define GCREG_HDISPLAY_TOTAL 28 : 16

/* Register gcregHSync **
** ~~~~~~~~~~~~~~~~~~~ */

/* Horizontal Sync Counter Register.  Horizontal Sync counters.  NOTE: This   **
** register is double buffered.                                               */

#define GCREG_HSYNC_Address 0x02104

/* Start of horizontal sync pulse.  NOTE: This field is double buffered. */
#define GCREG_HSYNC_START 12 : 0

/* End of horizontal sync pulse.  NOTE: This field is double buffered. */
#define GCREG_HSYNC_END 28 : 16

/* Horizontal sync pulse control.  NOTE: This field is double buffered. */
#define GCREG_HSYNC_PULSE	   30 : 30
#define GCREG_HSYNC_PULSE_DISABLED 0x0
#define GCREG_HSYNC_PULSE_ENABLED  0x1

/* Polarity of the horizontal sync pulse.  NOTE: This field is double         **
** buffered.                                                                  */
#define GCREG_HSYNC_POLARITY	      31 : 31
#define GCREG_HSYNC_POLARITY_POSITIVE 0x0
#define GCREG_HSYNC_POLARITY_NEGATIVE 0x1

/* Register gcregHCounter1 **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Horizontal Programmable Counter 1 Register.  */

#define GCREG_HCOUNTER1_Address 0x02108

/* Start of horizontal counter 1.  */
#define GCREG_HCOUNTER1_START 11 : 0

/* End of horizontal counter 1.  */
#define GCREG_HCOUNTER1_END 27 : 16

/* Horizontal counter 1 control.  */
#define GCREG_HCOUNTER1_PULSE	       30 : 30
#define GCREG_HCOUNTER1_PULSE_DISABLED 0x0
#define GCREG_HCOUNTER1_PULSE_ENABLED  0x1

/* Polarity of the pulse.  */
#define GCREG_HCOUNTER1_POLARITY	  31 : 31
#define GCREG_HCOUNTER1_POLARITY_POSITIVE 0x0
#define GCREG_HCOUNTER1_POLARITY_NEGATIVE 0x1

/* Register gcregHCounter2 **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Horizontal Programmable Counter 2 Register.  */

#define GCREG_HCOUNTER2_Address 0x0210C

/* Start of horizontal counter 2. */
#define GCREG_HCOUNTER2_START 11 : 0

/* End of horizontal counter 2. */
#define GCREG_HCOUNTER2_END 27 : 16

/* Horizontal counter 2 control. */
#define GCREG_HCOUNTER2_PULSE	       30 : 30
#define GCREG_HCOUNTER2_PULSE_DISABLED 0x0
#define GCREG_HCOUNTER2_PULSE_ENABLED  0x1

/* Polarity of the pulse. */
#define GCREG_HCOUNTER2_POLARITY	  31 : 31
#define GCREG_HCOUNTER2_POLARITY_POSITIVE 0x0
#define GCREG_HCOUNTER2_POLARITY_NEGATIVE 0x1

/* Register gcregVDisplay **
** ~~~~~~~~~~~~~~~~~~~~~~ */

/* Vertical Total and Visible Pixel Count Register.  Vertical Total and       **
** Display End counters.  NOTE: This register is double buffered.             */

#define GCREG_VDISPLAY_Address 0x02110

/* Number of visible vertical lines.  NOTE: This field is double buffered. */
#define GCREG_VDISPLAY_DISPLAY_END 11 : 0

/* Total number of vertical lines.  NOTE: This field is double buffered. */
#define GCREG_VDISPLAY_TOTAL 27 : 16

/* Register gcregVSync **
** ~~~~~~~~~~~~~~~~~~~ */

/* Vertical Sync Counter Register.  Vertical Sync counters.  NOTE: This       **
** register is double buffered.                                               */

#define GCREG_VSYNC_Address 0x02114

/* Start of the vertical sync pulse. NOTE: This field is double buffered. */
#define GCREG_VSYNC_START 11 : 0

/* End of the vertical sync pulse. NOTE: This field is double buffered. */
#define GCREG_VSYNC_END 27 : 16

/* Vertical sync pulse control. NOTE: This field is double buffered. */
#define GCREG_VSYNC_PULSE	   30 : 30
#define GCREG_VSYNC_PULSE_DISABLED 0x0
#define GCREG_VSYNC_PULSE_ENABLED  0x1

/* Polarity of the vertical sync pulse. NOTE: This field is double buffered. */
#define GCREG_VSYNC_POLARITY	      31 : 31
#define GCREG_VSYNC_POLARITY_POSITIVE 0x0
#define GCREG_VSYNC_POLARITY_NEGATIVE 0x1

/* Register gcregDisplayCurrentLocation **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Display Current Location Register.  Current x,y location of display        **
** controller. READ ONLY.                                                     */

#define GCREG_DISPLAY_CURRENT_LOCATION_Address 0x02118

/* Current X location. */
#define GCREG_DISPLAY_CURRENT_LOCATION_X 15 : 0

/* Current Y location. */
#define GCREG_DISPLAY_CURRENT_LOCATION_Y 31 : 16

/* Register gcregGammaIndex **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Gamma Index Register.  Index into gamma table.  See gcregGammaData for     **
** more  information.                                                         */

#define GCREG_GAMMA_INDEX_Address 0x0211C

/* Index into gamma table. */
#define GCREG_GAMMA_INDEX_INDEX 7 : 0

/* Register gcregGammaData **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Gamma Data Register.  Translation values for the gamma table.  When this   **
** register  gets written, the data gets stored in the gamma table at the     **
** index specified by the gcregGammaIndex register.  After the  register is   **
** written, the index gets incremented.                                       */

#define GCREG_GAMMA_DATA_Address 0x02120

/* Blue translation value. */
#define GCREG_GAMMA_DATA_BLUE 7 : 0

/* Green translation value. */
#define GCREG_GAMMA_DATA_GREEN 15 : 8

/* Red translation value. */
#define GCREG_GAMMA_DATA_RED 23 : 16

/* Register gcregCursorConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Cursor Configuration Register.  Configuration register for the cursor.     **
** Double-buffered values in this register cannot be read  while a flip is in **
** progress.  NOTE: This register is double buffered.                         */

#define GCREG_CURSOR_CONFIG_Address 0x02124

/* Format of the cursor.  NOTE: This field is double buffered. */
#define GCREG_CURSOR_CONFIG_FORMAT	    1 : 0
#define GCREG_CURSOR_CONFIG_FORMAT_DISABLED 0x0
#define GCREG_CURSOR_CONFIG_FORMAT_MASKED   0x1
#define GCREG_CURSOR_CONFIG_FORMAT_A8R8G8B8 0x2

/* Display Controller owning the cursor. Always 0 for this IP. NOTE: This     **
** field is double buffered.                                                  */
#define GCREG_CURSOR_CONFIG_DISPLAY	     4 : 4
#define GCREG_CURSOR_CONFIG_DISPLAY_DISPLAY0 0x0
#define GCREG_CURSOR_CONFIG_DISPLAY_DISPLAY1 0x1

/* Vertical offset to cursor hotspot.  NOTE: This field is double buffered. */
#define GCREG_CURSOR_CONFIG_HOT_SPOT_Y 12 : 8

/* Horizontal offset to cursor hotspot.  NOTE: This field is double buffered. */
#define GCREG_CURSOR_CONFIG_HOT_SPOT_X 20 : 16

/* When the cursor address gets written to, this bit gets set  to one.  It    **
** will be reset to zero at the start of the next  VBLANK of the owning       **
** display when the register gets copied  into the working set. NOTE: This    **
** field is double buffered.                                                  */
#define GCREG_CURSOR_CONFIG_FLIP_IN_PROGRESS	 31 : 31
#define GCREG_CURSOR_CONFIG_FLIP_IN_PROGRESS_NO	 0x0
#define GCREG_CURSOR_CONFIG_FLIP_IN_PROGRESS_YES 0x1

/* Register gcregCursorAddress **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Cursor Base Address Register.  Address of the cursor shape.  NOTE: This    **
** register is double buffered.                                               */

#define GCREG_CURSOR_ADDRESS_Address 0x02128

/* 32 bit address. */
#define GCREG_CURSOR_ADDRESS_ADDRESS 31 : 0

/* Register gcregCursorLocation **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Cursor Location Register.  Location of the cursor on the owning display.   **
** NOTE: This register is double buffered.                                    */

#define GCREG_CURSOR_LOCATION_Address 0x0212C

/* X location of cursor's hotspot.  NOTE: This field is double buffered. */
#define GCREG_CURSOR_LOCATION_X 12 : 0

/* Y location of cursor's hotspot.  NOTE: This field is double buffered. */
#define GCREG_CURSOR_LOCATION_Y 27 : 16

/* Register gcregCursorBackground **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Cursor Background Color Register.  The background color for Masked         **
** cursors.  NOTE: This register is double buffered.                          */

#define GCREG_CURSOR_BACKGROUND_Address 0x02130

/* Blue value. NOTE: This field is double buffered. */
#define GCREG_CURSOR_BACKGROUND_BLUE 7 : 0

/* Green value. NOTE: This field is double buffered. */
#define GCREG_CURSOR_BACKGROUND_GREEN 15 : 8

/* Red value. NOTE: This field is double buffered. */
#define GCREG_CURSOR_BACKGROUND_RED 23 : 16

/* Register gcregCursorForeground **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Cursor Foreground Color Register.  The foreground color for Masked         **
** cursors.  NOTE: This register is double buffered.                          */

#define GCREG_CURSOR_FOREGROUND_Address 0x02134

/* Blue value. NOTE: This field is double buffered. */
#define GCREG_CURSOR_FOREGROUND_BLUE 7 : 0

/* Green value. NOTE: This field is double buffered. */
#define GCREG_CURSOR_FOREGROUND_GREEN 15 : 8

/* Red value. NOTE: This field is double buffered. */
#define GCREG_CURSOR_FOREGROUND_RED 23 : 16

/* Register gcregDisplayIntr **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Display Interrupt Register.  This is the interrupt register for the        **
** Display.  This register will automatically clear after a read.  The        **
** interrupt bit is set when the current frame buffer  is used up. The        **
** interrupt signal will be pulled up only  when the interrupt enable bit in  **
** register  gcregDisplayIntrEnable is enabled.                               */

#define GCREG_DISPLAY_INTR_Address 0x02138

/* Display0 interrupt */
#define GCREG_DISPLAY_INTR_DISP0 0 : 0

/* Display1 interrupt */
#define GCREG_DISPLAY_INTR_DISP1 4 : 4

/* Cursor interrupt */
#define GCREG_DISPLAY_INTR_CURSOR 8 : 8

/* Display0 DBI configure error */
#define GCREG_DISPLAY_INTR_DISP0_DBI_CFG_ERROR 12 : 12

/* Display1 DBI configure error */
#define GCREG_DISPLAY_INTR_DISP1_DBI_CFG_ERROR 13 : 13

/* Panel underflow intr. */
#define GCREG_DISPLAY_INTR_PANEL_UNDERFLOW 29 : 29

/* Soft reset done */
#define GCREG_DISPLAY_INTR_SOFT_RESET_DONE 30 : 30

/* Bus error */
#define GCREG_DISPLAY_INTR_BUS_ERROR 31 : 31

/* Register gcregDisplayIntrEnable **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Display Interrupt Enable Register.  The interrupt enable register for      **
** display_0 (and display_1  if present).  NOTE: Interrupt enable for         **
** register gcregDisplayIntr.  NOTE: This register is double buffered.        */

#define GCREG_DISPLAY_INTR_ENABLE_Address 0x0213C

/* Display0 interrupt enable NOTE: This field is double buffered. */
#define GCREG_DISPLAY_INTR_ENABLE_DISP0 0 : 0

/* Display1 interrupt enable NOTE: This field is double buffered. */
#define GCREG_DISPLAY_INTR_ENABLE_DISP1 4 : 4

/* Display0 DBI configure error enable NOTE: This field is double buffered. */
#define GCREG_DISPLAY_INTR_ENABLE_DISP0_DBI_CFG_ERROR 12 : 12

/* Panel underflow intr enable. NOTE: This field is double buffered. */
#define GCREG_DISPLAY_INTR_ENABLE_PANEL_UNDERFLOW 29 : 29

/* Soft reset done enable NOTE: This field is double buffered. */
#define GCREG_DISPLAY_INTR_ENABLE_SOFT_RESET_DONE 30 : 30

/* Bus error enable NOTE: This field is double buffered. */
#define GCREG_DISPLAY_INTR_ENABLE_BUS_ERROR 31 : 31

/* Register gcregDbiConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* DBI Configuration Register.  Configuration register for DBI output.  */

#define GCREG_DBI_CONFIG_Address 0x02140

/* DBI Type select */
#define GCREG_DBI_CONFIG_DBI_TYPE		1 : 0
#define GCREG_DBI_CONFIG_DBI_TYPE_TYPE_AFIXED_E 0x0
#define GCREG_DBI_CONFIG_DBI_TYPE_TYPE_ACLOCK_E 0x1
#define GCREG_DBI_CONFIG_DBI_TYPE_TYPE_B	0x2
#define GCREG_DBI_CONFIG_DBI_TYPE_TYPE_C	0x3

/* DBI interface data format. Refer to DBI specification section  on          **
** Interface Color Coding for detail.                                         */
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT		  5 : 2
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D8R3G3B2	  0x0
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D8R4G4B4	  0x1
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D8R5G6B5	  0x2
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D8R6G6B6	  0x3
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D8R8G8B8	  0x4
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D9R6G6B6	  0x5
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16R3G3B2	  0x6
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16R4G4B4	  0x7
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16R5G6B5	  0x8
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16_R6_G6_B6_OP1 0x9
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16_R6_G6_B6_OP2 0xA
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16_R8_G8_B8_OP1 0xB
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D16_R8_G8_B8_OP2 0xC
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D1R5G6B5	  0xD
#define GCREG_DBI_CONFIG_DBI_DATA_FORMAT_D1R8G8B8	  0xE

/* Output bus select */
#define GCREG_DBI_CONFIG_BUS_OUTPUT_SEL	    6 : 6
#define GCREG_DBI_CONFIG_BUS_OUTPUT_SEL_DPI 0x0
#define GCREG_DBI_CONFIG_BUS_OUTPUT_SEL_DBI 0x1

/* D/CX Pin polarity */
#define GCREG_DBI_CONFIG_DBIX_POLARITY	       7 : 7
#define GCREG_DBI_CONFIG_DBIX_POLARITY_DEFAULT 0x0
#define GCREG_DBI_CONFIG_DBIX_POLARITY_REVERSE 0x1

/* Time unit for AC characteristics  */
#define GCREG_DBI_CONFIG_DBI_AC_TIME_UNIT 11 : 8

/* Options for DBI Type C Interface Read/Write Sequence;  0: Option 1;  1:    **
** Option 2;  2: Option 3.                                                    */
#define GCREG_DBI_CONFIG_DBI_TYPEC_OPT 13 : 12

/* Register gcregDbiIfReset **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* DBI Reset Register.  Reset DBI interface to idle state. WRITE ONLY. */

#define GCREG_DBI_IF_RESET_Address 0x02144

/* Reset DBI interface to idle state  */
#define GCREG_DBI_IF_RESET_DBI_IF_LEVEL_RESET	    0 : 0
#define GCREG_DBI_IF_RESET_DBI_IF_LEVEL_RESET_RESET 0x1

/* Register gcregDbiWrChar1 **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* DBI Write AC Characteristics 1 Register.  DBI write AC characteristics     **
** definition register 1                                                      */

#define GCREG_DBI_WR_CHAR1_Address 0x02148

/* Single write period duration. Cycle number=Setting*(DbiAcTimeUnit+1).      **
** This field must be no less than 3.                                         */
#define GCREG_DBI_WR_CHAR1_DBI_WR_PERIOD 7 : 0

/* Cycle number=Setting*(DbiAcTimeUnit+1).  0: When Type A Fixed E mode: Not  **
** used.  1: When Type A Clock E mode: Time to assert E.  2: When Type B      **
** mode: Time to assert WRX.                                                  */
#define GCREG_DBI_WR_CHAR1_DBI_WR_EOR_WR_ASSERT 11 : 8

/* Cycle number=Setting*(DbiAcTimeUnit+1).  0: When Type A Fixed E mode: Time **
** to assert CSX.  1: When Type A Clock E mode: Not used.  2: When Type B     **
** mode: Time to assert CSX.                                                  */
#define GCREG_DBI_WR_CHAR1_DBI_WR_CS_ASSERT 15 : 12

/* Register gcregDbiWrChar2 **
** ~~~~~~~~~~~~~~~~~~~~~~~~ */

/* DBI Write AC Characteristics 2 Register.  DBI write AC characteristics     **
** definition register 2                                                      */

#define GCREG_DBI_WR_CHAR2_Address 0x0214C

/* Cycle number=Setting*(DbiAcTimeUnit+1).  0: When Type A Fixed E mode: Not  **
** used.  1: When type A Clock E mode: Time to de-assert E.  2: When Type B   **
** mode: Time to de-assert WRX.                                               */
#define GCREG_DBI_WR_CHAR2_DBI_WR_EOR_WR_DE_ASRT 7 : 0

/* Cycle number=Setting*(DbiAcTimeUnit+1).  0: When type A fixed E mode: Time **
** to de-assert CSX.  1: When type A clock E mode: Not used.  2: When type B  **
** mode: Time to de-assert CSX.                                               */
#define GCREG_DBI_WR_CHAR2_DBI_WR_CS_DE_ASRT 15 : 8

/* Register gcregDbiCmd **
** ~~~~~~~~~~~~~~~~~~~~ */

/* DBI Command Control Register.  DBI Command in/out port. Writes to this     **
** register will send  command/data to the DBI port. WRITE ONLY.              */

#define GCREG_DBI_CMD_Address 0x02150

/* The type of data contained in this word is specified using                 **
** DBI_COMMANDFLAG[bits 31:30].  For Type C Options 1 and 2:                  **
** DBI_COMMAND_WORD[8] is used to indicate the polarity of D/CX.              **
** DBI_COMMAND_WORD[8] = 0 indicates DBI_COMMAND_WORD[7:0] is COMMAND WORD.   **
** DBI_COMMAND_WORD[8] = 1 indicates DBI_COMMAND_WORD[7:0] is DATA WORD.      */
#define GCREG_DBI_CMD_DBI_COMMAND_WORD 15 : 0

/* DBI command flag.  */
#define GCREG_DBI_CMD_DBI_COMMANDFLAG 31 : 30
/* //DBI_COMMAND_WORD will contain an address.  */
#define GCREG_DBI_CMD_DBI_COMMANDFLAG_ADDRESS 0x0
/* //Starts a write. Contents of DBI_COMMAND_WORD are ignored.  */
#define GCREG_DBI_CMD_DBI_COMMANDFLAG_WRITE_MEM_START 0x1
/* //DBI_COMMAND_WORD will contain a parameter or data.  */
#define GCREG_DBI_CMD_DBI_COMMANDFLAG_PARAMETER_OR_DATA 0x2

/* Register gcregDpiConfig **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* DPI Configuration Register.  The configuration register for DPI output.    **
** NOTE: This register is double buffered.                                    */

#define GCREG_DPI_CONFIG_Address 0x02154

/* DPI interface data format. Refer to DPI specification section for          **
** Interface Color Coding' for detail. NOTE: This field is double buffered.   */
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT	 2 : 0
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT_D16CFG1 0x0
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT_D16CFG2 0x1
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT_D16CFG3 0x2
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT_D18CFG1 0x3
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT_D18CFG2 0x4
#define GCREG_DPI_CONFIG_DPI_DATA_FORMAT_D24	 0x5

/* Register gcregDbiTypecCfg **
** ~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* DBI Type C Timing Control Register.  DBI Type C write timing definition. */

#define GCREG_DBI_TYPEC_CFG_Address 0x02158

/* Divide counter for Tas (address setup time). Specifies how many sdaClk     **
** cycles in Tas phase.  DCX Tas means number of DBI_AC_TIME_UNIT of sdaClk   **
** to be ahead of scl.  Only works for DBI TYPE C Option3. Option1/2 will     **
** ignore this setting.  This field needs to be at least 1.                   */
#define GCREG_DBI_TYPEC_CFG_TAS 3 : 0

/* Divide counter for Twrl (SCL L duration (write)). Specifies how many       **
** sdaClk cycles in Twrl phase.  SCL Twrl means number of DBI_AC_TIME_UNIT of **
** sdaClk for scl staying low This field needs to be at least 1.              */
#define GCREG_DBI_TYPEC_CFG_SCL_TWRL 11 : 4

/* Divide counter for Twrh (SCL H duration (write)). Specifies how many       **
** sdaClk cycles in Twrh phase.  SCL Twrh means number of DBI_AC_TIME_UNIT of **
** sdaClk for scl staying high.  This field need to be at least 1.            */
#define GCREG_DBI_TYPEC_CFG_SCL_TWRH 19 : 12

/* Determine the source of output Scl.  */
#define GCREG_DBI_TYPEC_CFG_SCL_SEL 20 : 20
/* //follow the SCL_TWRL and SCL_TWRH setting.  */
#define GCREG_DBI_TYPEC_CFG_SCL_SEL_DIVIDED_SDA_CLK 0x0
/* //output 1 bit per SdaClk, ignore SCL_TWRL and SCL_TWRH. */
#define GCREG_DBI_TYPEC_CFG_SCL_SEL_SDA_CLK 0x1

/* Register gcregDCStatus **
** ~~~~~~~~~~~~~~~~~~~~~~ */

/* Display Controller Status Register.  */

#define GCREG_DC_STATUS_Address 0x0215C

/* DBI Type C afifo full.  */
#define GCREG_DC_STATUS_DBI_TYPEC_FIFO_FULL 0 : 0

/* Register gcregSrcConfigEndian **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Endian control.  */

#define GCREG_SRC_CONFIG_ENDIAN_Address 0x02160

/* Control source endian. */
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL		    1 : 0
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_NO_SWAP	    0x0
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_SWAP_WORD   0x1
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DWORD  0x2
#define GCREG_SRC_CONFIG_ENDIAN_CONTROL_SWAP_DDWORD 0x3

/* Register gcregSoftReset **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Soft reset.  */

#define GCREG_SOFT_RESET_Address 0x02164

/* Writing a one in this register will force a soft reset of the display      **
** controller.                                                                */
#define GCREG_SOFT_RESET_RESET	     0 : 0
#define GCREG_SOFT_RESET_RESET_RESET 0x1

/* Register gcregDcControl **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Control register.  */

#define GCREG_DC_CONTROL_Address 0x02168

/* Enable or disable the debug register. */
#define GCREG_DC_CONTROL_DEBUG_REGISTER		 3 : 3
#define GCREG_DC_CONTROL_DEBUG_REGISTER_DISABLED 0x0
#define GCREG_DC_CONTROL_DEBUG_REGISTER_ENABLED	 0x1

/* Enable or disable ram clock gating. */
#define GCREG_DC_CONTROL_RAM_CLOCK_GATING	   4 : 4
#define GCREG_DC_CONTROL_RAM_CLOCK_GATING_DISABLED 0x0
#define GCREG_DC_CONTROL_RAM_CLOCK_GATING_ENABLED  0x1

/* Register gcregRegisterTimingControl **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Timing control register.  */

#define GCREG_REGISTER_TIMING_CONTROL_Address 0x0216C

#define GCREG_REGISTER_TIMING_CONTROL_FOR_PF1P 7 : 0

#define GCREG_REGISTER_TIMING_CONTROL_FOR_RF2P 15 : 8

#define GCREG_REGISTER_TIMING_CONTROL_FAST_RTC 17 : 16

#define GCREG_REGISTER_TIMING_CONTROL_FAST_WTC 19 : 18

#define GCREG_REGISTER_TIMING_CONTROL_POWER_DOWN 20 : 20

#define GCREG_REGISTER_TIMING_CONTROL_DEEP_SLEEP 21 : 21

#define GCREG_REGISTER_TIMING_CONTROL_LIGHT_SLEEP 22 : 22

#define GCREG_REGISTER_TIMING_CONTROL_RESERVED 31 : 23

/* Register gcregDebugCounterSelect **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Debug Counter Select Register.  */

#define GCREG_DEBUG_COUNTER_SELECT_Address 0x02170

/* Write a value to this field to pick up from 0~255 counters.  Then the      **
** counter will be in gcregDebugCounterValue.                                 */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT 7 : 0
/* //request number */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_AXI_RD_REQ_CNT 0x00
/* //read return data last number  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_AXI_RD_LAST_CNT 0x01
/* //number of 8bytes of request bytes  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_AXI_REQ_BURST_CNT 0x02
/* //number of 8bytes of read return data  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_AXI_RD_BURST_CUNT 0x03
/* //total pixels sent  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_PIXEL_CNT 0x04
/* //total frame sent  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_FRAME_CNT 0x05
/* //total dbi input cmd  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_INPUT_DBI_CMD_CNT 0x06
/* //total dbi output cmd  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_TOTAL_OUTPUT_DBI_CMD_CNT 0x07
/* //debug signals  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_DEBUG_SIGNALS0 0x08
/* //reset all debug counters  */
#define GCREG_DEBUG_COUNTER_SELECT_SELECT_RESET_ALL_DEBUG_COUNTERS 0xFF

/* Register gcregDebugCounterValue **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Debug Counter Value Register.  Debug Counter Value as specified in         **
** gcregDebugCounterSelect.                                                   */

#define GCREG_DEBUG_COUNTER_VALUE_Address 0x02174

/* Selected debug counter value  */
#define GCREG_DEBUG_COUNTER_VALUE_VALUE 31 : 0

/* Register gcregReserved **
** ~~~~~~~~~~~~~~~~~~~~~~ */

/* The register space is reserved for future use. */

#define GCREG_RESERVED_Address 0x02178

#define GCREG_RESERVED_RESERVED 31 : 0

/* Register gcregReserved1 **
** ~~~~~~~~~~~~~~~~~~~~~~~ */

/* The register space is reserved for future use. */

#define GCREG_RESERVED1_Address 0x0217C

#define GCREG_RESERVED1_RESERVED 31 : 0

/* Register gcregReservedGroup (8 in total) **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* The register space is reserved for future use. */

#define GCREG_RESERVED_GROUP_Address 0x02180

#define GCREG_RESERVED_GROUP_RESERVED 31 : 0

/* Register gcregLayerClockGate **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Layer clock gater Register. */

#define GCREG_LAYER_CLOCK_GATE_Address 0x021A0

#define GCREG_LAYER_CLOCK_GATE_DISABLE_VIDEO_CLK	  0 : 0
#define GCREG_LAYER_CLOCK_GATE_DISABLE_VIDEO_CLK_DISABLED 0x0
#define GCREG_LAYER_CLOCK_GATE_DISABLE_VIDEO_CLK_ENABLED  0x1

#define GCREG_LAYER_CLOCK_GATE_DISABLE_OVERLAY0_CLK	     1 : 1
#define GCREG_LAYER_CLOCK_GATE_DISABLE_OVERLAY0_CLK_DISABLED 0x0
#define GCREG_LAYER_CLOCK_GATE_DISABLE_OVERLAY0_CLK_ENABLED  0x1

#define GCREG_LAYER_CLOCK_GATE_DISABLE_OVERLAY1_CLK	     2 : 2
#define GCREG_LAYER_CLOCK_GATE_DISABLE_OVERLAY1_CLK_DISABLED 0x0
#define GCREG_LAYER_CLOCK_GATE_DISABLE_OVERLAY1_CLK_ENABLED  0x1

/* Register gcregDebugTotVideoReq **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_VIDEO_REQ_Address 0x021A4

/* Debug register */
#define GCREG_DEBUG_TOT_VIDEO_REQ_NUM 31 : 0

/* Register gcregDebugLstVideoReq **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_VIDEO_REQ_Address 0x021A8

/* Debug register */
#define GCREG_DEBUG_LST_VIDEO_REQ_NUM 31 : 0

/* Register gcregDebugTotVideoRrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_VIDEO_RRB_Address 0x021AC

/* Debug register */
#define GCREG_DEBUG_TOT_VIDEO_RRB_NUM 31 : 0

/* Register gcregDebugLstVideoRrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_VIDEO_RRB_Address 0x021B0

/* Debug register */
#define GCREG_DEBUG_LST_VIDEO_RRB_NUM 31 : 0

/* Register gcregDebugTotOverlay0Req **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_OVERLAY0_REQ_Address 0x021B4

/* Debug register */
#define GCREG_DEBUG_TOT_OVERLAY0_REQ_NUM 31 : 0

/* Register gcregDebugLstOverlay0Req **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_OVERLAY0_REQ_Address 0x021B8

/* Debug register */
#define GCREG_DEBUG_LST_OVERLAY0_REQ_NUM 31 : 0

/* Register gcregDebugTotOverlay0Rrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_OVERLAY0_RRB_Address 0x021BC

/* Debug register */
#define GCREG_DEBUG_TOT_OVERLAY0_RRB_NUM 31 : 0

/* Register gcregDebugLstOverlay0Rrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_OVERLAY0_RRB_Address 0x021C0

/* Debug register */
#define GCREG_DEBUG_LST_OVERLAY0_RRB_NUM 31 : 0

/* Register gcregDebugTotOverlay1Req **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_OVERLAY1_REQ_Address 0x021C4

/* Debug register */
#define GCREG_DEBUG_TOT_OVERLAY1_REQ_NUM 31 : 0

/* Register gcregDebugLstOverlay1Req **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_OVERLAY1_REQ_Address 0x021C8

/* Debug register */
#define GCREG_DEBUG_LST_OVERLAY1_REQ_NUM 31 : 0

/* Register gcregDebugTotOverlay1Rrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_OVERLAY1_RRB_Address 0x021CC

/* Debug register */
#define GCREG_DEBUG_TOT_OVERLAY1_RRB_NUM 31 : 0

/* Register gcregDebugLstOverlay1Rrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_OVERLAY1_RRB_Address 0x021D0

/* Debug register */
#define GCREG_DEBUG_LST_OVERLAY1_RRB_NUM 31 : 0

/* Register gcregDebugTotCursorReq **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_CURSOR_REQ_Address 0x021D4

/* Debug register */
#define GCREG_DEBUG_TOT_CURSOR_REQ_NUM 31 : 0

/* Register gcregDebugLstCursorReq **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_CURSOR_REQ_Address 0x021D8

/* Debug register */
#define GCREG_DEBUG_LST_CURSOR_REQ_NUM 31 : 0

/* Register gcregDebugTotCursorRrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_CURSOR_RRB_Address 0x021DC

/* Debug register */
#define GCREG_DEBUG_TOT_CURSOR_RRB_NUM 31 : 0

/* Register gcregDebugLstCursorRrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_CURSOR_RRB_Address 0x021E0

/* Debug register */
#define GCREG_DEBUG_LST_CURSOR_RRB_NUM 31 : 0

/* Register gcregDebugTotDCReq **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_DC_REQ_Address 0x021E4

/* Debug register */
#define GCREG_DEBUG_TOT_DC_REQ_NUM 31 : 0

/* Register gcregDebugLstDCReq **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_DC_REQ_Address 0x021E8

/* Debug register */
#define GCREG_DEBUG_LST_DC_REQ_NUM 31 : 0

/* Register gcregDebugTotDCRrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_TOT_DC_RRB_Address 0x021EC

/* Debug register */
#define GCREG_DEBUG_TOT_DC_RRB_NUM 31 : 0

/* Register gcregDebugLstDCRrb **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_LST_DC_RRB_Address 0x021F0

/* Debug register */
#define GCREG_DEBUG_LST_DC_RRB_NUM 31 : 0

/* Register gcregDebugFrameAndMisflag **
** ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_Address 0x021F4

/* Debug register */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_CURRENT_MISMATCH_FLAG_TOTAL 0 : 0

/* Debug register */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_CURRENT_MISMATCH_FLAG 4 : 1

/* Debug register */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_CURRENT_FRAME_CNT 15 : 5

/* Debug register */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_LAST_MISMATCH_FLAG_TOTAL 16 : 16

/* Debug register */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_LAST_MISMATCH_FLAG 20 : 17

/* Debug register */
#define GCREG_DEBUG_FRAME_AND_MISFLAG_LAST_FRAME_CNT 31 : 21

#endif /* __gcregDisplay_h__ */
