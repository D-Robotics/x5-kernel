//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_DBGCTL_H__
#define __TRUSTENGINE_HWA_DBGCTL_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_dbgctl_regs;
struct te_hwa_host;

/**
 * Trust engine debug control HWA structure
 */
typedef struct te_hwa_dbgctl {
    te_hwa_crypt_t base;
    int (*get_dbgctl)(struct te_hwa_dbgctl *h, uint32_t *ctl);
    int (*set_dbgctl)(struct te_hwa_dbgctl *h, const uint32_t ctl);
    int (*get_locken)(struct te_hwa_dbgctl *h, uint32_t *lock);
    int (*set_locken)(struct te_hwa_dbgctl *h, const uint32_t lock);
} te_hwa_dbgctl_t;

int te_hwa_dbgctl_alloc( struct te_dbgctl_regs *regs,
                         struct te_hwa_host *host,
                         te_hwa_dbgctl_t **hwa );

int te_hwa_dbgctl_free( te_hwa_dbgctl_t *hwa );

int te_hwa_dbgctl_init( struct te_dbgctl_regs *regs,
                        struct te_hwa_host *host,
                        te_hwa_dbgctl_t *hwa );

int te_hwa_dbgctl_exit( te_hwa_dbgctl_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_DBGCTL_H__ */
