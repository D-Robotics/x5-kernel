//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <asm/barrier.h>
#include "osal_mem_barrier.h"

void osal_wmb(void)
{
    smp_wmb();
    return;
}

void osal_rmb(void)
{
    smp_rmb();
    return;
}
