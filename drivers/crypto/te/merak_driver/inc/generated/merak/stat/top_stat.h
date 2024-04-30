//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __STAT_TOP_STAT_REG_H__
#define __STAT_TOP_STAT_REG_H__

#define STAT_TOP_STAT_CTX_POOL_RDY_SHIFT 24
#define STAT_TOP_STAT_CTX_POOL_RDY_WIDTH 1
#define STAT_TOP_STAT_HW_KEY_VALID_SHIFT 23
#define STAT_TOP_STAT_HW_KEY_VALID_WIDTH 1
#define STAT_TOP_STAT_OTP_INIT_DONE_SHIFT 22
#define STAT_TOP_STAT_OTP_INIT_DONE_WIDTH 1
#define STAT_TOP_STAT_AO_SHD_VALID_SHIFT 16 /**< 0~5, mid:modk:did:rootk:lcs:lock */
#define STAT_TOP_STAT_AO_SHD_VALID_WIDTH 6
#define STAT_TOP_STAT_DUMMY_LCS_CM_SHIFT 11
#define STAT_TOP_STAT_DUMMY_LCS_CM_WIDTH 1
#define STAT_TOP_STAT_DUMMY_LCS_DM_SHIFT 10
#define STAT_TOP_STAT_DUMMY_LCS_DM_WIDTH 1
#define STAT_TOP_STAT_DUMMY_LCS_DD_SHIFT 9
#define STAT_TOP_STAT_DUMMY_LCS_DD_WIDTH 1
#define STAT_TOP_STAT_DUMMY_LCS_DR_SHIFT 8
#define STAT_TOP_STAT_DUMMY_LCS_DR_WIDTH 1
#define STAT_TOP_STAT_OTP_READY_SHIFT 7
#define STAT_TOP_STAT_OTP_READY_WIDTH 1
#define STAT_TOP_STAT_SW_INIT_DONE_SHIFT 6 /**< sw_init_done flag set by host0 */
#define STAT_TOP_STAT_SW_INIT_DONE_WIDTH 1
#define STAT_TOP_STAT_DMA_SRESET_SHIFT 5
#define STAT_TOP_STAT_DMA_SRESET_WIDTH 1
#define STAT_TOP_STAT_TRNG_SRESET_SHIFT 4
#define STAT_TOP_STAT_TRNG_SRESET_WIDTH 1
#define STAT_TOP_STAT_OTP_SRESET_SHIFT 3
#define STAT_TOP_STAT_OTP_SRESET_WIDTH 1
#define STAT_TOP_STAT_ACA_SRESET_SHIFT 2
#define STAT_TOP_STAT_ACA_SRESET_WIDTH 1
#define STAT_TOP_STAT_SCA_SRESET_SHIFT 1
#define STAT_TOP_STAT_SCA_SRESET_WIDTH 1
#define STAT_TOP_STAT_HASH_SRESET_SHIFT 0
#define STAT_TOP_STAT_HASH_SRESET_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * stat_top_stat register definition.
 */
typedef union stat_top_statReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash_sreset: 1,
        sca_sreset: 1,
        aca_sreset: 1,
        otp_sreset: 1,
        trng_sreset: 1,
        dma_sreset: 1,
        sw_init_done: 1  /**< sw_init_done flag set by host0 */,
        otp_ready: 1,
        dummy_lcs_dr: 1,
        dummy_lcs_dd: 1,
        dummy_lcs_dm: 1,
        dummy_lcs_cm: 1,
        hole16: 4,
        ao_shd_valid: 6  /**< 0~5, mid:modk:did:rootk:lcs:lock */,
        otp_init_done: 1,
        hw_key_valid: 1,
        ctx_pool_rdy: 1,
        hole0: 7; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} stat_top_statReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_stat_top_stat(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("stat_top_stat: 0x%08x is\n", x);


    t = (x >> STAT_TOP_STAT_CTX_POOL_RDY_SHIFT) & ((1U << STAT_TOP_STAT_CTX_POOL_RDY_WIDTH) - 1);
    BITFIELD_LOG(" ctx_pool_rdy=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_CTX_POOL_RDY_WIDTH) - 1) << STAT_TOP_STAT_CTX_POOL_RDY_SHIFT);


    t = (x >> STAT_TOP_STAT_HW_KEY_VALID_SHIFT) & ((1U << STAT_TOP_STAT_HW_KEY_VALID_WIDTH) - 1);
    BITFIELD_LOG(" hw_key_valid=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_HW_KEY_VALID_WIDTH) - 1) << STAT_TOP_STAT_HW_KEY_VALID_SHIFT);


    t = (x >> STAT_TOP_STAT_OTP_INIT_DONE_SHIFT) & ((1U << STAT_TOP_STAT_OTP_INIT_DONE_WIDTH) - 1);
    BITFIELD_LOG(" otp_init_done=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_OTP_INIT_DONE_WIDTH) - 1) << STAT_TOP_STAT_OTP_INIT_DONE_SHIFT);


    t = (x >> STAT_TOP_STAT_AO_SHD_VALID_SHIFT) & ((1U << STAT_TOP_STAT_AO_SHD_VALID_WIDTH) - 1);
    BITFIELD_LOG(" ao_shd_valid=0x%x (0~5, mid:modk:did:rootk:lcs:lock)\n", t);
    x &= ~(((1U << STAT_TOP_STAT_AO_SHD_VALID_WIDTH) - 1) << STAT_TOP_STAT_AO_SHD_VALID_SHIFT);


    t = (x >> STAT_TOP_STAT_DUMMY_LCS_CM_SHIFT) & ((1U << STAT_TOP_STAT_DUMMY_LCS_CM_WIDTH) - 1);
    BITFIELD_LOG(" dummy_lcs_cm=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_DUMMY_LCS_CM_WIDTH) - 1) << STAT_TOP_STAT_DUMMY_LCS_CM_SHIFT);


    t = (x >> STAT_TOP_STAT_DUMMY_LCS_DM_SHIFT) & ((1U << STAT_TOP_STAT_DUMMY_LCS_DM_WIDTH) - 1);
    BITFIELD_LOG(" dummy_lcs_dm=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_DUMMY_LCS_DM_WIDTH) - 1) << STAT_TOP_STAT_DUMMY_LCS_DM_SHIFT);


    t = (x >> STAT_TOP_STAT_DUMMY_LCS_DD_SHIFT) & ((1U << STAT_TOP_STAT_DUMMY_LCS_DD_WIDTH) - 1);
    BITFIELD_LOG(" dummy_lcs_dd=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_DUMMY_LCS_DD_WIDTH) - 1) << STAT_TOP_STAT_DUMMY_LCS_DD_SHIFT);


    t = (x >> STAT_TOP_STAT_DUMMY_LCS_DR_SHIFT) & ((1U << STAT_TOP_STAT_DUMMY_LCS_DR_WIDTH) - 1);
    BITFIELD_LOG(" dummy_lcs_dr=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_DUMMY_LCS_DR_WIDTH) - 1) << STAT_TOP_STAT_DUMMY_LCS_DR_SHIFT);


    t = (x >> STAT_TOP_STAT_OTP_READY_SHIFT) & ((1U << STAT_TOP_STAT_OTP_READY_WIDTH) - 1);
    BITFIELD_LOG(" otp_ready=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_OTP_READY_WIDTH) - 1) << STAT_TOP_STAT_OTP_READY_SHIFT);


    t = (x >> STAT_TOP_STAT_SW_INIT_DONE_SHIFT) & ((1U << STAT_TOP_STAT_SW_INIT_DONE_WIDTH) - 1);
    BITFIELD_LOG(" sw_init_done=0x%x (sw_init_done flag set by host0)\n", t);
    x &= ~(((1U << STAT_TOP_STAT_SW_INIT_DONE_WIDTH) - 1) << STAT_TOP_STAT_SW_INIT_DONE_SHIFT);


    t = (x >> STAT_TOP_STAT_DMA_SRESET_SHIFT) & ((1U << STAT_TOP_STAT_DMA_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" dma_sreset=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_DMA_SRESET_WIDTH) - 1) << STAT_TOP_STAT_DMA_SRESET_SHIFT);


    t = (x >> STAT_TOP_STAT_TRNG_SRESET_SHIFT) & ((1U << STAT_TOP_STAT_TRNG_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" trng_sreset=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_TRNG_SRESET_WIDTH) - 1) << STAT_TOP_STAT_TRNG_SRESET_SHIFT);


    t = (x >> STAT_TOP_STAT_OTP_SRESET_SHIFT) & ((1U << STAT_TOP_STAT_OTP_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" otp_sreset=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_OTP_SRESET_WIDTH) - 1) << STAT_TOP_STAT_OTP_SRESET_SHIFT);


    t = (x >> STAT_TOP_STAT_ACA_SRESET_SHIFT) & ((1U << STAT_TOP_STAT_ACA_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" aca_sreset=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_ACA_SRESET_WIDTH) - 1) << STAT_TOP_STAT_ACA_SRESET_SHIFT);


    t = (x >> STAT_TOP_STAT_SCA_SRESET_SHIFT) & ((1U << STAT_TOP_STAT_SCA_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" sca_sreset=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_SCA_SRESET_WIDTH) - 1) << STAT_TOP_STAT_SCA_SRESET_SHIFT);


    t = (x >> STAT_TOP_STAT_HASH_SRESET_SHIFT) & ((1U << STAT_TOP_STAT_HASH_SRESET_WIDTH) - 1);
    BITFIELD_LOG(" hash_sreset=0x%x\n", t);
    x &= ~(((1U << STAT_TOP_STAT_HASH_SRESET_WIDTH) - 1) << STAT_TOP_STAT_HASH_SRESET_SHIFT);

    if (x) {
        int i = 0;
        BITFIELD_LOG(" (Unknown :");
        while (x) {
            if (x & 1) {
                BITFIELD_LOG(" %d", i);
            }
            x >>= 1;
            i++;
        }
        BITFIELD_LOG(")\n");
    }

    BITFIELD_LOG("\n");
}

#else  /* WITH_BITFIELD_LOG */

#define log_stat_top_stat(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __STAT_TOP_STAT_REG_H__ */
