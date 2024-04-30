//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_SCA_H__
#define __TRUSTENGINE_HWA_SCA_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_sca_regs;
struct te_hwa_host;

typedef enum te_sca_opcode_idx {
    OPCODE_IDX_NONE = 0,
    OPCODE_IDX_INIT,
    OPCODE_IDX_PROC,
    OPCODE_IDX_FINISH,
    OPCODE_IDX_CLONE,
    OPCODE_IDX_WRAPOUT,
    OPCODE_IDX_UNWRAPIN,
    OPCODE_IDX_CLEAR,
} te_sca_opcode_idx_t;

typedef enum te_sca_host_stat {
    HOST_STAT_RESET = 0,
    HOST_STAT_INACTIVE,
    HOST_STAT_ACTIVE,
    HOST_STAT_SUSPEND,
} te_sca_host_stat_t;

typedef struct te_sca_csq_entry {
    uint32_t stat:5;
    uint32_t code:3;
    uint32_t slot:5;
} te_sca_csq_entry_t;

typedef struct te_sca_stat {
    uint32_t host_stat:2;
    uint32_t actv_slot_id:5;
    uint32_t csq_ocpd_slots:4;
    uint32_t cq_avail_slots:5;
} te_sca_stat_t;

typedef struct te_sca_ctl {
    uint32_t csq_en:1;
    uint32_t cq_wm:5;
    uint32_t clk_en:1;
    uint32_t run:1;
} te_sca_ctl_t;

typedef struct te_sca_int_stat {
    uint32_t cq_wm:1;
    uint32_t opcode_err:1;
    uint32_t csq_rd_err:1;
    uint32_t cq_wr_err:1;
    uint32_t axi_to_err:1;
    uint32_t para_err:1;
} te_sca_int_stat_t;

typedef struct te_sca_int {
    te_sca_int_stat_t stat;
    uint32_t cmd_fin;
    uint32_t op_err;
} te_sca_int_t;

typedef struct te_sca_axi_err {
    uint32_t err;
    uint32_t slot;
    uint32_t addr_hi;
    uint32_t addr_lo;
} te_sca_axi_err_t;

typedef struct te_sca_err_info {
    uint32_t cmd_err;
    uint32_t key_err;
    uint32_t slot_err;
    te_sca_axi_err_t axi;
    uint32_t cq_wdata;
} te_sca_err_info_t;

typedef struct te_sca_suspd_msk {
    uint32_t opcode_err:1;
    uint32_t csq_rd_err:1;
    uint32_t cq_wr_err:1;
    uint32_t op_err:1;
    uint32_t cmd_fin:1;
    uint32_t para_err:1;
} te_sca_suspd_msk_t;

/**
 * Trust engine SCA HWA structure
 */
typedef struct te_hwa_sca {
    te_hwa_crypt_t base;
    int (*cq_write_func)(struct te_hwa_sca *h, const uint32_t func);
    int (*cq_write_para)(struct te_hwa_sca *h, const uint32_t *para,
                         uint32_t nbytes);
    int (*csq_read)(struct te_hwa_sca *h, te_sca_csq_entry_t *ent);
    int (*get_ctrl)(struct te_hwa_sca *h, te_sca_ctl_t *ctl);
    int (*set_ctrl)(struct te_hwa_sca *h, const te_sca_ctl_t *ctl);
    int (*state)(struct te_hwa_sca *h, te_sca_stat_t *stat);
    int (*int_state)(struct te_hwa_sca *h, te_sca_int_t *status);
    int (*eoi)(struct te_hwa_sca *h, const te_sca_int_t *status);
    int (*get_int_msk)(struct te_hwa_sca *h, te_sca_int_t *msk);
    int (*set_int_msk)(struct te_hwa_sca *h, const te_sca_int_t *msk);
    int (*get_err_info)(struct te_hwa_sca *h, te_sca_err_info_t *info);
    int (*get_key)(struct te_hwa_sca *h, uint8_t key[32]);
    int (*get_suspd_msk)(struct te_hwa_sca *h, te_sca_suspd_msk_t *suspd);
    int (*set_suspd_msk)(struct te_hwa_sca *h, const te_sca_suspd_msk_t *suspd);
} te_hwa_sca_t;

int te_hwa_sca_alloc( struct te_sca_regs *regs,
                      struct te_hwa_host *host,
                      bool ishash,
                      te_hwa_sca_t **hwa );

int te_hwa_sca_free( te_hwa_sca_t *hwa );

int te_hwa_sca_init( struct te_sca_regs *regs,
                     struct te_hwa_host *host,
                     bool ishash,
                     te_hwa_sca_t *hwa );

int te_hwa_sca_exit( te_hwa_sca_t *hwa );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_SCA_H__ */
