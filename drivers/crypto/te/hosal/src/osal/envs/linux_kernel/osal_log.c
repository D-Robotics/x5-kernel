//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <linux/kernel.h>
#include <linux/printk.h>
#include "osal_log.h"
#include <linux/stdarg.h>

extern int vprintk_default(const char *fmt, va_list args);

__attribute__((format(printf, 1, 2))) int32_t osal_log_printf(const char *fmt,
                                                              ...)
{
    va_list args;

    va_start(args, fmt);
    vprintk_default(fmt, args);
    va_end(args);

    return 0;
}
