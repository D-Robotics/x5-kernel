//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRNGCTL_REGS_H__
#define __TRNGCTL_REGS_H__

#include "trngctl/ctrl.h"
#include "trngctl/entropy_src.h"
#include "trngctl/entropy_sample.h"
#include "trngctl/ro_setting.h"
#include "trngctl/post_proc.h"
#include "trngctl/eval_setting.h"
#include "trngctl/stat.h"
#include "trngctl/intr.h"
#include "trngctl/intr_msk.h"
#include "trngctl/err_cnt_th.h"

#define TRNGCTL_OFS_CTRL             0x0000
#define TRNGCTL_OFS_ENTROPY_SRC      0x0004
#define TRNGCTL_OFS_ENTROPY_SAMPLE   0x0008
#define TRNGCTL_OFS_RO_SETTING       0x000c
#define TRNGCTL_OFS_POST_PROC        0x0010
#define TRNGCTL_OFS_EVAL_SETTING     0x0014
#define TRNGCTL_OFS_STAT             0x0018
#define TRNGCTL_OFS_INTR             0x001c
#define TRNGCTL_OFS_INTR_MSK         0x0020
#define TRNGCTL_OFS_ERR_CNT_TH       0x0024
#define TRNGCTL_OFS_VN_TOTAL         0x0028
#define TRNGCTL_OFS_CRNG_TOTAL       0x002c
#define TRNGCTL_OFS_REP_TOTAL        0x0030
#define TRNGCTL_OFS_ADAP_TOTAL       0x0034

#define TRNGCTL_REGS_SIZE            0x0038

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_trngctl module register file definition.
 */
typedef struct te_trngctl_regs {
    volatile trngctl_ctrlReg_t ctrl; /**< +0x000 control */
    volatile trngctl_entropy_srcReg_t entropy_src; /**< +0x004 entropy source */
    volatile trngctl_entropy_sampleReg_t entropy_sample; /**< +0x008  */
    volatile trngctl_ro_settingReg_t ro_setting; /**< +0x00c  */
    volatile trngctl_post_procReg_t post_proc; /**< +0x010 post processing */
    volatile trngctl_eval_settingReg_t eval_setting; /**< +0x014 evaluation and test */
    volatile trngctl_statReg_t stat; /**< +0x018  */
    volatile trngctl_intrReg_t intr; /**< +0x01c  */
    volatile trngctl_intr_mskReg_t intr_msk; /**< +0x020  */
    volatile trngctl_err_cnt_thReg_t err_cnt_th; /**< +0x024  */
    volatile uint32_t vn_total;      /**< +0x028 VN total counter */
    volatile uint32_t crng_total;    /**< +0x02c CRNG total counter */
    volatile uint32_t rep_total;     /**< +0x030 REP total counter */
    volatile uint32_t adap_total;    /**< +0x034 ADAPT total counter */
} te_trngctl_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRNGCTL_REGS_H__ */
