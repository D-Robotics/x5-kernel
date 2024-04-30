//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#ifndef __OSAL_STRING_H__
#define __OSAL_STRING_H__

#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define osal_memset memset
#define osal_memcpy memcpy
#define osal_memcmp memcmp
#define osal_strlen strlen
#define osal_strcpy strcpy
#define osal_strncpy strncpy
#define osal_strcmp strcmp
#define osal_strncmp strncmp
#define osal_snprintf snprintf

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_STRING_H__ */
