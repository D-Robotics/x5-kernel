//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_REGS_H__
#define __SCA_REGS_H__

#include "sca/csq.h"
#include "sca/ctrl.h"
#include "sca/stat.h"
#include "sca/intr_stat0.h"
#include "sca/err_stat4.h"
#include "sca/intr_msk0.h"
#include "sca/suspd_msk.h"

#define SCA_OFS_CQ_FUNC          0x0000
#define SCA_OFS_CQ_PARA          0x0004
#define SCA_OFS_CSQ              0x0008
#define SCA_OFS_CTRL             0x000c
#define SCA_OFS_STAT             0x0010
#define SCA_OFS_INTR_STAT0       0x0014
#define SCA_OFS_INTR_STAT1       0x0018
#define SCA_OFS_INTR_STAT2       0x001c
#define SCA_OFS_ERR_STAT0        0x0020
#define SCA_OFS_ERR_STAT1        0x0024
#define SCA_OFS_ERR_STAT2        0x0028
#define SCA_OFS_ERR_STAT3        0x002c
#define SCA_OFS_ERR_STAT4        0x0030
#define SCA_OFS_ERR_STAT5        0x0034
#define SCA_OFS_ERR_STAT6        0x0038
#define SCA_OFS_ERR_STAT7        0x003c
#define SCA_OFS_INTR_MSK0        0x0040
#define SCA_OFS_INTR_MSK1        0x0044
#define SCA_OFS_INTR_MSK2        0x0048
#define SCA_OFS_KEY              0x004c
#define SCA_OFS_SUSPD_MSK        0x006c

#define SCA_REGS_SIZE            0x0070

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_sca module register file definition.
 */
typedef struct te_sca_regs {
    volatile uint32_t cq_func;       /**< +0x000 command queue for function */
    volatile uint32_t cq_para;       /**< +0x004 command queue for parameters */
    volatile sca_csqReg_t csq;       /**< +0x008 command status queue */
    volatile sca_ctrlReg_t ctrl;     /**< +0x00c control */
    volatile sca_statReg_t stat;     /**< +0x010 status */
    volatile sca_intr_stat0Reg_t intr_stat0; /**< +0x014 interrupt status register0 */
    volatile uint32_t intr_stat1;    /**< +0x018 interrupt status register1 */
    volatile uint32_t intr_stat2;    /**< +0x01c interrupt status register2 */
    volatile uint32_t err_stat0;     /**< +0x020 error status register0 */
    volatile uint32_t err_stat1;     /**< +0x024 error status register1 */
    volatile uint32_t err_stat2;     /**< +0x028 error status register2 */
    volatile uint32_t err_stat3;     /**< +0x02c error status register3 */
    volatile sca_err_stat4Reg_t err_stat4; /**< +0x030 error status register4 */
    volatile uint32_t err_stat5;     /**< +0x034 error status register5 */
    volatile uint32_t err_stat6;     /**< +0x038 error status register6 */
    volatile uint32_t err_stat7;     /**< +0x03c error status register7 */
    volatile sca_intr_msk0Reg_t intr_msk0; /**< +0x040 interrupt mask register0 */
    volatile uint32_t intr_msk1;     /**< +0x044 interrupt mask register1 */
    volatile uint32_t intr_msk2;     /**< +0x048 interrupt mask register2 */
    volatile uint32_t key[8];        /**< +0x04c key-ladder derived key */
    volatile sca_suspd_mskReg_t suspd_msk; /**< +0x06c suspend mask */
} te_sca_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_REGS_H__ */
