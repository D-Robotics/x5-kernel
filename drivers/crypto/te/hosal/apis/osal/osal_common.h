//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_COMMON_H__
#define __OSAL_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined(OSAL_ENV_UBOOT)
#include <common.h>
#include <malloc.h>
#elif defined(OSAL_ENV_LINUX_KERNEL)
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#endif

#define OSAL_API

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_COMMON_H__ */
