/****************************************************************************
*
*    Copyright 2012 - 2022 Vivante Corporation, Santa Clara, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#include "nano2D_user_query.h"

#define NAMEFORMAT(format) #format,N2D_##format

/*
** Notice: It needs to be consistent with the definition order of the format enumeration
*/
/*RGBA format info.*/
static struct FormatInfo RGBAFormatInfoCom[] = {
    /* RGBA packed */
    {
        NAMEFORMAT(RGBA8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(RGBX8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(R10G10B10A2), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(R5G5B5A1), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(R5G5B5X1), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(RGBA4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(RGBX4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(RGB565), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(RGB888), N2D_FORMARTTYPE_RGBA, 24, N2D_ONEPLANE, {24, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(RGB888I), N2D_FORMARTTYPE_RGBA, 24, N2D_ONEPLANE, {24, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(RGB161616I), N2D_FORMARTTYPE_RGBA, 48, N2D_ONEPLANE, {48, 0, 0}, 1, 1, {4, 0, 0}, {6, 0, 0},
    },

    /* BGRA packed */
    {
        NAMEFORMAT(BGRA8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(BGRX8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(B10G10R10A2), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(B5G5R5A1), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(B5G5R5X1), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(BGRA4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(BGRX4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(BGR565), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(BGR888), N2D_FORMARTTYPE_RGBA, 24, N2D_ONEPLANE, {24, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    /* ABGR packed */
    {
        NAMEFORMAT(ABGR8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(XBGR8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(A2B10G10R10), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(A1B5G5R5), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(X1B5G5R5), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(ABGR4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(XBGR4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(ABGR16161616I), N2D_FORMARTTYPE_RGBA, 64, N2D_ONEPLANE, {64, 0, 0}, 1, 1, {4, 0, 0}, {6, 0, 0},
    },

    {
        NAMEFORMAT(ABGR8888I), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    /* ARGB packed */
    {
        NAMEFORMAT(ARGB8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(XRGB8888), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(A2R10G10B10), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(A1R5G5B5), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(X1R5G5B5), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(ARGB4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(XRGB4444), N2D_FORMARTTYPE_RGBA, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {2, 0, 0}, {2, 0, 0},
    },

    {
        NAMEFORMAT(ARGB16161616I), N2D_FORMARTTYPE_RGBA, 64, N2D_ONEPLANE, {64, 0, 0}, 1, 1, {4, 0, 0}, {6, 0, 0},
    },

    {
        NAMEFORMAT(ARGB8888I), N2D_FORMARTTYPE_RGBA, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {4, 0, 0}, {4, 0, 0},
    },

    /* RGB planar */
    {
        NAMEFORMAT(RGB888_PLANAR), N2D_FORMARTTYPE_RGBA, 24, N2D_THREEPLANES, {8, 8, 8}, 1, 1, {4, 4, 4}, {4, 4, 4},
    },

    {
        NAMEFORMAT(RGB888I_PLANAR), N2D_FORMARTTYPE_RGBA, 24, N2D_THREEPLANES, {8, 8, 8}, 1, 1, {4, 4, 4}, {4, 4, 4},
    },

    {
        NAMEFORMAT(RGB161616I_PLANAR), N2D_FORMARTTYPE_RGBA, 48, N2D_THREEPLANES, {16, 16, 16}, 1, 1, {8, 8, 8}, {8, 8, 8},
    },
};

/*YUV format info.*/
static struct FormatInfo YUVFormatInfoCom[] = {
    {
        NAMEFORMAT(YUYV), N2D_FORMARTTYPE_YUV, 16, N2D_ONEPLANE, {16, 0, 0}, 2, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(UYVY), N2D_FORMARTTYPE_YUV, 16, N2D_ONEPLANE, {16, 0, 0}, 2, 1, {4, 0, 0}, {4, 0, 0},
    },

    {
        NAMEFORMAT(YV12), N2D_FORMARTTYPE_YUV, 24, N2D_THREEPLANES, {8, 8, 8}, 2, 2, {64, 32, 32}, {64, 64, 64},
    },

    {
        NAMEFORMAT(I420), N2D_FORMARTTYPE_YUV, 24, N2D_THREEPLANES, {8, 8, 8}, 2, 2, {64, 32, 32}, {64, 64, 64},
    },

    {
        NAMEFORMAT(NV12), N2D_FORMARTTYPE_YUV, 16, N2D_TWOPLANES, {8, 8, 0}, 2, 2, {64, 64, 0}, {64, 64, 0},
    },

    {
        NAMEFORMAT(NV21), N2D_FORMARTTYPE_YUV, 16, N2D_TWOPLANES, {8, 8, 0}, 2, 2, {64, 64, 0}, {64, 64, 0},
    },

    {
        NAMEFORMAT(NV16), N2D_FORMARTTYPE_YUV, 16, N2D_TWOPLANES, {8, 8, 0}, 2, 1, {64, 64, 0}, {64, 64, 0},
    },

    {
        NAMEFORMAT(NV61), N2D_FORMARTTYPE_YUV, 16, N2D_TWOPLANES, {8, 8, 0}, 2, 1, {64, 64, 0}, {64, 64, 0},
    },

    {
        NAMEFORMAT(AYUV), N2D_FORMARTTYPE_YUV, 24, N2D_THREEPLANES, {8, 8, 8}, 1, 1, {16, 16, 16}, {16, 16, 16},
    },

    {
        NAMEFORMAT(GRAY8), N2D_FORMARTTYPE_YUV, 8, N2D_ONEPLANE, {8, 0, 0}, 1, 1, {16, 0, 0}, {16, 0, 0},
    },

    {
        NAMEFORMAT(GRAY8I), N2D_FORMARTTYPE_YUV, 8, N2D_ONEPLANE, {8, 0, 0}, 1, 1, {16, 0, 0}, {16, 0, 0},
    },

    {
        NAMEFORMAT(NV12_10BIT), N2D_FORMARTTYPE_YUV, 24, N2D_TWOPLANES, {16, 8, 0}, 4, 2, {80, 80, 0}, {80, 80, 0},
    },

    {
        NAMEFORMAT(NV21_10BIT), N2D_FORMARTTYPE_YUV, 24, N2D_TWOPLANES, {16, 8, 0}, 4, 2, {80, 80, 0}, {80, 80, 0},
    },

    {
        NAMEFORMAT(NV16_10BIT), N2D_FORMARTTYPE_YUV, 32, N2D_TWOPLANES, {16, 16, 0}, 2, 1, {40, 40, 0}, {80, 80, 0},
    },

    {
        NAMEFORMAT(NV61_10BIT), N2D_FORMARTTYPE_YUV, 32, N2D_TWOPLANES, {16, 16, 0}, 2, 1, {40, 40, 0}, {80, 80, 0},
    },

    /* P010: like NV12, with 10bpp per component */
    {
        NAMEFORMAT(P010_MSB), N2D_FORMARTTYPE_YUV, 32, N2D_TWOPLANES, {16, 16, 0}, 2, 2, {128, 128, 0}, {128, 128, 0},
    },

    {
        NAMEFORMAT(P010_LSB), N2D_FORMARTTYPE_YUV, 32, N2D_TWOPLANES, {16, 16, 0}, 2, 2, {128, 128, 0}, {128, 128, 0},
    },

    /* I010: like YV12, with 10bpp per component */
    {
        NAMEFORMAT(I010), N2D_FORMARTTYPE_YUV, 48, N2D_THREEPLANES, {16, 16, 16}, 2, 2, {128, 64, 64}, {128, 64, 64},
    },

    {
        NAMEFORMAT(I010_LSB), N2D_FORMARTTYPE_YUV, 48, N2D_THREEPLANES, {16, 16, 16}, 2, 2, {128, 64, 64}, {128, 64, 64},
    },
};

/*Alpha format info.*/
static struct FormatInfo AlphaFormatInfoCom[] = {
    {
        NAMEFORMAT(A8), N2D_FORMARTTYPE_RGBA, 8, N2D_ONEPLANE, {8, 0, 0}, 1, 1, {1, 0, 0}, {1, 0, 0},
    },
};

/*Index format info.*/
static struct FormatInfo IndexFormatInfoCom[] = {
    {
        NAMEFORMAT(INDEX1), N2D_FORMARTTYPE_INDEX, 0, N2D_ONEPLANE, {0, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(INDEX8), N2D_FORMARTTYPE_INDEX, 8, N2D_ONEPLANE, {8, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },
};

/*Float format info.*/
static struct FormatInfo FloatFormatInfoCom[] = {
    {
        NAMEFORMAT(RGB161616F), N2D_FORMARTTYPE_FLOAT, 48, N2D_ONEPLANE, {48, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(RGB323232F), N2D_FORMARTTYPE_FLOAT, 96, N2D_ONEPLANE, {96, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(ARGB16161616F), N2D_FORMARTTYPE_FLOAT, 64, N2D_ONEPLANE, {64, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(ABGR16161616F), N2D_FORMARTTYPE_FLOAT, 64, N2D_ONEPLANE, {64, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(ARGB32323232F), N2D_FORMARTTYPE_RGBA, 128, N2D_ONEPLANE, {128, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(ABGR32323232F), N2D_FORMARTTYPE_RGBA, 128, N2D_ONEPLANE, {128, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(RGB161616F_PLANAR), N2D_FORMARTTYPE_FLOAT, 48, N2D_THREEPLANES, {16, 16, 16}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(RGB323232F_PLANAR), N2D_FORMARTTYPE_FLOAT, 96, N2D_THREEPLANES, {32, 32, 32}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(GRAY16F), N2D_FORMARTTYPE_FLOAT, 16, N2D_ONEPLANE, {16, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },

    {
        NAMEFORMAT(GRAY32F), N2D_FORMARTTYPE_FLOAT, 32, N2D_ONEPLANE, {32, 0, 0}, 1, 1, {0, 0, 0}, {0, 0, 0},
    },
};

static FormatInfoArray_t formatInfoArray[] = {
    /*0 ~ 0x100*/
    {N2D_NULL,0},
    /* 0x100~0x200 */
    {RGBAFormatInfoCom,N2D_COUNTOF(RGBAFormatInfoCom)},
    /* 0x200~0x300 */
    {YUVFormatInfoCom,N2D_COUNTOF(YUVFormatInfoCom)},
    /* 0x300~0x400 */
    {AlphaFormatInfoCom,N2D_COUNTOF(AlphaFormatInfoCom)},
    /* 0x400~0x500 */
    {IndexFormatInfoCom,N2D_COUNTOF(IndexFormatInfoCom)},
    /* 0x500~0x600 */
    {FloatFormatInfoCom,N2D_COUNTOF(FloatFormatInfoCom)},
};

n2d_error_t gcGetFormatInfo (
    IN n2d_buffer_format_t   Format,
    OUT FormatInfo_t         **FormatInfo
    )
{
    n2d_error_t     error = N2D_SUCCESS;
    n2d_uint32_t    arrayIndex  = Format / 0x100;
    n2d_uint32_t    formatIndex = Format % 0x100;

    if (arrayIndex >= N2D_COUNTOF(formatInfoArray) || formatIndex > formatInfoArray[arrayIndex].count)
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);

    *FormatInfo = &formatInfoArray[arrayIndex].formatInfo[formatIndex];

on_error:
    /* Return the status. */
    return error;

}

