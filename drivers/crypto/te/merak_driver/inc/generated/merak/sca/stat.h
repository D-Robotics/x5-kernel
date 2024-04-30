//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __SCA_STAT_REG_H__
#define __SCA_STAT_REG_H__

#define SCA_STAT_CQ_AVAIL_SLOTS_SHIFT 11 /**< number of available slots in cq */
#define SCA_STAT_CQ_AVAIL_SLOTS_WIDTH 5
#define SCA_STAT_CSQ_OCPD_SLOTS_SHIFT 7 /**< number of occupied slots in csq */
#define SCA_STAT_CSQ_OCPD_SLOTS_WIDTH 4
#define SCA_STAT_ACTV_SLOT_ID_SHIFT 2 /**< valid only when host_stat is ACTIVE */
#define SCA_STAT_ACTV_SLOT_ID_WIDTH 5
#define SCA_STAT_HOST_STAT_SHIFT 0 /**< RESET:0,INACTIVE:1,ACTIVE:2,SUSPEND:3 */
#define SCA_STAT_HOST_STAT_WIDTH 2

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * sca_stat register definition.
 */
typedef union sca_statReg {
    /**
     * bit assignments
     */
    struct { __extension__ uint32_t /**< lsbs... */
        host_stat: 2  /**< RESET:0,INACTIVE:1,ACTIVE:2,SUSPEND:3 */,
        actv_slot_id: 5  /**< valid only when host_stat is ACTIVE */,
        csq_ocpd_slots: 4  /**< number of occupied slots in csq */,
        cq_avail_slots: 5  /**< number of available slots in cq */,
        hole0: 16; /**< ...to msbs */
    } bits;

    /**
     * value
     */
    uint32_t val;
} sca_statReg_t;

#ifdef WITH_BITFIELD_LOG

#ifndef BITFIELD_LOG
#error "BITFIELD_LOG is not defined"
#endif /* !BITFIELD_LOG */

__attribute__((unused)) static void log_sca_stat(uint32_t x)
{
    /**
     * Field by field, extract it, print it, and blank it in x.
     * If x is not empty in the end, it means undocumented bits are set.
     * Then print the indices.
     */
    __attribute__((unused)) uint32_t t;
    BITFIELD_LOG("sca_stat: 0x%08x is\n", x);


    t = (x >> SCA_STAT_CQ_AVAIL_SLOTS_SHIFT) & ((1U << SCA_STAT_CQ_AVAIL_SLOTS_WIDTH) - 1);
    BITFIELD_LOG(" cq_avail_slots=0x%x (number of available slots in cq)\n", t);
    x &= ~(((1U << SCA_STAT_CQ_AVAIL_SLOTS_WIDTH) - 1) << SCA_STAT_CQ_AVAIL_SLOTS_SHIFT);


    t = (x >> SCA_STAT_CSQ_OCPD_SLOTS_SHIFT) & ((1U << SCA_STAT_CSQ_OCPD_SLOTS_WIDTH) - 1);
    BITFIELD_LOG(" csq_ocpd_slots=0x%x (number of occupied slots in csq)\n", t);
    x &= ~(((1U << SCA_STAT_CSQ_OCPD_SLOTS_WIDTH) - 1) << SCA_STAT_CSQ_OCPD_SLOTS_SHIFT);


    t = (x >> SCA_STAT_ACTV_SLOT_ID_SHIFT) & ((1U << SCA_STAT_ACTV_SLOT_ID_WIDTH) - 1);
    BITFIELD_LOG(" actv_slot_id=0x%x (valid only when host_stat is ACTIVE)\n", t);
    x &= ~(((1U << SCA_STAT_ACTV_SLOT_ID_WIDTH) - 1) << SCA_STAT_ACTV_SLOT_ID_SHIFT);


    t = (x >> SCA_STAT_HOST_STAT_SHIFT) & ((1U << SCA_STAT_HOST_STAT_WIDTH) - 1);
    BITFIELD_LOG(" host_stat=0x%x (RESET:0,INACTIVE:1,ACTIVE:2,SUSPEND:3)\n", t);
    x &= ~(((1U << SCA_STAT_HOST_STAT_WIDTH) - 1) << SCA_STAT_HOST_STAT_SHIFT);

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

#define log_sca_stat(x) do{}while(0)

#endif /* !WITH_BITFIELD_LOG */

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __SCA_STAT_REG_H__ */
