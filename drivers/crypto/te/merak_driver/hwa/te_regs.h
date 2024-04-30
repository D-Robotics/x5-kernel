//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUST_ENGINE_REGS_H__
#define __TRUST_ENGINE_REGS_H__

#include "common_regs.h"
#include "host_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

static inline void te_write32(uint32_t val, void* addr)
{
    *(volatile uint32_t*)addr = val;
}

static inline uint32_t te_read32(void* addr)
{
    return *(volatile uint32_t*)addr;
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUST_ENGINE_REGS_H__ */
