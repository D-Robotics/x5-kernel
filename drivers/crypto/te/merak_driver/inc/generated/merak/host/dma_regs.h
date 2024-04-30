//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_REGS_H__
#define __DMA_REGS_H__

#include "dma/timeout_ctrl.h"
#include "dma/arb_status.h"
#include "dma/sca_intr_stat.h"
#include "dma/sca_aw_ctrl.h"
#include "dma/sca_ar_ctrl.h"
#include "dma/sca_usr_ctrl.h"
#include "dma/sca_ace_ctrl.h"
#include "dma/sca_dma_misc.h"
#include "dma/sca_dma_fsm.h"
#include "dma/hash_intr_stat.h"
#include "dma/hash_aw_ctrl.h"
#include "dma/hash_ar_ctrl.h"
#include "dma/hash_usr_ctrl.h"
#include "dma/hash_ace_ctrl.h"
#include "dma/hash_dma_misc.h"
#include "dma/hash_dma_fsm.h"

#define DMA_OFS_TIMEOUT_CTRL     0x0000
#define DMA_OFS_ARB_STATUS       0x0004
#define DMA_OFS_SCA_INTR_STAT    0x0040
#define DMA_OFS_SCA_AW_CTRL      0x0044
#define DMA_OFS_SCA_AR_CTRL      0x0048
#define DMA_OFS_SCA_USR_CTRL     0x004c
#define DMA_OFS_SCA_ACE_CTRL     0x0050
#define DMA_OFS_SCA_DMA_MISC     0x0054
#define DMA_OFS_SCA_DMA_FSM      0x0058
#define DMA_OFS_HASH_INTR_STAT   0x0080
#define DMA_OFS_HASH_AW_CTRL     0x0084
#define DMA_OFS_HASH_AR_CTRL     0x0088
#define DMA_OFS_HASH_USR_CTRL    0x008c
#define DMA_OFS_HASH_ACE_CTRL    0x0090
#define DMA_OFS_HASH_DMA_MISC    0x0094
#define DMA_OFS_HASH_DMA_FSM     0x0098

#define DMA_REGS_SIZE            0x009c

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * te_dma module register file definition.
 */
typedef struct te_dma_regs {
    volatile dma_timeout_ctrlReg_t timeout_ctrl; /**< +0x000  */
    volatile dma_arb_statusReg_t arb_status; /**< +0x004  */
    volatile uint32_t hole2[14];
    volatile dma_sca_intr_statReg_t sca_intr_stat; /**< +0x040  */
    volatile dma_sca_aw_ctrlReg_t sca_aw_ctrl; /**< +0x044  */
    volatile dma_sca_ar_ctrlReg_t sca_ar_ctrl; /**< +0x048  */
    volatile dma_sca_usr_ctrlReg_t sca_usr_ctrl; /**< +0x04c  */
    volatile dma_sca_ace_ctrlReg_t sca_ace_ctrl; /**< +0x050  */
    volatile dma_sca_dma_miscReg_t sca_dma_misc; /**< +0x054 misc status of DMA for SCA */
    volatile dma_sca_dma_fsmReg_t sca_dma_fsm; /**< +0x058 status of DMA FSM for SCA */
    volatile uint32_t hole9[9];
    volatile dma_hash_intr_statReg_t hash_intr_stat; /**< +0x080  */
    volatile dma_hash_aw_ctrlReg_t hash_aw_ctrl; /**< +0x084  */
    volatile dma_hash_ar_ctrlReg_t hash_ar_ctrl; /**< +0x088  */
    volatile dma_hash_usr_ctrlReg_t hash_usr_ctrl; /**< +0x08c  */
    volatile dma_hash_ace_ctrlReg_t hash_ace_ctrl; /**< +0x090  */
    volatile dma_hash_dma_miscReg_t hash_dma_misc; /**< +0x094 misc status for DMA for HASH */
    volatile dma_hash_dma_fsmReg_t hash_dma_fsm; /**< +0x098 status of DMA FSM for HASH */
} te_dma_regs_t;

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_REGS_H__ */
