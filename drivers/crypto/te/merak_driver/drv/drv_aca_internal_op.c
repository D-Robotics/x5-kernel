//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include <driver/te_drv_aca.h>
#include <hwa/te_hwa_aca.h>
#include "drv_aca_internal.h"

/**
 * \brief This file contains the OP_CTX(operation context) usages.
 * One OP_CTX contains all necessary data structure to operate the ACA engine,
 *such as sram_block, gr_info. Details about OP_CTX see structure aca_op_ctx_t
 *
 * The usage of OP_CTX(s) follows this step:
 * 1. Check all input OP_CTX(s) operation size.
 * 2. Initialize temporary OP_CTX(s).
 * 3. Resize output(R) OP_CTX operation size.
 * 4. ACA OP Lock.
 * 5. Allocate len_type_id(s).
 * 6. Get all used OP_CTX(s).
 * 7. Use OP_EXEC_XXX or OP_XXX_BATCH_XXX macros to call ACA engine.
 * 8. Check operation status(op_status) if need.
 * 9. Free len_type_id(s).
 * 10. Put all OP_CTX(s).
 * 11. ACA OP Unlock.
 * 12. Cleanup temporary OP_CTX(s).
 * 13. Return.
 *
 * For step 3, SUPPORTS:
 * 1. Output OP_CTX R is not initialized, will initialize R with current
 *    operation size.
 * 2. Output OP_CTX R is initialized, but size is NOT equals to current
 *    operation size.
 * 3. Output OP_CTX R == one of Input OP_CTX (A, B or C), and size is NOT equals
 *    to current operation size.
 * 4. Output OP_CTX R is NULL, will not save to result.
 *
 * Variables:
 * op_ctx:          The OP_CTX pointer.
 * aca_hwa:         The ACA HWA(hardware adaption) pointer.
 * len_type_pool:   The LengthType Pool pointer.
 * len_type_id:     The length type id allocated from LengthType Pool.
 * op_code:         Operation code.
 * op_bits:         Operation size in bits. Also the input parameters for
 *                  allocating len_type_id.
 * op_sram_size:    The maximum required TE SRAM size for current operation.
 *                  Mostly, max required SRAM size == required R SRAM size.
 *                  And most temporary OP_CTX(s) is initialized to this size.
 * op_status:       The operation result status, such as whether there is div by
 *                  zero, mode by zero or ALU operation carry.
 **/

/* OP_CTX magic */
#define ACA_OPERATION_MAGIC (0x416F704DU) /* AopM */

/* In GR pool GR ID 0 is reserved */
#define _OP_INVALID_GR_ID (0)

/* used to set operation status */
#define _SET_OP_STATUS(__intr_stat_ptr__, __name__, __op_stat_ptr__)           \
    do {                                                                       \
        if (__intr_stat_ptr__->__name__) {                                     \
            __op_stat_ptr__->__name__ = true;                                  \
        }                                                                      \
    } while (0)

/* configs to enable/disable interrupt. */
#ifdef CFG_TE_IRQ_EN
#define _OP_CMD_NEED_INTR true
#else
#define _OP_CMD_NEED_INTR false
#endif

/* whether to force disable OP debug */
static int _g_op_debug_force_disable = 0;

static int __op_prepare_run(aca_operation_t *op);
static int __op_submit_cmd(aca_operation_t *op, te_aca_op_entry_t *op_entry);
static int __op_trigger_run(aca_operation_t *op, bool need_intr);
static int __op_wait_finish(aca_operation_t *op, bool need_intr);

static int __aca_intr_config(aca_operation_t *op, bool is_enable);

/* assemble to one te_aca_op_entry_t from several OP_CTXs and len_type_id. */
static void __op_prepare_op_entry(aca_op_ctx_t *op_A,
                                  aca_op_ctx_t *op_B,
                                  aca_op_ctx_t *op_C,
                                  aca_op_ctx_t *op_R,
                                  int32_t len_type_id,
                                  te_aca_op_code_t op_code,
                                  te_aca_op_entry_t *op_entry)
{
    TE_ASSERT(op_entry);
    TE_ASSERT(op_A);
    /* gr_id 0 is reserved, so we can assert on > 0 */
    TE_ASSERT(op_A->gr_info->gr_id > 0);
    if (op_B) {
        TE_ASSERT(op_B->gr_info->gr_id > 0);
    }
    if (op_C) {
        TE_ASSERT(op_C->gr_info->gr_id > 0);
    }
    if (op_R) {
        TE_ASSERT(op_R->gr_info->gr_id > 0);
    }

    /* always disable CMD finish interrupt. we use FIFO empty && engine done
     * interrupt */
    op_entry->need_intr = 0;
    op_entry->op_c_id = ((op_C) ? (op_C->gr_info->gr_id) : (_OP_INVALID_GR_ID));
    op_entry->op_r_id = ((op_R) ? (op_R->gr_info->gr_id) : (_OP_INVALID_GR_ID));
    op_entry->no_save_to_r = (op_R) ? 0 : 1;
    op_entry->op_b =
        TE_ACA_MK_B_GR((op_B) ? (op_B->gr_info->gr_id) : (_OP_INVALID_GR_ID));
    op_entry->op_a_id     = op_A->gr_info->gr_id;
    op_entry->len_type_id = len_type_id;
    op_entry->op_code     = op_code;
}

/* assemble to one te_aca_op_entry_t from several OP_CTXs, immediate value
 * imme_B and len_type_id. max imme_B is 0x1F */
static void __op_prepare_op_entry_imme_b(aca_op_ctx_t *op_A,
                                         int32_t imme_B,
                                         aca_op_ctx_t *op_C,
                                         aca_op_ctx_t *op_R,
                                         int32_t len_type_id,
                                         te_aca_op_code_t op_code,
                                         te_aca_op_entry_t *op_entry)
{
    TE_ASSERT(op_entry);
    TE_ASSERT(imme_B <= 0x1F);
    TE_ASSERT(op_A);
    TE_ASSERT(op_A->gr_info->gr_id > 0);
    if (op_C) {
        TE_ASSERT(op_C->gr_info->gr_id > 0);
    }
    if (op_R) {
        TE_ASSERT(op_R->gr_info->gr_id > 0);
    }
    op_entry->need_intr = 0;
    op_entry->op_c_id = ((op_C) ? (op_C->gr_info->gr_id) : (_OP_INVALID_GR_ID));
    op_entry->op_r_id = ((op_R) ? (op_R->gr_info->gr_id) : (_OP_INVALID_GR_ID));
    op_entry->no_save_to_r = (op_R) ? 0 : 1;
    op_entry->op_b         = TE_ACA_MK_B_IMM(imme_B);
    op_entry->op_a_id      = op_A->gr_info->gr_id;
    op_entry->len_type_id  = len_type_id;
    op_entry->op_code      = op_code;
}

/* assemble to one te_aca_op_entry_t from several OP_CTXs, immediate value
 * imme_B and len_type_id for SHR0/SHL0/SHR1 operations. For shift operation,
 * max imme_B is 0x7F */
static void __op_prepare_op_entry_shift(aca_op_ctx_t *op_A,
                                        int32_t imme_B,
                                        aca_op_ctx_t *op_R,
                                        int32_t len_type_id,
                                        te_aca_op_code_t op_code,
                                        te_aca_op_entry_t *op_entry)
{
    TE_ASSERT(op_entry);
    TE_ASSERT(imme_B <= 0x7F);
    TE_ASSERT(op_A);
    TE_ASSERT(op_A->gr_info->gr_id > 0);
    /* SHR0/SHL0/SHR1 doesn't support not saving to R. So R must NOT be NULL */
    TE_ASSERT(op_R);
    TE_ASSERT(op_R->gr_info->gr_id > 0);
    op_entry->need_intr    = 0;
    op_entry->op_c_id      = _OP_INVALID_GR_ID;
    op_entry->op_r_id      = op_R->gr_info->gr_id;
    op_entry->no_save_to_r = (imme_B & 0x01);
    op_entry->op_b         = (imme_B >> 1) & 0x3F;
    op_entry->op_a_id      = op_A->gr_info->gr_id;
    op_entry->len_type_id  = len_type_id;
    op_entry->op_code      = op_code;
}

/* Execute one operation and wait finished. */
static int _op_exec_one_cmd(bool need_intr,
                            const te_aca_drv_t *aca_drv,
                            aca_op_ctx_t *op_A,
                            aca_op_ctx_t *op_B,
                            aca_op_ctx_t *op_C,
                            aca_op_ctx_t *op_R,
                            int32_t len_type_id,
                            te_aca_op_code_t op_code)
{
    int ret                  = TE_SUCCESS;
    te_aca_op_entry_t tmp_op = {0};
#ifdef CFG_TE_DYNCLK_CTL
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
#endif
    __op_prepare_op_entry(op_A, op_B, op_C, op_R, len_type_id, op_code,
                          &tmp_op);
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    ret = __op_prepare_run(ACA_DRV_GET_OP(aca_drv));
    CHECK_RET_GO;
    ret = __op_submit_cmd(ACA_DRV_GET_OP(aca_drv), &tmp_op);
    CHECK_RET_GO;
    ret = __op_trigger_run(ACA_DRV_GET_OP(aca_drv), need_intr);
    CHECK_RET_GO;
    ret = __op_wait_finish(ACA_DRV_GET_OP(aca_drv), need_intr);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Exec CMD: %d failed!\n", op_code);
    }
    return ret;
}

/* Execute one operation wth immediate value imme_B and wait finished. */
static int _op_exec_one_cmd_imme_b(bool need_intr,
                                   const te_aca_drv_t *aca_drv,
                                   aca_op_ctx_t *op_A,
                                   int32_t imme_B,
                                   aca_op_ctx_t *op_C,
                                   aca_op_ctx_t *op_R,
                                   int32_t len_type_id,
                                   te_aca_op_code_t op_code)
{
    int ret                  = TE_SUCCESS;
    te_aca_op_entry_t tmp_op = {0};
#ifdef CFG_TE_DYNCLK_CTL
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
#endif

    __op_prepare_op_entry_imme_b(op_A, imme_B, op_C, op_R, len_type_id, op_code,
                                 &tmp_op);

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    ret = __op_prepare_run(ACA_DRV_GET_OP(aca_drv));
    CHECK_RET_GO;
    ret = __op_submit_cmd(ACA_DRV_GET_OP(aca_drv), &tmp_op);
    CHECK_RET_GO;
    ret = __op_trigger_run(ACA_DRV_GET_OP(aca_drv), need_intr);
    CHECK_RET_GO;
    ret = __op_wait_finish(ACA_DRV_GET_OP(aca_drv), need_intr);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Exec CMD: %d failed!\n", op_code);
    }
    return ret;
}

/* Execute one SHR0/SHL0/SHR1 operation wth shift value imme_B and wait
 * finished. */
static int _op_exec_one_cmd_shift(bool need_intr,
                                  const te_aca_drv_t *aca_drv,
                                  aca_op_ctx_t *op_A,
                                  int32_t imme_B,
                                  aca_op_ctx_t *op_R,
                                  int32_t len_type_id,
                                  te_aca_op_code_t op_code)
{
    int ret                  = TE_SUCCESS;
    te_aca_op_entry_t tmp_op = {0};
#ifdef CFG_TE_DYNCLK_CTL
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
#endif
    __op_prepare_op_entry_shift(op_A, imme_B, op_R, len_type_id, op_code,
                                &tmp_op);
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    ret = __op_prepare_run(ACA_DRV_GET_OP(aca_drv));
    CHECK_RET_GO;
    ret = __op_submit_cmd(ACA_DRV_GET_OP(aca_drv), &tmp_op);
    CHECK_RET_GO;
    ret = __op_trigger_run(ACA_DRV_GET_OP(aca_drv), need_intr);
    CHECK_RET_GO;
    ret = __op_wait_finish(ACA_DRV_GET_OP(aca_drv), need_intr);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Exec CMD: %d failed!\n", op_code);
    }
    return ret;
}

/* Submit one command to FIFO and return. Used in batched operations */
static int _op_submit_one_cmd(const te_aca_drv_t *aca_drv,
                              aca_op_ctx_t *op_A,
                              aca_op_ctx_t *op_B,
                              aca_op_ctx_t *op_C,
                              aca_op_ctx_t *op_R,
                              int32_t len_type_id,
                              te_aca_op_code_t op_code)
{
    int ret                  = TE_SUCCESS;
    te_aca_op_entry_t tmp_op = {0};

    __op_prepare_op_entry(op_A, op_B, op_C, op_R, len_type_id, op_code,
                          &tmp_op);

    ret = __op_submit_cmd(ACA_DRV_GET_OP(aca_drv), &tmp_op);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/* Submit one command with immediate value imme_B to FIFO and return. Used in
 * batched operations */
static int _op_submit_one_cmd_imme_b(const te_aca_drv_t *aca_drv,
                                     aca_op_ctx_t *op_A,
                                     int32_t imme_B,
                                     aca_op_ctx_t *op_C,
                                     aca_op_ctx_t *op_R,
                                     int32_t len_type_id,
                                     te_aca_op_code_t op_code)
{
    int ret                  = TE_SUCCESS;
    te_aca_op_entry_t tmp_op = {0};

    __op_prepare_op_entry_imme_b(op_A, imme_B, op_C, op_R, len_type_id, op_code,
                                 &tmp_op);

    ret = __op_submit_cmd(ACA_DRV_GET_OP(aca_drv), &tmp_op);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

#if 0
/* Submit one SHR0/SHL0/SHR1 command with shift value imme_B to FIFO and return.
Used in batched operations */
static int _op_submit_one_cmd_shift(const te_aca_drv_t *aca_drv,
                                    aca_op_ctx_t *op_A,
                                    int32_t imme_B,
                                    aca_op_ctx_t *op_R,
                                    int32_t len_type_id,
                                    te_aca_op_code_t op_code)
{
    int ret                  = TE_SUCCESS;
    te_aca_op_entry_t tmp_op = {0};

    __op_prepare_op_entry_shift(op_A, imme_B, op_R, len_type_id, op_code,
                                &tmp_op);

    ret = __op_submit_cmd(ACA_DRV_GET_OP(aca_drv), &tmp_op);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}
#endif

/* The following three definations are interfaces of calling ACA engine to
 * execute one command. Suppose that the following usage of OP_CTX(s) always
 * uses these definations instead of upper static functions */
#define OP_EXEC_ONE_CMD(__aca_drv__, __op_A__, __op_B__, __op_C__, __op_R__,   \
                        __len_type_id__, __op_code__)                          \
    _op_exec_one_cmd(_OP_CMD_NEED_INTR, __aca_drv__, __op_A__, __op_B__,       \
                     __op_C__, __op_R__, __len_type_id__, __op_code__)

#define OP_EXEC_ONE_CMD_IMME_B(__aca_drv__, __op_A__, __imme_B__, __op_C__,    \
                               __op_R__, __len_type_id__, __op_code__)         \
    _op_exec_one_cmd_imme_b(_OP_CMD_NEED_INTR, __aca_drv__, __op_A__,          \
                            __imme_B__, __op_C__, __op_R__, __len_type_id__,   \
                            __op_code__)

#define OP_EXEC_ONE_CMD_SHIFT(__aca_drv__, __op_A__, __imme_B__, __op_R__,     \
                              __len_type_id__, __op_code__)                    \
    _op_exec_one_cmd_shift(_OP_CMD_NEED_INTR, __aca_drv__, __op_A__,           \
                           __imme_B__, __op_R__, __len_type_id__, __op_code__)

/* batch command, always disable interrupt */

/**
 * The following OP_XXX_BATCH_XXX definations are interfaces of calling ACA
 * engine to execute several commands in a batch. Suppose that the using of
 * batched command always uses these definations instead of uppoer static
 * functions.
 *
 * The usage of batched commands flow are:
 * 1. OP_PREPARE_BATCH_CMD
 * 2. OP_SUBMIT_BATCH_CMD or OP_SUBMIT_BATCH_CMD_IMME_B or
 *    OP_SUBMIT_BATCH_CMD_SHIFT
 * 3. OP_START_BATCH_CMD
 * 4. OP_WAIT_BATCH_CMD_FINISH
 */
#define OP_PREPARE_BATCH_CMD(__aca_drv__)                                      \
    __op_prepare_run(ACA_DRV_GET_OP(__aca_drv__))

#define OP_SUBMIT_BATCH_CMD(__aca_drv__, __op_A__, __op_B__, __op_C__,         \
                            __op_R__, __len_type_id__, __op_code__)            \
    _op_submit_one_cmd(__aca_drv__, __op_A__, __op_B__, __op_C__, __op_R__,    \
                       __len_type_id__, __op_code__)
#define OP_SUBMIT_BATCH_CMD_IMME_B(__aca_drv__, __op_A__, __imme_B__,          \
                                   __op_C__, __op_R__, __len_type_id__,        \
                                   __op_code__)                                \
    _op_submit_one_cmd_imme_b(__aca_drv__, __op_A__, __imme_B__, __op_C__,     \
                              __op_R__, __len_type_id__, __op_code__)
#if 0
#define OP_SUBMIT_BATCH_CMD_SHIFT(__aca_drv__, __op_A__, __imme_B__, __op_R__, \
                                  __len_type_id__, __op_code__)                \
    _op_submit_one_cmd_shift(__aca_drv__, __op_A__, __imme_B__, __op_R__,      \
                             __len_type_id__, __op_code__)
#endif

#define OP_START_BATCH_CMD(__aca_drv__)                                        \
    __op_trigger_run(ACA_DRV_GET_OP(__aca_drv__), _OP_CMD_NEED_INTR)
#define OP_WAIT_BATCH_CMD_FINISH(__aca_drv__)                                  \
    __op_wait_finish(ACA_DRV_GET_OP(__aca_drv__), _OP_CMD_NEED_INTR)

/**
 * Only use FIFO empty && engine done interrupt
 * The FIFO empty && engine done interrupt is a status interrupt, not electrical
 * level interrupt. Usage of this interrupt is:
 *
 * 1. At initial state, disable FIFO empty && engine done interrupt.
 * 2. Submmit one or more OP commands to ACA FIFO.
 * 3. Enable FIFO empty && engine done interrupt.
 * 4. Wait interrupt.
 * 5. In interrupt handler:
 *   5.1 Read interrupt status.
 *   5.2 Disable FIFO empty && engine done interrupt.
 *   5.3 Send signal(completion)
 */
static int __aca_intr_config(aca_operation_t *op, bool is_enable)
{
#ifdef CFG_TE_IRQ_EN
    int ret                = TE_SUCCESS;
    te_aca_int_t intr_mask = {0};
    te_hwa_aca_t *aca_hwa  = (te_hwa_aca_t *)(op->hwa_ctx);

    (void)(op);
    intr_mask.cmd_finish      = 1;
    intr_mask.op_num_under_wm = 1;
    intr_mask.fifo_overflow   = 1;
    intr_mask.div_zero        = 1;
    intr_mask.modinv_zero     = 1;
    intr_mask.mult_red_err    = 1;
    intr_mask.red_byd_th_evt  = 1;
    intr_mask.mod_n_zero_err  = 1;

    if (is_enable) {
        intr_mask.fifo_emtpy_engine_done = 0;
    } else {
        intr_mask.fifo_emtpy_engine_done = 1;
    }

    ret = aca_hwa->set_intr_mask((te_hwa_aca_t *)aca_hwa,
                                 (const te_aca_int_t *)&intr_mask);
    TE_ASSERT(TE_SUCCESS == ret);

#else
    (void)(op);
    /* why config interupt with CFG_TE_IRQ_EN undefined? */
    TE_ASSERT(!is_enable);
#endif
    return TE_SUCCESS;
}

/* set operation status from ACA engine interrupt status. The ACA engine
 * interrupt status mainly saves error status(div_zero, modinv_zero, etc) */
static inline void _set_op_status(aca_op_status_t *op_status,
                                  te_aca_intr_stat_t *intr_stat)
{
    /* op_status MUST be invalid before calling this function */
    TE_ASSERT_MSG((op_status->is_valid != true),
                  "Fatal error, last operation status is not cleared!\n");

    /* we don't enable cmd finish interrupt */
    TE_ASSERT(!intr_stat->cmd_finish);

    _SET_OP_STATUS(intr_stat, div_zero, op_status);
    _SET_OP_STATUS(intr_stat, modinv_zero, op_status);
    _SET_OP_STATUS(intr_stat, mult_red_err, op_status);
    _SET_OP_STATUS(intr_stat, red_byd_th_evt, op_status);
    _SET_OP_STATUS(intr_stat, mod_n_zero_err, op_status);

    if (intr_stat->fifo_overflow) {
        op_status->internal_err = true;
    }

    if (intr_stat->fifo_emtpy_engine_done) {
        op_status->done     = true;
        op_status->is_valid = true;
    }
}

#ifdef CFG_TE_IRQ_EN
/* The ACA engine interrupt handler */
static int _te_aca_irqhandler(const uint32_t type, void *uparam)
{
    int ret                      = TE_SUCCESS;
    aca_operation_t *op          = NULL;
    te_hwa_aca_t *aca_hwa        = NULL;
    te_aca_intr_stat_t intr_stat = {0};

    TE_ASSERT(uparam != NULL);
    TE_ASSERT_MSG((TE_IRQ_TYPE_ACA == type),
                  "Wrong IRQ type(%d) expected(%d)\n", type, TE_IRQ_TYPE_ACA);
    op      = (aca_operation_t *)uparam;
    aca_hwa = (te_hwa_aca_t *)(op->hwa_ctx);

    /* read interrupt status */
    ret = aca_hwa->int_state(aca_hwa, &intr_stat);
    TE_ASSERT(ret == TE_SUCCESS);
#if 0
    OSAL_LOG_DEBUG(
        "INTR cmd_finish: %d, op_num_under_wm: %d, fifo_overflow: %d, "
        "div_zero: "
        "%d, modinv_zero : %d, fifo_emtpy_engine_done: %d, mult_red_err: "
        "%d, red_byd_th_evt: %d, mod_n_zero_err: %d\n ",
        intr_stat.cmd_finish, intr_stat.op_num_under_wm,
        intr_stat.fifo_overflow, intr_stat.div_zero, intr_stat.modinv_zero,
        intr_stat.fifo_emtpy_engine_done, intr_stat.mult_red_err,
        intr_stat.red_byd_th_evt, intr_stat.mod_n_zero_err);
#endif

    /* assert on fifo empty and engin done status, because we only enabled this
     * interrupt */
    TE_ASSERT(intr_stat.fifo_emtpy_engine_done);

    /* set operation status */
    _set_op_status(&(op->op_status), &intr_stat);

    /* clear intr for safe */
    intr_stat.op_num_under_wm        = 0;
    intr_stat.fifo_emtpy_engine_done = 0;

    ret = aca_hwa->eoi(aca_hwa, (const te_aca_int_t *)(&intr_stat));
    TE_ASSERT(ret == TE_SUCCESS);

    /* disable fifo empty && engine done interrupt. */
    ret = __aca_intr_config(op, false);
    TE_ASSERT(ret == TE_SUCCESS);

    /* Send signal */
    osal_completion_signal(&(op->completion));

    return TE_SUCCESS;
}
#endif

/* wait command finish in interrupt mode */
static void __aca_intr_wait_finish(aca_operation_t *op)
{
    (void)(op);
#ifdef CFG_TE_IRQ_EN
    osal_completion_wait(&(op->completion));
#else
    TE_ASSERT(0);
#endif
}

/* wait command finish in polling mode */
static void __aca_poll_wait_finish(aca_operation_t *op)
{
    int ret                      = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa        = (te_hwa_aca_t *)(op->hwa_ctx);
    te_aca_intr_stat_t intr_stat = {0};
    bool completion_done         = false;

    /* Wait on FIFO empty && engine done */
    do {
        /* read interrupt status */
        ret = aca_hwa->int_state(aca_hwa, &intr_stat);
        TE_ASSERT(ret == TE_SUCCESS);

#if 0
        OSAL_LOG_DEBUG(
            "POLL cmd_finish: %d, op_num_under_wm: %d, fifo_overflow: %d, "
            "div_zero: "
            "%d, modinv_zero : %d, fifo_emtpy_engine_done: %d, mult_red_err: "
            "%d, red_byd_th_evt: %d, mod_n_zero_err: %d\n ",
            intr_stat.cmd_finish, intr_stat.op_num_under_wm,
            intr_stat.fifo_overflow, intr_stat.div_zero, intr_stat.modinv_zero,
            intr_stat.fifo_emtpy_engine_done, intr_stat.mult_red_err,
            intr_stat.red_byd_th_evt, intr_stat.mod_n_zero_err);
#endif

        /* set operation status */
        _set_op_status(&(op->op_status), &intr_stat);

        completion_done = intr_stat.fifo_emtpy_engine_done;
        /* clear intr for safe */
        intr_stat.op_num_under_wm        = 0;
        intr_stat.fifo_emtpy_engine_done = 0;
        ret = aca_hwa->eoi(aca_hwa, (const te_aca_int_t *)(&intr_stat));
        TE_ASSERT(ret == TE_SUCCESS);

        if (completion_done) {
            /* disable fifo empty && engine done interrupt. */
            ret = __aca_intr_config(op, false);
            TE_ASSERT(ret == TE_SUCCESS);
            break;
        }
    } while (true);

    return;
}

/* wait command finish, in interupt mode or polling mode */
static void __aca_wait_cmd_finish(aca_operation_t *op, bool need_intr)
{
    if (need_intr) {
        __aca_intr_wait_finish(op);
    } else {
        __aca_poll_wait_finish(op);
    }
    return;
}

/* reset operation status, prepare to run new command */
static int __op_prepare_run(aca_operation_t *op)
{
    /* reset operation status */
    memset(&(op->op_status), 0, sizeof(aca_op_status_t));
    op->op_status.is_valid = false;
    return TE_SUCCESS;
}

/* trigger ACA engine to execute commands. also enable interrupt of needed */
static int __op_trigger_run(aca_operation_t *op, bool need_intr)
{
    int ret               = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa = (te_hwa_aca_t *)(op->hwa_ctx);

    /* run */
    ret = aca_hwa->set_op_run(aca_hwa, true);
    TE_ASSERT(TE_SUCCESS == ret);

    if (need_intr) {
        /* enable FIFO empty and engine done interrupt */
        ret = __aca_intr_config(op, true);
        TE_ASSERT(TE_SUCCESS == ret);
    }
    return TE_SUCCESS;
}

/* Submit one commmand to ACA FIFO */
static int __op_submit_cmd(aca_operation_t *op, te_aca_op_entry_t *op_entry)
{
    int ret                = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa  = (te_hwa_aca_t *)(op->hwa_ctx);
    te_aca_stat_t aca_stat = {0};

    do {
        ret = aca_hwa->get_status(aca_hwa, &aca_stat);
        TE_ASSERT(TE_SUCCESS == ret);

        if (aca_stat.op_fifo_free_num) {
            break;
        }
    } while (true);

#if ACA_DEBUG
    memcpy(&(op->dbg_last_op_entry), op_entry, sizeof(te_aca_op_entry_t));
#if ACA_OP_DEBUG
    if (!_g_op_debug_force_disable) {
        ret = aca_hwa->dbg_dump(aca_hwa, op_entry, false, ACA_OP_DEBUG_LEVEL);
        TE_ASSERT(TE_SUCCESS == ret);
    }
#endif /* ACA_OP_DEBUG */
#endif /* ACA_DEBUG */

    /* fix HW abcn block number */
    ret = aca_hwa->set_op(aca_hwa, (const te_aca_op_entry_t *)(op_entry));
    TE_ASSERT(TE_SUCCESS == ret);

    return TE_SUCCESS;
}

/* Wait one or more command(s) finish, and also update operation status. */
static int __op_wait_finish(aca_operation_t *op, bool need_intr)
{
    int ret                    = TE_SUCCESS;
    aca_op_status_t *op_status = &(op->op_status);
    te_hwa_aca_t *aca_hwa      = (te_hwa_aca_t *)(op->hwa_ctx);
    te_aca_stat_t aca_stat     = {0};

    /* wait op finish */
    __aca_wait_cmd_finish(op, need_intr);

#if ACA_DEBUG
#if ACA_OP_DEBUG
    if (!_g_op_debug_force_disable) {
        ret = aca_hwa->dbg_dump(aca_hwa, &(op->dbg_last_op_entry), true,
                                ACA_OP_DEBUG_LEVEL);
        TE_ASSERT(TE_SUCCESS == ret);
    }
#endif
    memset(&(op->dbg_last_op_entry), 0, sizeof(te_aca_op_entry_t));
#endif

    /* Process OP status */
    TE_ASSERT(op_status->is_valid && op_status->done);
    CHECK_COND_LOG_GO(!(op_status->internal_err), TE_ERROR_GENERIC,
                      "ACA Run CMD failed!\n");

    /* read status */
    ret = aca_hwa->get_status(aca_hwa, &aca_stat);
    TE_ASSERT(TE_SUCCESS == ret);

    /* assert on bad status */
    TE_ASSERT(aca_stat.op_fifo_empty);
    TE_ASSERT((aca_stat.engine_stat == TE_ACA_ENGINE_IDLE) ||
              (aca_stat.engine_stat == TE_ACA_ENGINE_BUSY_BY_OTHER_HOSTS));

    /* update operation status */
    op_status->reduction_times = (int32_t)(aca_stat.reduction_times);
    op_status->alu_carry       = ((aca_stat.is_carry) ? (true) : (false));
    op_status->xor_result_zero =
        ((aca_stat.xor_result_zero) ? (true) : (false));
    op_status->and_result_zero =
        ((aca_stat.and_result_zero) ? (true) : (false));
    op_status->add_result_zero =
        ((aca_stat.add_result_zero) ? (true) : (false));

    ret = TE_SUCCESS;
finish:

    return ret;
}

/**
 * \brief Initialize ACA Operation manager.
 */
int aca_op_init(aca_operation_t *op, const te_hwa_aca_t *aca_hwa)
{
#ifdef CFG_TE_IRQ_EN
    int ret = TE_SUCCESS;
#endif

    CHECK_PARAM(op);
    CHECK_PARAM(aca_hwa);

    if (OSAL_SUCCESS != osal_mutex_create(&(op->lock))) {
        return TE_ERROR_OOM;
    }

    /* register interrupt */
#ifdef CFG_TE_IRQ_EN
    /* init completion */
    ret = osal_completion_init(&(op->completion));
    if (ret != OSAL_SUCCESS) {
        osal_mutex_destroy(op->lock);
        return TE_ERROR_OOM;
    }

    ret = te_hwa_register_notifier(aca_hwa->base.host, TE_IRQ_TYPE_ACA,
                                   _te_aca_irqhandler, (void *)op,
                                   &(op->nb_hanlde));
    if (ret != TE_SUCCESS) {
        OSAL_LOG_ERR("Register HWA notifier failed!\n");
        osal_mutex_destroy(op->lock);
        osal_completion_destroy(&(op->completion));
        return ret;
    }
#endif

    op->hwa_ctx = (void *)(aca_hwa);
    op->magic   = ACA_OPERATION_MAGIC;

    return TE_SUCCESS;
}

/**
 * \brief Cleanup ACA Operation manager.
 */
void aca_op_cleanup(aca_operation_t *op)
{
#ifdef CFG_TE_IRQ_EN
    int ret = TE_SUCCESS;
#endif
    if (!op) {
        return;
    }
    if (ACA_OPERATION_MAGIC != op->magic) {
        OSAL_LOG_ERR("Invalid Operation!\n");
        return;
    }

#ifdef CFG_TE_IRQ_EN
    ret = te_hwa_unregister_notifier(
        ((const te_hwa_aca_t *)(op->hwa_ctx))->base.host, op->nb_hanlde);
    TE_ASSERT_MSG(TE_SUCCESS == ret, "Unregister interrupt notifier failed!\n");

    osal_completion_destroy(&(op->completion));
#endif
    osal_mutex_destroy(op->lock);
    memset(op, 0, sizeof(aca_operation_t));
}

/* invert data endian */
static void _invert_data(uint8_t *src, uint8_t *dst, size_t size)
{
    size_t i    = 0;
    uint8_t tmp = 0;
    for (i = 0; i < size / 2; i++) {
        tmp               = src[size - 1 - i];
        dst[size - 1 - i] = src[i];
        dst[i]            = tmp;
    }
}

/* dump one OP_CTX */
void op_ctx_dump(const char *name, aca_op_ctx_t *op_ctx)
{
    int ret             = TE_SUCCESS;
    uint8_t *tmp_buf    = NULL;
    size_t size         = 0;
    uint32_t sram_flags = 0;
    void *sram_addr     = NULL;
    void *swapped_addr  = NULL;

    if (!op_ctx) {
        return;
    }
    OSAL_LOG_INFO("########## Start Dump %s ##########\n", name);
    if (op_ctx->sram_block) {

        /* FIXME, here we have risk of directly get sram flags and ptr */
        sram_flags   = op_ctx->sram_block->flags;
        sram_addr    = op_ctx->sram_block->sram_addr;
        swapped_addr = op_ctx->sram_block->swapped_addr;

        ret = aca_sram_get_size(op_ctx->sram_block, &size);
        CHECK_RET_GO;

        tmp_buf = osal_malloc(size);
        CHECK_COND_GO(tmp_buf, TE_ERROR_OOM);

        ret = aca_sram_read(op_ctx->sram_block, tmp_buf, size);
        CHECK_RET_GO;

        /* sram read in big endian, invert to little endian */
        _invert_data(tmp_buf, tmp_buf, size);

        OSAL_LOG_INFO(
            "SRAM Info:: Flag: 0x%x, SRAM addr: 0x%08x, SWAPPED addr: %p\n",
            sram_flags, (uint32_t)(uintptr_t)(sram_addr),
            (void *)(swapped_addr));
        OSAL_LOG_INFO_DUMP_DATA("SRAM Data (Little Endian)", tmp_buf, size);
    } else {
        OSAL_LOG_ERR("Dump %s, No SRAM data!\n", name);
    }

    if (op_ctx->gr_info) {
        OSAL_LOG_INFO("GR Info(Dirty data):: ID: %d, usage: 0x%x\n",
                       op_ctx->gr_info->gr_id,
                       op_ctx->gr_info->usage);
    } else {
        OSAL_LOG_ERR("Dump %s, No GR info!\n", name);
    }

    (void)swapped_addr;
    (void)sram_addr;
    (void)sram_flags;

    ret = TE_SUCCESS;
finish:
    OSAL_SAFE_FREE(tmp_buf);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Dump %s failed!\n", name);
    }
    OSAL_LOG_INFO("========== End Dump %s ==========\n", name);
    return;
}

/* Initialize one OP_CTX, with op size bytelen_hint. */
int op_ctx_init(const te_aca_drv_t *aca_drv,
                aca_op_ctx_t *op_ctx,
                int32_t bytelen_hint)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    aca_gr_pool_t *gr_pool     = NULL;

    CHECK_PARAM(op_ctx && aca_drv);
    CHECK_PARAM(bytelen_hint >= 0);

    if (bytelen_hint == 0) {
        op_ctx->sram_block = NULL;
    } else {
        sram_pool = &(((aca_priv_drv_t *)(aca_drv->priv_drv))->sram_pool);
        gr_pool   = &(((aca_priv_drv_t *)(aca_drv->priv_drv))->gr_pool);

        /* allocate sram_block and gr_info if caller tells us the operation size
         */
        ret = aca_sram_alloc_block(sram_pool, bytelen_hint,
                                   &(op_ctx->sram_block));
        CHECK_RET_GO;

        op_ctx->gr_info = osal_calloc(1, sizeof(gr_info_t));
        CHECK_COND_GO(op_ctx->gr_info, TE_ERROR_OOM);

        op_ctx->gr_info->pool  = (void *)(gr_pool);
        op_ctx->gr_info->gr_id = -1;
        op_ctx->gr_info->usage = GR_USAGE_NULL;
    }

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (op_ctx->sram_block) {
            aca_sram_free_block(op_ctx->sram_block);
            op_ctx->sram_block = NULL;
        }
        if (op_ctx->gr_info) {
            osal_free(op_ctx->gr_info);
            op_ctx->gr_info = NULL;
        }
    }
    return ret;
}

/* cleanup one OP_CTX */
void op_ctx_clean(aca_op_ctx_t *op_ctx)
{
    aca_op_ctx_t *P     = NULL;
    aca_op_ctx_t *ref_N = NULL;

    if (!op_ctx) {
        return;
    }
    if (op_ctx->sram_block) {
        aca_sram_free_block(op_ctx->sram_block);
        op_ctx->sram_block = NULL;
    }
    if (op_ctx->gr_info) {
        osal_free(op_ctx->gr_info);
        op_ctx->gr_info = NULL;
    }

    /* free extra context if have */
    if (op_ctx->extra_np) {
        P     = &(((aca_drv_extra_np_t *)(op_ctx->extra_np))->op_P_ctx);
        ref_N = &(((aca_drv_extra_np_t *)(op_ctx->extra_np))->op_N_ctx);

        if (P->sram_block) {
            aca_sram_free_block(P->sram_block);
            P->sram_block = NULL;
        }
        if (P->gr_info) {
            osal_free(P->gr_info);
            P->gr_info = NULL;
        }
        if (ref_N->sram_block) {
            aca_sram_free_block(ref_N->sram_block);
            ref_N->sram_block = NULL;
        }
        if (ref_N->gr_info) {
            osal_free(ref_N->gr_info);
            ref_N->gr_info = NULL;
        }
        osal_free(op_ctx->extra_np);
        op_ctx->extra_np = NULL;
    }
}

/**
 * Lock one OP_CTX. This will:
 * 1. Lock sram to TE SRAM.
 * 2. Allocate one gr id from GR pool.
 *
 * Don't call this function direclty in usage, should call op_ctx_get_all.
 */
static int _op_ctx_get_one(aca_op_ctx_t *op_ctx, gr_usage_hint_t bn_usage)
{
    int ret          = TE_SUCCESS;
    void *sram_addr  = NULL;
    size_t sram_size = 0;

    CHECK_PARAM((op_ctx) && (op_ctx->sram_block) && (op_ctx->gr_info));
    CHECK_PARAM(ACA_GR_IS_VALID_USAGE(bn_usage));

    ret = aca_sram_get(op_ctx->sram_block, &sram_addr, &sram_size);
    if (TE_ERROR_NO_SRAM_SPACE == (unsigned int)ret) {
        goto finish;
    }
    CHECK_RET_GO;

    if (op_ctx->gr_info->gr_id == -1) {
        TE_ASSERT(op_ctx->gr_info->usage == GR_USAGE_NULL);
        ret = aca_gr_alloc((aca_gr_pool_t *)(op_ctx->gr_info->pool), bn_usage,
                           sram_addr, sram_size, &(op_ctx->gr_info->gr_id));
        CHECK_RET_GO;
        op_ctx->gr_info->usage = bn_usage;
    }

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * Unock one OP_CTX. This will:
 * 1. Unock sram.
 * 2. Free one gr id from GR pool.
 *
 * Don't call this function direclty in usage, should call op_ctx_put_all.
 */
static void _op_ctx_put_one(aca_op_ctx_t *op_ctx)
{
    int ret = TE_SUCCESS;

    CHECK_COND_GO(op_ctx, TE_ERROR_BAD_PARAMS);

    if ((!op_ctx->sram_block) || (!op_ctx->gr_info)) {
        return;
    }

    /* unlock SRAM */
    ret = aca_sram_put(op_ctx->sram_block);
    CHECK_RET_GO;

    /* free GR */
    aca_gr_free((aca_gr_pool_t *)(op_ctx->gr_info->pool),
                op_ctx->gr_info->gr_id);

    op_ctx->gr_info->gr_id = -1;
    op_ctx->gr_info->usage = GR_USAGE_NULL;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        TE_ASSERT(0);
    }
    return;
}

/**
 * Lock several OP_CTX(s).
 * The last parameter MUST be NULL for terminating of searching OP_CTX.
 * This will repeatedly call _op_ctx_get_one for all the OP_CTX(s).
 * When we get TE_ERROR_NO_SRAM_SPACE error, will:
 * 1. Put all the OP_CTX(s) in the parameters.
 * 2. Swap out all the TE SRAM.
 * 3. Try again.
 */
int op_ctx_get_all(aca_op_ctx_t *op_ctx, int32_t bn_usage, ...)
{
    int ret                    = TE_SUCCESS;
    aca_sram_pool_t *sram_pool = NULL;
    aca_op_ctx_t *cur          = NULL;
    gr_usage_hint_t usage      = GR_USAGE_NULL;
    size_t total_size          = 0;
    size_t cur_size            = 0;
    va_list args;
    va_list free_list;

    CHECK_PARAM(op_ctx);

    sram_pool = (aca_sram_pool_t *)(op_ctx->sram_block->pool);

    /* calc total required sram size */
    cur   = op_ctx;
    usage = bn_usage;
    va_start(args, bn_usage);
    while (cur != NULL) {
        ret = aca_sram_get_size(cur->sram_block, &cur_size);
        CHECK_RET_GO;
        total_size += cur_size;
        cur   = va_arg(args, aca_op_ctx_t *);
        usage = va_arg(args, int32_t);
    }
    va_end(args);

    /* Check required size, Must <= total sram size */
    CHECK_COND_GO(total_size <= sram_pool->sram_size, TE_ERROR_NOT_ACCEPTABLE);

    /* get all op_context in a loop */
    do {
        cur   = op_ctx;
        usage = bn_usage;
        va_start(args, bn_usage);
        while (cur != NULL) {
            ret = _op_ctx_get_one(cur, usage);
            if (TE_ERROR_NO_SRAM_SPACE == (unsigned int)ret) {
                /* no sram space, swap all sram and try again */
                va_end(args);
                goto __swap_all_sram;
            }
            CHECK_RET_GO;

            cur   = va_arg(args, aca_op_ctx_t *);
            usage = va_arg(args, int32_t);
        }
        va_end(args);
        ret = TE_SUCCESS;
        goto finish;

        /* swap all the sram and retry */
    __swap_all_sram:
        OSAL_LOG_DEBUG("Swap All Sram!\n");

        /* unlock all OP_CTX(s) */
        cur   = op_ctx;
        usage = bn_usage;
        va_start(free_list, bn_usage);
        while (cur != NULL) {
            _op_ctx_put_one(cur);
            cur   = va_arg(free_list, aca_op_ctx_t *);
            usage = va_arg(free_list, int32_t);
        }
        va_end(free_list);

        osal_sleep_ms(10);
        ret = aca_sram_swap_all_blocks(sram_pool);
        CHECK_RET_GO;
    } while (true);

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        /* unlock all */
        cur   = op_ctx;
        usage = bn_usage;
        va_start(free_list, bn_usage);
        while (cur != NULL) {
            _op_ctx_put_one(cur);

            cur   = va_arg(free_list, aca_op_ctx_t *);
            usage = va_arg(free_list, int32_t);
        }
        va_end(free_list);
    }

    return ret;
}

/**
 * Unlock several OP_CTX(s).
 * The last parameter MUST be NULL for terminating of searching OP_CTX.
 * This will repeatedly call _op_ctx_put_one for all the OP_CTX(s).
 */
void op_ctx_put_all(aca_op_ctx_t *op_ctx, ...)
{
    aca_op_ctx_t *cur = op_ctx;
    va_list args;

    va_start(args, op_ctx);
    while (cur != NULL) {
        _op_ctx_put_one(cur);
        cur = va_arg(args, aca_op_ctx_t *);
    }
    va_end(args);
    return;
}

/* Calculate operation required maximum SRAM size from operation bits */
static int _op_calc_op_sram_size(const te_aca_drv_t *aca_drv,
                                 size_t op_bits,
                                 te_aca_op_code_t op_code,
                                 size_t *op_sram_size)
{
    int ret                = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa  = NULL;
    size_t r_t0_t1_op_bits = 0;

    aca_hwa = ACA_DRV_GET_HWA(aca_drv);

    ret = aca_hwa->get_op_bits(aca_hwa, op_code, op_bits, NULL, NULL, NULL,
                               &r_t0_t1_op_bits);
    TE_ASSERT(TE_SUCCESS == ret);

    *op_sram_size = r_t0_t1_op_bits / 8;

    return TE_SUCCESS;
}

/* Resize the output OP_CTX R's size. Handles R == one of input OP_CTX */
static int _op_ctx_resize_output(const te_aca_drv_t *aca_drv,
                                 aca_op_ctx_t *r,
                                 size_t op_sram_size,
                                 bool is_inout)
{
    int ret            = TE_SUCCESS;
    size_t r_sram_size = 0;

    TE_ASSERT(r);

    if ((r->sram_block) && (r->gr_info)) {
        ret = aca_sram_get_size(r->sram_block, &r_sram_size);
        CHECK_RET_GO;
        TE_ASSERT(r_sram_size);
        if (r_sram_size != op_sram_size) {
            /* reset r if r is output only, otherwise, extend r */
            if (!is_inout) {
                ret = aca_sram_reset(r->sram_block, op_sram_size);
                CHECK_RET_GO;
            } else {
                ret = aca_sram_try_change_size(r->sram_block, op_sram_size);
                CHECK_RET_GO;
            }
        }
    } else {
        /* For uninitialized r, MUST be output */
        TE_ASSERT(!is_inout);
        ret = op_ctx_init(aca_drv, r, op_sram_size);
        CHECK_RET_GO;
    }

    /* check again, assert on bad op bits */
    ret = aca_sram_get_size(r->sram_block, &r_sram_size);
    CHECK_RET_GO;
    TE_ASSERT(r_sram_size == op_sram_size);

finish:
    return ret;
}

/* initialize T0, T1 without locking ACA OP */
static int aca_op_prepare_t0_t1_no_lock(const te_aca_drv_t *aca_drv,
                                        aca_op_ctx_t *t0,
                                        aca_op_ctx_t *t1,
                                        size_t op_sram_size)
{
    int ret = TE_SUCCESS;
    if (t0) {
        ret = op_ctx_init(aca_drv, t0, op_sram_size);
        CHECK_RET_GO;
    }
    if (t1) {
        ret = op_ctx_init(aca_drv, t1, op_sram_size);
        CHECK_RET_GO;
    }

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (t0) {
            op_ctx_clean(t0);
        }
        if (t1) {
            op_ctx_clean(t1);
        }
    }
    return ret;
}

/* initialize T0, T1 with locking ACA OP */
int aca_op_prepare_t0_t1(const te_aca_drv_t *aca_drv,
                         aca_op_ctx_t *t0,
                         aca_op_ctx_t *t1,
                         size_t op_sram_size)
{
    int ret = TE_SUCCESS;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM(t0 && t1);
    CHECK_PARAM(op_sram_size > 0);

    ACA_OP_LOCK(aca_drv);
    ret = aca_op_prepare_t0_t1_no_lock(aca_drv, t0, t1, op_sram_size);
    ACA_OP_UNLOCK(aca_drv);
    return ret;
}

/* Case1 of update NP: kbits < 272 */
static int _op_ctx_update_np_case1(const te_aca_drv_t *aca_drv,
                                   aca_op_ctx_t *N,
                                   aca_op_ctx_t *P,
                                   size_t aca_granule_bits,
                                   size_t k_bits,
                                   size_t param0,
                                   size_t param1,
                                   size_t param2)
{
    int ret                            = TE_SUCCESS;
    int32_t len_type_id                = -1;
    size_t n_sram_size                 = 0;
    size_t op_bits                     = 0;
    aca_op_ctx_t tmp                   = {0};
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    bool is_aca_lock                   = false;

    (void)(param0);
    (void)(param2);
    CHECK_FUNC(aca_sram_get_size(N->sram_block, &n_sram_size));

    /* extra one bit, because 2^(k + param1) may be granule bit aligned */
    op_bits = UTILS_MAX(UTILS_ROUND_UP(k_bits + param1 + 1, aca_granule_bits),
                        n_sram_size * 8);

    /* op_sram_size is op_bits / 8 */
    ret = op_ctx_init(aca_drv, P, op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tmp, op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t0, op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t1, op_bits / 8);
    CHECK_RET_GO;

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all(N, GR_USAGE_IN, P, GR_USAGE_OUT, &tmp, GR_USAGE_INOUT,
                         &t0, GR_USAGE_T0, &t1, GR_USAGE_T1, NULL);
    CHECK_RET_GO;

    /* set zero */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &tmp, 0, NULL, &tmp, len_type_id,
                                 TE_ACA_OP_AND);
    CHECK_RET_GO;

    /* 2^(k + 135) */
    ret = aca_sram_set_bit(tmp.sram_block, k_bits + param1, 1);
    CHECK_RET_GO;

    /* P = 2^(k + 135) / N */
    ret =
        OP_EXEC_ONE_CMD(aca_drv, &tmp, N, NULL, P, len_type_id, TE_ACA_OP_DIV);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    op_ctx_put_all(N, P, &tmp, &t0, &t1, NULL);
    op_ctx_clean(&tmp);
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    if (is_aca_lock) {
        ACA_OP_UNLOCK(aca_drv);
    }
    return ret;
}

/* Case2 of update NP: kbits >= 272 */
static int _op_ctx_update_np_case2(const te_aca_drv_t *aca_drv,
                                   aca_op_ctx_t *N,
                                   aca_op_ctx_t *P,
                                   size_t aca_granule_bits,
                                   size_t k_bits,
                                   size_t param0,
                                   size_t param1,
                                   size_t param2)
{
    int ret             = TE_SUCCESS;
    int32_t len_type_id = -1;
    size_t n_sram_size  = 0;
    size_t op_bits      = 0;
    size_t shift_size = 0, cur_shift_size = 0;
    aca_op_ctx_t tmp[2]                = {0};
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    bool is_aca_lock                   = false;

    (void)(param1);

    shift_size = k_bits - param0;
    TE_ASSERT(shift_size);

    CHECK_FUNC(aca_sram_get_size(N->sram_block, &n_sram_size));
    op_bits = UTILS_MAX(UTILS_ROUND_UP(param2 + 1, aca_granule_bits),
                        n_sram_size * 8);

    ret = op_ctx_init(aca_drv, P, op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &(tmp[0]), op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &(tmp[1]), op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t0, op_bits / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t1, op_bits / 8);
    CHECK_RET_GO;

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* length type bits is full bit length */
    ret = op_ctx_get_all(N, GR_USAGE_IN, P, GR_USAGE_OUT, &(tmp[0]),
                         GR_USAGE_INOUT, &(tmp[1]), GR_USAGE_INOUT, &t0,
                         GR_USAGE_T0, &t1, GR_USAGE_T1, NULL);
    CHECK_RET_GO;

    /* 1. Calculate ceil(N/(2^(k-272))) */
    /* tmp0 == N - 1 */
    /* prevent further ceiling increment, if it not needed */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, N, 1, NULL, &(tmp[0]), len_type_id,
                                 TE_ACA_OP_SUB);
    CHECK_RET_GO;
    /* start to shift right */
    while (shift_size) {
        cur_shift_size = UTILS_MIN(shift_size, 0x80);
        /* tmp0 == tmp0 >> cur_shift_size */
        ret = OP_EXEC_ONE_CMD_SHIFT(aca_drv, &(tmp[0]), cur_shift_size - 1,
                                    &(tmp[0]), len_type_id, TE_ACA_OP_SHR0);
        CHECK_RET_GO;
        shift_size -= cur_shift_size;
    }
    /* tmp0 == tmp0 + 1 */
    /* Ceiling */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &(tmp[0]), 1, NULL, &(tmp[0]),
                                 len_type_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;

    /* 1. init 2^407 */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &(tmp[1]), 0, NULL, &(tmp[1]),
                                 len_type_id, TE_ACA_OP_AND);
    CHECK_RET_GO;

    /* 2^407 */
    ret = aca_sram_set_bit(tmp[1].sram_block, param2, 1);
    CHECK_RET_GO;

    /* P = (2^407)/ceil(N/(2^(k-272))) */
    ret = OP_EXEC_ONE_CMD(aca_drv, &(tmp[1]), &(tmp[0]), NULL, P, len_type_id,
                          TE_ACA_OP_DIV);
    CHECK_RET_GO;
finish:
    op_ctx_put_all(N, P, &(tmp[0]), &(tmp[1]), &t0, &t1, NULL);
    op_ctx_clean(&(tmp[0]));
    op_ctx_clean(&(tmp[1]));
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    if (is_aca_lock) {
        ACA_OP_UNLOCK(aca_drv);
    }
    return ret;
}
/**
 * Update NP. Algorithm:
 *
 * param0: 272
 * param1: 135
 * param2: 407
 * If k ≤ 272, P = floor (2^(k+135)/N);
 * If k > 272, P = floor(top/bottom); top= 2^407; bottom=ceil(N/(2^(k-272)))
 */
int op_ctx_update_np(const te_aca_drv_t *aca_drv, aca_op_ctx_t *N)
{
    int ret                 = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa   = NULL;
    size_t aca_granule_bits = 0;
    aca_op_ctx_t *P         = NULL;
    aca_op_ctx_t *ref_N     = NULL;
    size_t k_bits           = 0;
    uint32_t param0 = 0, param1 = 0, param2 = 0;
    int result = 0;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((N) && (N->sram_block) && (N->gr_info));

    aca_hwa          = ACA_DRV_GET_HWA(aca_drv);
    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);

    ret = aca_hwa->get_param_for_calc_np(aca_hwa, &param0, &param1, &param2);
    TE_ASSERT(TE_SUCCESS == ret);

    if (!N->extra_np) {
        N->extra_np = osal_calloc(1, sizeof(aca_drv_extra_np_t));
        CHECK_COND_GO(N->extra_np, TE_ERROR_OOM);
        P     = &(((aca_drv_extra_np_t *)(N->extra_np))->op_P_ctx);
        ref_N = &(((aca_drv_extra_np_t *)(N->extra_np))->op_N_ctx);
    } else {
        P     = &(((aca_drv_extra_np_t *)(N->extra_np))->op_P_ctx);
        ref_N = &(((aca_drv_extra_np_t *)(N->extra_np))->op_N_ctx);
        TE_ASSERT((P) && (P->sram_block) && (P->gr_info));
        TE_ASSERT((ref_N) && (ref_N->sram_block) && (ref_N->gr_info));
        ret = aca_op_cmp(aca_drv, ref_N, N, &result);
        CHECK_RET_GO;
        if (result == 0) {
            ret = TE_SUCCESS;
            goto finish;
        }
        /* N is changed, clean old P and ref_N */
        op_ctx_clean(P);
        op_ctx_clean(ref_N);
    }

    /* do calculate np */
    ret = aca_sram_get_bit_len(N->sram_block, &k_bits);
    CHECK_RET_GO;

    if (k_bits <= param0) {
        ret = _op_ctx_update_np_case1(aca_drv, N, P, aca_granule_bits, k_bits,
                                      param0, param1, param2);
    } else {
        ret = _op_ctx_update_np_case2(aca_drv, N, P, aca_granule_bits, k_bits,
                                      param0, param1, param2);
    }
    CHECK_RET_GO;

    /* update ref_N */
    ret = aca_op_copy(aca_drv, ref_N, N);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    if (TE_SUCCESS != ret) {
        if (P) {
            op_ctx_clean(P);
        }
        if (ref_N) {
            op_ctx_clean(ref_N);
        }
        OSAL_SAFE_FREE(N->extra_np);
    }
    return ret;
}

/* subfunction of aca_op_run. */
static int _op_core(const te_aca_drv_t *aca_drv,
                    aca_len_type_pool_t *len_type_pool,
                    size_t op_bits,
                    size_t op_sram_size,
                    aca_op_ctx_t *R,
                    aca_op_ctx_t *A,
                    aca_op_ctx_t *B,
                    int32_t imme_B,
                    aca_op_ctx_t *C,
                    aca_op_ctx_t *N,
                    aca_op_ctx_t *P,
                    aca_op_ctx_t *T0,
                    aca_op_ctx_t *T1,
                    te_aca_op_code_t op_code,
                    aca_op_status_t *result_status)
{
    int ret                        = TE_SUCCESS;
    int32_t len_type_id            = -1;
    aca_op_ctx_t *op_ctx_array[8]  = {NULL};
    gr_usage_hint_t usage_array[8] = {0};
    int32_t i                      = 0;
    bool is_aca_lock               = false;

    if (T0) {
        ret = op_ctx_init(aca_drv, T0, op_sram_size);
        CHECK_RET_GO;
    }
    if (T1) {
        ret = op_ctx_init(aca_drv, T1, op_sram_size);
        CHECK_RET_GO;
    }

    if (R) {
        ret = _op_ctx_resize_output(
            aca_drv, R, op_sram_size,
            ((R == A) || (R == B) || (R == C) || (R == N)));
        CHECK_RET_GO;
    }

    i               = 0;
    op_ctx_array[i] = A;
    usage_array[i]  = GR_USAGE_IN;
    i++;
    if (B) {
        op_ctx_array[i] = B;
        usage_array[i]  = GR_USAGE_IN;
        i++;
    }
    if (C) {
        op_ctx_array[i] = C;
        usage_array[i]  = GR_USAGE_IN;
        i++;
    }
    if (R) {
        op_ctx_array[i] = R;
        usage_array[i]  = GR_USAGE_OUT;
        i++;
    }
    if (N) {
        op_ctx_array[i] = N;
        usage_array[i]  = GR_USAGE_N;
        i++;
    }
    if (P) {
        op_ctx_array[i] = P;
        usage_array[i]  = GR_USAGE_P;
        i++;
    }
    if (T0) {
        op_ctx_array[i] = T0;
        usage_array[i]  = GR_USAGE_T0;
        i++;
    }
    if (T1) {
        op_ctx_array[i] = T1;
        usage_array[i]  = GR_USAGE_T1;
        i++;
    }
    TE_ASSERT(i <= 8);

    /* Start to operate */
    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    /* allocate length type id */
    len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* GET all BNs */
    ret = op_ctx_get_all(
        op_ctx_array[0], usage_array[0], op_ctx_array[1], usage_array[1],
        op_ctx_array[2], usage_array[2], op_ctx_array[3], usage_array[3],
        op_ctx_array[4], usage_array[4], op_ctx_array[5], usage_array[5],
        op_ctx_array[6], usage_array[6], op_ctx_array[7], usage_array[7], NULL);
    CHECK_RET_GO;

    /* call hw */
    if (B) {
        ret = OP_EXEC_ONE_CMD(aca_drv, A, B, C, R, len_type_id, op_code);
    } else {
        ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, A, imme_B, C, R, len_type_id,
                                     op_code);
    }
    CHECK_RET_GO;

    if (result_status) {
        memcpy(result_status, ACA_DRV_GET_OP_STATUS(aca_drv),
               sizeof(aca_op_status_t));
    }

    ret = TE_SUCCESS;
finish:
    op_ctx_put_all(op_ctx_array[0], op_ctx_array[1], op_ctx_array[2],
                   op_ctx_array[3], op_ctx_array[4], op_ctx_array[5],
                   op_ctx_array[6], op_ctx_array[7], NULL);
    op_ctx_clean(T0);
    op_ctx_clean(T1);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    if (is_aca_lock) {
        ACA_OP_UNLOCK(aca_drv);
    }
    return ret;
}

/**
 * \brief The all in one function to call ACA engine to execute one command
 * except: SHR0/SHL0/SHL1/DIV/MODINV. These operations has individual function.
 *
 * Support:
 * 1. R == NULL, not save to R.
 * 2. R != NULL, and R is not initialized. In this case, will initialize R with
 *    operation size.
 * 3. R != NULL, and R is initialized. In this case will do resize_outpu
 * necessary.
 *
 * Test points:
 * R size == full bit len in MODXXX
 * R size == N bit len in MODXXX
 * R size < N bit len in MODXXX
 * R size > full bit len in MODXXX
 * R size > A/B size
 * R size < A/B size
 * R is not initialized, should created to N bit len in MODXXX
 * R is not initialized, should created to max(A,B) size in ALU/MULTLU
 */
int aca_op_run(const te_aca_drv_t *aca_drv,
               aca_op_ctx_t *R,
               aca_op_ctx_t *A,
               aca_op_ctx_t *B,
               int32_t imme_B,
               aca_op_ctx_t *C,
               aca_op_ctx_t *N,
               te_aca_op_code_t op_code,
               aca_op_status_t *result_status)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    te_hwa_aca_t *aca_hwa              = ACA_DRV_GET_HWA(aca_drv);
    size_t op_bits                     = 0;
    size_t aca_granule_bits            = 0;
    size_t aca_max_op_size             = 0;
    size_t a_bit_len = 0, b_bit_len = 0, n_bit_len = 0;
    size_t a_sram_size = 0, b_sram_size = 0, c_sram_size = 0, n_sram_size = 0,
           r_sram_size = 0, op_sram_size = 0;
    aca_op_ctx_t tmp_t0 = {0};
    aca_op_ctx_t tmp_t1 = {0};
    aca_op_ctx_t *T0    = NULL;
    aca_op_ctx_t *T1    = NULL;
    aca_op_ctx_t *P     = NULL;
    bool need_p = false, need_t0 = false, need_t1 = false,
         r_initialized = false;

    CHECK_PARAM(A);
    CHECK_PARAM(((B) && (-1 == imme_B)) ||
                ((!B) && ((imme_B >= 0) && (imme_B <= 0x1F))));

    /* Check OP code */
    CHECK_PARAM((op_code != TE_ACA_OP_SHR0) && (op_code != TE_ACA_OP_SHL0) &&
                (op_code != TE_ACA_OP_SHL1) && (op_code != TE_ACA_OP_MODINV) &&
                (op_code != TE_ACA_OP_DIV));

    if (imme_B != -1) {
        /* only these commands supports immediate B */
        CHECK_PARAM(
            (op_code == TE_ACA_OP_ADD) || (op_code == TE_ACA_OP_SUB) ||
            (op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB) ||
            (op_code == TE_ACA_OP_AND) || (op_code == TE_ACA_OP_OR) ||
            (op_code == TE_ACA_OP_XOR) || (op_code == TE_ACA_OP_MUL_LOW) ||
            (op_code == TE_ACA_OP_MUL_HIGH) || (op_code == TE_ACA_OP_MODMUL) ||
            (op_code == TE_ACA_OP_MODMULACC) ||
            (op_code == TE_ACA_OP_MODMULNR) ||
            (op_code == TE_ACA_OP_MODMULACCNR));
    }

    /* Check R is NULL */
    if ((op_code != TE_ACA_OP_ADD) && (op_code != TE_ACA_OP_SUB) &&
        (op_code != TE_ACA_OP_AND) && (op_code != TE_ACA_OP_OR) &&
        (op_code != TE_ACA_OP_XOR)) {
        /* R can be NULL only for ADD, SUB, AND, OR, XOR */
        CHECK_PARAM(R);
    }

    /* Check R == input */
    if ((op_code != TE_ACA_OP_ADD) && (op_code != TE_ACA_OP_SUB) &&
        (op_code != TE_ACA_OP_AND) && (op_code != TE_ACA_OP_OR) &&
        (op_code != TE_ACA_OP_XOR) && (op_code != TE_ACA_OP_MODADD) &&
        (op_code != TE_ACA_OP_MODSUB)) {
        if (op_code != TE_ACA_OP_MODRED) {
            CHECK_PARAM(R != A);
        }
        CHECK_PARAM(R != B);
        CHECK_PARAM(R != C);
    }

    /* For Mod operation, MUST have N */
    if ((op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB) ||
        (op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODINV) ||
        (op_code == TE_ACA_OP_MODEXP) || (op_code == TE_ACA_OP_MODMULNR) ||
        (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR)) {
        CHECK_PARAM(N);
    }

    /* Check C */
    if ((op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR)) {
        CHECK_PARAM(C);
    }

    /* Check input not equal */
    if (op_code == TE_ACA_OP_MODINV) {
        CHECK_PARAM(A != N);
    }

    if ((op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB)) {
        CHECK_PARAM(A != N);
        CHECK_PARAM(B != N);
    }

    /* Here we skip checking A < N && B < N for ModADD/ModSUB */
    if ((op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB)) {
    }

    /* check A, B, N bit length < N*/
    if ((op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODMULNR) ||
        (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR) || (op_code == TE_ACA_OP_MODEXP)) {
        CHECK_FUNC(aca_sram_get_bit_len(A->sram_block, &a_bit_len));
        if (B) {
            CHECK_FUNC(aca_sram_get_bit_len(B->sram_block, &b_bit_len));
        } else {
            b_bit_len = 0;
        }
        CHECK_FUNC(aca_sram_get_bit_len(N->sram_block, &n_bit_len));
        CHECK_PARAM(a_bit_len <= n_bit_len);
        CHECK_PARAM(b_bit_len <= n_bit_len);
    }

    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);
    aca_max_op_size = aca_hwa->get_core_max_op_len(aca_hwa);
    TE_ASSERT(aca_max_op_size > 0);

    /* Check OP context have data */
    if (A) {
        CHECK_PARAM((A->sram_block) && (A->gr_info));
    }
    if (B) {
        CHECK_PARAM((B->sram_block) && (B->gr_info));
    }
    if (C) {
        CHECK_PARAM((C->sram_block) && (C->gr_info));
    }
    if (N) {
        CHECK_PARAM((N->sram_block) && (N->gr_info));
    }
    /* Check R initialized */
    if (R && (R->sram_block) && (R->gr_info)) {
        r_initialized = true;
    }

    /* Get all op bits */
    if (R && r_initialized) {
        CHECK_FUNC(aca_sram_get_size(R->sram_block, &r_sram_size));
    }
    if (A) {
        CHECK_FUNC(aca_sram_get_size(A->sram_block, &a_sram_size));
    }
    if (B) {
        CHECK_FUNC(aca_sram_get_size(B->sram_block, &b_sram_size));
    }
    if (C) {
        CHECK_FUNC(aca_sram_get_size(C->sram_block, &c_sram_size));
    }
    if (N) {
        CHECK_FUNC(aca_sram_get_size(N->sram_block, &n_sram_size));
    }
    /* get length type length accroding to different op code */
    switch (op_code) {
    case TE_ACA_OP_ADD:
    case TE_ACA_OP_SUB:
    case TE_ACA_OP_AND:
    case TE_ACA_OP_OR:
    case TE_ACA_OP_XOR:
    case TE_ACA_OP_MUL_LOW:
    case TE_ACA_OP_MUL_HIGH:
    case TE_ACA_OP_MODRED:
        /* these operations only uses A and B */
        op_bits =
            8 * (UTILS_MAX(r_sram_size, UTILS_MAX(a_sram_size, b_sram_size)));
        TE_ASSERT(UTILS_IS_ALIGNED(op_bits, aca_granule_bits));
        break;
    case TE_ACA_OP_MODADD:
    case TE_ACA_OP_MODSUB:
    case TE_ACA_OP_MODMUL:
    case TE_ACA_OP_MODMULNR:
    case TE_ACA_OP_MODMULACC:
    case TE_ACA_OP_MODMULACCNR:
    case TE_ACA_OP_MODEXP:
        /* for all modxxx ops, the length type is N's bit length */
        CHECK_FUNC(aca_sram_get_bit_len(N->sram_block, &op_bits));
        break;
    default:
        TE_ASSERT(0);
    }

    TE_ASSERT(op_bits > 0);

    /* check op bits */
    CHECK_COND_RETURN(op_bits <= aca_max_op_size, TE_ERROR_OP_TOO_LONG);

    /* calculate op_sram_size */
    CHECK_FUNC(_op_calc_op_sram_size(aca_drv, op_bits, op_code, &op_sram_size));
    TE_ASSERT(op_sram_size > 0);

    /* Check if we need calculate P */
    if ((op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODMULNR) ||
        (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR) || (op_code == TE_ACA_OP_MODEXP)) {
        need_p = true;
    } else {
        need_p = false;
    }

    /* Check if we need T0 */
    if ((op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB) ||
        (op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_DIV) || (op_code == TE_ACA_OP_MODRED) ||
        (op_code == TE_ACA_OP_MODINV) || (op_code == TE_ACA_OP_MODEXP)) {
        need_t0 = true;
    } else {
        need_t0 = false;
    }

    /* Check if we need T1 */
    if ((op_code == TE_ACA_OP_DIV) || (op_code == TE_ACA_OP_MODINV)) {
        need_t1 = true;
    } else {
        need_t1 = false;
    }

    /* update NP */
    if (need_p) {
        CHECK_FUNC(op_ctx_update_np(aca_drv, N));
        TE_ASSERT(N->extra_np);
        P = &(((aca_drv_extra_np_t *)(N->extra_np))->op_P_ctx);
    }

    /* init t0 */
    if (need_t0) {
        T0 = &tmp_t0;
    }

    /* init t1 */
    if (need_t1) {
        T1 = &tmp_t1;
    }

    ret = _op_core(aca_drv,
                   len_type_pool,
                   op_bits,
                   op_sram_size,
                   R,
                   A,
                   B,
                   imme_B,
                   C,
                   N,
                   P,
                   T0,
                   T1,
                   op_code,
                   result_status);
    if (TE_SUCCESS != ret) {
        OSAL_LOG_ERR("Execute CMD: %d failed!\n", op_code);
        return ret;
    }
    return ret;
}

/**
 * \brief This function do the shift operation.
 * The final R size is:
 * SHIFT Left: R size == A bit size + shift value.
 * SHIFT Right: R size == A bit size.
 *
 * Test point:
 * R == NULL, SL, created R size with the same as A
 * R == NULL, SH, created R size with enhanced size.
 * Any R size.
 * A == 0
 */
int aca_op_shift(const te_aca_drv_t *aca_drv,
                 aca_op_ctx_t *R,
                 aca_op_ctx_t *A,
                 int32_t shift_value,
                 te_aca_op_code_t op_code)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    te_hwa_aca_t *aca_hwa              = ACA_DRV_GET_HWA(aca_drv);
    int32_t len_type_id                = -1;
    size_t aca_granule_bits            = 0;
    size_t a_bit_len = 0, a_sram_size = 0, r_sram_size = 0,
           r_required_bit_len = 0;
    size_t cur_shift = 0, total_shift_size = 0;
    bool is_aca_lock = false;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));
    CHECK_PARAM(R);
    CHECK_PARAM(shift_value >= 0);
    CHECK_PARAM((op_code == TE_ACA_OP_SHR0) || (op_code == TE_ACA_OP_SHL0) ||
                (op_code == TE_ACA_OP_SHL1));

    ret = aca_sram_get_size(A->sram_block, &a_sram_size);
    CHECK_RET_GO;
    TE_ASSERT(a_sram_size > 0);

    ret = aca_sram_get_bit_len(A->sram_block, &a_bit_len);
    CHECK_RET_GO;

    if (a_bit_len == 0) {
        return aca_op_copy(aca_drv, R, A);
    }

    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);

    if ((op_code == TE_ACA_OP_SHL0) || (op_code == TE_ACA_OP_SHL1)) {
        r_required_bit_len = a_bit_len + shift_value;
    } else if (op_code == TE_ACA_OP_SHR0) {
        /* for shift R, we required that R size equals to A size */
        r_required_bit_len = a_bit_len;
    } else {
        TE_ASSERT(0);
    }

    ret = _op_ctx_resize_output(
        aca_drv, R, UTILS_ROUND_UP(r_required_bit_len, aca_granule_bits) / 8,
        (R == A));
    CHECK_RET_GO;

    ret = aca_sram_get_size(R->sram_block, &r_sram_size);
    CHECK_RET_GO;
    /* R sram size may < a_sram_size, which means there are may 0 at HSB of A */
    // TE_ASSERT(r_sram_size >= a_sram_size);

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    /* OP bits is r_sram_size *8 */
    len_type_id = aca_len_type_alloc(len_type_pool, r_sram_size * 8);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* get bns */
    ret = op_ctx_get_all(A, GR_USAGE_IN, R, GR_USAGE_OUT, NULL);
    CHECK_RET_GO;

    /* copy A to R first */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, A, 0, NULL, R, len_type_id,
                                 TE_ACA_OP_OR);
    CHECK_RET_GO;

    /* loop to shift */
    total_shift_size = shift_value;
    while (total_shift_size) {
        cur_shift = UTILS_MIN(total_shift_size, 0x80);

        ret = OP_EXEC_ONE_CMD_SHIFT(aca_drv, R, cur_shift - 1, R, len_type_id,
                                    op_code);
        CHECK_RET_GO;
        total_shift_size -= cur_shift;
    }

    ret = TE_SUCCESS;
finish:
    op_ctx_put_all(A, R, NULL);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    if (is_aca_lock) {
        ACA_OP_UNLOCK(aca_drv);
    }
    return ret;
}

/* Make sure dst op bits >= src op bits  */
static int aca_op_copy_no_lock(const te_aca_drv_t *aca_drv,
                               aca_op_ctx_t *dst,
                               const aca_op_ctx_t *src)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    size_t src_sram_size = 0, dst_sram_size = 0;
    int32_t len_type_id = -1;

    CHECK_FUNC(aca_sram_get_size(src->sram_block, &src_sram_size));
    CHECK_FUNC(aca_sram_get_size(dst->sram_block, &dst_sram_size));

    /* The upper logic should make sure this */
    TE_ASSERT(dst_sram_size >= src_sram_size);

    /* OP bits always use dst sram size. */
    len_type_id = aca_len_type_alloc(len_type_pool, 8 * dst_sram_size);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all((aca_op_ctx_t *)src, GR_USAGE_IN, dst, GR_USAGE_OUT,
                         NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)src, 0, NULL, dst,
                                 len_type_id, TE_ACA_OP_ADD);

    CHECK_RET_GO;

finish:
    op_ctx_put_all((aca_op_ctx_t *)src, dst, NULL);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    return ret;
}

/**
 * \brief copy aca operation context from src to dst.
 * Support:
 * 1. dst is not initialize, initialize dst to same size as src.
 * 2. src size == dst size.
 * 3. src size > dst size, will extend dst to src size.
 * 4. src size < dst size, will fill 0 to high bits of dst.
 */
int aca_op_copy(const te_aca_drv_t *aca_drv,
                aca_op_ctx_t *dst,
                const aca_op_ctx_t *src)
{
    int ret              = TE_SUCCESS;
    size_t src_sram_size = 0, dst_sram_size = 0;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((src) && (src->sram_block) && (src->gr_info));
    CHECK_PARAM(dst);

    /* if src == dst, direct return */
    if (src == dst) {
        return TE_SUCCESS;
    }

    CHECK_FUNC(aca_sram_get_size(src->sram_block, &src_sram_size));

    /* why not using _op_ctx_resize_output? because _op_ctx_resize_output will
    always resize dst to src op bits, but here for copy we support dst op bits >
    src op bits. */
    if ((dst->sram_block) && (dst->gr_info)) {
        CHECK_FUNC(aca_sram_get_size(dst->sram_block, &dst_sram_size));
        if (dst_sram_size < src_sram_size) {
            /* enlarge dst size */
            CHECK_FUNC(aca_sram_reset(dst->sram_block, src_sram_size));
        }
    } else {
        CHECK_FUNC(op_ctx_init(aca_drv, dst, src_sram_size));
    }

    ACA_OP_LOCK(aca_drv);
    ret = aca_op_copy_no_lock(aca_drv, dst, src);
    ACA_OP_UNLOCK(aca_drv);
    return ret;
}

/**
 * compare A with B. Result:
 * 1:  A > B
 * -1: A < B
 * 0:  A == B
 */
static int aca_op_cmp_no_lock(const te_aca_drv_t *aca_drv,
                              aca_op_ctx_t *op_a,
                              aca_op_ctx_t *op_b,
                              int *result)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    size_t op_a_sram_size = 0, op_b_sram_size = 0;
    int32_t len_type_id = -1;

    CHECK_FUNC(aca_sram_get_size(op_a->sram_block, &op_a_sram_size));
    CHECK_FUNC(aca_sram_get_size(op_b->sram_block, &op_b_sram_size));

    /* OP bits uses max(a, b). */
    len_type_id = aca_len_type_alloc(
        len_type_pool, 8 * UTILS_MAX(op_a_sram_size, op_b_sram_size));
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all(op_a, GR_USAGE_IN, op_b, GR_USAGE_IN, NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD(aca_drv, op_a, op_b, NULL, NULL, len_type_id,
                          TE_ACA_OP_XOR);
    CHECK_RET_GO;
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
        *result = 0;
        ret     = TE_SUCCESS;
        goto finish;
    }

    ret = OP_EXEC_ONE_CMD(aca_drv, op_a, op_b, NULL, NULL, len_type_id,
                          TE_ACA_OP_SUB);
    CHECK_RET_GO;
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->alu_carry) {
        *result = -1;
    } else {
        *result = 1;
    }

finish:
    op_ctx_put_all(op_a, op_b, NULL);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    return ret;
}

int aca_op_cmp(const te_aca_drv_t *aca_drv,
               aca_op_ctx_t *op_a,
               aca_op_ctx_t *op_b,
               int *result)
{
    int ret = TE_SUCCESS;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((op_a) && (op_a->sram_block) && (op_a->gr_info));
    CHECK_PARAM((op_b) && (op_b->sram_block) && (op_b->gr_info));
    CHECK_PARAM(result);

    if (op_a == op_b) {
        *result = 0;
        return TE_SUCCESS;
    }

    ACA_OP_LOCK(aca_drv);
    ret = aca_op_cmp_no_lock(aca_drv, op_a, op_b, result);
    ACA_OP_UNLOCK(aca_drv);
    return ret;
}

/* Checks if OP_CTX A equals to imme_b */
static int aca_op_cmp_immeb_no_lock(const te_aca_drv_t *aca_drv,
                                    aca_op_ctx_t *op_a,
                                    int32_t imme_b,
                                    bool *is_equal)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    size_t op_a_sram_size              = 0;
    int32_t len_type_id                = -1;

    CHECK_FUNC(aca_sram_get_size(op_a->sram_block, &op_a_sram_size));

    /* OP bits uses max(a, b). */
    len_type_id = aca_len_type_alloc(len_type_pool, 8 * op_a_sram_size);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all(op_a, GR_USAGE_IN, NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, op_a, imme_b, NULL, NULL, len_type_id,
                                 TE_ACA_OP_XOR);
    CHECK_RET_GO;
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
        *is_equal = true;
    } else {
        *is_equal = false;
    }

finish:
    op_ctx_put_all(op_a, NULL);
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    return ret;
}

int aca_op_cmp_immeb(const te_aca_drv_t *aca_drv,
                     aca_op_ctx_t *op_a,
                     int32_t imme_b,
                     bool *is_equal)
{
    int ret = TE_SUCCESS;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((op_a) && (op_a->sram_block) && (op_a->gr_info));
    CHECK_PARAM((imme_b >= 0) && (imme_b <= 0x1F));
    CHECK_PARAM(is_equal);

    ACA_OP_LOCK(aca_drv);
    ret = aca_op_cmp_immeb_no_lock(aca_drv, op_a, imme_b, is_equal);
    ACA_OP_UNLOCK(aca_drv);
    return ret;
}

/* Set OP_CTX to one uint32 data */
static int aca_op_set_u32_no_lock(const te_aca_drv_t *aca_drv,
                                  aca_op_ctx_t *op_ctx,
                                  uint32_t value)
{
    int ret            = TE_SUCCESS;
    uint8_t tmp_buf[4] = {0};

    (void)aca_drv;

    tmp_buf[0] = (((value) >> 24) & 0xFF);
    tmp_buf[1] = (((value) >> 16) & 0xFF);
    tmp_buf[2] = (((value) >> 8) & 0xFF);
    tmp_buf[3] = (((value) >> 0) & 0xFF);

    ret = aca_sram_zeroize(op_ctx->sram_block);
    CHECK_RET_GO;

    ret = aca_sram_write(op_ctx->sram_block, tmp_buf, 4);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

int aca_op_set_u32(const te_aca_drv_t *aca_drv,
                   aca_op_ctx_t *op_ctx,
                   uint32_t value)
{
    int ret               = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
    size_t op_bits        = 0;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM(op_ctx);

    if (!((op_ctx->sram_block) && (op_ctx->gr_info))) {
        op_bits = aca_hwa->get_core_granularity(aca_hwa);
        TE_ASSERT(op_bits);
        CHECK_FUNC(op_ctx_init(aca_drv, op_ctx, op_bits / 8));
    }

    ACA_OP_LOCK(aca_drv);
    ret = aca_op_set_u32_no_lock(aca_drv, op_ctx, value);
    ACA_OP_UNLOCK(aca_drv);
    return ret;
}

/**
 * Calculate R = A mod B, Q = A / B
 * Both R and Q can be NULL.
 */
int aca_op_div_bn(const te_aca_drv_t *aca_drv,
                  aca_op_ctx_t *R,
                  aca_op_ctx_t *Q,
                  const aca_op_ctx_t *A,
                  const aca_op_ctx_t *B)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = NULL;
    aca_op_ctx_t tmp_a                 = {0};
    aca_op_ctx_t tmp_q                 = {0};
    size_t a_sram_size                 = 0;
    size_t b_sram_size                 = 0;
    size_t op_bits                     = 0;
    size_t op_sram_size                = 0;
    int32_t len_type_id                = -1;
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    bool is_aca_lock                   = false;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));
    CHECK_PARAM((B) && (B->sram_block) && (B->gr_info));

    if ((!R) && (!Q)) {
        return TE_SUCCESS;
    }

    len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);

    ret = aca_sram_get_size(A->sram_block, &a_sram_size);
    CHECK_RET_GO;
    ret = aca_sram_get_size(B->sram_block, &b_sram_size);
    CHECK_RET_GO;

    /* for div, op bits is max of a and b */
    op_bits = UTILS_MAX(a_sram_size, b_sram_size) * 8;

    ret = _op_calc_op_sram_size(aca_drv, op_bits, TE_ACA_OP_DIV, &op_sram_size);
    CHECK_RET_GO;
    TE_ASSERT(op_sram_size);

    /* init tmp_a and tmp_q to be final op bits */
    ret = op_ctx_init(aca_drv, &tmp_a, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tmp_q, op_sram_size);
    CHECK_RET_GO;

    /* copy A to tmp_a */
    ret = aca_op_copy(aca_drv, &tmp_a, A);
    CHECK_RET_GO;

    /* prepare T0, T1 */
    ret = aca_op_prepare_t0_t1_no_lock(aca_drv, &t0, &t1, op_sram_size);
    CHECK_RET_GO;

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* lock all */
    ret = op_ctx_get_all((aca_op_ctx_t *)B, GR_USAGE_IN, &tmp_a, GR_USAGE_INOUT,
                         &tmp_q, GR_USAGE_INOUT, &t0, GR_USAGE_T0, &t1,
                         GR_USAGE_T1, NULL);
    CHECK_RET_GO;

    /* do div */
    ret = OP_EXEC_ONE_CMD(aca_drv, &tmp_a, (aca_op_ctx_t *)B, NULL, &tmp_q,
                          len_type_id, TE_ACA_OP_DIV);
    CHECK_RET_GO;
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->div_zero) {
        OSAL_LOG_ERR("Div by zero!\n");
        ret = TE_ERROR_DIV_BY_ZERO;
        goto finish;
    }

    op_ctx_put_all((aca_op_ctx_t *)B, &tmp_a, &tmp_q, &t0, &t1, NULL);

    aca_len_type_free(len_type_pool, len_type_id);
    len_type_id = -1;
    ACA_OP_UNLOCK(aca_drv);
    is_aca_lock = false;

    if (R) {
        ret = aca_op_copy(aca_drv, R, (const aca_op_ctx_t *)(&tmp_a));
        CHECK_RET_GO;
    }
    if (Q) {
        ret = aca_op_copy(aca_drv, Q, (const aca_op_ctx_t *)(&tmp_q));
        CHECK_RET_GO;
    }

    ret = TE_SUCCESS;

finish:
    if (TE_SUCCESS != ret) {
        op_ctx_put_all((aca_op_ctx_t *)B, &tmp_a, &tmp_q, &t0, &t1, NULL);
        if (len_type_id >= 0) {
            aca_len_type_free(len_type_pool, len_type_id);
        }
        if (is_aca_lock) {
            ACA_OP_UNLOCK(aca_drv);
        }
    }
    op_ctx_clean(&tmp_a);
    op_ctx_clean(&tmp_q);
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    return ret;
}

/* Calculate R = A mod B. Use div to calcualte mod */
int aca_op_mod_bn(const te_aca_drv_t *aca_drv,
                  aca_op_ctx_t *op_r_ctx,
                  aca_op_ctx_t *op_a_ctx,
                  aca_op_ctx_t *op_b_ctx)
{
    int ret = TE_SUCCESS;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((op_a_ctx) && (op_a_ctx->sram_block) && (op_a_ctx->gr_info));
    CHECK_PARAM((op_b_ctx) && (op_b_ctx->sram_block) && (op_b_ctx->gr_info));
    CHECK_PARAM(op_r_ctx);

    ret = aca_op_div_bn(aca_drv,
                        op_r_ctx,
                        NULL,
                        (const aca_op_ctx_t *)op_a_ctx,
                        (const aca_op_ctx_t *)op_b_ctx);
    if (TE_ERROR_DIV_BY_ZERO == (unsigned int)ret) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/**
 * Calculate MODINV, N MUST be odd. Also calculate GCD
 * return:
 * TE_SUCCESS: MODINV and GCD is valid.
 * TE_ERROR_BAD_INPUT_DATA: Bad input A, N.
 * TE_ERROR_INVAL_MOD: MODINV is invlaid, but GCD is valid.
 */
static int aca_op_modinv_odd(const te_aca_drv_t *aca_drv,
                             const aca_op_ctx_t *N,
                             const aca_op_ctx_t *A,
                             aca_op_ctx_t *R,
                             aca_op_ctx_t *GCD)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = NULL;
    te_hwa_aca_t *aca_hwa              = NULL;
    int32_t mod_inv_len_type_id        = -1;
    int32_t full_op_len_type_id        = -1;
    size_t N_bit_len = 0, A_bit_len = 0, op_bits = 0, op_sram_size = 0;
    size_t aca_max_op_size  = 0;
    aca_op_ctx_t tmp_r      = {0};
    aca_op_ctx_t a_ext_copy = {0};
    aca_op_ctx_t t0         = {0};
    aca_op_ctx_t t1         = {0};
    aca_op_ctx_t *used_r    = NULL;
    int32_t n_lsb = 0, a_lsb = 0;
    bool is_aca_lock = false;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((N) && (N->sram_block) && (N->gr_info));
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));

    if ((!R) && (!GCD)) {
        return TE_SUCCESS;
    }

    ret = aca_sram_get_bit(N->sram_block, 0, &n_lsb);
    CHECK_RET_GO;
    ret = aca_sram_get_bit(A->sram_block, 0, &a_lsb);
    CHECK_RET_GO;

    CHECK_COND_GO((n_lsb == 1) || ((n_lsb == 0) && (a_lsb == 1)),
                  TE_ERROR_BAD_INPUT_DATA);

    len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    aca_hwa       = ACA_DRV_GET_HWA(aca_drv);

    ret = aca_sram_get_bit_len(A->sram_block, &A_bit_len);
    CHECK_RET_GO;
    ret = aca_sram_get_bit_len(N->sram_block, &N_bit_len);
    CHECK_RET_GO;

    aca_max_op_size = aca_hwa->get_core_max_op_len(aca_hwa);
    TE_ASSERT(aca_max_op_size > 0);

    op_bits = UTILS_MAX(N_bit_len, A_bit_len);
    ret     = _op_calc_op_sram_size(aca_drv, op_bits, TE_ACA_OP_MODINV,
                                &op_sram_size);
    CHECK_RET_GO;
    TE_ASSERT(op_sram_size);

    /* Resize R */
    if (R) {
        ret = _op_ctx_resize_output(aca_drv, R, op_sram_size,
                                    ((R == A) || (R == N)));
        CHECK_RET_GO;
        used_r = R;
    } else {
        ret = op_ctx_init(aca_drv, &tmp_r, op_sram_size);
        CHECK_RET_GO;
        used_r = &tmp_r;
    }

    /* Resize GCD */
    if (GCD) {
        ret = _op_ctx_resize_output(aca_drv, GCD, op_sram_size,
                                    ((GCD == A) || (GCD == N)));
        CHECK_RET_GO;
    }

    ret = op_ctx_init(aca_drv, &a_ext_copy, op_sram_size);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, &a_ext_copy, A);
    CHECK_RET_GO;
    ret = aca_op_prepare_t0_t1_no_lock(aca_drv, &t0, &t1, op_sram_size);
    CHECK_RET_GO;

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    /* step1: call modinv */
    mod_inv_len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(mod_inv_len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all((aca_op_ctx_t *)N, GR_USAGE_N, &a_ext_copy,
                         GR_USAGE_INOUT, used_r, GR_USAGE_OUT, &t0, GR_USAGE_T0,
                         &t1, GR_USAGE_T1, NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD(aca_drv, &a_ext_copy, NULL, NULL, used_r,
                          mod_inv_len_type_id, TE_ACA_OP_MODINV);
    CHECK_RET_GO;
    /* check if HW reports modinv_zero */
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->modinv_zero) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    op_ctx_put_all((aca_op_ctx_t *)N, &a_ext_copy, used_r, &t0, &t1, NULL);
    aca_len_type_free(len_type_pool, mod_inv_len_type_id);
    mod_inv_len_type_id = -1;

    /* step2: compare result */
    op_bits             = op_sram_size * 8;
    full_op_len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(full_op_len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all(&a_ext_copy, GR_USAGE_IN, used_r, GR_USAGE_IN, GCD,
                         GR_USAGE_OUT, NULL);
    CHECK_RET_GO;

    /* copy ext A to GCD (GCD is always valid) */
    if (GCD) {
        ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &a_ext_copy, 0, NULL, GCD,
                                     full_op_len_type_id, TE_ACA_OP_ADD);
        CHECK_RET_GO;
    }

    /* check if GCD is 1 (MODINV is valid only when GCD == 1) */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &a_ext_copy, 1, NULL, NULL,
                                 full_op_len_type_id, TE_ACA_OP_XOR);
    CHECK_RET_GO;
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
        ret = TE_SUCCESS;
    } else {
        /* R is invalid */
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    op_ctx_put_all(&a_ext_copy, used_r, GCD, NULL);
    aca_len_type_free(len_type_pool, full_op_len_type_id);
    full_op_len_type_id = -1;

    /* check if N is 1 (N == 1, MODINV is also invalid) */
    op_bits             = N_bit_len;
    full_op_len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(full_op_len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all((aca_op_ctx_t *)N, GR_USAGE_IN, NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)N, 1, NULL, NULL,
                                 full_op_len_type_id, TE_ACA_OP_XOR);
    CHECK_RET_GO;
    if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }
    op_ctx_put_all((aca_op_ctx_t *)N, NULL);
    aca_len_type_free(len_type_pool, full_op_len_type_id);
    full_op_len_type_id = -1;

finish:
    op_ctx_put_all((aca_op_ctx_t *)N, &a_ext_copy, used_r, &t0, &t1, GCD, NULL);
    op_ctx_clean(&a_ext_copy);
    op_ctx_clean(&tmp_r);
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    if (mod_inv_len_type_id >= 0) {
        aca_len_type_free(len_type_pool, mod_inv_len_type_id);
    }
    if (full_op_len_type_id >= 0) {
        aca_len_type_free(len_type_pool, full_op_len_type_id);
    }
    if (is_aca_lock) {
        ACA_OP_UNLOCK(aca_drv);
    }
    return ret;
}

/**
 * \brief The following sections calculate modinv, with any N, A.
 */

/* when N is odd, directly call HW */
static int _modinv_odd(const te_aca_drv_t *aca_drv,
                       const aca_op_ctx_t *N,
                       const aca_op_ctx_t *A,
                       aca_op_ctx_t *R)
{
    return aca_op_modinv_odd(aca_drv, N, A, R, NULL);
}

/* long div used when N is even */
static int _long_div(const te_aca_drv_t *aca_drv,
                     size_t k_bit_size,
                     aca_op_ctx_t *H,
                     aca_op_ctx_t *A,
                     aca_op_ctx_t *Q,
                     aca_op_ctx_t *R)
{
    int ret               = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
    size_t max_op_size = 0, w = 0;
    size_t aca_granule_bits = 0;
    void *sram_base         = NULL;
    size_t sram_size        = 0;
    size_t max_bits         = 0;
    aca_op_ctx_t tmp[7]     = {0};

    max_op_size = aca_hwa->get_core_max_op_len(aca_hwa);
    TE_ASSERT(max_op_size);

    ret = aca_hwa->get_sram_info(aca_hwa, &sram_base, &sram_size);
    TE_ASSERT(TE_SUCCESS == ret);
    TE_ASSERT(sram_size);

    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);

    /* in aca_op_div_bn, we use 5 GRs */
    max_bits = (((sram_size * 8) / 5) / aca_granule_bits) * aca_granule_bits;
    max_bits = UTILS_MIN(max_op_size, max_bits);

    if (k_bit_size <= max_bits / 2) {
        ret = op_ctx_init(aca_drv, &(tmp[0]), (2 * k_bit_size + 7) / 8);
        CHECK_RET_GO;

        /* copy H to t0 */
        ret = aca_op_copy(aca_drv, &(tmp[0]), H);
        CHECK_RET_GO;

        /* tmp0 = tmp0 << k */
        ret =
            aca_op_shift(aca_drv, &tmp[0], &tmp[0], k_bit_size, TE_ACA_OP_SHL0);
        CHECK_RET_GO;

        ret = aca_op_div_bn(aca_drv,
                            R,
                            Q,
                            (const aca_op_ctx_t *)(&(tmp[0])),
                            (const aca_op_ctx_t *)A);
        CHECK_RET_GO;

    } else {
        w = max_bits - k_bit_size;

        /* k-w and 2k-w should < max_bits */
        TE_ASSERT(k_bit_size - w < max_bits);
        TE_ASSERT(2 * k_bit_size - w < max_bits);

        ret = op_ctx_init(aca_drv, &(tmp[0]), max_bits / 8);
        CHECK_RET_GO;
        ret = op_ctx_init(aca_drv, &(tmp[1]), max_bits / 8);
        CHECK_RET_GO;
        ret = op_ctx_init(aca_drv, &(tmp[2]), max_bits / 8);
        CHECK_RET_GO;
        ret = op_ctx_init(aca_drv, &(tmp[3]), max_bits / 8);
        CHECK_RET_GO;

        /* copy H to t0 */
        ret = aca_op_copy(aca_drv, &(tmp[0]), H);
        CHECK_RET_GO;

        /* tmp0 = tmp0 << w */
        ret = aca_op_shift(aca_drv, &tmp[0], &tmp[0], w, TE_ACA_OP_SHL0);
        CHECK_RET_GO;

        /* copy H to t3 */
        ret = aca_op_copy(aca_drv, &(tmp[3]), &(tmp[0]));
        CHECK_RET_GO;

        /* TMP1(q1): tmp0/A, TMP2(r1): tmp0 % A */
        ret = aca_op_div_bn(aca_drv,
                            &(tmp[2]),
                            &(tmp[1]),
                            (const aca_op_ctx_t *)(&(tmp[3])),
                            (const aca_op_ctx_t *)A);
        CHECK_RET_GO;

        /* tmp3: TMP2(r1) << (k - w) */
        ret = aca_op_shift(aca_drv, &(tmp[3]), &(tmp[2]), k_bit_size - w,
                           TE_ACA_OP_SHL0);
        CHECK_RET_GO;

        /* shrink tmp1 tmp2 tmp3 */
        ret = aca_sram_try_change_size(tmp[1].sram_block, 0);
        CHECK_RET_GO;
        ret = aca_sram_try_change_size(tmp[2].sram_block, 0);
        CHECK_RET_GO;
        ret = aca_sram_try_change_size(tmp[3].sram_block, 0);
        CHECK_RET_GO;

        /* TMP4(q2): tmp3/A, TMP5(r2): tmp3 % A */
        ret = aca_op_div_bn(aca_drv,
                            &(tmp[5]),
                            &(tmp[4]),
                            (const aca_op_ctx_t *)(&(tmp[3])),
                            (const aca_op_ctx_t *)A);
        CHECK_RET_GO;

        /* tmp6: TMP1(q1) << (k - w) */
        ret = aca_op_shift(aca_drv, &(tmp[6]), &(tmp[1]), k_bit_size - w,
                           TE_ACA_OP_SHL0);
        CHECK_RET_GO;

        /* Q = tmp6 + TMP4(q2) */
        ret = aca_op_run(aca_drv, Q, &(tmp[6]), &(tmp[4]), -1, NULL, NULL,
                         TE_ACA_OP_ADD, NULL);
        CHECK_RET_GO;

        /* R = TMP5(r2) */
        ret = aca_op_copy(aca_drv, R, &(tmp[5]));
        CHECK_RET_GO;
    }
finish:
    op_ctx_clean(&(tmp[0]));
    op_ctx_clean(&(tmp[1]));
    op_ctx_clean(&(tmp[2]));
    op_ctx_clean(&(tmp[3]));
    op_ctx_clean(&(tmp[4]));
    op_ctx_clean(&(tmp[5]));
    op_ctx_clean(&(tmp[6]));
    return ret;
}

/* modinv when N is even */
static int _modinv_even(const te_aca_drv_t *aca_drv,
                        const aca_op_ctx_t *N,
                        aca_op_ctx_t *A,
                        aca_op_ctx_t *R)
{
    int ret            = TE_SUCCESS;
    aca_op_ctx_t inv_n = {0};
    aca_op_ctx_t H     = {0};
    aca_op_ctx_t L     = {0};
    aca_op_ctx_t H_R   = {0};
    aca_op_ctx_t H_Q   = {0};
    size_t n_sram_size = 0, inv_n_sram_size = 0;
    size_t k_bits                 = 0;
    aca_op_ctx_t tmp[4]           = {0};
    aca_op_status_t result_status = {0};
    bool is_a_1                   = false;

    memset(tmp, 0, sizeof(tmp));

    /* if A == 1, don't calculate inv_n mod A, set to 1 */
    ret = aca_op_cmp_immeb(aca_drv, A, 1, &is_a_1);
    CHECK_RET_GO;
    if (is_a_1) {
        ret = aca_op_set_u32(aca_drv, &inv_n, 1);
        CHECK_RET_GO;
    } else {
        ret = _modinv_odd(aca_drv, (const aca_op_ctx_t *)A, (aca_op_ctx_t *)N,
                          &inv_n);
        CHECK_RET_GO;
    }

    /* shrink A, N and inv_n as much as possible */
    ret = aca_sram_try_change_size(A->sram_block, 0);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(N->sram_block, 0);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(inv_n.sram_block, 0);
    CHECK_RET_GO;

    /* L = Low of inv_n * N */
    ret = aca_op_run(aca_drv, &L, (aca_op_ctx_t *)N, &inv_n, -1, NULL, NULL,
                     TE_ACA_OP_MUL_LOW, NULL);
    CHECK_RET_GO;
    /* L = L -1 */
    ret = aca_op_run(aca_drv, &L, &L, NULL, 1, NULL, NULL, TE_ACA_OP_SUB, NULL);
    CHECK_RET_GO;

    /* H = High of inv_n * N */
    ret = aca_op_run(aca_drv, &H, (aca_op_ctx_t *)N, &inv_n, -1, NULL, NULL,
                     TE_ACA_OP_MUL_HIGH, NULL);
    CHECK_RET_GO;

    ret = aca_sram_get_size(N->sram_block, &n_sram_size);
    CHECK_RET_GO;

    ret = aca_sram_get_size(inv_n.sram_block, &inv_n_sram_size);
    CHECK_RET_GO;

    k_bits = (UTILS_MAX(n_sram_size, inv_n_sram_size) * 8);

    /* long div */
    ret = _long_div(aca_drv, k_bits, &H, A, &H_Q, &H_R);
    CHECK_RET_GO;

    ret = aca_op_run(aca_drv, &(tmp[0]), &L, &H_R, -1, NULL, NULL,
                     TE_ACA_OP_ADD, NULL);
    CHECK_RET_GO;

    /* tmp1: tmp0/A, tmp0: tmp0 % A */
    ret = aca_op_div_bn(aca_drv, &(tmp[0]), &(tmp[1]), &(tmp[0]), A);
    CHECK_RET_GO;

    /* tmp2: tmp1 + H_Q */
    ret = aca_op_run(aca_drv, &(tmp[2]), &(tmp[1]), &H_Q, -1, NULL, NULL,
                     TE_ACA_OP_ADD, NULL);
    CHECK_RET_GO;

    /* tmp3(R) = N - tmp2 */
    ret = aca_op_run(aca_drv, &(tmp[3]), (aca_op_ctx_t *)N, &(tmp[2]), -1, NULL,
                     NULL, TE_ACA_OP_SUB, &result_status);
    CHECK_RET_GO;
    TE_ASSERT(!result_status.alu_carry);

    ret = aca_op_copy(aca_drv, R, &(tmp[3]));
    CHECK_RET_GO;

finish:
    op_ctx_clean(&(tmp[0]));
    op_ctx_clean(&(tmp[1]));
    op_ctx_clean(&(tmp[2]));
    op_ctx_clean(&(tmp[3]));
    op_ctx_clean(&inv_n);
    op_ctx_clean(&H);
    op_ctx_clean(&L);
    op_ctx_clean(&H_R);
    op_ctx_clean(&H_Q);
    return ret;
}

/* Calculate modinv */
int aca_op_modinv(const te_aca_drv_t *aca_drv,
                  const aca_op_ctx_t *N,
                  const aca_op_ctx_t *A,
                  aca_op_ctx_t *R)
{
    int ret                 = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa   = NULL;
    size_t aca_granule_bits = 0;
    aca_op_ctx_t tmp_a      = {0};
    int N_lsb               = 0;
    int A_lsb               = 0;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((N) && (N->sram_block) && (N->gr_info));
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));
    CHECK_PARAM(R);
    CHECK_PARAM((N != A) && (A != R) && (N != R));

    aca_hwa = ACA_DRV_GET_HWA(aca_drv);

    /* Check A and N */
    ret = aca_sram_get_bit(N->sram_block, 0, &N_lsb);
    CHECK_RET_GO;

    ret = aca_sram_get_bit(A->sram_block, 0, &A_lsb);
    CHECK_RET_GO;

    if ((0 == N_lsb) && (0 == A_lsb)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);

    /* tmp_a = A mod N */
    ret = aca_op_mod_bn(aca_drv, &tmp_a, (aca_op_ctx_t *)A, (aca_op_ctx_t *)N);
    CHECK_RET_GO;

    /* Check tmp A lsb */
    ret = aca_sram_get_bit(tmp_a.sram_block, 0, &A_lsb);
    CHECK_RET_GO;
    if ((0 == N_lsb) && (0 == A_lsb)) {
        ret = TE_ERROR_INVAL_MOD;
        goto finish;
    }

    if (N_lsb) {
        ACA_DBG_LOG("Modinv Odd!\n");
        ret = _modinv_odd(aca_drv, N, &tmp_a, R);
    } else {
        ACA_DBG_LOG("Modinv Even!\n");
        ret = _modinv_even(aca_drv, N, &tmp_a, R);
    }
    CHECK_RET_GO;

finish:
    op_ctx_clean(&tmp_a);
    return ret;
}

/* Calculate G = GCD(A, B) */
int aca_op_gcd(const te_aca_drv_t *aca_drv,
               const aca_op_ctx_t *A,
               const aca_op_ctx_t *B,
               aca_op_ctx_t *G)
{
    int ret             = TE_SUCCESS;
    aca_op_ctx_t tmp_a  = {0};
    aca_op_ctx_t tmp_b  = {0};
    aca_op_ctx_t tmp_r  = {0};
    int32_t shift_r_num = 0;
    int32_t a_lsb = 0, b_lsb = 0;
    aca_op_ctx_t *N_ref = NULL, *A_ref = NULL;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));
    CHECK_PARAM((B) && (B->sram_block) && (B->gr_info));
    CHECK_PARAM(G);

    ret = aca_op_copy(aca_drv, &tmp_a, A);
    CHECK_RET_GO;

    ret = aca_op_copy(aca_drv, &tmp_b, B);
    CHECK_RET_GO;

    /* shift right to get one odd number */
    do {
        ret = aca_sram_get_bit(tmp_a.sram_block, 0, &a_lsb);
        CHECK_RET_GO;

        ret = aca_sram_get_bit(tmp_b.sram_block, 0, &b_lsb);
        CHECK_RET_GO;

        if ((a_lsb == 1) || (b_lsb == 1)) {
            break;
        }

        /* shift right 1 bit */
        ret = aca_op_shift(aca_drv, &tmp_a, &tmp_a, 1, TE_ACA_OP_SHR0);
        CHECK_RET_GO;

        ret = aca_op_shift(aca_drv, &tmp_b, &tmp_b, 1, TE_ACA_OP_SHR0);
        CHECK_RET_GO;
        shift_r_num++;
    } while (1);

    if (a_lsb == 1) {
        N_ref = &tmp_a;
        A_ref = &tmp_b;
    } else if (b_lsb == 1) {
        N_ref = &tmp_b;
        A_ref = &tmp_a;
    } else {
        TE_ASSERT(0);
    }

    /* call modinv odd to get GCD, ignore modinv */
    ret = aca_op_modinv_odd(aca_drv,
                            (const aca_op_ctx_t *)N_ref,
                            (const aca_op_ctx_t *)A_ref,
                            NULL,
                            &tmp_r);
    if (TE_ERROR_INVAL_MOD == (unsigned int)ret) {
        ret = TE_SUCCESS;
    }
    CHECK_RET_GO;

    /* now gcd in tmp_r */
    /* shift left shift_r_num to amend gcd */
    if (shift_r_num) {
        ret = aca_op_shift(aca_drv, G, &tmp_r, shift_r_num, TE_ACA_OP_SHL0);
        CHECK_RET_GO;
    } else {
        ret = aca_op_copy(aca_drv, G, &tmp_r);
        CHECK_RET_GO;
    }

finish:
    op_ctx_clean(&tmp_a);
    op_ctx_clean(&tmp_b);
    op_ctx_clean(&tmp_r);
    return ret;
}

/* MODEXP with E < N, directly call ACA engine */
static int __op_exp_mod_small_e(const te_aca_drv_t *aca_drv,
                                aca_op_ctx_t *R,
                                aca_op_ctx_t *A,
                                aca_op_ctx_t *E,
                                aca_op_ctx_t *N)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    aca_op_ctx_t t0                    = {0};
    int32_t len_type_id                = -1;
    size_t op_bits                     = 0;
    size_t op_sram_size                = 0;
    size_t tmp_size                    = 0;
    aca_op_ctx_t *np                   = NULL;
    bool is_aca_lock                   = false;

    ret = aca_sram_get_bit_len(N->sram_block, &op_bits);
    CHECK_RET_GO;

    ret = _op_calc_op_sram_size(aca_drv, op_bits, TE_ACA_OP_MODEXP,
                                &op_sram_size);
    CHECK_RET_GO;

    ret = aca_sram_get_size(N->sram_block, &tmp_size);
    CHECK_RET_GO;
    TE_ASSERT(tmp_size >= op_sram_size);

    ret = aca_sram_get_size(A->sram_block, &tmp_size);
    CHECK_RET_GO;
    TE_ASSERT(tmp_size >= op_sram_size);

    ret = aca_sram_get_size(E->sram_block, &tmp_size);
    CHECK_RET_GO;
    TE_ASSERT(tmp_size >= op_sram_size);

    ret = aca_sram_get_size(R->sram_block, &tmp_size);
    CHECK_RET_GO;
    TE_ASSERT(tmp_size >= op_sram_size);

    /* init t0 */
    ret = op_ctx_init(aca_drv, &t0, op_sram_size);
    CHECK_RET_GO;

    /* update NP */
    ret = op_ctx_update_np(aca_drv, (aca_op_ctx_t *)N);
    CHECK_RET_GO;
    TE_ASSERT(N->extra_np);
    np = &(((aca_drv_extra_np_t *)(N->extra_np))->op_P_ctx);

    /* lock */
    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    len_type_id = aca_len_type_alloc(len_type_pool, op_bits);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all(A, GR_USAGE_INOUT, E, GR_USAGE_INOUT, N, GR_USAGE_N, R,
                         GR_USAGE_OUT, np, GR_USAGE_P, &t0, GR_USAGE_T0, NULL);
    CHECK_RET_GO;

    ret =
        OP_EXEC_ONE_CMD(aca_drv, A, E, NULL, R, len_type_id, TE_ACA_OP_MODEXP);
    CHECK_RET_GO;
    CHECK_COND_GO(((!ACA_DRV_GET_OP_STATUS(aca_drv)->mult_red_err) &&
                   (!ACA_DRV_GET_OP_STATUS(aca_drv)->mod_n_zero_err)),
                  TE_ERROR_INVAL_MOD);

    ret = TE_SUCCESS;
finish:
    if (len_type_id >= 0) {
        aca_len_type_free(len_type_pool, len_type_id);
    }
    op_ctx_put_all(A, E, N, R, np, &t0, NULL);
    op_ctx_clean(&t0);
    if (is_aca_lock) {
        ACA_OP_UNLOCK(aca_drv);
    }
    return ret;
}

/**
 * Calcualte MODEXP when E > N.
 * Algorithm:
 * R = A^E mod N
 *
 * E = EM * EQ + ER, where EM is max E supported by ACA engine for N.
 * A ^ E = A ^ (EM * EQ + ER) = A^(EM * EQ) * A^ER = (A^EM)^EQ * A^ER
 *
 * RR  = 1
 * EE = E
 * AA = A
 * while (true) {
 *      EQ = EE / EM
 *      ER = EE % EM
 *      tr = AA ^ ER
 *      RR  = RR * tr
 *      if (EQ == 0) {
 *          break;
 *      }
 *      AA = AA ^ EM
 *      EE = EQ
 *}
 *
 **/

static int __op_exp_mod_large_e(const te_aca_drv_t *aca_drv,
                                aca_op_ctx_t *R,
                                aca_op_ctx_t *A,
                                aca_op_ctx_t *E,
                                aca_op_ctx_t *N)
{
    int ret                       = TE_SUCCESS;
    aca_op_ctx_t EM               = {0};
    aca_op_ctx_t RR               = {0};
    aca_op_ctx_t EE               = {0};
    aca_op_ctx_t EQ               = {0};
    aca_op_ctx_t ER               = {0};
    aca_op_ctx_t tr               = {0};
    aca_op_ctx_t AA               = {0};
    aca_op_ctx_t tmp              = {0};
    size_t n_bit_len              = 0;
    size_t op_sram_size           = 0;
    size_t e_sram_size            = 0;
    aca_op_status_t result_status = {0};

    ret = aca_sram_get_bit_len(N->sram_block, &n_bit_len);
    CHECK_RET_GO;

    ret = _op_calc_op_sram_size(aca_drv, n_bit_len, TE_ACA_OP_MODEXP,
                                &op_sram_size);
    CHECK_RET_GO;

    ret = aca_sram_get_size(E->sram_block, &e_sram_size);
    CHECK_RET_GO;

    ret = op_ctx_init(aca_drv, &RR, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &AA, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tr, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tmp, op_sram_size);
    CHECK_RET_GO;

    ret = op_ctx_init(aca_drv, &EM, e_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &EE, e_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &ER, e_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &EQ, e_sram_size);
    CHECK_RET_GO;

    /* EM = (1 << (n_bit_len))  - 1 */
    ret =
        aca_op_run(aca_drv, &EM, &EM, NULL, 0, NULL, NULL, TE_ACA_OP_AND, NULL);
    CHECK_RET_GO;
    ret = aca_sram_set_bit(EM.sram_block, n_bit_len, 1);
    CHECK_RET_GO;
    ret =
        aca_op_run(aca_drv, &EM, &EM, NULL, 1, NULL, NULL, TE_ACA_OP_SUB, NULL);
    CHECK_RET_GO;

    /* RR = 1 */
    ret =
        aca_op_run(aca_drv, &RR, &RR, NULL, 0, NULL, NULL, TE_ACA_OP_AND, NULL);
    CHECK_RET_GO;
    ret =
        aca_op_run(aca_drv, &RR, &RR, NULL, 1, NULL, NULL, TE_ACA_OP_ADD, NULL);
    CHECK_RET_GO;

    /* EE = E */
    ret = aca_op_run(aca_drv, &EE, E, NULL, 0, NULL, NULL, TE_ACA_OP_ADD, NULL);
    CHECK_RET_GO;

    /* AA = A */
    ret = aca_op_run(aca_drv, &AA, A, NULL, 0, NULL, NULL, TE_ACA_OP_ADD, NULL);
    CHECK_RET_GO;

    while (true) {
        /* EQ = EE / EM, ER = EE % EM */
        ret = aca_op_div_bn(aca_drv, &ER, &EQ, (const aca_op_ctx_t *)(&EE),
                            (const aca_op_ctx_t *)(&EM));
        CHECK_RET_GO;

        /* tr = AA ^ ER mod N */
        ret = __op_exp_mod_small_e(aca_drv, &tr, &AA, &ER, N);
        CHECK_RET_GO;

        /* RR = (RR * tr) mod N */
        ret = aca_op_run(aca_drv, &tmp, &RR, &tr, -1, NULL, N, TE_ACA_OP_MODMUL,
                         &result_status);
        CHECK_RET_GO;
        CHECK_COND_GO(
            ((!result_status.mult_red_err) && (!result_status.mod_n_zero_err)),
            TE_ERROR_INVAL_MOD);
        ret = aca_op_copy(aca_drv, &RR, (const aca_op_ctx_t *)(&tmp));
        CHECK_RET_GO;

        /* Check EQ == 0 */
        ret = aca_op_run(aca_drv, NULL, &EQ, NULL, 0, NULL, NULL, TE_ACA_OP_ADD,
                         &result_status);
        CHECK_RET_GO;
        if (result_status.add_result_zero) {
            break;
        }

        /* AA = AA^EM mod N */
        ret = __op_exp_mod_small_e(aca_drv, &tr, &AA, &EM, N);
        CHECK_RET_GO;
        ret = aca_op_copy(aca_drv, &AA, (const aca_op_ctx_t *)(&tr));
        CHECK_RET_GO;

        /* EE = EQ */
        ret = aca_op_copy(aca_drv, &EE, (const aca_op_ctx_t *)(&EQ));
        CHECK_RET_GO;
    }

    ret = aca_op_copy(aca_drv, R, (const aca_op_ctx_t *)(&RR));
    CHECK_RET_GO;

finish:
    op_ctx_clean(&EM);
    op_ctx_clean(&RR);
    op_ctx_clean(&EE);
    op_ctx_clean(&EQ);
    op_ctx_clean(&ER);
    op_ctx_clean(&AA);
    op_ctx_clean(&tr);
    op_ctx_clean(&tmp);
    return ret;
}

static int _op_exp_mod(const te_aca_drv_t *aca_drv,
                       aca_op_ctx_t *R,
                       aca_op_ctx_t *A,
                       aca_op_ctx_t *E,
                       aca_op_ctx_t *N)
{
    int ret          = TE_SUCCESS;
    size_t n_bit_len = 0;
    size_t e_bit_len = 0;

    ret = aca_sram_get_bit_len(N->sram_block, &n_bit_len);
    CHECK_RET_GO;
    ret = aca_sram_get_bit_len(E->sram_block, &e_bit_len);
    CHECK_RET_GO;

    if (e_bit_len > n_bit_len) {
        ret = __op_exp_mod_large_e(aca_drv, R, A, E, N);
        CHECK_RET_GO;
    } else {
        ret = __op_exp_mod_small_e(aca_drv, R, A, E, N);
        CHECK_RET_GO;
    }
finish:
    return ret;
}

/**
 * \brief calculate R = A^E mod N.
 * Support:
 * 1. A > N
 * 2. E > N
 * 3. A < N
 * 4. E < N
 * 5. N is odd
 * 6. N is even (mbedtls doesn't support N is even)
 */
int aca_op_mod_exp(const te_aca_drv_t *aca_drv,
                   aca_op_ctx_t *R,
                   aca_op_ctx_t *A,
                   aca_op_ctx_t *E,
                   aca_op_ctx_t *N)
{
    int ret              = TE_SUCCESS;
    aca_op_ctx_t tmp_n   = {0};
    aca_op_ctx_t tmp_a   = {0};
    aca_op_ctx_t tmp_e   = {0};
    aca_op_ctx_t tmp_r   = {0};
    size_t op_bits       = 0;
    size_t a_bit_len     = 0;
    size_t op_sram_size  = 0;
    size_t tmp_size      = 0;
    aca_op_ctx_t *used_n = NULL, *used_a = NULL, *used_e = NULL, *used_r = NULL;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((N) && (N->sram_block) && (N->gr_info));
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));
    CHECK_PARAM((E) && (E->sram_block) && (E->gr_info));
    CHECK_PARAM(R);
    CHECK_PARAM((R != N) && (A != N) && (E != N));

    ret = aca_sram_get_bit_len(N->sram_block, &op_bits);
    CHECK_RET_GO;

    ret = aca_sram_get_bit_len(A->sram_block, &a_bit_len);
    CHECK_RET_GO;

    ret = _op_calc_op_sram_size(aca_drv, op_bits, TE_ACA_OP_MODEXP,
                                &op_sram_size);
    CHECK_RET_GO;

    /* Resize N. TODO: remove this */
    ret = aca_sram_get_size(N->sram_block, &tmp_size);
    CHECK_RET_GO;
    if (tmp_size < op_sram_size) {
        ret = op_ctx_init(aca_drv, &tmp_n, op_sram_size);
        CHECK_RET_GO;
        ret = aca_op_copy(aca_drv, &tmp_n, (const aca_op_ctx_t *)N);
        CHECK_RET_GO;
        used_n = &tmp_n;
    } else {
        used_n = N;
    }

    /* Resize A. TODO: remove this */
    ret = aca_sram_get_size(A->sram_block, &tmp_size);
    CHECK_RET_GO;
    if ((tmp_size < op_sram_size) || (a_bit_len > op_bits)) {
        ret = op_ctx_init(aca_drv, &tmp_a, op_sram_size);
        CHECK_RET_GO;
        ret = aca_op_copy(aca_drv, &tmp_a, (const aca_op_ctx_t *)A);
        CHECK_RET_GO;
        if (a_bit_len > op_bits) {
            /* a = a mod N */
            ret = aca_op_mod_bn(aca_drv, &tmp_a, &tmp_a, N);
            CHECK_RET_GO;
        }
        used_a = &tmp_a;
    } else {
        used_a = A;
    }

    /* Resize E. TODO: remove this */
    ret = aca_sram_get_size(E->sram_block, &tmp_size);
    CHECK_RET_GO;
    if (tmp_size < op_sram_size) {
        ret = op_ctx_init(aca_drv, &tmp_e, op_sram_size);
        CHECK_RET_GO;
        ret = aca_op_copy(aca_drv, &tmp_e, (const aca_op_ctx_t *)E);
        CHECK_RET_GO;
        used_e = &tmp_e;
    } else {
        used_e = E;
    }

    /* Resize R */
    if ((R == A) || (R == E) || (R == N)) {
        /* use tmpe R is R is one of input */
        ret = op_ctx_init(aca_drv, &tmp_r, op_sram_size);
        CHECK_RET_GO;
        used_r = &tmp_r;
    } else {
        ret = _op_ctx_resize_output(aca_drv, R, op_sram_size,
                                    ((R == A) || (R == E) || (R == N)));
        CHECK_RET_GO;
        used_r = R;
    }

    ret = _op_exp_mod(aca_drv, used_r, used_a, used_e, used_n);
    CHECK_RET_GO;

    /* copy back? */
    if (used_r != R) {
        ret = aca_op_copy(aca_drv, R, (const aca_op_ctx_t *)used_r);
        CHECK_RET_GO;
    }

finish:
    op_ctx_clean(&tmp_a);
    op_ctx_clean(&tmp_e);
    op_ctx_clean(&tmp_n);
    op_ctx_clean(&tmp_r);
    return ret;
}

#if 0
/****************************/
/* check prime small factor */
/****************************/
/**
 * TODO: Not done.
 */
static const int _small_prime_array[] = {
    3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,
    53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109,
    113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191,
    193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269,
    271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353,
    359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439,
    443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523,
    541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617,
    619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709,
    719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811,
    821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907,
    911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997, -103};

/*
 * Small divisors test (X must be positive)
 *
 * Return values:
 * 0: no small factor (possible prime, more tests needed)
 * 1: certain prime
 * TE_ERROR_NOT_ACCEPTABLE: certain non-prime
 * other negative: error
 */

static int _aca_op_check_prime_small_factors(const te_aca_drv_t *aca_drv,
                                             const aca_op_ctx_t *X)
{
    int ret                            = TE_SUCCESS;
    size_t i                           = 0;
    aca_op_ctx_t small_prime           = {0};
    aca_op_ctx_t tmp_X                 = {0};
    aca_op_ctx_t tmp_r                 = {0};
    aca_op_ctx_t tmp_q                 = {0};
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    size_t op_sram_size                = 0;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    int32_t len_type_id                = -1;
    int bit_val                        = 0;
    bool is_aca_lock                   = false;
    uint8_t tmp_buf[4]                 = {0};

    ret = aca_sram_get_size(X->sram_block, &op_sram_size);
    CHECK_RET_GO;

    /* check the lsb of X */
    ret = aca_sram_get_bit(X->sram_block, 0, &bit_val);
    CHECK_RET_GO;
    if (bit_val == 0) {
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    }

    /* start to check small factor */
    /* init temp op context */
    ret = op_ctx_init(aca_drv, &small_prime, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tmp_X, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tmp_r, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &tmp_q, op_sram_size);
    CHECK_RET_GO;

    /* init small prime to 0 */
    ret = aca_op_set_u32(aca_drv, &small_prime, 0);
    CHECK_RET_GO;

    /* init t0, t1 */
    ret = aca_op_prepare_t0_t1(aca_drv, &t0, &t1, op_sram_size);
    CHECK_RET_GO;

    /* OP lock */
    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    len_type_id = aca_len_type_alloc(len_type_pool, op_sram_size * 8);
    CHECK_COND_GO(len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* lock GRs */
    ret = op_ctx_get_all((aca_op_ctx_t *)X, GR_USAGE_IN, &tmp_X, GR_USAGE_INOUT,
                         &tmp_r, GR_USAGE_INOUT, &tmp_q, GR_USAGE_INOUT,
                         &small_prime, GR_USAGE_INOUT, &t0, GR_USAGE_T0, &t1,
                         GR_USAGE_T1, NULL);
    CHECK_RET_GO;

    for (i = 0; _small_prime_array[i] > 0; i++) {
        tmp_buf[0] = (((_small_prime_array[i]) >> 24) & 0xFF);
        tmp_buf[1] = (((_small_prime_array[i]) >> 16) & 0xFF);
        tmp_buf[2] = (((_small_prime_array[i]) >> 8) & 0xFF);
        tmp_buf[3] = (((_small_prime_array[i]) >> 0) & 0xFF);

        /* wrtei small_prime */
        ret = aca_sram_write(small_prime.sram_block, tmp_buf, 4);
        CHECK_RET_GO;

        /* cmpare X with small prime */
        ret = OP_EXEC_ONE_CMD(aca_drv, &small_prime, (aca_op_ctx_t *)X, NULL,
                              NULL, len_type_id, TE_ACA_OP_SUB);
        CHECK_RET_GO;
        if (!ACA_DRV_GET_OP_STATUS(aca_drv)->alu_carry) {
            ret = 1;
            goto finish;
        }

        /* copy X to tmp_X */
        ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)X, 0, NULL,
                                     &tmp_X, len_type_id, TE_ACA_OP_ADD);
        CHECK_RET_GO;

        /* calc X mod small prime */
        ret = OP_EXEC_ONE_CMD(aca_drv, &tmp_X, &small_prime, NULL, &tmp_r,
                              len_type_id, TE_ACA_OP_DIV);
        CHECK_RET_GO;

        /* now tmp_X contains X mod small_prime */

        /* check tmp_X is 0 */
        ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &tmp_X, 0, NULL, NULL,
                                     len_type_id, TE_ACA_OP_ADD);
        CHECK_RET_GO;
        if (ACA_DRV_GET_OP_STATUS(aca_drv)->add_result_zero) {
            ret = TE_ERROR_NOT_ACCEPTABLE;
            goto finish;
        }
    }

finish:
    if (is_aca_lock) {
        if (len_type_id > 0) {
            aca_len_type_free(len_type_pool, len_type_id);
        }
        op_ctx_put_all((aca_op_ctx_t *)X, &tmp_X, &tmp_r, &tmp_q, &small_prime,
                       &t0, &t1, NULL);
        ACA_OP_UNLOCK(aca_drv);
        is_aca_lock = false;
    }

    op_ctx_clean(&small_prime);
    op_ctx_clean(&tmp_X);
    op_ctx_clean(&tmp_r);
    op_ctx_clean(&tmp_q);
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    return ret;
}

static int
_aca_op_check_prime_miller_rabin(const te_aca_drv_t *aca_drv,
                                 const aca_op_ctx_t *X,
                                 int32_t rounds,
                                 int (*f_rng)(void *, uint8_t *, size_t),
                                 void *p_rng)
{
    int ret                            = TE_SUCCESS;
    aca_op_ctx_t W                     = {0};
    aca_op_ctx_t R                     = {0};
    aca_op_ctx_t A1                    = {0};
    aca_op_ctx_t A2                    = {0};
    aca_op_ctx_t tmp                   = {0};
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    aca_op_ctx_t *np                   = NULL;
    int32_t i                          = 0;
    int32_t count                      = 0;
    uint8_t *rand_a_buf                = NULL;
    size_t rand_a_size                 = 0;
    size_t j                           = 0;
    size_t s                           = 0;
    int bit_val                        = 0;
    size_t X_bit_len                   = 0;
    size_t op_sram_size                = 0;
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    int32_t full_len_type_id           = -1;
    int32_t mod_len_type_id            = -1;
    bool is_aca_lock                   = false;

    ret = aca_sram_get_bit_len(X->sram_block, &X_bit_len);
    CHECK_RET_GO;

    ret = aca_sram_get_size(X->sram_block, &op_sram_size);
    CHECK_RET_GO;

    /* extra one bytes */
    op_sram_size++;

    /* init W, R, A, t0, t1 and other tmp OP_CTX to op_sram_size */
    ret = op_ctx_init(aca_drv, &W, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &R, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &A1, op_sram_size);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &A2, op_sram_size);
    CHECK_RET_GO;
    CHECK_RET_GO;
    ret = aca_op_prepare_t0_t1(aca_drv, &t0, &t1, op_sram_size);
    CHECK_RET_GO;

    /* update np */
    ret = op_ctx_update_np(aca_drv, (aca_op_ctx_t *)X);
    CHECK_RET_GO;
    TE_ASSERT(X->extra_np);
    np = &(((aca_drv_extra_np_t *)(X->extra_np))->op_P_ctx);

    /* init random A buffer */
    rand_a_size = (X_bit_len + 7) / 8;
    rand_a_buf  = osal_malloc(rand_a_size);
    CHECK_COND_GO(rand_a_buf, TE_ERROR_OOM);

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    full_len_type_id = aca_len_type_alloc(len_type_pool, op_sram_size * 8);
    CHECK_COND_GO(full_len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    mod_len_type_id = aca_len_type_alloc(len_type_pool, X_bit_len);
    CHECK_COND_GO(full_len_type_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all((aca_op_ctx_t *)X, GR_USAGE_N, np, GR_USAGE_P, &t0,
                         GR_USAGE_T0, &t1, GR_USAGE_T1, &W, GR_USAGE_INOUT, &R,
                         GR_USAGE_INOUT, &A1, GR_USAGE_INOUT, &A2,
                         GR_USAGE_INOUT, NULL);
    CHECK_RET_GO;

    /* W = X - 1 */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)X, 1, NULL, &W,
                                 full_len_type_id, TE_ACA_OP_SUB);
    CHECK_RET_GO;
    TE_ASSERT(!ACA_DRV_GET_OP_STATUS(aca_drv)->alu_carry);

    /* R = W */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &W, 0, NULL, &R, full_len_type_id,
                                 TE_ACA_OP_ADD);

    /* R = R >> s, where s = lsb W */
    s = 0;
    while (true) {
        /* Check bit0 of R */
        ret = aca_sram_get_bit(R.sram_block, 0, &bit_val);
        CHECK_RET_GO;
        if (bit_val) {
            break;
        }
        /* R = R >> 1 */
        ret = OP_EXEC_ONE_CMD_SHIFT(aca_drv, &R, 0, &R, full_len_type_id,
                                    TE_ACA_OP_SHR0);
        CHECK_RET_GO;
        s++;
    }
    OSAL_LOG_DEBUG("S: %d\n", s);

    /* start check loop */
    for (i = 0; i < rounds; i++) {
        /* pick a random A1, 1 < A1 < |X| - 1 */
        count = 0;
        while (true) {
            if (count++ > 30) {
                ret = TE_ERROR_NOT_ACCEPTABLE;
                goto finish;
            }

            ret = f_rng(p_rng, rand_a_buf, rand_a_size);
            CHECK_RET_GO;
            rand_a_buf[0] &= ((1 << (X_bit_len % 8)) - 1);

            ret = aca_sram_write(A1.sram_block, (const uint8_t *)(rand_a_buf),
                                 rand_a_size);
            CHECK_RET_GO;

            ret = OP_EXEC_ONE_CMD(aca_drv, &A1, &W, NULL, NULL,
                                  full_len_type_id, TE_ACA_OP_SUB);
            CHECK_RET_GO;
            if (!ACA_DRV_GET_OP_STATUS(aca_drv)->alu_carry) {
                continue;
            }

            ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &A1, 2, NULL, NULL,
                                         full_len_type_id, TE_ACA_OP_SUB);
            CHECK_RET_GO;
            if (ACA_DRV_GET_OP_STATUS(aca_drv)->alu_carry) {
                continue;
            }
            break;
        }

        /* A2 = A1^R mod |X| */
        ret = OP_EXEC_ONE_CMD(aca_drv, &A1, &R, NULL, &A2, mod_len_type_id,
                              TE_ACA_OP_MODEXP);
        CHECK_RET_GO;

        /* Check A2 == W or 1 */
        ret = OP_EXEC_ONE_CMD(aca_drv, &A2, &W, NULL, NULL, full_len_type_id,
                              TE_ACA_OP_XOR);
        CHECK_RET_GO;
        if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
            goto __next_loop;
        }
        ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &A2, 1, NULL, NULL,
                                     full_len_type_id, TE_ACA_OP_XOR);
        CHECK_RET_GO;
        if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
            goto __next_loop;
        }

        for (j = 1; j < s; j++) {
            if (j & 1) {
                /* A1 = A2 * A2 mod X */
                ret = OP_EXEC_ONE_CMD(aca_drv, &A2, &A2, NULL, &A1,
                                      mod_len_type_id, TE_ACA_OP_MODMUL);
                CHECK_RET_GO;
                /* Check A1 == W */
                ret = OP_EXEC_ONE_CMD(aca_drv, &A1, &W, NULL, NULL,
                                      full_len_type_id, TE_ACA_OP_XOR);
                CHECK_RET_GO;
                if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
                    goto __next_loop;
                }
                /* Check A1 == 1 */
                ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &A1, 1, NULL, NULL,
                                             full_len_type_id, TE_ACA_OP_XOR);
                CHECK_RET_GO;
                if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
                    ret = TE_ERROR_NOT_ACCEPTABLE;
                    goto finish;
                }
            } else {
                /* A2 = A1 * A1 mod X */
                ret = OP_EXEC_ONE_CMD(aca_drv, &A1, &A1, NULL, &A2,
                                      mod_len_type_id, TE_ACA_OP_MODMUL);
                CHECK_RET_GO;
                /* Check A2 == W */
                ret = OP_EXEC_ONE_CMD(aca_drv, &A2, &W, NULL, NULL,
                                      full_len_type_id, TE_ACA_OP_XOR);
                CHECK_RET_GO;
                if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
                    goto __next_loop;
                }
                /* Check A2 == 1 */
                ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, &A2, 1, NULL, NULL,
                                             full_len_type_id, TE_ACA_OP_XOR);
                CHECK_RET_GO;
                if (ACA_DRV_GET_OP_STATUS(aca_drv)->xor_result_zero) {
                    ret = TE_ERROR_NOT_ACCEPTABLE;
                    goto finish;
                }
            }
        }
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    __next_loop:
        continue;
    }
finish:
    if (is_aca_lock) {
        if (full_len_type_id > 0) {
            aca_len_type_free(len_type_pool, full_len_type_id);
        }
        if (mod_len_type_id > 0) {
            aca_len_type_free(len_type_pool, mod_len_type_id);
        }
        op_ctx_put_all((aca_op_ctx_t *)X, np, &t0, &t1, &W, &R, &A1, &A2, NULL);
        ACA_OP_UNLOCK(aca_drv);
        is_aca_lock = false;
    }

    op_ctx_clean(&W);
    op_ctx_clean(&R);
    op_ctx_clean(&A1);
    op_ctx_clean(&A2);
    OSAL_SAFE_FREE(rand_a_buf);
    return ret;
}

int aca_op_check_prime(const te_aca_drv_t *aca_drv,
                       const aca_op_ctx_t *X,
                       int32_t rounds,
                       int (*f_rng)(void *, uint8_t *, size_t),
                       void *p_rng)
{
    int ret        = TE_SUCCESS;
    size_t bit_len = 0;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM(X);
    CHECK_PARAM(rounds);
    CHECK_PARAM(f_rng);

    ret = aca_sram_get_bit_len(X->sram_block, &bit_len);
    CHECK_RET_GO;
    if ((bit_len == 0) || (bit_len == 1)) {
        /* 0 and 1 not prime */
        ret = TE_ERROR_NOT_ACCEPTABLE;
        goto finish;
    } else if (bit_len == 2) {
        /* 2 and 3 are prime */
        ret = TE_SUCCESS;
        goto finish;
    }

    ret = _aca_op_check_prime_small_factors(aca_drv, X);
    if (1 == ret) {
        ret = TE_SUCCESS;
        goto finish;
    } else if (TE_SUCCESS == ret) {
        ret =
            _aca_op_check_prime_miller_rabin(aca_drv, X, rounds, f_rng, p_rng);
    } else {
        /* do nothing */
    }

finish:
    return ret;
}

#endif

/********************/
/***     ECP OP   ***/
/********************/

/* macros to support ECP debug */
#undef ECP_DBG_PRINT_DETAIL_EN
#undef ECP_DBG_PRINT_SIMPLE_EN
/**
 * ECP calling sequence:
 * 1. OP_ECP_PREPARE
 * 2. OP_ECP_EXEC
 * 3. OP_ECP_EXEC
 *    ...
 * 4. OP_ECP_WAIT
 */

#ifdef ECP_DBG_PRINT_DETAIL_EN

#define OP_ECP_PREPARE(__empty__)
#define OP_ECP_EXEC(__code__, __len_type_id__, __op_A__, __is_B_imme__, __B__, \
                    __op_C__, __op_R__)                                        \
    do {                                                                       \
        if (__is_B_imme__) {                                                   \
            ret = OP_EXEC_ONE_CMD_IMME_B(                                      \
                aca_drv, __op_A__, (int32_t)(uintptr_t)__B__, __op_C__,        \
                __op_R__, __len_type_id__, TE_ACA_OP_##__code__);              \
            CHECK_RET_GO;                                                      \
        } else {                                                               \
            ret = OP_EXEC_ONE_CMD(aca_drv, __op_A__, (aca_op_ctx_t *)__B__,    \
                                  __op_C__, __op_R__, __len_type_id__,         \
                                  TE_ACA_OP_##__code__);                       \
            CHECK_RET_GO;                                                      \
        }                                                                      \
    } while (0)

#define OP_ECP_WAIT(__empty__)

#else /* ECP_DBG_PRINT_DETAIL_EN */

#define OP_ECP_PREPARE(__empty__)                                              \
    do {                                                                       \
        ret = OP_PREPARE_BATCH_CMD(aca_drv);                                   \
        CHECK_RET_GO;                                                          \
    } while (0)

#define OP_ECP_EXEC(__code__, __len_type_id__, __op_A__, __is_B_imme__, __B__, \
                    __op_C__, __op_R__)                                        \
    do {                                                                       \
        if (__is_B_imme__) {                                                   \
            ret = OP_SUBMIT_BATCH_CMD_IMME_B(                                  \
                aca_drv, __op_A__, (int32_t)(uintptr_t)__B__, __op_C__,        \
                __op_R__, __len_type_id__, TE_ACA_OP_##__code__);              \
            CHECK_RET_GO;                                                      \
        } else {                                                               \
            ret = OP_SUBMIT_BATCH_CMD(                                         \
                aca_drv, __op_A__, (aca_op_ctx_t *)__B__, __op_C__, __op_R__,  \
                __len_type_id__, TE_ACA_OP_##__code__);                        \
            CHECK_RET_GO;                                                      \
        }                                                                      \
    } while (0)

#define OP_ECP_WAIT(__empty__)                                                 \
    do {                                                                       \
        ret = OP_START_BATCH_CMD(aca_drv);                                     \
        CHECK_RET_GO;                                                          \
        ret = OP_WAIT_BATCH_CMD_FINISH(aca_drv);                               \
        CHECK_RET_GO;                                                          \
    } while (0)

#endif /* ECP_DBG_PRINT_DETAIL_EN */

/* convert X, Y in affine to jacobian */
int aca_ecp_op_convert_affine_to_jacobian(const te_aca_drv_t *aca_drv,
                                          const aca_op_ctx_t *P,
                                          const aca_op_ctx_t *X,
                                          const aca_op_ctx_t *Y,
                                          aca_op_ctx_t *jx,
                                          aca_op_ctx_t *jy,
                                          aca_op_ctx_t *jz)
{
    int ret = TE_SUCCESS;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((P) && (P->sram_block) && (P->gr_info));
    CHECK_PARAM((X) && (X->sram_block) && (X->gr_info));
    CHECK_PARAM((Y) && (Y->sram_block) && (Y->gr_info));
    CHECK_PARAM(jx);
    CHECK_PARAM(jy);
    CHECK_PARAM(jz);

    ret = aca_op_copy(aca_drv, jx, X);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, jy, Y);
    CHECK_RET_GO;
    ret = aca_op_set_u32(aca_drv, jz, 1);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
    return ret;
}

/* convert jacobian jx, jy, jz to affine X, Y */
int aca_ecp_op_convert_jacobian_to_affine(const te_aca_drv_t *aca_drv,
                                          const aca_op_ctx_t *P,
                                          const aca_op_ctx_t *jx,
                                          const aca_op_ctx_t *jy,
                                          const aca_op_ctx_t *jz,
                                          aca_op_ctx_t *X,
                                          aca_op_ctx_t *Y,
                                          aca_op_ctx_t *Z)
{
    int ret                            = TE_SUCCESS;
    aca_len_type_pool_t *len_type_pool = NULL;
    te_hwa_aca_t *aca_hwa              = NULL;
    size_t aca_granule_bits = 0, N_bit_len = 0, full_bit_len = 0;
    int32_t len_type_N_id = -1;
    aca_op_ctx_t t0       = {0};
    aca_op_ctx_t t1       = {0};
    aca_op_ctx_t *np      = NULL;
    aca_op_ctx_t tmp[5]   = {0};
    aca_op_ctx_t jz_copy  = {0};
    size_t i              = 0;
    bool is_aca_lock      = false;
#ifdef CFG_TE_DYNCLK_CTL
    bool is_clk_en = false;
#endif

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((P) && (P->sram_block) && (P->gr_info));
    CHECK_PARAM((jx) && (jx->sram_block) && (jx->gr_info));
    CHECK_PARAM((jy) && (jy->sram_block) && (jy->gr_info));
    CHECK_PARAM((jz) && (jz->sram_block) && (jz->gr_info));
    CHECK_PARAM(X);
    CHECK_PARAM(Y);
    CHECK_PARAM(Z);

    len_type_pool    = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    aca_hwa          = ACA_DRV_GET_HWA(aca_drv);
    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits);

    ret = aca_sram_get_bit_len(P->sram_block, &N_bit_len);
    CHECK_RET_GO;
    /* extra 3 bits */
    full_bit_len = UTILS_ROUND_UP(N_bit_len + 3, aca_granule_bits);

    /* update np */
    ret = op_ctx_update_np(aca_drv, (aca_op_ctx_t *)P);
    CHECK_RET_GO;
    TE_ASSERT(P->extra_np);
    np = &(((aca_drv_extra_np_t *)(P->extra_np))->op_P_ctx);

    /* resize */
    ret = aca_sram_try_change_size(P->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(jx->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(jy->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(jz->sram_block, full_bit_len / 8);
    CHECK_RET_GO;

    /* init jz */
    ret = op_ctx_init(aca_drv, &jz_copy, full_bit_len / 8);
    CHECK_RET_GO;

    /* copy jz to jz_copy. jz_copy size > jz size */
    ret = aca_op_copy(aca_drv, &jz_copy, jz);
    CHECK_RET_GO;

    ret = aca_op_prepare_t0_t1_no_lock(aca_drv, &t0, &t1, full_bit_len / 8);
    CHECK_RET_GO;

    for (i = 0; i < 5; i++) {
        ret = op_ctx_init(aca_drv, &tmp[i], full_bit_len / 8);
        CHECK_RET_GO;
    }

    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    len_type_N_id = aca_len_type_alloc(len_type_pool, N_bit_len);
    CHECK_COND_GO(len_type_N_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    ret = op_ctx_get_all((aca_op_ctx_t *)jx, GR_USAGE_IN, (aca_op_ctx_t *)jy,
                         GR_USAGE_IN, &jz_copy, GR_USAGE_INOUT, P, GR_USAGE_N,
                         np, GR_USAGE_P, &t0, GR_USAGE_T0, &t1, GR_USAGE_T1,
                         &tmp[0], GR_USAGE_INOUT, &tmp[1], GR_USAGE_INOUT,
                         &tmp[2], GR_USAGE_INOUT, &tmp[3], GR_USAGE_INOUT,
                         &tmp[4], GR_USAGE_INOUT, NULL);
    CHECK_RET_GO;

    /*
    #GR14: modINV(Z)=1/Z
    #GR15: (1/Z)*(1/Z) = 1/(Z*Z)
    #GR16: (1/Z)*(1/Z)*(1/Z) = 1/(Z*Z*Z)
    #GR18: modMUL(x*(1/(Z*Z)))     --- affine result
    #GR19: modMUL(y*(1/(Z*Z*Z)))   --- affine result
    */

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    is_clk_en = true;
#endif

    OP_ECP_PREPARE();

    OP_ECP_EXEC(MODINV, len_type_N_id, &jz_copy, 0, 0, 0, &tmp[0]);
    OP_ECP_EXEC(MODMUL, len_type_N_id, &tmp[0], 0, &tmp[0], 0, &tmp[1]);
    OP_ECP_EXEC(MODMUL, len_type_N_id, &tmp[0], 0, &tmp[1], 0, &tmp[2]);
    OP_ECP_EXEC(MODMUL, len_type_N_id, (aca_op_ctx_t *)jx, 0, &tmp[1], 0,
                &tmp[3]);
    OP_ECP_EXEC(MODMUL, len_type_N_id, (aca_op_ctx_t *)jy, 0, &tmp[2], 0,
                &tmp[4]);

    OP_ECP_WAIT();

    CHECK_COND_GO(((!ACA_DRV_GET_OP_STATUS(aca_drv)->mult_red_err) &&
                   (!ACA_DRV_GET_OP_STATUS(aca_drv)->mod_n_zero_err)),
                  TE_ERROR_INVAL_MOD);

    op_ctx_put_all((aca_op_ctx_t *)jx, (aca_op_ctx_t *)jy, &jz_copy, P, np, &t0,
                   &t1, &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], NULL);

    aca_len_type_free(len_type_pool, len_type_N_id);
    len_type_N_id = -1;

    ACA_OP_UNLOCK(aca_drv);
    is_aca_lock = false;

    /* X/Y size == tmp size */
    ret = aca_op_copy(aca_drv, X, &tmp[3]);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, Y, &tmp[4]);
    CHECK_RET_GO;
    ret = aca_op_set_u32(aca_drv, Z, 1);
    CHECK_RET_GO;

    ret = TE_SUCCESS;
finish:
#ifdef CFG_TE_DYNCLK_CTL
    if (is_clk_en) {
        ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
        OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    }
#endif
    if (TE_SUCCESS != ret) {
        op_ctx_put_all((aca_op_ctx_t *)jx, (aca_op_ctx_t *)jy, &jz_copy, P, np,
                       &t0, &t1, &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4],
                       NULL);
        if (len_type_N_id >= 0) {
            aca_len_type_free(len_type_pool, len_type_N_id);
        }
        if (is_aca_lock) {
            ACA_OP_UNLOCK(aca_drv);
        }
    }
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    op_ctx_clean(&jz_copy);
    for (i = 0; i < 5; i++) {
        op_ctx_clean(&tmp[i]);
    }
    return ret;
}

/* the upper call MUST makes sure these op context are get and locked in ACA
 * drv
 */
static int _double_point_jj(const te_aca_drv_t *aca_drv,
                            int32_t full_id,
                            int32_t modmul_id,
                            aca_op_ctx_t *P,
                            aca_op_ctx_t *HP1,
                            aca_op_ctx_t *P2,
                            aca_op_ctx_t *P3,
                            aca_op_ctx_t *P6,
                            aca_op_ctx_t *P9,
                            aca_op_ctx_t *A,
                            aca_op_ctx_t *Tx,
                            aca_op_ctx_t *Ty,
                            aca_op_ctx_t *Tz,
                            aca_op_ctx_t *W,
                            bool i_mj_en,
                            bool o_mj_en,
                            aca_op_ctx_t u[])
{
    int ret = TE_SUCCESS;
#ifdef CFG_TE_DYNCLK_CTL
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
#endif

    (void)(P);
    (void)(HP1);
    (void)(P2);
    (void)(P9);

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    OP_ECP_PREPARE();
    OP_ECP_EXEC(ADD, full_id, Tx, 0, Tx, 0, &u[0]);
    OP_ECP_EXEC(ADD, full_id, Tx, 0, &u[0], 0, &u[4]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    ACA_DBG_LOG("-------------double point jj");
    op_ctx_dump("u1:", &u[0]);
    op_ctx_dump("u2:", &u[4]);
#endif
    OP_ECP_EXEC(ADD, full_id, Ty, 0, Ty, 0, &u[1]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, &u[1], 0, &u[2]);
    OP_ECP_EXEC(ADD, full_id, &u[2], 1, 0, 0, Tz);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Ty, 0, &u[1], 0, &u[2]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[0], 0, &u[2], 0, &u[5]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u3:", &u[1]);
    op_ctx_dump("u4:", &u[2]);
#endif
    OP_ECP_EXEC(ADD, full_id, &u[2], 0, &u[2], 0, &u[0]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[2], 0, &u[0], 0, &u[6]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u5:", &u[0]);
#endif
    if (!i_mj_en) {
        OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, Tz, 0, W);
        OP_ECP_EXEC(MODMUL7NR, modmul_id, W, 0, W, 0, &u[0]);
        OP_ECP_EXEC(MODMUL7NR, modmul_id, A, 0, &u[0], 0, W);
    }
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u7:", &u[5]);
    op_ctx_dump("u8:", &u[6]);
#endif

    OP_ECP_EXEC(MODMULACC7NR, modmul_id, Tx, 0, &u[4], W, &u[0]);
    OP_ECP_EXEC(ADD, full_id, &u[5], 0, &u[5], 0, &u[2]);
    OP_ECP_EXEC(SUB, full_id, P6, 0, &u[2], 0, &u[1]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, &u[0], 0, &u[0], &u[1], Tx);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u9:", &u[0]);
    op_ctx_dump("u11:", &u[2]);
#endif
    OP_ECP_EXEC(SUB, full_id, P3, 0, Tx, 0, &u[1]);
    OP_ECP_EXEC(ADD, full_id, &u[5], 0, &u[1], 0, &u[1]);
    OP_ECP_EXEC(SUB, full_id, P3, 0, &u[6], 0, &u[2]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, &u[0], 0, &u[1], &u[2], Ty);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u12:", &u[1]);
#endif
    if (o_mj_en) {
        OP_ECP_EXEC(ADD, full_id, &u[6], 0, &u[6], 0, &u[0]);
        OP_ECP_EXEC(MODMUL7NR, modmul_id, W, 0, &u[0], 0, &u[1]);
        OP_ECP_EXEC(ADD, full_id, &u[1], 1, 0, 0, W);
#ifdef ECP_DBG_PRINT_DETAIL_EN
        op_ctx_dump("u14:", &u[0]);
        op_ctx_dump("u15:", &u[1]);
#endif
    }
    OP_ECP_WAIT();

finish:
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    return ret;
}

static int _add_point_ajj(const te_aca_drv_t *aca_drv,
                          int32_t full_id,
                          int32_t modmul_id,
                          aca_op_ctx_t *P,
                          aca_op_ctx_t *HP1,
                          aca_op_ctx_t *P2,
                          aca_op_ctx_t *P3,
                          aca_op_ctx_t *P6,
                          aca_op_ctx_t *P9,
                          aca_op_ctx_t *A,
                          aca_op_ctx_t *X,
                          aca_op_ctx_t *Y,
                          aca_op_ctx_t *Tx,
                          aca_op_ctx_t *Ty,
                          aca_op_ctx_t *Tz,
                          aca_op_ctx_t *W,
                          bool o_mj_en,
                          bool is_point_sub,
                          aca_op_ctx_t u[])
{
    int ret = TE_SUCCESS;
#ifdef CFG_TE_DYNCLK_CTL
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
#endif

    (void)(P);
    (void)(HP1);
    (void)(P2);
    (void)(P6);
    (void)(is_point_sub);

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    OP_ECP_PREPARE();
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, Tz, 0, &u[0]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, &u[0], 0, &u[1]);
    OP_ECP_EXEC(SUB, full_id, P3, 0, Ty, 0, &u[2]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, Y, 0, &u[1], &u[2], &u[4]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    if (is_point_sub) {
        ACA_DBG_LOG("-------------point sub ajj");
    } else {
        ACA_DBG_LOG("-------------point add ajj");
    }
    op_ctx_dump("u1:", &u[0]);
    op_ctx_dump("u2:", &u[1]);
#endif

    OP_ECP_EXEC(SUB, full_id, P3, 0, Tx, 0, &u[1]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, X, 0, &u[0], &u[1], &u[2]);

    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, &u[2], 0, &u[1]);
    OP_ECP_EXEC(ADD, full_id, &u[1], 1, 0, 0, Tz);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u5:", &u[2]);
#endif
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[2], 0, &u[2], 0, &u[1]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tx, 0, &u[1], 0, &u[6]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[2], 0, &u[1], 0, &u[7]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u6:", &u[1]);
    op_ctx_dump("u7:", &u[4]);
    op_ctx_dump("u9:", &u[7]);
    op_ctx_dump("u10:", &u[6]);
#endif

    OP_ECP_EXEC(ADD, full_id, &u[6], 0, &u[6], 0, &u[0]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u11:", &u[0]);
#endif
    OP_ECP_EXEC(ADD, full_id, &u[7], 0, &u[0], 0, &u[0]);
    OP_ECP_EXEC(SUB, full_id, P9, 0, &u[0], 0, &u[5]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, &u[4], 0, &u[4], &u[5], Tx);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u12:", &u[0]);
#endif

    OP_ECP_EXEC(SUB, full_id, P3, 0, Tx, 0, &u[0]);
    OP_ECP_EXEC(ADD, full_id, &u[6], 0, &u[0], 0, &u[0]);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u13:", &u[0]);
#endif

    OP_ECP_EXEC(MODMUL7NR, modmul_id, Ty, 0, &u[7], 0, &u[1]);
    OP_ECP_EXEC(SUB, full_id, P3, 0, &u[1], 0, &u[1]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, &u[4], 0, &u[0], &u[1], Ty);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u15:", &u[0]);
#endif

    if (o_mj_en) {
        OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, Tz, 0, W);
#ifdef ECP_DBG_PRINT_DETAIL_EN
        op_ctx_dump("u16:", W);
#endif
        OP_ECP_EXEC(MODMUL7NR, modmul_id, W, 0, W, 0, &u[0]);
        OP_ECP_EXEC(MODMUL7NR, modmul_id, A, 0, &u[0], 0, W);
#ifdef ECP_DBG_PRINT_DETAIL_EN
        op_ctx_dump("u17:", &u[0]);
#endif
    }
    OP_ECP_WAIT();

#ifdef ECP_DBG_PRINT_SIMPLE_EN
    if (is_point_sub) {
        ACA_DBG_LOG("Point sub ajj Result----\n");
    } else {
        ACA_DBG_LOG("Point add ajj Result----\n");
    }

    op_ctx_dump("Tx,", Tx);
    op_ctx_dump("Ty,", Ty);
    op_ctx_dump("Tz,", Tz);
    if (o_mj_en) {
        op_ctx_dump("W", W);
    }
#endif

finish:
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    return ret;
}

static int _add_point_jjj(const te_aca_drv_t *aca_drv,
                          int32_t full_id,
                          int32_t modmul_id,
                          aca_op_ctx_t *P,
                          aca_op_ctx_t *HP1,
                          aca_op_ctx_t *P2,
                          aca_op_ctx_t *P3,
                          aca_op_ctx_t *P6,
                          aca_op_ctx_t *P9,
                          aca_op_ctx_t *A,
                          aca_op_ctx_t *X,
                          aca_op_ctx_t *Y,
                          aca_op_ctx_t *Z,
                          aca_op_ctx_t *Tx,
                          aca_op_ctx_t *Ty,
                          aca_op_ctx_t *Tz,
                          aca_op_ctx_t *W,
                          bool o_mj_en,
                          bool is_point_sub,
                          aca_op_ctx_t u[])
{
    int ret = TE_SUCCESS;
#ifdef CFG_TE_DYNCLK_CTL
    te_hwa_aca_t *aca_hwa = ACA_DRV_GET_HWA(aca_drv);
#endif

    (void)(P);
    (void)(P2);
    (void)(P9);
    (void)(is_point_sub);

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif

    OP_ECP_PREPARE();
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Z, 0, Z, 0, &u[0]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tx, 0, &u[0], 0, &u[1]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Z, 0, &u[0], 0, &u[2]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Ty, 0, &u[2], 0, &u[3]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    if (is_point_sub) {
        ACA_DBG_LOG("-------------point sub jjj\n");
    } else {
        ACA_DBG_LOG("-------------point add jjj\n");
    }
    op_ctx_dump("u2:", &u[0]);
    op_ctx_dump("u6:", &u[1]);
    op_ctx_dump("u4:", &u[2]);
    op_ctx_dump("u8:", &u[3]);
#endif

    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, Tz, 0, &u[0]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, X, 0, &u[0], 0, &u[2]);
    OP_ECP_EXEC(SUB, full_id, P3, 0, &u[1], 0, &u[4]);
    OP_ECP_EXEC(ADD, full_id, &u[2], 0, &u[4], 0, &u[4]);
    OP_ECP_EXEC(ADD, full_id, &u[1], 0, &u[2], 0, &u[5]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u1:", &u[0]);
    op_ctx_dump("u5:", &u[2]);
#endif

    OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, &u[0], 0, &u[1]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, Y, 0, &u[1], 0, &u[0]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u3:", &u[1]);
    op_ctx_dump("u7:", &u[0]);
#endif

    OP_ECP_EXEC(ADD, full_id, &u[0], 0, &u[3], 0, &u[6]);
    OP_ECP_EXEC(SUB, full_id, P3, 0, &u[3], 0, &u[7]);
    OP_ECP_EXEC(ADD, full_id, &u[0], 0, &u[7], 0, &u[7]);

    OP_ECP_EXEC(MODMUL7NR, modmul_id, Z, 0, Tz, 0, &u[0]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[0], 0, &u[4], 0, Tz);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u9:", &u[0]);
    op_ctx_dump("u10:", &u[4]);
    op_ctx_dump("u11:", &u[7]);
    op_ctx_dump("u12:", &u[5]);
    op_ctx_dump("u13:", &u[6]);
#endif

    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[4], 0, &u[4], 0, &u[1]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[5], 0, &u[1], 0, &u[2]);

    OP_ECP_EXEC(SUB, full_id, P3, 0, &u[2], 0, &u[0]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, &u[7], 0, &u[7], &u[0], Tx);

#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u15:", &u[1]);
    op_ctx_dump("u16:", &u[2]);
#endif

    OP_ECP_EXEC(ADD, full_id, Tx, 0, Tx, 0, &u[0]);
    OP_ECP_EXEC(SUB, full_id, P6, 0, &u[0], 0, &u[3]);
    OP_ECP_EXEC(ADD, full_id, &u[2], 0, &u[3], 0, &u[3]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u17:", &u[0]);
    op_ctx_dump("u18:", &u[3]);
#endif

    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[4], 0, &u[1], 0, &u[2]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, &u[6], 0, &u[2], 0, &u[1]);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u20:", &u[2]);
    op_ctx_dump("u21:", &u[1]);
#endif

    OP_ECP_EXEC(SUB, full_id, P3, 0, &u[1], 0, &u[1]);
    OP_ECP_EXEC(MODMULACC7NR, modmul_id, &u[3], 0, &u[7], &u[1], &u[2]);
    OP_ECP_EXEC(MODMUL7NR, modmul_id, HP1, 0, &u[2], 0, Ty);
#ifdef ECP_DBG_PRINT_DETAIL_EN
    op_ctx_dump("u22:", &u[2]);
    op_ctx_dump("(p+1)/2:", HP1);
#endif

    if (o_mj_en) {
        OP_ECP_EXEC(MODMUL7NR, modmul_id, Tz, 0, Tz, 0, W);
        OP_ECP_EXEC(MODMUL7NR, modmul_id, W, 0, W, 0, &u[0]);
        OP_ECP_EXEC(MODMUL7NR, modmul_id, A, 0, &u[0], 0, W);
    }
    OP_ECP_WAIT();

#ifdef ECP_DBG_PRINT_SIMPLE_EN
    if (is_point_sub) {
        ACA_DBG_LOG("Point sub jjj Result----\n");
    } else {
        ACA_DBG_LOG("Point add jjj Result----\n");
    }
    op_ctx_dump("Tx,", Tx);
    op_ctx_dump("Ty,", Ty);
    op_ctx_dump("Tz,", Tz);
#endif

finish:
#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
#endif
    return ret;
}

/**
 * NOTE: Current use_modified_jacobian MUST be enabled.
 */
static int kP_bin_ext(const te_aca_drv_t *aca_drv,
                      aca_op_ctx_t *P,
                      aca_op_ctx_t *A,
                      aca_op_ctx_t *G_X,
                      aca_op_ctx_t *G_Y,
                      aca_op_ctx_t *G_Z,
                      aca_op_ctx_t *k,
                      bool use_modified_jacobian,
                      bool use_mix_jacobian_in_point_add,
                      aca_op_ctx_t *R_X,
                      aca_op_ctx_t *R_Y,
                      aca_op_ctx_t *R_Z)
{
    int ret                            = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa              = ACA_DRV_GET_HWA(aca_drv);
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    aca_op_ctx_t neg_Y                 = {0};
    aca_op_ctx_t *used_y               = NULL;
    aca_op_ctx_t k3                    = {0};
    aca_op_ctx_t Tx                    = {0};
    aca_op_ctx_t Ty                    = {0};
    aca_op_ctx_t Tz                    = {0};
    aca_op_ctx_t W                     = {0};
    aca_op_ctx_t HP1                   = {0};
    aca_op_ctx_t P2                    = {0};
    aca_op_ctx_t P3                    = {0};
    aca_op_ctx_t P6                    = {0};
    aca_op_ctx_t P9                    = {0};
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    aca_op_ctx_t u[8]                  = {0};
    aca_op_ctx_t *np                   = NULL;
    size_t mul_bit_len                 = 0;
    size_t full_bit_len                = 0;
    size_t k_bit_len = 0, k3_bit_len = 0;
    int k_bit_val = 0, k3_bit_val = 0;
    int32_t full_id         = -1;
    int32_t modmul_id       = -1;
    size_t i                = 0;
    size_t aca_granule_bits = 0;
    bool is_aca_lock        = false;
#ifdef CFG_TE_DYNCLK_CTL
    bool is_clk_en = false;
#endif

    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);

    ret = aca_sram_get_bit_len(P->sram_block, &mul_bit_len);
    CHECK_RET_GO;
    ret = aca_sram_get_bit_len(k->sram_block, &k_bit_len);
    CHECK_RET_GO;
    CHECK_PARAM(k_bit_len <= mul_bit_len);

    /* extra 7 bits */
    full_bit_len = UTILS_ROUND_UP(mul_bit_len + 7, aca_granule_bits);

    /* enlarge P, A, G_X, G_Y, G_Z size */
    ret = aca_sram_try_change_size(P->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(A->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(G_X->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(G_Y->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(G_Z->sram_block, full_bit_len / 8);
    CHECK_RET_GO;

    /* update np */
    ret = op_ctx_update_np(aca_drv, (aca_op_ctx_t *)P);
    CHECK_RET_GO;
    TE_ASSERT(P->extra_np);
    np = &(((aca_drv_extra_np_t *)(P->extra_np))->op_P_ctx);

    /* init k3 */
    ret = op_ctx_init(aca_drv, &k3,
                      UTILS_ROUND_UP(k_bit_len + 2, aca_granule_bits) / 8);
    CHECK_RET_GO;
    ret = aca_op_run(aca_drv,
                     &k3,
                     (aca_op_ctx_t *)k,
                     NULL,
                     3,
                     NULL,
                     NULL,
                     TE_ACA_OP_MUL_LOW,
                     NULL);
    CHECK_RET_GO;
    /* swap out k3 and k, because latter we won't use them for op */
    ret = aca_sram_swap_out(k3.sram_block);
    CHECK_RET_GO;
    ret = aca_sram_swap_out(k->sram_block);
    CHECK_RET_GO;
    ret = aca_sram_get_bit_len(k3.sram_block, &k3_bit_len);
    CHECK_RET_GO;
    TE_ASSERT((k3_bit_len >= k_bit_len + 1) && (k3_bit_len <= k_bit_len + 2));
    TE_ASSERT(k3_bit_len >= 2); /* FIXME:ecp_mul(m=1,G) makes k3_bit_len=2 */

    /* init tmp register's space */
    ret = op_ctx_init(aca_drv, &neg_Y, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &Tx, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &Ty, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &Tz, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &W, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t0, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t1, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &HP1, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P2, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P3, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P6, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P9, full_bit_len / 8);
    CHECK_RET_GO;

    /* 8 tmp op ctx */
    for (i = 0; i < 8; i++) {
        ret = op_ctx_init(aca_drv, &u[i], full_bit_len / 8);
        CHECK_RET_GO;
    }

    /* OK, lock and do op */
    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    full_id = aca_len_type_alloc(len_type_pool, full_bit_len);
    CHECK_COND_GO(full_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    modmul_id = aca_len_type_alloc(len_type_pool, mul_bit_len);
    CHECK_COND_GO(modmul_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* Init Tx, Ty, Tz, W, neg_Y, HP1, P2, P3, P6, P9. Set P usage to N,
     * because calculing W may use MODMUL */
    ret = op_ctx_get_all(
        (aca_op_ctx_t *)G_X, GR_USAGE_IN, &Tx, GR_USAGE_OUT,
        (aca_op_ctx_t *)G_Y, GR_USAGE_IN, &Ty, GR_USAGE_OUT,
        (aca_op_ctx_t *)G_Z, GR_USAGE_IN, &Tz, GR_USAGE_OUT, (aca_op_ctx_t *)A,
        GR_USAGE_IN, &W, GR_USAGE_OUT, (aca_op_ctx_t *)P, GR_USAGE_N, &neg_Y,
        GR_USAGE_OUT, &HP1, GR_USAGE_INOUT, &P2, GR_USAGE_INOUT, &P3,
        GR_USAGE_INOUT, &P6, GR_USAGE_INOUT, &P9, GR_USAGE_INOUT, np,
        GR_USAGE_P, &t0, GR_USAGE_T0, &t1, GR_USAGE_T1, NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)G_X, 0, NULL, &Tx,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)G_Y, 0, NULL, &Ty,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)G_Z, 0, NULL, &Tz,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;

    /* W = A * Z^4 */
    if (use_mix_jacobian_in_point_add) {
        /* Z == 1, A * Z^4 = A */
        ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)A, 0, NULL, &W,
                                     full_id, TE_ACA_OP_ADD);
        CHECK_RET_GO;
    } else {
        /* P2 = (Z * Z) MOD P */
        ret = OP_EXEC_ONE_CMD(aca_drv, &Tz, &Tz, NULL, &P2, modmul_id,
                              TE_ACA_OP_MODMUL);
        CHECK_RET_GO;
        /* P6 = (P2 * P2) MOD P */
        ret = OP_EXEC_ONE_CMD(aca_drv, &P2, &P2, NULL, &P6, modmul_id,
                              TE_ACA_OP_MODMUL);
        CHECK_RET_GO;
        /* W = (P6 * A) MOD P */
        ret = OP_EXEC_ONE_CMD(aca_drv, (aca_op_ctx_t *)A, &P6, NULL, &W,
                              modmul_id, TE_ACA_OP_MODMUL);
        CHECK_RET_GO;
    }

    /* neg_Y = P - G_Y */
    ret = OP_EXEC_ONE_CMD(aca_drv, (aca_op_ctx_t *)P, (aca_op_ctx_t *)G_Y, NULL,
                          &neg_Y, full_id, TE_ACA_OP_SUB);
    CHECK_RET_GO;

    /* HP1 = (P + 1) / 2 */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, P, 1, NULL, &HP1, full_id,
                                 TE_ACA_OP_ADD);
    CHECK_RET_GO;
    ret =
        OP_EXEC_ONE_CMD_SHIFT(aca_drv, &HP1, 0, &HP1, full_id, TE_ACA_OP_SHR0);
    CHECK_RET_GO;

    /* P2 = P + P */
    ret = OP_EXEC_ONE_CMD(aca_drv, (aca_op_ctx_t *)P, (aca_op_ctx_t *)P, NULL,
                          &P2, full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    /* P3 = P2 + P */
    ret = OP_EXEC_ONE_CMD(aca_drv, &P2, (aca_op_ctx_t *)P, NULL, &P3, full_id,
                          TE_ACA_OP_ADD);
    CHECK_RET_GO;
    /* P6 = P3 + P3 */
    ret = OP_EXEC_ONE_CMD(aca_drv, &P3, &P3, NULL, &P6, full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    /* P9 = P6 + P3 */
    ret = OP_EXEC_ONE_CMD(aca_drv, &P6, &P3, NULL, &P9, full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;

    /* put op contexs, so that we can change usage to inout */
    op_ctx_put_all((aca_op_ctx_t *)G_X, &Tx, (aca_op_ctx_t *)G_Y, &Ty,
                   (aca_op_ctx_t *)G_Z, &Tz, (aca_op_ctx_t *)A, &W,
                   (aca_op_ctx_t *)P, &neg_Y, &HP1, &P2, &P3, &P6, &P9, np, &t0,
                   &t1, NULL);

    /* lock all GRs to INOUT */
    ret = op_ctx_get_all(
        (aca_op_ctx_t *)G_X, GR_USAGE_INOUT, &Tx, GR_USAGE_INOUT,
        (aca_op_ctx_t *)G_Y, GR_USAGE_INOUT, &Ty, GR_USAGE_INOUT,
        (aca_op_ctx_t *)G_Z, GR_USAGE_INOUT, &Tz, GR_USAGE_INOUT,
        (aca_op_ctx_t *)A, GR_USAGE_INOUT, &W, GR_USAGE_INOUT,
        (aca_op_ctx_t *)P, GR_USAGE_N, &neg_Y, GR_USAGE_INOUT, &HP1,
        GR_USAGE_INOUT, &P2, GR_USAGE_INOUT, &P3, GR_USAGE_INOUT, &P6,
        GR_USAGE_INOUT, &P9, GR_USAGE_INOUT, &t0, GR_USAGE_T0, &t1, GR_USAGE_T1,
        np, GR_USAGE_P, &u[0], GR_USAGE_INOUT, &u[1], GR_USAGE_INOUT, &u[2],
        GR_USAGE_INOUT, &u[3], GR_USAGE_INOUT, &u[4], GR_USAGE_INOUT, &u[5],
        GR_USAGE_INOUT, &u[6], GR_USAGE_INOUT, &u[7], GR_USAGE_INOUT, NULL);
    CHECK_RET_GO;

#ifdef ECP_DBG_PRINT_SIMPLE_EN
    op_ctx_dump("NP", np);
    op_ctx_dump("(P+1)/2", &HP1);
    op_ctx_dump("2P", &P2);
    op_ctx_dump("3P", &P3);
    op_ctx_dump("6P", &P6);
    op_ctx_dump("9P", &P9);
#endif

    /* parse NAF */
    for (i = k3_bit_len - 2; i >= 1; i--) {
        ret = aca_sram_get_bit(k3.sram_block, i, &k3_bit_val);
        CHECK_RET_GO;
        if (i >= k_bit_len) {
            k_bit_val = 0;
        } else {
            ret = aca_sram_get_bit(k->sram_block, i, &k_bit_val);
            CHECK_RET_GO;
        }

        if (!use_modified_jacobian) {
            ret = _double_point_jj(aca_drv, full_id, modmul_id, P, &HP1, &P2,
                                   &P3, &P6, &P9, A, &Tx, &Ty, &Tz, &W, false,
                                   false, u);
            CHECK_RET_GO;
        } else {
            if (k3_bit_val == k_bit_val) {
                /*0 */
                ret = _double_point_jj(aca_drv, full_id, modmul_id, P, &HP1,
                                       &P2, &P3, &P6, &P9, A, &Tx, &Ty, &Tz, &W,
                                       true, true, u);
                CHECK_RET_GO;
            } else {
                /* +- */
                ret = _double_point_jj(aca_drv, full_id, modmul_id, P, &HP1,
                                       &P2, &P3, &P6, &P9, A, &Tx, &Ty, &Tz, &W,
                                       true, false, u);
                CHECK_RET_GO;
            }
        }

        if ((k3_bit_val == 1) && (k_bit_val == 0)) {
            /* + */
            used_y = (aca_op_ctx_t *)G_Y;
        } else if ((k3_bit_val == 0) && (k_bit_val == 1)) {
            used_y = &neg_Y;
        } else {
            used_y = NULL;
        }
        if (used_y) {
            /* do point add */
            if (!use_mix_jacobian_in_point_add) {
                ret = _add_point_jjj(
                    aca_drv, full_id, modmul_id, (aca_op_ctx_t *)P, &HP1, &P2,
                    &P3, &P6, &P9, A, (aca_op_ctx_t *)G_X, used_y, G_Z, &Tx,
                    &Ty, &Tz, &W, use_modified_jacobian,
                    ((used_y == &neg_Y) ? (true) : (false)), u);
                CHECK_RET_GO;
            } else {
                ret = _add_point_ajj(
                    aca_drv, full_id, modmul_id, (aca_op_ctx_t *)P, &HP1, &P2,
                    &P3, &P6, &P9, A, (aca_op_ctx_t *)G_X, used_y, &Tx, &Ty,
                    &Tz, &W, use_modified_jacobian,
                    ((used_y == &neg_Y) ? (true) : (false)), u);
                CHECK_RET_GO;
            }
        } else {
#ifdef CFG_ACA_BLINDING_EN
            /* do fake point add, only when use_mix_jacobian_in_point_add is
             * disabled */
            if (!use_mix_jacobian_in_point_add) {
                ret = _add_point_jjj(
                    aca_drv, full_id, modmul_id, (aca_op_ctx_t *)P, &HP1, &P2,
                    &P3, &P6, &P9, A, (aca_op_ctx_t *)G_X, (aca_op_ctx_t *)G_Y,
                    G_Z, &u[0], &u[0], &u[0], &u[0], use_modified_jacobian,
                    true, u);
                CHECK_RET_GO;
            }
#endif
        }
    }

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    is_clk_en = true;
#endif

    OP_ECP_PREPARE();
    OP_ECP_EXEC(MODRED, full_id, &Tx, 0, P, 0, &Tx);
    OP_ECP_EXEC(MODRED, full_id, &Ty, 0, P, 0, &Ty);
    OP_ECP_EXEC(MODRED, full_id, &Tz, 0, P, 0, &Tz);
    OP_ECP_WAIT();

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    is_clk_en = false;
#endif

    /* put all necessary GRs */
    op_ctx_put_all((aca_op_ctx_t *)G_X, &Tx, (aca_op_ctx_t *)G_Y, &Ty,
                   (aca_op_ctx_t *)G_Z, &Tz, (aca_op_ctx_t *)A, &W,
                   (aca_op_ctx_t *)P, &neg_Y, &HP1, &P2, &P3, &P6, &P9, &t0,
                   &t1, np, &u[0], &u[1], &u[2], &u[3], &u[4], &u[5], &u[6],
                   &u[7], NULL);

    aca_len_type_free(len_type_pool, full_id);
    full_id = -1;
    aca_len_type_free(len_type_pool, modmul_id);
    modmul_id = -1;

    /* unlock */
    ACA_OP_UNLOCK(aca_drv);
    is_aca_lock = false;

    /* free others */
    op_ctx_clean(&neg_Y);
    op_ctx_clean(&k3);
    op_ctx_clean(&W);
    op_ctx_clean(&HP1);
    op_ctx_clean(&P2);
    op_ctx_clean(&P3);
    op_ctx_clean(&P6);
    op_ctx_clean(&P9);
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    /* free all u[i] */
    for (i = 0; i < 8; i++) {
        op_ctx_clean(&u[i]);
    }

    /* Copy to result */
    ret = aca_op_copy(aca_drv, R_X, &Tx);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, R_Y, &Ty);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, R_Z, &Tz);
    CHECK_RET_GO;

finish:
#ifdef CFG_TE_DYNCLK_CTL
    if (is_clk_en) {
        ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
        OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    }
#endif
    if (TE_SUCCESS != ret) {
        if (full_id >= 0) {
            aca_len_type_free(len_type_pool, full_id);
            full_id = -1;
        }
        if (modmul_id >= 0) {
            aca_len_type_free(len_type_pool, modmul_id);
            modmul_id = -1;
        }
        /* put all necessary GRs */
        op_ctx_put_all((aca_op_ctx_t *)G_X, &Tx, (aca_op_ctx_t *)G_Y, &Ty,
                       (aca_op_ctx_t *)G_Z, &Tz, (aca_op_ctx_t *)A, &W,
                       (aca_op_ctx_t *)P, &neg_Y, &HP1, &P2, &P3, &P6, &P9, &t0,
                       &t1, np, &u[0], &u[1], &u[2], &u[3], &u[4], &u[5], &u[6],
                       &u[7], NULL);
        if (is_aca_lock) {
            ACA_OP_UNLOCK(aca_drv);
        }
        op_ctx_clean(&neg_Y);
        op_ctx_clean(&k3);
        op_ctx_clean(&W);
        op_ctx_clean(&HP1);
        op_ctx_clean(&P2);
        op_ctx_clean(&P3);
        op_ctx_clean(&P6);
        op_ctx_clean(&P9);
        op_ctx_clean(&t0);
        op_ctx_clean(&t1);
        for (i = 0; i < 8; i++) {
            op_ctx_clean(&u[i]);
        }
    }
    op_ctx_clean(&Tx);
    op_ctx_clean(&Ty);
    op_ctx_clean(&Tz);
    return ret;
}
/* Calculate R = k * G */
int aca_op_ecp_mul(const te_aca_drv_t *aca_drv,
                   const aca_op_ctx_t *P,
                   const aca_op_ctx_t *A,
                   const aca_op_ctx_t *G_X,
                   const aca_op_ctx_t *G_Y,
                   const aca_op_ctx_t *G_Z,
                   const aca_op_ctx_t *k,
                   aca_op_ctx_t *R_X,
                   aca_op_ctx_t *R_Y,
                   aca_op_ctx_t *R_Z)
{
    int ret              = TE_SUCCESS;
    bool is_equal        = false;
    bool enable_mixd_add = false;

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((P) && (P->sram_block) && (P->gr_info));
    CHECK_PARAM((A) && (A->sram_block) && (A->gr_info));
    CHECK_PARAM((G_X) && (G_X->sram_block) && (G_X->gr_info));
    CHECK_PARAM((G_Y) && (G_Y->sram_block) && (G_Y->gr_info));
    CHECK_PARAM((G_Z) && (G_Z->sram_block) && (G_Z->gr_info));
    CHECK_PARAM((k) && (k->sram_block) && (k->gr_info));
    CHECK_PARAM(R_X);
    CHECK_PARAM(R_Y);
    CHECK_PARAM(R_Z);

    _g_op_debug_force_disable = 1;

    /* Check G_Z == 1 */
    ret = aca_op_cmp_immeb(aca_drv, (aca_op_ctx_t *)G_Z, 1, &is_equal);
    CHECK_RET_GO;
    if (is_equal) {
        enable_mixd_add = true;
        OSAL_LOG_DEBUG("Mixed Jacobian is enabled!\n");
    } else {
        enable_mixd_add = false;
        OSAL_LOG_DEBUG("Mixed Jacobian is disabled!\n");
    }

    /* always enable modified jacobian */
    ret = kP_bin_ext(aca_drv, (aca_op_ctx_t *)P, (aca_op_ctx_t *)A,
                     (aca_op_ctx_t *)G_X, (aca_op_ctx_t *)G_Y,
                     (aca_op_ctx_t *)G_Z, (aca_op_ctx_t *)k, true,
                     enable_mixd_add, R_X, R_Y, R_Z);
    CHECK_RET_GO;

finish:
    _g_op_debug_force_disable = 0;
    return ret;
}

/* Calculate R = G1 + G2 */
int aca_op_ecp_add(const te_aca_drv_t *aca_drv,
                   const aca_op_ctx_t *P,
                   const aca_op_ctx_t *G1_X,
                   const aca_op_ctx_t *G1_Y,
                   const aca_op_ctx_t *G1_Z,
                   const aca_op_ctx_t *G2_X,
                   const aca_op_ctx_t *G2_Y,
                   const aca_op_ctx_t *G2_Z,
                   aca_op_ctx_t *R_X,
                   aca_op_ctx_t *R_Y,
                   aca_op_ctx_t *R_Z)
{
    int ret                            = TE_SUCCESS;
    te_hwa_aca_t *aca_hwa              = ACA_DRV_GET_HWA(aca_drv);
    aca_len_type_pool_t *len_type_pool = ACA_DRV_GET_LEN_TYPE_POOL(aca_drv);
    aca_op_ctx_t Tx                    = {0};
    aca_op_ctx_t Ty                    = {0};
    aca_op_ctx_t Tz                    = {0};
    aca_op_ctx_t HP1                   = {0};
    aca_op_ctx_t P2                    = {0};
    aca_op_ctx_t P3                    = {0};
    aca_op_ctx_t P6                    = {0};
    aca_op_ctx_t P9                    = {0};
    aca_op_ctx_t t0                    = {0};
    aca_op_ctx_t t1                    = {0};
    aca_op_ctx_t u[8]                  = {0};
    aca_op_ctx_t *np                   = NULL;
    size_t mul_bit_len                 = 0;
    size_t full_bit_len                = 0;
    int32_t full_id                    = -1;
    int32_t modmul_id                  = -1;
    size_t i                           = 0;
    size_t aca_granule_bits            = 0;
    bool is_aca_lock                   = false;
#ifdef CFG_TE_DYNCLK_CTL
    bool is_clk_en = false;
#endif

    CHECK_PARAM(aca_drv);
    CHECK_PARAM((P) && (P->sram_block) && (P->gr_info));
    CHECK_PARAM((G1_X) && (G1_X->sram_block) && (G1_X->gr_info));
    CHECK_PARAM((G1_Y) && (G1_Y->sram_block) && (G1_Y->gr_info));
    CHECK_PARAM((G1_Z) && (G1_Z->sram_block) && (G1_Z->gr_info));
    CHECK_PARAM((G2_X) && (G2_X->sram_block) && (G2_X->gr_info));
    CHECK_PARAM((G2_Y) && (G2_Y->sram_block) && (G2_Y->gr_info));
    CHECK_PARAM((G2_Z) && (G2_Z->sram_block) && (G2_Z->gr_info));
    CHECK_PARAM(R_X);
    CHECK_PARAM(R_Y);
    CHECK_PARAM(R_Z);

    _g_op_debug_force_disable = 1;

    aca_granule_bits = aca_hwa->get_core_granularity(aca_hwa);
    TE_ASSERT(aca_granule_bits > 0);

    ret = aca_sram_get_bit_len(P->sram_block, &mul_bit_len);
    CHECK_RET_GO;

    /* extra 7 bits */
    full_bit_len = UTILS_ROUND_UP(mul_bit_len + 7, aca_granule_bits);
    /* enlarge P, A, G2_X, G2_Y, G2_Z size */
    ret = aca_sram_try_change_size(P->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(G2_X->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(G2_Y->sram_block, full_bit_len / 8);
    CHECK_RET_GO;
    ret = aca_sram_try_change_size(G2_Z->sram_block, full_bit_len / 8);
    CHECK_RET_GO;

    /* update np */
    ret = op_ctx_update_np(aca_drv, (aca_op_ctx_t *)P);
    CHECK_RET_GO;
    TE_ASSERT(P->extra_np);
    np = &(((aca_drv_extra_np_t *)(P->extra_np))->op_P_ctx);

    /* init tmp register's space */
    ret = op_ctx_init(aca_drv, &Tx, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &Ty, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &Tz, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t0, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &t1, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &HP1, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P2, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P3, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P6, full_bit_len / 8);
    CHECK_RET_GO;
    ret = op_ctx_init(aca_drv, &P9, full_bit_len / 8);
    CHECK_RET_GO;

    /* 8 tmp op ctx */
    for (i = 0; i < 8; i++) {
        ret = op_ctx_init(aca_drv, &u[i], full_bit_len / 8);
        CHECK_RET_GO;
    }

    /* OK, lock and do op */
    ACA_OP_LOCK(aca_drv);
    is_aca_lock = true;

    full_id = aca_len_type_alloc(len_type_pool, full_bit_len);
    CHECK_COND_GO(full_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    modmul_id = aca_len_type_alloc(len_type_pool, mul_bit_len);
    CHECK_COND_GO(modmul_id >= 0, TE_ERROR_NO_AVAIL_LEN_TYPE);

    /* copy G1_X, G1_Y, G1_Z to Tx, Ty, Tz */
    ret = op_ctx_get_all((aca_op_ctx_t *)G1_X, GR_USAGE_IN, &Tx, GR_USAGE_OUT,
                         (aca_op_ctx_t *)G1_Y, GR_USAGE_IN, &Ty, GR_USAGE_OUT,
                         (aca_op_ctx_t *)G1_Z, GR_USAGE_IN, &Tz, GR_USAGE_OUT,
                         (aca_op_ctx_t *)P, GR_USAGE_IN, &HP1, GR_USAGE_INOUT,
                         &P2, GR_USAGE_INOUT, &P3, GR_USAGE_INOUT, &P6,
                         GR_USAGE_INOUT, &P9, GR_USAGE_INOUT, NULL);
    CHECK_RET_GO;

    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)G1_X, 0, NULL, &Tx,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)G1_Y, 0, NULL, &Ty,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)G1_Z, 0, NULL, &Tz,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;

    /* HP1 = (P + 1) / 2 */
    ret = OP_EXEC_ONE_CMD_IMME_B(aca_drv, (aca_op_ctx_t *)P, 1, NULL, &HP1,
                                 full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    ret =
        OP_EXEC_ONE_CMD_SHIFT(aca_drv, &HP1, 0, &HP1, full_id, TE_ACA_OP_SHR0);
    CHECK_RET_GO;

    /* P2 = P + P */
    ret = OP_EXEC_ONE_CMD(aca_drv, (aca_op_ctx_t *)P, (aca_op_ctx_t *)P, NULL,
                          &P2, full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    /* P3 = P2 + P */
    ret = OP_EXEC_ONE_CMD(aca_drv, &P2, (aca_op_ctx_t *)P, NULL, &P3, full_id,
                          TE_ACA_OP_ADD);
    CHECK_RET_GO;
    /* P6 = P3 + P3 */
    ret = OP_EXEC_ONE_CMD(aca_drv, &P3, &P3, NULL, &P6, full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;
    /* P9 = P6 + P3 */
    ret = OP_EXEC_ONE_CMD(aca_drv, &P6, &P3, NULL, &P9, full_id, TE_ACA_OP_ADD);
    CHECK_RET_GO;

    /* put op contexs, so that we can change usage to inout */
    op_ctx_put_all((aca_op_ctx_t *)G1_X, &Tx, (aca_op_ctx_t *)G1_Y, &Ty,
                   (aca_op_ctx_t *)G1_Z, &Tz, (aca_op_ctx_t *)P, &HP1, &P2, &P3,
                   &P6, &P9, NULL);
    CHECK_RET_GO;

    /* put all GRs to INOUT */
    ret = op_ctx_get_all(
        (aca_op_ctx_t *)G2_X, GR_USAGE_INOUT, &Tx, GR_USAGE_INOUT,
        (aca_op_ctx_t *)G2_Y, GR_USAGE_INOUT, &Ty, GR_USAGE_INOUT,
        (aca_op_ctx_t *)G2_Z, GR_USAGE_INOUT, &Tz, GR_USAGE_INOUT,
        (aca_op_ctx_t *)P, GR_USAGE_N, &HP1, GR_USAGE_INOUT, &P2,
        GR_USAGE_INOUT, &P3, GR_USAGE_INOUT, &P6, GR_USAGE_INOUT, &P9,
        GR_USAGE_INOUT, &t0, GR_USAGE_T0, &t1, GR_USAGE_T1, np, GR_USAGE_P,
        &u[0], GR_USAGE_INOUT, &u[1], GR_USAGE_INOUT, &u[2], GR_USAGE_INOUT,
        &u[3], GR_USAGE_INOUT, &u[4], GR_USAGE_INOUT, &u[5], GR_USAGE_INOUT,
        &u[6], GR_USAGE_INOUT, &u[7], GR_USAGE_INOUT, NULL);
    CHECK_RET_GO;

#ifdef ECP_DBG_PRINT_SIMPLE_EN
    op_ctx_dump("NP", np);
    op_ctx_dump("(P+1)/2", &HP1);
    op_ctx_dump("2P", &P2);
    op_ctx_dump("3P", &P3);
    op_ctx_dump("6P", &P6);
    op_ctx_dump("9P", &P9);
#endif

    ret = _add_point_jjj(aca_drv, full_id, modmul_id, (aca_op_ctx_t *)P, &HP1,
                         &P2, &P3, &P6, &P9, NULL, (aca_op_ctx_t *)G2_X,
                         (aca_op_ctx_t *)G2_Y, (aca_op_ctx_t *)G2_Z, &Tx, &Ty,
                         &Tz, NULL, false, false, u);
    CHECK_RET_GO;

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, true);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    is_clk_en = true;
#endif

    OP_ECP_PREPARE();
    OP_ECP_EXEC(MODRED, full_id, &Tx, 0, P, 0, &Tx);
    OP_ECP_EXEC(MODRED, full_id, &Ty, 0, P, 0, &Ty);
    OP_ECP_EXEC(MODRED, full_id, &Tz, 0, P, 0, &Tz);
    OP_ECP_WAIT();

#ifdef CFG_TE_DYNCLK_CTL
    ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
    OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    is_clk_en = false;
#endif

    /* put all GRs */
    op_ctx_put_all((aca_op_ctx_t *)G2_X, &Tx, (aca_op_ctx_t *)G2_Y, &Ty,
                   (aca_op_ctx_t *)G2_Z, &Tz, (aca_op_ctx_t *)P, &HP1, &P2, &P3,
                   &P6, &P9, &t0, &t1, np, &u[0], &u[1], &u[2], &u[3], &u[4],
                   &u[5], &u[6], &u[7], NULL);

    aca_len_type_free(len_type_pool, full_id);
    full_id = -1;
    aca_len_type_free(len_type_pool, modmul_id);
    modmul_id = -1;

    /* unlock */
    ACA_OP_UNLOCK(aca_drv);
    is_aca_lock = false;

    /* free others */
    op_ctx_clean(&HP1);
    op_ctx_clean(&P2);
    op_ctx_clean(&P3);
    op_ctx_clean(&P6);
    op_ctx_clean(&P9);
    op_ctx_clean(&t0);
    op_ctx_clean(&t1);
    /* free all u[i] */
    for (i = 0; i < 8; i++) {
        op_ctx_clean(&u[i]);
    }

    /* Copy to result */
    ret = aca_op_copy(aca_drv, R_X, &Tx);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, R_Y, &Ty);
    CHECK_RET_GO;
    ret = aca_op_copy(aca_drv, R_Z, &Tz);
    CHECK_RET_GO;

finish:
#ifdef CFG_TE_DYNCLK_CTL
    if (is_clk_en) {
        ret = aca_hwa->dynamic_clock_ctrl(aca_hwa, false);
        OSAL_ASSERT(TE_SUCCESS == (uint32_t)ret);
    }
#endif

    if (TE_SUCCESS != ret) {
        if (full_id >= 0) {
            aca_len_type_free(len_type_pool, full_id);
            full_id = -1;
        }
        if (modmul_id >= 0) {
            aca_len_type_free(len_type_pool, modmul_id);
            modmul_id = -1;
        }

        /* put all GRs */
        op_ctx_put_all((aca_op_ctx_t *)G2_X, &Tx, (aca_op_ctx_t *)G2_Y, &Ty,
                       (aca_op_ctx_t *)G2_Z, &Tz, (aca_op_ctx_t *)P, &HP1, &P2,
                       &P3, &P6, &P9, &t0, &t1, np, &u[0], &u[1], &u[2], &u[3],
                       &u[4], &u[5], &u[6], &u[7], NULL);
        if (is_aca_lock) {
            ACA_OP_UNLOCK(aca_drv);
        }
        op_ctx_clean(&HP1);
        op_ctx_clean(&P2);
        op_ctx_clean(&P3);
        op_ctx_clean(&P6);
        op_ctx_clean(&P9);
        op_ctx_clean(&t0);
        op_ctx_clean(&t1);
        for (i = 0; i < 8; i++) {
            op_ctx_clean(&u[i]);
        }
    }
    op_ctx_clean(&Tx);
    op_ctx_clean(&Ty);
    op_ctx_clean(&Tz);
    _g_op_debug_force_disable = 0;
    return ret;
}
