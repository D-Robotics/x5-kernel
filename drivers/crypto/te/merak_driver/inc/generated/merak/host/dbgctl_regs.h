//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DBGCTL_REGS_H__
#define __DBGCTL_REGS_H__


#define DBGCTL_OFS_CTRL             0x0000
#define DBGCTL_OFS_LOCK             0x0004

#define DBGCTL_REGS_SIZE            0x0008

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_dbgctl module register file definition.
 */
typedef struct te_dbgctl_regs {
    volatile uint32_t ctrl;          /**< +0x000 debug ctrl */
    volatile uint32_t lock;          /**< +0x004 debug ctlr lock */
} te_dbgctl_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DBGCTL_REGS_H__ */
