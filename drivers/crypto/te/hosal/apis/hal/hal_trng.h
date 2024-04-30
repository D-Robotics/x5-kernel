//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __HAL_TRNG_H__
#define __HAL_TRNG_H__

#include "osal_err.h"
#include "hal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

osal_err_t hal_trng_gen(uint8_t *buf, size_t size);

void hal_trng_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_TRNG_H__ */
