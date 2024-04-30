//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/random.h>
#include "hal_trng.h"

osal_err_t hal_trng_gen(uint8_t *buf, size_t size)
{
    get_random_bytes(buf, size);
    return OSAL_SUCCESS;
}

void hal_trng_cleanup(void)
{
    return;
}
