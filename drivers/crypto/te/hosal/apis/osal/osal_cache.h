//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_CACHE_H__
#define __OSAL_CACHE_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The cache line size is tight with different platform
 */
#define OSAL_CACHE_LINE_SIZE	(64U)

OSAL_API osal_err_t osal_cache_clean(uint8_t *buf, size_t size);
OSAL_API osal_err_t osal_cache_flush(uint8_t *buf, size_t size);
OSAL_API osal_err_t osal_cache_invalidate(uint8_t *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_CACHE_H__ */
