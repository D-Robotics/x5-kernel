//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __CTL_CFG_HOST_REG_H__
#define __CTL_CFG_HOST_REG_H__

#define CTL_CFG_HOST_RNP_ATTR_SEC_SHIFT 27 /**< host0 is read only */
#define CTL_CFG_HOST_RNP_ATTR_SEC_WIDTH 1
#define CTL_CFG_HOST_ACA_ATTR_SEC_SHIFT 26 /**< host0 is read only */
#define CTL_CFG_HOST_ACA_ATTR_SEC_WIDTH 1
#define CTL_CFG_HOST_SCA_ATTR_SEC_SHIFT 25 /**< host0 is read only */
#define CTL_CFG_HOST_SCA_ATTR_SEC_WIDTH 1
#define CTL_CFG_HOST_HASH_ATTR_SEC_SHIFT 24 /**< host0 is read only */
#define CTL_CFG_HOST_HASH_ATTR_SEC_WIDTH 1
#define CTL_CFG_HOST_SCA_CTX2_NUM_SHIFT 18
#define CTL_CFG_HOST_SCA_CTX2_NUM_WIDTH 6
#define CTL_CFG_HOST_SCA_CTX1_NUM_SHIFT 12
#define CTL_CFG_HOST_SCA_CTX1_NUM_WIDTH 6
#define CTL_CFG_HOST_HASH_CTX2_NUM_SHIFT 6
#define CTL_CFG_HOST_HASH_CTX2_NUM_WIDTH 6
#define CTL_CFG_HOST_HASH_CTX1_NUM_SHIFT 0
#define CTL_CFG_HOST_HASH_CTX1_NUM_WIDTH 6

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * ctl_cfg_host register definition.
 */
typedef union ctl_cfg_hostReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        hash_ctx1_num: 6,
        hash_ctx2_num: 6,
        sca_ctx1_num: 6,
        sca_ctx2_num: 6,
        hash_attr_sec: 1  /**< host0 is read only */,
        sca_attr_sec: 1  /**< host0 is read only */,
        aca_attr_sec: 1  /**< host0 is read only */,
        rnp_attr_sec: 1  /**< host0 is read only */,
        hole0: 4; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} ctl_cfg_hostReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_ctl_cfg_host(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("ctl_cfg_host: 0x%08x is\n", x);


    t = (x >> CTL_CFG_HOST_RNP_ATTR_SEC_SHIFT) & ((1U << CTL_CFG_HOST_RNP_ATTR_SEC_WIDTH) - 1);
    BITFIELD_LOG(" rnp_attr_sec=0x%x (host0 is read only)\n", t);
    x &= ~(((1U << CTL_CFG_HOST_RNP_ATTR_SEC_WIDTH) - 1) << CTL_CFG_HOST_RNP_ATTR_SEC_SHIFT);


    t = (x >> CTL_CFG_HOST_ACA_ATTR_SEC_SHIFT) & ((1U << CTL_CFG_HOST_ACA_ATTR_SEC_WIDTH) - 1);
    BITFIELD_LOG(" aca_attr_sec=0x%x (host0 is read only)\n", t);
    x &= ~(((1U << CTL_CFG_HOST_ACA_ATTR_SEC_WIDTH) - 1) << CTL_CFG_HOST_ACA_ATTR_SEC_SHIFT);


    t = (x >> CTL_CFG_HOST_SCA_ATTR_SEC_SHIFT) & ((1U << CTL_CFG_HOST_SCA_ATTR_SEC_WIDTH) - 1);
    BITFIELD_LOG(" sca_attr_sec=0x%x (host0 is read only)\n", t);
    x &= ~(((1U << CTL_CFG_HOST_SCA_ATTR_SEC_WIDTH) - 1) << CTL_CFG_HOST_SCA_ATTR_SEC_SHIFT);


    t = (x >> CTL_CFG_HOST_HASH_ATTR_SEC_SHIFT) & ((1U << CTL_CFG_HOST_HASH_ATTR_SEC_WIDTH) - 1);
    BITFIELD_LOG(" hash_attr_sec=0x%x (host0 is read only)\n", t);
    x &= ~(((1U << CTL_CFG_HOST_HASH_ATTR_SEC_WIDTH) - 1) << CTL_CFG_HOST_HASH_ATTR_SEC_SHIFT);


    t = (x >> CTL_CFG_HOST_SCA_CTX2_NUM_SHIFT) & ((1U << CTL_CFG_HOST_SCA_CTX2_NUM_WIDTH) - 1);
    BITFIELD_LOG(" sca_ctx2_num=0x%x\n", t);
    x &= ~(((1U << CTL_CFG_HOST_SCA_CTX2_NUM_WIDTH) - 1) << CTL_CFG_HOST_SCA_CTX2_NUM_SHIFT);


    t = (x >> CTL_CFG_HOST_SCA_CTX1_NUM_SHIFT) & ((1U << CTL_CFG_HOST_SCA_CTX1_NUM_WIDTH) - 1);
    BITFIELD_LOG(" sca_ctx1_num=0x%x\n", t);
    x &= ~(((1U << CTL_CFG_HOST_SCA_CTX1_NUM_WIDTH) - 1) << CTL_CFG_HOST_SCA_CTX1_NUM_SHIFT);


    t = (x >> CTL_CFG_HOST_HASH_CTX2_NUM_SHIFT) & ((1U << CTL_CFG_HOST_HASH_CTX2_NUM_WIDTH) - 1);
    BITFIELD_LOG(" hash_ctx2_num=0x%x\n", t);
    x &= ~(((1U << CTL_CFG_HOST_HASH_CTX2_NUM_WIDTH) - 1) << CTL_CFG_HOST_HASH_CTX2_NUM_SHIFT);


    t = (x >> CTL_CFG_HOST_HASH_CTX1_NUM_SHIFT) & ((1U << CTL_CFG_HOST_HASH_CTX1_NUM_WIDTH) - 1);
    BITFIELD_LOG(" hash_ctx1_num=0x%x\n", t);
    x &= ~(((1U << CTL_CFG_HOST_HASH_CTX1_NUM_WIDTH) - 1) << CTL_CFG_HOST_HASH_CTX1_NUM_SHIFT);

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

#define log_ctl_cfg_host(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __CTL_CFG_HOST_REG_H__ */
