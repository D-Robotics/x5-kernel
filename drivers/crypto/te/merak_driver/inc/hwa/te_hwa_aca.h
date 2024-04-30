//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_ACA_H__
#define __TRUSTENGINE_HWA_ACA_H__

#include "te_hwa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_aca_regs;
struct te_hwa_host;

typedef enum te_aca_op_code {
    TE_ACA_OP_ADD          = 0x01,
    TE_ACA_OP_SUB          = 0x02,
    TE_ACA_OP_MUL_LOW      = 0x03,
    TE_ACA_OP_DIV          = 0x04,
    TE_ACA_OP_AND          = 0x05,
    TE_ACA_OP_OR           = 0x06,
    TE_ACA_OP_XOR          = 0x07,
    TE_ACA_OP_SHR0         = 0x08,
    TE_ACA_OP_SHL0         = 0x0A,
    TE_ACA_OP_SHL1         = 0x0B,
    TE_ACA_OP_MUL_HIGH     = 0x0C,
    TE_ACA_OP_MODRED       = 0x10,
    TE_ACA_OP_MODADD       = 0x11,
    TE_ACA_OP_MODSUB       = 0x12,
    TE_ACA_OP_MODMUL       = 0x13,
    TE_ACA_OP_MODINV       = 0x14,
    TE_ACA_OP_MODEXP       = 0x15,
    TE_ACA_OP_MODMULNR     = 0x16,
    TE_ACA_OP_MODMULACC    = 0x17,
    TE_ACA_OP_MODMULACCNR  = 0x18,
    TE_ACA_OP_MODMUL2      = 0x1B,
    TE_ACA_OP_MODMUL2NR    = 0x1C,
    TE_ACA_OP_MODMUL7      = 0x1D,
    TE_ACA_OP_MODMUL7NR    = 0x1E,
    TE_ACA_OP_MODMULACC7NR = 0x1F,
} te_aca_op_code_t;

/**
 * operand b type.
 *   1. OP=SHR0|SHL0|SHL1, MSB=x  -- immediate
 *   2. OP=others
 *      MSB = 1                   -- immediate
 *      MSB = 0                   -- GR ID
 */
typedef enum te_aca_operandb_type {
    TE_ACA_OPERAND_B_GR  = 0,        /**< GR ID */
    TE_ACA_OPERAND_B_IMM = (1 << 5), /**< immediate */
} te_aca_operandb_type_t;

#define TE_ACA_MK_B_IMM(imm) (TE_ACA_OPERAND_B_IMM | ((imm)&0x1F))
#define TE_ACA_MK_B_GR(id) (TE_ACA_OPERAND_B_GR | ((id)&0x1F))

typedef enum te_aca_engine_stat {
    TE_ACA_ENGINE_IDLE                = 0x0,
    TE_ACA_ENGINE_BUSY_BY_OTHER_HOSTS = 0x1,
    TE_ACA_ENGINE_BUSY_BY_THIS_HOSTS  = 0x2,
    TE_ACA_ENGINE_UNDER_RESET         = 0x3,
} te_aca_engine_stat_t;

typedef enum te_aca_sram_mode {
    TE_ACA_SRAM_8K  = 0x1,
    TE_ACA_SRAM_16K = 0x2,
} te_aca_sram_mode_t;

typedef struct te_aca_op_entry {
    uint32_t need_intr : 1;
    uint32_t op_c_id : 5;
    uint32_t op_r_id : 5;
    uint32_t no_save_to_r : 1;
    uint32_t op_b : 6;
    uint32_t op_a_id : 5;
    uint32_t len_type_id : 4;
    uint32_t op_code : 5;
} te_aca_op_entry_t;

typedef struct te_aca_ctrl {
    uint32_t run : 1;
    uint32_t rsvd0 : 3;
    uint32_t fifo_wm : 4;
    uint32_t rsvd1 : 1;
    uint32_t rsvd2 : 1;
    uint32_t clock_enable : 1;
} te_aca_ctrl_t;

typedef struct te_aca_stat {
    uint32_t op_fifo_free_num : 4;
    uint32_t op_fifo_empty : 1;
    uint32_t op_fifo_full : 1;
    uint32_t add_result_zero : 1;
    uint32_t and_result_zero : 1;
    uint32_t xor_result_zero : 1;
    uint32_t is_carry : 1;
    uint32_t reduction_times : 6;
    uint32_t engine_stat : 2;
    uint32_t sram_size_mode : 2;
} te_aca_stat_t;

typedef struct te_aca_intr_stat_t {
    uint32_t cmd_finish : 1;
    uint32_t op_num_under_wm : 1;
    uint32_t fifo_overflow : 1;
    uint32_t div_zero : 1;
    uint32_t modinv_zero : 1;
    uint32_t fifo_emtpy_engine_done : 1;
    uint32_t mult_red_err : 1;
    uint32_t red_byd_th_evt : 1;
    uint32_t mod_n_zero_err : 1;
} te_aca_intr_stat_t;

typedef struct te_aca_suspend_mask {
    uint32_t cmd_finish : 1;
    uint32_t rsvd0 : 1;
    uint32_t fifo_overflow : 1;
    uint32_t div_zero : 1;
    uint32_t modinv_zero : 1;
    uint32_t rsvd1 : 1;
    uint32_t mult_red_err : 1;
    uint32_t red_byd_th_evt : 1;
    uint32_t mod_n_zero_err : 1;
} te_aca_suspend_mask_t;

/**
 * aliases
 */
typedef te_aca_intr_stat_t te_aca_int_t;
typedef te_aca_suspend_mask_t te_aca_suspd_mask_t;

/**
 * Trust engine ACA HWA structure
 */
typedef struct te_hwa_aca {
    te_hwa_crypt_t base;
    /* register operations */
    int (*config_gr_sram_addr)(struct te_hwa_aca *h,
                               int8_t gr_id,
                               uint32_t sram_addr,
                               uint32_t abcn_nblk);
    int (*config_len_type)(struct te_hwa_aca *h,
                           int8_t len_type_id,
                           uint32_t len);
    int (*config_gr_for_n)(struct te_hwa_aca *h, int8_t gr_id_n);
    int (*config_gr_for_p)(struct te_hwa_aca *h, int8_t gr_id_p);
    int (*config_gr_for_t0)(struct te_hwa_aca *h, int8_t gr_id_t0);
    int (*config_gr_for_t1)(struct te_hwa_aca *h, int8_t gr_id_t1);
    int (*set_ctrl)(struct te_hwa_aca *h, const te_aca_ctrl_t *ctrl);
    int (*get_ctrl)(struct te_hwa_aca *h, te_aca_ctrl_t *ctrl);
    int (*set_op_run)(struct te_hwa_aca *h, bool is_run);
    int (*set_op)(struct te_hwa_aca *h, const te_aca_op_entry_t *op);
    int (*get_status)(struct te_hwa_aca *h, te_aca_stat_t *stat);
    int (*write_sram)(struct te_hwa_aca *h,
                      void *sram_addr,
                      size_t sram_size,
                      const uint8_t *data,
                      size_t size);
    int (*read_sram)(struct te_hwa_aca *h,
                     void *sram_addr,
                     size_t sram_size,
                     uint8_t *data,
                     size_t size);
    int (*zeroize_sram)(struct te_hwa_aca *h,
                        void *sram_addr,
                        size_t sram_size);
    int (*swap_sram)(struct te_hwa_aca *h,
                     void *sram_addr,
                     uint32_t *io_buf,
                     size_t io_size,
                     bool is_swap_in);
    int (*eoi)(struct te_hwa_aca *h, const te_aca_int_t *stat);
    int (*int_state)(struct te_hwa_aca *h, te_aca_int_t *stat);
    int (*set_intr_mask)(struct te_hwa_aca *h, const te_aca_int_t *mask);
    int (*get_intr_mask)(struct te_hwa_aca *h, te_aca_int_t *mask);
    int (*set_suspend_mask)(struct te_hwa_aca *h,
                            const te_aca_suspd_mask_t *suspd_mask);
    int (*get_suspend_mask)(struct te_hwa_aca *h,
                            te_aca_suspd_mask_t *suspd_mask);

    /* internal operations */
    int (*get_gr_num)(struct te_hwa_aca *h);
    int (*get_len_type_num)(struct te_hwa_aca *h);
    int (*get_core_granularity)(struct te_hwa_aca *h);
    int (*get_core_max_op_len)(struct te_hwa_aca *h);
    int (*get_sram_info)(struct te_hwa_aca *h, void **base, size_t *size);
    int (*get_cq_num)(struct te_hwa_aca *h);
    int (*get_op_bits)(struct te_hwa_aca *h,
                       te_aca_op_code_t code,
                       size_t op_bit_len,
                       size_t *n_op_bits,
                       size_t *ac_op_bits,
                       size_t *b_op_bits,
                       size_t *r_t0_t1_op_bits);
    int (*get_param_for_calc_np)(struct te_hwa_aca *h,
                                 uint32_t *param0,
                                 uint32_t *param1,
                                 uint32_t *param2);
    /* debug functions */
    int (*dbg_dump)(struct te_hwa_aca *h,
                    const te_aca_op_entry_t *op,
                    bool is_result,
                    int dump_level);
#ifdef CFG_TE_DYNCLK_CTL
    int (*dynamic_clock_ctrl)(struct te_hwa_aca *h, bool is_enable);
    int (*dynamic_clock_status)(struct te_hwa_aca *h, bool *is_enable);
#endif
} te_hwa_aca_t;

int te_hwa_aca_alloc(struct te_aca_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_aca_t **hwa);

int te_hwa_aca_free(te_hwa_aca_t *hwa);

int te_hwa_aca_init(struct te_aca_regs *regs,
                    struct te_hwa_host *host,
                    te_hwa_aca_t *hwa);

int te_hwa_aca_exit(te_hwa_aca_t *hwa);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_ACA_H__ */
