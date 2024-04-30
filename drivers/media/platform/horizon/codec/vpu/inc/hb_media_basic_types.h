/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/
#ifndef __HB_MEDIA_BASIC_TYPES_H__
#define __HB_MEDIA_BASIC_TYPES_H__

#include <linux/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** hb_u8 is an 8 bit unsigned quantity that is byte aligned */
typedef uint8_t hb_u8;

/** hb_s8 is an 8 bit signed quantity that is byte aligned */
typedef int8_t hb_s8;

/** hb_u16 is a 16 bit unsigned quantity that is 16 bit word aligned */
typedef uint16_t hb_u16;

/** hb_s16 is a 16 bit signed quantity that is 16 bit word aligned */
typedef int16_t hb_s16;

/** hb_u32 is a 32 bit unsigned quantity that is 32 bit word aligned */
typedef uint32_t hb_u32;

/** hb_s32 is a 32 bit signed quantity that is 32 bit word aligned */
typedef int32_t hb_s32;

/** hb_u64 is a 64 bit unsigned quantity that is 64 bit word aligned */
typedef uint64_t hb_u64;

/** hb_s64 is a 64 bit signed quantity that is 64 bit word aligned */
typedef int64_t hb_s64;

/**
* The hb_bool type is intended to be used to represent a true or a false
* value. The hb_bool is a 32 bit quantity and is aligned on a 32 bit word
* boundary.
*/
typedef int32_t hb_bool;

/**
* The hb_ptr type is intended to be used to pass pointers.
* This is a 32 bit pointer and is aligned on a 32 bit boundary.
*/
typedef void *hb_ptr;

/**
* The hb_string type is intended to be used to pass "C" type strings.
* The hb_string type is a 32 bit pointer to a zero terminated string.
* The pointer is word aligned and the string is byte aligned.
*/
typedef char *hb_string;

/**
* The hb_byte type is intended to be used to pass arrays of bytes such as
* buffers. The hb_byte type is a 32 bit pointer to a zero terminated string.
* The pointer is word aligned and the string is byte aligned.
*/
typedef unsigned char *hb_byte;

#define HB_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __HB_MEDIA_BASIC_TYPES_H__ */
