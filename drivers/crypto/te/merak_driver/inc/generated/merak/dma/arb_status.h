//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __DMA_ARB_STATUS_REG_H__
#define __DMA_ARB_STATUS_REG_H__

#define DMA_ARB_STATUS_GRANT_SEL_RDATA_CHN_SHIFT 26
#define DMA_ARB_STATUS_GRANT_SEL_RDATA_CHN_WIDTH 1
#define DMA_ARB_STATUS_HASH_GRANT_DONE_SHIFT 25
#define DMA_ARB_STATUS_HASH_GRANT_DONE_WIDTH 1
#define DMA_ARB_STATUS_SCA_GRANT_DONE_SHIFT 24
#define DMA_ARB_STATUS_SCA_GRANT_DONE_WIDTH 1
#define DMA_ARB_STATUS_HASH_RDMA_GRANTED_SHIFT 23
#define DMA_ARB_STATUS_HASH_RDMA_GRANTED_WIDTH 1
#define DMA_ARB_STATUS_SCA_RDMA_GRANTED_SHIFT 22
#define DMA_ARB_STATUS_SCA_RDMA_GRANTED_WIDTH 1
#define DMA_ARB_STATUS_HASH_RDMA_RDATA_RREQ_SHIFT 21
#define DMA_ARB_STATUS_HASH_RDMA_RDATA_RREQ_WIDTH 1
#define DMA_ARB_STATUS_HASH_WDMA_LL_INFO_RREQ_SHIFT 20
#define DMA_ARB_STATUS_HASH_WDMA_LL_INFO_RREQ_WIDTH 1
#define DMA_ARB_STATUS_HASH_RDMA_LL_INFO_RREQ_SHIFT 19
#define DMA_ARB_STATUS_HASH_RDMA_LL_INFO_RREQ_WIDTH 1
#define DMA_ARB_STATUS_SCA_RDMA_RDATA_RREQ_SHIFT 18
#define DMA_ARB_STATUS_SCA_RDMA_RDATA_RREQ_WIDTH 1
#define DMA_ARB_STATUS_SCA_WDMA_LL_INFO_RREQ_SHIFT 17
#define DMA_ARB_STATUS_SCA_WDMA_LL_INFO_RREQ_WIDTH 1
#define DMA_ARB_STATUS_SCA_RDMA_LL_INFO_RREQ_SHIFT 16
#define DMA_ARB_STATUS_SCA_RDMA_LL_INFO_RREQ_WIDTH 1
#define DMA_ARB_STATUS_GRANT_SEL_BRESP_SHIFT 6
#define DMA_ARB_STATUS_GRANT_SEL_BRESP_WIDTH 1
#define DMA_ARB_STATUS_HASH_WDATA_PENDING_SHIFT 5
#define DMA_ARB_STATUS_HASH_WDATA_PENDING_WIDTH 1
#define DMA_ARB_STATUS_HASH_WRREQ_PENDING_SHIFT 4
#define DMA_ARB_STATUS_HASH_WRREQ_PENDING_WIDTH 1
#define DMA_ARB_STATUS_HASH_WDMA_GRANTED_SHIFT 3
#define DMA_ARB_STATUS_HASH_WDMA_GRANTED_WIDTH 1
#define DMA_ARB_STATUS_SCA_WDATA_PENDING_SHIFT 2
#define DMA_ARB_STATUS_SCA_WDATA_PENDING_WIDTH 1
#define DMA_ARB_STATUS_SCA_WRREQ_PENDING_SHIFT 1
#define DMA_ARB_STATUS_SCA_WRREQ_PENDING_WIDTH 1
#define DMA_ARB_STATUS_SCA_WDMA_GRANTED_SHIFT 0
#define DMA_ARB_STATUS_SCA_WDMA_GRANTED_WIDTH 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * dma_arb_status register definition.
 */
typedef union dma_arb_statusReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        sca_wdma_granted: 1,
        sca_wrreq_pending: 1,
        sca_wdata_pending: 1,
        hash_wdma_granted: 1,
        hash_wrreq_pending: 1,
        hash_wdata_pending: 1,
        grant_sel_bresp: 1,
        hole16: 9,
        sca_rdma_ll_info_rreq: 1,
        sca_wdma_ll_info_rreq: 1,
        sca_rdma_rdata_rreq: 1,
        hash_rdma_ll_info_rreq: 1,
        hash_wdma_ll_info_rreq: 1,
        hash_rdma_rdata_rreq: 1,
        sca_rdma_granted: 1,
        hash_rdma_granted: 1,
        sca_grant_done: 1,
        hash_grant_done: 1,
        grant_sel_rdata_chn: 1,
        hole0: 5; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} dma_arb_statusReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_dma_arb_status(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("dma_arb_status: 0x%08x is\n", x);


    t = (x >> DMA_ARB_STATUS_GRANT_SEL_RDATA_CHN_SHIFT) & ((1U << DMA_ARB_STATUS_GRANT_SEL_RDATA_CHN_WIDTH) - 1);
    BITFIELD_LOG(" grant_sel_rdata_chn=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_GRANT_SEL_RDATA_CHN_WIDTH) - 1) << DMA_ARB_STATUS_GRANT_SEL_RDATA_CHN_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_GRANT_DONE_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_GRANT_DONE_WIDTH) - 1);
    BITFIELD_LOG(" hash_grant_done=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_GRANT_DONE_WIDTH) - 1) << DMA_ARB_STATUS_HASH_GRANT_DONE_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_GRANT_DONE_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_GRANT_DONE_WIDTH) - 1);
    BITFIELD_LOG(" sca_grant_done=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_GRANT_DONE_WIDTH) - 1) << DMA_ARB_STATUS_SCA_GRANT_DONE_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_RDMA_GRANTED_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_RDMA_GRANTED_WIDTH) - 1);
    BITFIELD_LOG(" hash_rdma_granted=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_RDMA_GRANTED_WIDTH) - 1) << DMA_ARB_STATUS_HASH_RDMA_GRANTED_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_RDMA_GRANTED_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_RDMA_GRANTED_WIDTH) - 1);
    BITFIELD_LOG(" sca_rdma_granted=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_RDMA_GRANTED_WIDTH) - 1) << DMA_ARB_STATUS_SCA_RDMA_GRANTED_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_RDMA_RDATA_RREQ_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_RDMA_RDATA_RREQ_WIDTH) - 1);
    BITFIELD_LOG(" hash_rdma_rdata_rreq=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_RDMA_RDATA_RREQ_WIDTH) - 1) << DMA_ARB_STATUS_HASH_RDMA_RDATA_RREQ_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_WDMA_LL_INFO_RREQ_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_WDMA_LL_INFO_RREQ_WIDTH) - 1);
    BITFIELD_LOG(" hash_wdma_ll_info_rreq=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_WDMA_LL_INFO_RREQ_WIDTH) - 1) << DMA_ARB_STATUS_HASH_WDMA_LL_INFO_RREQ_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_RDMA_LL_INFO_RREQ_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_RDMA_LL_INFO_RREQ_WIDTH) - 1);
    BITFIELD_LOG(" hash_rdma_ll_info_rreq=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_RDMA_LL_INFO_RREQ_WIDTH) - 1) << DMA_ARB_STATUS_HASH_RDMA_LL_INFO_RREQ_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_RDMA_RDATA_RREQ_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_RDMA_RDATA_RREQ_WIDTH) - 1);
    BITFIELD_LOG(" sca_rdma_rdata_rreq=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_RDMA_RDATA_RREQ_WIDTH) - 1) << DMA_ARB_STATUS_SCA_RDMA_RDATA_RREQ_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_WDMA_LL_INFO_RREQ_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_WDMA_LL_INFO_RREQ_WIDTH) - 1);
    BITFIELD_LOG(" sca_wdma_ll_info_rreq=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_WDMA_LL_INFO_RREQ_WIDTH) - 1) << DMA_ARB_STATUS_SCA_WDMA_LL_INFO_RREQ_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_RDMA_LL_INFO_RREQ_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_RDMA_LL_INFO_RREQ_WIDTH) - 1);
    BITFIELD_LOG(" sca_rdma_ll_info_rreq=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_RDMA_LL_INFO_RREQ_WIDTH) - 1) << DMA_ARB_STATUS_SCA_RDMA_LL_INFO_RREQ_SHIFT);


    t = (x >> DMA_ARB_STATUS_GRANT_SEL_BRESP_SHIFT) & ((1U << DMA_ARB_STATUS_GRANT_SEL_BRESP_WIDTH) - 1);
    BITFIELD_LOG(" grant_sel_bresp=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_GRANT_SEL_BRESP_WIDTH) - 1) << DMA_ARB_STATUS_GRANT_SEL_BRESP_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_WDATA_PENDING_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_WDATA_PENDING_WIDTH) - 1);
    BITFIELD_LOG(" hash_wdata_pending=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_WDATA_PENDING_WIDTH) - 1) << DMA_ARB_STATUS_HASH_WDATA_PENDING_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_WRREQ_PENDING_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_WRREQ_PENDING_WIDTH) - 1);
    BITFIELD_LOG(" hash_wrreq_pending=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_WRREQ_PENDING_WIDTH) - 1) << DMA_ARB_STATUS_HASH_WRREQ_PENDING_SHIFT);


    t = (x >> DMA_ARB_STATUS_HASH_WDMA_GRANTED_SHIFT) & ((1U << DMA_ARB_STATUS_HASH_WDMA_GRANTED_WIDTH) - 1);
    BITFIELD_LOG(" hash_wdma_granted=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_HASH_WDMA_GRANTED_WIDTH) - 1) << DMA_ARB_STATUS_HASH_WDMA_GRANTED_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_WDATA_PENDING_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_WDATA_PENDING_WIDTH) - 1);
    BITFIELD_LOG(" sca_wdata_pending=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_WDATA_PENDING_WIDTH) - 1) << DMA_ARB_STATUS_SCA_WDATA_PENDING_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_WRREQ_PENDING_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_WRREQ_PENDING_WIDTH) - 1);
    BITFIELD_LOG(" sca_wrreq_pending=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_WRREQ_PENDING_WIDTH) - 1) << DMA_ARB_STATUS_SCA_WRREQ_PENDING_SHIFT);


    t = (x >> DMA_ARB_STATUS_SCA_WDMA_GRANTED_SHIFT) & ((1U << DMA_ARB_STATUS_SCA_WDMA_GRANTED_WIDTH) - 1);
    BITFIELD_LOG(" sca_wdma_granted=0x%x\n", t);
    x &= ~(((1U << DMA_ARB_STATUS_SCA_WDMA_GRANTED_WIDTH) - 1) << DMA_ARB_STATUS_SCA_WDMA_GRANTED_SHIFT);

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

#define log_dma_arb_status(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_ARB_STATUS_REG_H__ */
