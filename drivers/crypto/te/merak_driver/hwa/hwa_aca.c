//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <hwa/te_hwa_aca.h>
#include <hwa/te_hwa.h>
#include "te_regs.h"

/**
 * ACA Block size in bits
 */
#define ACA_BLOCK_BITS 128
#define ACA_EXTRA_BITS 8
#define ACA_MAX_OP_BITS (16000)
/**
 * ACA SRAM Base Address
 */
#define ACA_SRAM_SCRAMBLING_BASE 0x80000000

#define ACA_WORD_SIZE 4

#define ACA_NP_MAX_BITS (160)
/**
 * Derive the ACA hwa ctx pointer from the ACA hwa handler
 */
#define HWA_ACA_CTX(_h)                                                        \
    __extension__({                                                            \
        hwa_aca_ctx_t *_ctx = NULL;                                            \
        _ctx                = (hwa_aca_ctx_t *)hwa_crypt_ctx(&(_h)->base);     \
        TE_ASSERT(_ctx != NULL);                                             \
        _ctx;                                                                  \
    })

/**
 * Get aca_\p rn.fn field value.
 */
#define ACA_FIELD_GET(val, rn, fn) HWA_FIELD_GET((val), ACA_##rn, fn)

/**
 * Set aca_\p rn.fn field value to \p fv.
 */
#define ACA_FIELD_SET(val, rn, fn, fv) HWA_FIELD_SET((val), ACA_##rn, fn, (fv))

/**
 * Get aca HWA register
 */
#define ACA_REG_GET(regs, nm) HWA_REG_GET(regs, aca, nm)

/**
 * Set aca HWA register
 */
#define ACA_REG_SET(regs, nm, nv) HWA_REG_SET(regs, aca, nm, nv)

/**
 * ACA HWA private context structure
 */
typedef struct hwa_aca_ctx {
    struct te_aca_regs *regs;   /**< ACA register file */
    osal_spin_lock_t spin;      /**< lock */
    osal_spin_lock_t sram_spin; /**< sram operation lock */
    uint32_t n_gr;              /**< number of GR */
    uint32_t n_len_type;        /**< number of length type */
    void *sram_base;            /**< ACA sram base address */
    uint32_t sram_size;         /**< ACA sram size in byte */
    uint32_t cq_num;            /**< ACA command queue number */
    osal_spin_lock_t clock_spin;/**< dynamic clock lock */
    volatile uint32_t clock_ref_cnt;/**< dynamic clock reference count*/
} hwa_aca_ctx_t;

static int aca_conf_gr_sram_addr(struct te_hwa_aca *h,
                                 int8_t gr_id,
                                 uint32_t sram_addr,
                                 uint32_t abcn_nblk)
{
    uint32_t val        = 0;
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    if (gr_id < 0 || gr_id > (int8_t)ctx->n_gr - 1) {
        return TE_ERROR_BAD_PARAMS;
    }

    if ((sram_addr < (uint32_t)(uintptr_t)(ctx->sram_base)) ||
        (sram_addr >= (uint32_t)(uintptr_t)(ctx->sram_base) + ctx->sram_size)) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* remove the scrambling base */
    sram_addr -= ACA_SRAM_SCRAMBLING_BASE;

    ACA_FIELD_SET(val, GR_SRAM_ADDR, ADDR, sram_addr >> 2);
    ACA_FIELD_SET(val, GR_SRAM_ADDR, LEN, abcn_nblk);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    log_aca_gr_sram_addr(val);
    ctx->regs->gr_sram_addr[gr_id].val = HTOLE32(val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_conf_len_type(struct te_hwa_aca *h,
                             int8_t len_type_id,
                             uint32_t len)
{
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    if (len_type_id < 0 || len_type_id > (int8_t)ctx->n_len_type - 1) {
        return TE_ERROR_BAD_PARAMS;
    }

    osal_spin_lock_irqsave(&ctx->spin, &flags);
    log_aca_gr_len_type(len);
    ctx->regs->gr_len_type[len_type_id].val = HTOLE32(len);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_conf_gr_for_n(struct te_hwa_aca *h, int8_t gr_id_n)
{
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->use_grid.bits.n_grid = (gr_id_n & 0x1F);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_conf_gr_for_p(struct te_hwa_aca *h, int8_t gr_id_p)
{
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->use_grid.bits.p_grid = (gr_id_p & 0x1F);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_conf_gr_for_t0(struct te_hwa_aca *h, int8_t gr_id_t0)
{
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->use_grid.bits.t0_grid = (gr_id_t0 & 0x1F);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_conf_gr_for_t1(struct te_hwa_aca *h, int8_t gr_id_t1)
{
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->use_grid.bits.t1_grid = (gr_id_t1 & 0x1F);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_set_ctrl(struct te_hwa_aca *h, const te_aca_ctrl_t *ctrl)
{
    uint32_t val        = 0;
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !ctrl) {
        return TE_ERROR_BAD_PARAMS;
    }

    val = *(uint32_t *)ctrl;
    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ACA_REG_SET(ctx->regs, ctrl, val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_get_ctrl(struct te_hwa_aca *h, te_aca_ctrl_t *ctrl)
{
    union {
        te_aca_ctrl_t ctrl;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !ctrl) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = ACA_REG_GET(ctx->regs, ctrl);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *ctrl = u.ctrl;
    return TE_SUCCESS;
}

static int aca_set_op_run(struct te_hwa_aca *h, bool is_run)
{
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ctx->regs->ctrl.bits.op_run = !!is_run;
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_set_op(struct te_hwa_aca *h, const te_aca_op_entry_t *op)
{
    union {
        te_aca_op_entry_t op;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.op = *op;
    ctx  = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ACA_REG_SET(ctx->regs, entry, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_get_status(struct te_hwa_aca *h, te_aca_stat_t *stat)
{
    union {
        te_aca_stat_t stat;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = ACA_REG_GET(ctx->regs, status);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *stat = u.stat;
    return TE_SUCCESS;
}

/**
 * \brief           This function writes 'size' byte data to the ACA SRAM
 *                  starting at LSB of SRAM. if the write data size < SRAM size,
 *                  the data is written to the lower bits, and the higher bits
 *                  are filled with 0.
 *
 *                  The data is written reverted to little endian.
 * \param[in] h         The ACA hwa handler.
 * \param[in] sram_addr SRAM offset addr to write, always be SRAM block size
 * aligned. \param[in] sram_size SRAM size in byte to write, always be SRAM
 * block size aligned. \param[in] data      Data to write. \param[in] size Write
 * data length, in byte. Must <= sram_size. \return          \c TE_SUCCESS on
 * success. \return          \c <0 on failure.
 */
static int aca_write_sram(struct te_hwa_aca *h,
                          void *sram_addr,
                          size_t sram_size,
                          const uint8_t *data,
                          size_t size)
{
    size_t i = 0, j = 0;
    uint32_t val       = 0;
    hwa_aca_ctx_t *ctx = NULL;
    uint32_t addr      = 0;

    if (!h || !data) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (sram_size < size) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (!UTILS_IS_ALIGNED(sram_addr, ACA_BLOCK_BITS / 8)) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (!UTILS_IS_ALIGNED(sram_size, ACA_BLOCK_BITS / 8)) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (0 == size) {
        return TE_SUCCESS;
    }

    ctx = HWA_ACA_CTX(h);
    if (sram_addr < ctx->sram_base) {
        return TE_ERROR_BAD_PARAMS;
    }
    if ((uintptr_t)sram_addr + sram_size >
        (uintptr_t)(ctx->sram_base) + ctx->sram_size) {
        return TE_ERROR_EXCESS_DATA;
    }

    /* remove the scrambling base */
    addr = (uint32_t)((uintptr_t)sram_addr - ACA_SRAM_SCRAMBLING_BASE);

    osal_spin_lock(&ctx->sram_spin);

    ctx->regs->sram_waddr = HTOLE32(addr >> 2); /**< word offset */
    val                   = 0;
    j                     = size;
    /* write data to little endian in SRAM */
    for (i = 0; i < sram_size / ACA_WORD_SIZE; i++) {
        val = (j > 0) ? (((uint32_t)data[j - 1]) << 0) : (0);
        j   = (j > 0) ? (j - 1) : (0);
        val |= (j > 0) ? (((uint32_t)data[j - 1]) << 8) : (0);
        j = (j > 0) ? (j - 1) : (0);
        val |= (j > 0) ? (((uint32_t)data[j - 1]) << 16) : (0);
        j = (j > 0) ? (j - 1) : (0);
        val |= (j > 0) ? (((uint32_t)data[j - 1]) << 24) : (0);
        j                     = (j > 0) ? (j - 1) : (0);
        ctx->regs->sram_wdata = HTOLE32(val);
    }

    osal_spin_unlock(&ctx->sram_spin);

    return TE_SUCCESS;
}

/**
 * \brief           This function read 'size' byte data from the ACA SRAM
 *                  starting at LSB of SRAM. if the read data size < SRAM size,
 *                  the data is read from the lower bits, and the higher bits
 *                  in SRAM are not read.
 *
 *                  The data is read reverted to big endian.
 * \param[in] h         The ACA hwa handler.
 * \param[in] sram_addr SRAM offset addr to read, always be SRAM block size
 * aligned. \param[in] sram_size SRAM size in byte to read, always be SRAM block
 * size aligned. \param[in] data      Buffer filled with read data on success.
 * \param[in] size      Read data length, in byte. Must <= sram_size
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static int aca_read_sram(struct te_hwa_aca *h,
                         void *sram_addr,
                         size_t sram_size,
                         uint8_t *data,
                         size_t size)
{
    hwa_aca_ctx_t *ctx = NULL;
    uint32_t val       = 0;
    size_t i = 0, j = 0;
    uint32_t addr = 0;

    if (!h || !data) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (sram_size < size) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (!UTILS_IS_ALIGNED(sram_addr, ACA_BLOCK_BITS / 8)) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (!UTILS_IS_ALIGNED(sram_size, ACA_BLOCK_BITS / 8)) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (0 == size) {
        return TE_SUCCESS;
    }

    ctx = HWA_ACA_CTX(h);
    if (sram_addr < ctx->sram_base) {
        return TE_ERROR_BAD_PARAMS;
    }
    if ((uintptr_t)sram_addr + sram_size >
        (uintptr_t)(ctx->sram_base) + ctx->sram_size) {
        return TE_ERROR_EXCESS_DATA;
    }

    /* remove the scrambling base */
    addr = (uint32_t)((uintptr_t)sram_addr - ACA_SRAM_SCRAMBLING_BASE);

    osal_spin_lock(&ctx->sram_spin);
    ctx->regs->sram_raddr = HTOLE32(addr >> 2); /**< word offset */

    val = 0;
    j   = size;
    /* read data in big endian to user buffer */
    for (i = 0; i < sram_size / ACA_WORD_SIZE; i++) {
        val         = LE32TOH(ctx->regs->sram_rdata); /**< read one word */
        data[j - 1] = (val >> 0) & 0xFF;
        j--;
        if (j <= 0)
            break;
        data[j - 1] = (val >> 8) & 0xFF;
        j--;
        if (j <= 0)
            break;
        data[j - 1] = (val >> 16) & 0xFF;
        j--;
        if (j <= 0)
            break;
        data[j - 1] = (val >> 24) & 0xFF;
        j--;
        if (j <= 0)
            break;
    }

    TE_ASSERT(j == 0);

    osal_spin_unlock(&ctx->sram_spin);

    return TE_SUCCESS;
}

/**
 * \brief           This function zeroize 'size' byte data in the ACA SRAM
 *
 * \param[in] h         The ACA hwa handler.
 * \param[in] sram_addr SRAM offset addr to zeroize, always be SRAM block size
 * aligned. \param[in] sram_size SRAM size in byte to zeroize, always be SRAM
 * block size aligned. \return          \c TE_SUCCESS on success. \return \c <0
 * on failure.
 */
static int aca_zeroize_sram(struct te_hwa_aca *h,
                            void *sram_addr,
                            size_t sram_size)
{
    size_t i           = 0;
    hwa_aca_ctx_t *ctx = NULL;
    uint32_t addr      = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (!UTILS_IS_ALIGNED(sram_addr, ACA_BLOCK_BITS / 8)) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (!UTILS_IS_ALIGNED(sram_size, ACA_BLOCK_BITS / 8)) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    if (sram_addr < ctx->sram_base) {
        return TE_ERROR_BAD_PARAMS;
    }
    if ((uintptr_t)sram_addr + sram_size >
        (uintptr_t)(ctx->sram_base) + ctx->sram_size) {
        return TE_ERROR_EXCESS_DATA;
    }

    /* remove the scrambling base */
    addr = (uint32_t)((uintptr_t)sram_addr - ACA_SRAM_SCRAMBLING_BASE);

    osal_spin_lock(&ctx->sram_spin);

    ctx->regs->sram_waddr = HTOLE32(addr >> 2); /**< word offset */
    /* write 0 to SRAM */
    for (i = 0; i < sram_size / ACA_WORD_SIZE; i++) {
        ctx->regs->sram_wdata = HTOLE32(0);
    }

    osal_spin_unlock(&ctx->sram_spin);

    return TE_SUCCESS;
}

/**
 * \brief           This function swaps in/out the SRAM data from/to the
 * external swapped area. Unlick sram read/write, this function handles uint32_t
 * data which can be read/write directly.
 *
 * \param[in] h             The ACA hwa handler.
 * \param[in] sram_addr     SRAM offset addr to read. MUST be word aligned.
 * \param[in/out] io_Buf    Swapped buffer pointer.
 * \param[in] io_size       Swapped size in byte, MUST be word aligned.
 * \param[in] is_swap_in    wheter current operation is swap in or swap out.
 * \return          \c TE_SUCCESS on success.
 * \return          \c <0 on failure.
 */
static int aca_swap_sram(struct te_hwa_aca *h,
                         void *sram_addr,
                         uint32_t *io_buf,
                         size_t io_size,
                         bool is_swap_in)
{
    hwa_aca_ctx_t *ctx = NULL;
    size_t i           = 0;
    uint32_t addr      = 0;

    if (!h || !io_buf) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (!UTILS_IS_ALIGNED(sram_addr, ACA_WORD_SIZE)) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (!UTILS_IS_ALIGNED(io_size, ACA_WORD_SIZE)) {
        return TE_ERROR_BAD_PARAMS;
    }

    if (0 == io_size) {
        return TE_SUCCESS;
    }

    ctx = HWA_ACA_CTX(h);
    if (sram_addr < ctx->sram_base) {
        return TE_ERROR_BAD_PARAMS;
    }
    if ((uintptr_t)sram_addr + io_size >
        (uintptr_t)(ctx->sram_base) + ctx->sram_size) {
        return TE_ERROR_EXCESS_DATA;
    }

    /* remove the scrambling base */
    addr = (uint32_t)((uintptr_t)sram_addr - ACA_SRAM_SCRAMBLING_BASE);

    osal_spin_lock(&ctx->sram_spin);

    if (is_swap_in) {
        ctx->regs->sram_waddr = HTOLE32(addr >> 2); /**< word offset */
    } else {
        ctx->regs->sram_raddr = HTOLE32(addr >> 2); /**< word offset */
    }
    for (i = 0; i < io_size / ACA_WORD_SIZE; i++) {
        if (is_swap_in) {
            ctx->regs->sram_wdata = HTOLE32(io_buf[i]);
        } else {
            io_buf[i] = LE32TOH(ctx->regs->sram_rdata);
        }
    }

    osal_spin_unlock(&ctx->sram_spin);

    return TE_SUCCESS;
}

static int aca_eoi(struct te_hwa_aca *h, const te_aca_int_t *stat)
{
    union {
        te_aca_int_t stat;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.stat = *stat;
    ctx    = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ACA_REG_SET(ctx->regs, intr_stat, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_int_state(struct te_hwa_aca *h, te_aca_int_t *stat)
{
    union {
        te_aca_int_t stat;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !stat) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = ACA_REG_GET(ctx->regs, intr_stat);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *stat = u.stat;
    return TE_SUCCESS;
}

static int aca_set_int_mask(struct te_hwa_aca *h, const te_aca_int_t *mask)
{
    union {
        te_aca_int_t mask;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !mask) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.mask = *mask;
    ctx    = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ACA_REG_SET(ctx->regs, intr_msk, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_get_int_mask(struct te_hwa_aca *h, te_aca_int_t *mask)
{
    union {
        te_aca_int_t mask;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !mask) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = ACA_REG_GET(ctx->regs, intr_msk);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *mask = u.mask;
    return TE_SUCCESS;
}

static int aca_set_suspd_mask(struct te_hwa_aca *h,
                              const te_aca_suspd_mask_t *suspd_mask)
{
    union {
        te_aca_suspd_mask_t suspd_mask;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !suspd_mask) {
        return TE_ERROR_BAD_PARAMS;
    }

    u.suspd_mask = *suspd_mask;
    ctx          = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    ACA_REG_SET(ctx->regs, suspd_msk, u.val);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    return TE_SUCCESS;
}

static int aca_get_suspd_mask(struct te_hwa_aca *h,
                              te_aca_suspd_mask_t *suspd_mask)
{
    union {
        te_aca_suspd_mask_t suspd_mask;
        uint32_t val;
    } u                 = {0};
    hwa_aca_ctx_t *ctx  = NULL;
    unsigned long flags = 0;

    if (!h || !suspd_mask) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    osal_spin_lock_irqsave(&ctx->spin, &flags);
    u.val = ACA_REG_GET(ctx->regs, intr_msk);
    osal_spin_unlock_irqrestore(&ctx->spin, flags);

    *suspd_mask = u.suspd_mask;
    return TE_SUCCESS;
}

static int aca_get_gr_num(struct te_hwa_aca *h)
{
    hwa_aca_ctx_t *ctx = NULL;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    return ctx->n_gr;
}

static int aca_get_len_type_num(struct te_hwa_aca *h)
{
    hwa_aca_ctx_t *ctx = NULL;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    return ctx->n_len_type;
}

static int aca_get_core_granule(struct te_hwa_aca *h)
{
    (void)h;
    return ACA_BLOCK_BITS;
}
static int aca_get_core_max_op_len(struct te_hwa_aca *h)
{
    (void)h;
    return ACA_MAX_OP_BITS;
}

static int aca_get_param_for_calc_np(struct te_hwa_aca *h,
                                     uint32_t *param0,
                                     uint32_t *param1,
                                     uint32_t *param2)
{
    if ((!h) || (!param0) || (!param1) || (!param2)) {
        return TE_ERROR_BAD_PARAMS;
    }

    *param0 = ((ACA_BLOCK_BITS + ACA_EXTRA_BITS) * 2);
    *param1 = (ACA_BLOCK_BITS + ACA_EXTRA_BITS - 1);
    *param2 = (((ACA_BLOCK_BITS + ACA_EXTRA_BITS) * 3) - 1);
    return TE_SUCCESS;
}

static int aca_get_sram_info(struct te_hwa_aca *h, void **base, size_t *size)
{
    hwa_aca_ctx_t *ctx = NULL;

    if ((!h) || (!base) || (!size)) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx   = HWA_ACA_CTX(h);
    *base = ctx->sram_base;
    *size = ctx->sram_size;
    return TE_SUCCESS;
}

static int aca_get_cq_num(struct te_hwa_aca *h)
{
    hwa_aca_ctx_t *ctx = NULL;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);
    return ctx->cq_num;
}

#define _SET_OP_BITS(__ptr__, __op_bit_len__, __extra_bit_num__)               \
    do {                                                                       \
        if (__ptr__) {                                                         \
            *(__ptr__) = UTILS_ROUND_UP(__op_bit_len__ + __extra_bit_num__,     \
                                       ACA_BLOCK_BITS);                        \
        }                                                                      \
    } while (0);
static int aca_get_op_bits(struct te_hwa_aca *h,
                           te_aca_op_code_t code,
                           size_t op_bit_len,
                           size_t *n_op_bits,
                           size_t *ac_op_bits,
                           size_t *b_op_bits,
                           size_t *r_t0_t1_op_bits)
{
    (void)h;
    switch (code) {
    case TE_ACA_OP_MODINV:
        _SET_OP_BITS(n_op_bits, op_bit_len, 16);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 16);
        _SET_OP_BITS(b_op_bits, op_bit_len, 0); /* actually we don't need b */
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 16);
        break;
    case TE_ACA_OP_MODEXP:
        _SET_OP_BITS(n_op_bits, op_bit_len, 2);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 2);
        _SET_OP_BITS(b_op_bits, op_bit_len, 0);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 2);
        break;
    case TE_ACA_OP_MODMUL:
    case TE_ACA_OP_MODMULACC:
        _SET_OP_BITS(n_op_bits, op_bit_len, 2);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 0);
        _SET_OP_BITS(b_op_bits, op_bit_len, 0);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 2);
        break;
    case TE_ACA_OP_MODMULNR:
    case TE_ACA_OP_MODMULACCNR:
        _SET_OP_BITS(n_op_bits, op_bit_len, 0);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 0);
        _SET_OP_BITS(b_op_bits, op_bit_len, 0);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 2);
        break;
    case TE_ACA_OP_MODMUL2:
        _SET_OP_BITS(n_op_bits, op_bit_len, 2);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 2);
        _SET_OP_BITS(b_op_bits, op_bit_len, 2);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 2);
        break;
    case TE_ACA_OP_MODMUL2NR:
        _SET_OP_BITS(n_op_bits, op_bit_len, 0);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 2);
        _SET_OP_BITS(b_op_bits, op_bit_len, 2);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 0);
        break;
    case TE_ACA_OP_MODMUL7:
        _SET_OP_BITS(n_op_bits, op_bit_len, 2);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 7);
        _SET_OP_BITS(b_op_bits, op_bit_len, 7);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 2);
        break;
    case TE_ACA_OP_MODMUL7NR:
    case TE_ACA_OP_MODMULACC7NR:
        _SET_OP_BITS(n_op_bits, op_bit_len, 0);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 7);
        _SET_OP_BITS(b_op_bits, op_bit_len, 7);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 2);
        break;
    default:
        _SET_OP_BITS(n_op_bits, op_bit_len, 0);
        _SET_OP_BITS(ac_op_bits, op_bit_len, 0);
        _SET_OP_BITS(b_op_bits, op_bit_len, 0);
        _SET_OP_BITS(r_t0_t1_op_bits, op_bit_len, 0);
    }
    return TE_SUCCESS;
}

__attribute__((unused)) static const char *
__get_op_code_str(te_aca_op_code_t code)
{
    switch (code) {
    case TE_ACA_OP_ADD:
        return "ADD";
    case TE_ACA_OP_SUB:
        return "SUB";
    case TE_ACA_OP_MUL_LOW:
        return "MUL_LOW";
    case TE_ACA_OP_DIV:
        return "DIV";
    case TE_ACA_OP_AND:
        return "AND";
    case TE_ACA_OP_OR:
        return "OR";
    case TE_ACA_OP_XOR:
        return "XOR";
    case TE_ACA_OP_SHR0:
        return "SHR0";
    case TE_ACA_OP_SHL0:
        return "SHL0";
    case TE_ACA_OP_SHL1:
        return "SHL1";
    case TE_ACA_OP_MUL_HIGH:
        return "MUL_HIGH";
    case TE_ACA_OP_MODRED:
        return "MODRED";
    case TE_ACA_OP_MODADD:
        return "MODADD";
    case TE_ACA_OP_MODSUB:
        return "MODSUB";
    case TE_ACA_OP_MODMUL:
        return "MODMUL";
    case TE_ACA_OP_MODINV:
        return "MODINV";
    case TE_ACA_OP_MODEXP:
        return "MODEXP";
    case TE_ACA_OP_MODMULNR:
        return "MODMULNR";
    case TE_ACA_OP_MODMULACC:
        return "MODMULACC";
    case TE_ACA_OP_MODMULACCNR:
        return "MODMULACCNR";
    case TE_ACA_OP_MODMUL2:
        return "MODMUL2";
    case TE_ACA_OP_MODMUL2NR:
        return "MODMUL2NR";
    case TE_ACA_OP_MODMUL7:
        return "MODMUL7";
    case TE_ACA_OP_MODMUL7NR:
        return "MODMUL7NR";
    case TE_ACA_OP_MODMULACC7NR:
        return "MODMULACC7NR";
    default:
        return "NULL OP Code";
    }
}

static void __dbg_dump_gr_info(hwa_aca_ctx_t *ctx,
                               uint32_t gr_id,
                               const char *gr_name,
                               int32_t op_bits,
                               bool dump_data,
                               int dump_level)
{
    uint32_t val    = 0;
    uint32_t tmp_sz = 0;
    uint32_t addr   = 0;
    uint32_t nblk   = 0;
    uint32_t *p     = NULL;
    uint32_t i      = 0;

    val  = LE32TOH(ctx->regs->gr_sram_addr[gr_id].val);
    addr = (uint32_t)ACA_FIELD_GET(val, GR_SRAM_ADDR, ADDR);
    nblk = (uint32_t)ACA_FIELD_GET(val, GR_SRAM_ADDR, LEN);

    if (0 == nblk) {
        tmp_sz = op_bits / 8;
    } else {
        tmp_sz = (nblk * ACA_BLOCK_BITS) / 8;
    }

    if (nblk) {
        OSAL_LOG_DEBUG(
            "     %s: %d  Sram: [0x%x - 0x%x] BLK: %d(%d Bytes), OP: %d(%d "
            "Bytes)\n",
            gr_name, gr_id,
            (ACA_SRAM_SCRAMBLING_BASE + (addr << 2)) /* Sram Start */,
            ((ACA_SRAM_SCRAMBLING_BASE + (addr << 2)) +
             (nblk * ACA_BLOCK_BITS) / 8) /* Sram end */,
            nblk /* blkcs */, (nblk * ACA_BLOCK_BITS) / 8 /* blkcs in bytes */,
            op_bits / ACA_BLOCK_BITS /* op blocks */, op_bits / 8);
        if (((nblk * ACA_BLOCK_BITS) / 8) > (uint32_t)(op_bits / 8)) {
            OSAL_LOG_WARN("Warning, BLK Size %d > OP Size %d!!!\n",
                          ((nblk * ACA_BLOCK_BITS) / 8), (op_bits / 8));
        }
    } else {
        OSAL_LOG_DEBUG(
            "     %s: %d  Sram: [0x%x - 0x%x], BLK: NA-NA, OP: %d(%d Bytes)\n",
            gr_name, gr_id,
            (ACA_SRAM_SCRAMBLING_BASE + (addr << 2)) /* Sram Start */,
            (ACA_SRAM_SCRAMBLING_BASE + (addr << 2)) +
                op_bits / 8 /* Sram End */,
            op_bits / ACA_BLOCK_BITS /* op blocks */, op_bits / 8);
    }

    if (dump_level == 0) {
        /* force NOT dump data */
        dump_data = false;
    }

    if (dump_data) {
        p = (uint32_t *)osal_calloc(1, tmp_sz);
        if (!p) {
            OSAL_LOG_ERR("Malloc %d failed, skip dump %s\n", tmp_sz, gr_name);
            return;
        }

        ctx->regs->sram_raddr = HTOLE32(addr); /**< word offset */

        for (i = 0; i < tmp_sz / ACA_WORD_SIZE; i++) {
            p[i] = LE32TOH(ctx->regs->sram_rdata);
        }

        if (nblk) {
            OSAL_LOG_DEBUG_DUMP_DATA("     FULL BLK Data (Partial OP):", p,
                                     tmp_sz);
        } else {
            OSAL_LOG_DEBUG_DUMP_DATA("     OP Data:", p, tmp_sz);
        }
        OSAL_SAFE_FREE(p);
    }
    return;
}

static int aca_dbg_dump(struct te_hwa_aca *h,
                        const te_aca_op_entry_t *op,
                        bool is_result,
                        int dump_level)
{
    int ret                = 0;
    hwa_aca_ctx_t *ctx     = NULL;
    uint32_t gr_id         = 0;
    uint32_t len_type_id   = 0;
    uint32_t len_type_bits = 0;
    size_t n_op_bits       = 0;
    size_t ac_op_bits      = 0;
    size_t b_op_bits       = 0;
    size_t r_t0_t1_op_bits = 0;
    uint32_t tmp           = 0;
    bool need_b = false, need_c = false, need_n = false, need_p = false,
         need_t0 = false, need_t1 = false, update_a = false, save_to_r = false;
    te_aca_op_code_t op_code;

    if (!h || !op) {
        return TE_ERROR_BAD_PARAMS;
    }
    ctx = HWA_ACA_CTX(h);

    memcpy(&tmp, op, sizeof(uint32_t));

    op_code = op->op_code;

    /* if we need B */
    if (op_code != TE_ACA_OP_MODINV) {
        need_b = true;
    } else {
        need_b = false;
    }
    /* if we need C */
    if ((op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR)) {
        need_c = true;
    } else {
        need_c = false;
    }

    /* if we need N */
    if ((op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB) ||
        (op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODINV) ||
        (op_code == TE_ACA_OP_MODEXP) || (op_code == TE_ACA_OP_MODMULNR) ||
        (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR)) {
        need_n = true;
    } else {
        need_n = false;
    }

    /* if we need P */
    if ((op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODMULNR) ||
        (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_MODMULACCNR) || (op_code == TE_ACA_OP_MODEXP)) {
        need_p = true;
    } else {
        need_p = false;
    }

    /* if we need T0 */
    if ((op_code == TE_ACA_OP_MODADD) || (op_code == TE_ACA_OP_MODSUB) ||
        (op_code == TE_ACA_OP_MODMUL) || (op_code == TE_ACA_OP_MODMULACC) ||
        (op_code == TE_ACA_OP_DIV) || (op_code == TE_ACA_OP_MODRED) ||
        (op_code == TE_ACA_OP_MODINV) || (op_code == TE_ACA_OP_MODEXP)) {
        need_t0 = true;
    } else {
        need_t0 = false;
    }

    /* if we need T1 */
    if ((op_code == TE_ACA_OP_DIV) || (op_code == TE_ACA_OP_MODINV)) {
        need_t1 = true;
    } else {
        need_t1 = false;
    }

    /* if we updated A */
    if ((op_code == TE_ACA_OP_DIV) || (op_code == TE_ACA_OP_MODINV)) {
        update_a = true;
    } else {
        update_a = false;
    }

    /* check if we should save to R */
    if ((op_code == TE_ACA_OP_ADD) || (op_code == TE_ACA_OP_SUB) ||
        (op_code == TE_ACA_OP_AND) || (op_code == TE_ACA_OP_OR) ||
        (op_code == TE_ACA_OP_XOR)) {
        save_to_r = !op->no_save_to_r;
    } else {
        save_to_r = true;
    }

    /* calculate length type */
    len_type_id   = op->len_type_id;
    len_type_bits = LE32TOH(ctx->regs->gr_len_type[len_type_id].val);
    ret = aca_get_op_bits(h, op_code, (int32_t)(len_type_bits), &n_op_bits,
                          &ac_op_bits, &b_op_bits, &r_t0_t1_op_bits);
    TE_ASSERT(TE_SUCCESS == ret);
    if (!is_result) {
        /* Dump Parameters */
        OSAL_LOG_DEBUG(">>>>> CMD: 0x%08x <<<<<\n", tmp);

        /* dump OP code */
        OSAL_LOG_DEBUG("     OP Code: %s: %d Parameters:\n",
                       __get_op_code_str(op_code), op_code);

        /* dump A */
        gr_id = op->op_a_id;
        __dbg_dump_gr_info(ctx, gr_id, "A", ac_op_bits, true, dump_level);

        if (need_b) {
            if ((op_code == TE_ACA_OP_SHR0) || (op_code == TE_ACA_OP_SHL0) ||
                (op_code == TE_ACA_OP_SHL1)) {
                gr_id = op->op_b;
                gr_id = gr_id << 1;
                if (op->no_save_to_r) {
                    gr_id |= 0x01;
                }
                OSAL_LOG_DEBUG("     SHIFT Imme B: 0x%x\n", gr_id);
            } else {
                /* dump B */
                gr_id = op->op_b;
                if (gr_id & TE_ACA_OPERAND_B_IMM) {
                    OSAL_LOG_DEBUG("     Imme B: 0x%x\n",
                                   (gr_id & (~TE_ACA_OPERAND_B_IMM)));
                } else {
                    __dbg_dump_gr_info(ctx, gr_id, "B", b_op_bits, true,
                                       dump_level);
                }
            }
        }

        if (need_c) {
            /* dump C */
            gr_id = op->op_c_id;
            __dbg_dump_gr_info(ctx, gr_id, "C", ac_op_bits, true, dump_level);
        }

        /* dump Save to R */
        if (save_to_r) {
            /* dump R */
            gr_id = op->op_r_id;
            __dbg_dump_gr_info(ctx, gr_id, "R", r_t0_t1_op_bits, false,
                               dump_level);
        }

        if (need_n) {
            /* dump N */
            gr_id = ctx->regs->use_grid.bits.n_grid;
            __dbg_dump_gr_info(ctx, gr_id, "N", n_op_bits, true, dump_level);
        }

        if (need_p) {
            gr_id = ctx->regs->use_grid.bits.p_grid;
            /* P is fixed to 160 bits */
            __dbg_dump_gr_info(ctx, gr_id, "P",
                               UTILS_ROUND_UP(ACA_NP_MAX_BITS, ACA_BLOCK_BITS),
                               true, dump_level);
        }

        if (need_t0) {
            gr_id = ctx->regs->use_grid.bits.t0_grid;
            __dbg_dump_gr_info(ctx, gr_id, "T0", r_t0_t1_op_bits, false,
                               dump_level);
        }

        if (need_t1) {
            gr_id = ctx->regs->use_grid.bits.t1_grid;
            __dbg_dump_gr_info(ctx, gr_id, "T1", r_t0_t1_op_bits, false,
                               dump_level);
        }

        OSAL_LOG_DEBUG("     Length Type: %d  Value(bits): 0x%x(%d)\n",
                       len_type_id, len_type_bits, len_type_bits);

        /* dump if need intr */
#if 0
        OSAL_LOG_DEBUG("     Need Interrupt: %s\n",
                       (op->need_intr) ? ("True") : ("False"));
        OSAL_LOG_DEBUG("     Save to R: %d\n", save_to_r);

        OSAL_LOG_DEBUG("     GR A is updated: %s\n",
                       (update_a) ? ("True") : ("False"));
#endif
    } else {
        /* Dump Result */
        /* dump OP code */
        OSAL_LOG_DEBUG("     OP Code: %s: %d Result:\n",
                       __get_op_code_str(op_code), op_code);

        if (save_to_r) {
            /* dump R */
            gr_id = op->op_r_id;
            __dbg_dump_gr_info(ctx, gr_id, "R", r_t0_t1_op_bits, true,
                               dump_level);
        }

        /* dump A */
        if (update_a) {
            gr_id = op->op_a_id;
            __dbg_dump_gr_info(ctx, gr_id, "Updated A", ac_op_bits, true,
                               dump_level);
        }
        OSAL_LOG_DEBUG(">>>>> END <<<<<\n");
    }

    return TE_SUCCESS;
}

#ifdef CFG_TE_DYNCLK_CTL

static void _aca_clock_ctrl(hwa_aca_ctx_t *ctx, bool is_enable)
{
    union {
        te_aca_ctrl_t ctrl;
        uint32_t val;
    } u = {0};

    u.val = ACA_REG_GET(ctx->regs, ctrl);

    if (is_enable) {
        u.ctrl.clock_enable = 1;
    } else {
        u.ctrl.clock_enable = 0;
    }

    ACA_REG_SET(ctx->regs, ctrl, u.val);

    return;
}

static int aca_dynamic_clock_ctrl(struct te_hwa_aca *h, bool is_enable)
{
    hwa_aca_ctx_t *ctx = NULL;
    unsigned long flags = 0;

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);

    osal_spin_lock_irqsave(&ctx->clock_spin, &flags);

    if (is_enable) {
        ctx->clock_ref_cnt++;
        if (1 == ctx->clock_ref_cnt) {
            _aca_clock_ctrl(ctx, true);
        }
    } else {
        ctx->clock_ref_cnt--;
        if (0 == ctx->clock_ref_cnt) {
            _aca_clock_ctrl(ctx, false);
        }
    }

    osal_spin_unlock_irqrestore(&ctx->clock_spin, flags);
    return TE_SUCCESS;
}

static int aca_dynamic_clock_status(struct te_hwa_aca *h, bool *is_enable)
{
    hwa_aca_ctx_t *ctx = NULL;
    unsigned long flags = 0;
    union {
        te_aca_ctrl_t ctrl;
        uint32_t val;
    } u                 = {0};

    if (!h) {
        return TE_ERROR_BAD_PARAMS;
    }
    if (!is_enable) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = HWA_ACA_CTX(h);

    osal_spin_lock_irqsave(&ctx->clock_spin, &flags);

    u.val = ACA_REG_GET(ctx->regs, ctrl);
    *is_enable = (u.ctrl.clock_enable) ? (true) : (false);
    osal_spin_unlock_irqrestore(&ctx->clock_spin, flags);
    return TE_SUCCESS;
}

#endif

int te_hwa_aca_alloc(struct te_aca_regs *regs,
                     struct te_hwa_host *host,
                     te_hwa_aca_t **hwa)
{
    int rc            = TE_SUCCESS;
    te_hwa_aca_t *aca = NULL;

    if (!regs || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    /* alloc mem */
    if ((aca = osal_calloc(1, sizeof(*aca))) == NULL) {
        return TE_ERROR_OOM;
    }

    /* init hwa */
    rc = te_hwa_aca_init(regs, host, aca);
    if (rc != TE_SUCCESS) {
        osal_free(aca);
        return rc;
    }

    *hwa = aca;
    return TE_SUCCESS;
}

int te_hwa_aca_free(te_hwa_aca_t *hwa)
{
    int rc = TE_SUCCESS;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    rc = te_hwa_aca_exit(hwa);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    osal_memset(hwa, 0, sizeof(*hwa));
    osal_free(hwa);
    return TE_SUCCESS;
}

int te_hwa_aca_init(struct te_aca_regs *regs,
                    struct te_hwa_host *host,
                    te_hwa_aca_t *hwa)
{
    int rc             = TE_SUCCESS;
    hwa_aca_ctx_t *ctx = NULL;
    te_rtl_conf_t conf = {0};

    if (!regs || !host || !hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    TE_ASSERT(host->stat.conf);
    rc = host->stat.conf(&host->stat, &conf);
    if (rc != TE_SUCCESS) {
        return rc;
    }

    if ((ctx = osal_calloc(1, sizeof(*ctx))) == NULL) {
        return TE_ERROR_OOM;
    }

    rc = osal_spin_lock_init(&ctx->spin);
    if (rc != OSAL_SUCCESS) {
        goto err;
    }

    rc = osal_spin_lock_init(&ctx->sram_spin);
    if (rc != OSAL_SUCCESS) {
        goto err1;
    }

    rc = osal_spin_lock_init(&ctx->clock_spin);
    if (rc != OSAL_SUCCESS) {
        goto err2;
    }

    ctx->clock_ref_cnt = 0;

    ctx->regs       = regs;
    ctx->n_gr       = ARRAY_SIZE(regs->gr_sram_addr);
    ctx->n_len_type = ARRAY_SIZE(regs->gr_len_type);
    ctx->sram_base  = (void *)(ACA_SRAM_SCRAMBLING_BASE);
    ctx->sram_size  = conf.sram.aca_sram_sz;
    ctx->cq_num     = conf.sram.aca_cq_depth;
    osal_memset(hwa, 0, sizeof(*hwa));
    hwa_crypt_init(&hwa->base, host, (void *)ctx);

    /* set ops */
    hwa->config_gr_sram_addr   = aca_conf_gr_sram_addr;
    hwa->config_len_type       = aca_conf_len_type;
    hwa->config_gr_for_n       = aca_conf_gr_for_n;
    hwa->config_gr_for_p       = aca_conf_gr_for_p;
    hwa->config_gr_for_t0      = aca_conf_gr_for_t0;
    hwa->config_gr_for_t1      = aca_conf_gr_for_t1;
    hwa->set_ctrl              = aca_set_ctrl;
    hwa->get_ctrl              = aca_get_ctrl;
    hwa->set_op_run            = aca_set_op_run;
    hwa->set_op                = aca_set_op;
    hwa->get_status            = aca_get_status;
    hwa->write_sram            = aca_write_sram;
    hwa->read_sram             = aca_read_sram;
    hwa->zeroize_sram          = aca_zeroize_sram;
    hwa->swap_sram             = aca_swap_sram;
    hwa->eoi                   = aca_eoi;
    hwa->int_state             = aca_int_state;
    hwa->set_intr_mask         = aca_set_int_mask;
    hwa->get_intr_mask         = aca_get_int_mask;
    hwa->set_suspend_mask      = aca_set_suspd_mask;
    hwa->get_suspend_mask      = aca_get_suspd_mask;
    hwa->get_gr_num            = aca_get_gr_num;
    hwa->get_len_type_num      = aca_get_len_type_num;
    hwa->get_core_granularity  = aca_get_core_granule;
    hwa->get_core_max_op_len   = aca_get_core_max_op_len;
    hwa->get_sram_info         = aca_get_sram_info;
    hwa->get_cq_num            = aca_get_cq_num;
    hwa->get_op_bits           = aca_get_op_bits;
    hwa->get_param_for_calc_np = aca_get_param_for_calc_np;
    hwa->dbg_dump              = aca_dbg_dump;
#ifdef CFG_TE_DYNCLK_CTL
    hwa->dynamic_clock_ctrl    = aca_dynamic_clock_ctrl;
    hwa->dynamic_clock_status  = aca_dynamic_clock_status;
#endif

    return TE_SUCCESS;

err2:
    osal_spin_lock_destroy(&ctx->sram_spin);
err1:
    osal_spin_lock_destroy(&ctx->spin);
err:
    osal_free(ctx);
    ctx = NULL;
    return rc;
}

int te_hwa_aca_exit(te_hwa_aca_t *hwa)
{
    hwa_aca_ctx_t *ctx = NULL;

    if (!hwa) {
        return TE_ERROR_BAD_PARAMS;
    }

    ctx = (hwa_aca_ctx_t *)hwa_crypt_ctx(&hwa->base);
    osal_spin_lock_destroy(&ctx->spin);
    osal_spin_lock_destroy(&ctx->sram_spin);
    osal_memset(ctx, 0, sizeof(*ctx));
    osal_free(ctx);
    hwa_crypt_exit(&hwa->base);
    return TE_SUCCESS;
}
