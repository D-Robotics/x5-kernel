//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "osal_assert.h"

__attribute__((__noreturn__)) void osal_assert_enter(void)
{
    while (1)
        ;
}