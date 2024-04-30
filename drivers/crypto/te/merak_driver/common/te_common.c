//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "te_common.h"

int te_com_set_nsec_attr(uint64_t phy_mem, uint32_t len,
                             uint32_t *attr, uint32_t ns_bit)
{
    int ret = TE_SUCCESS;
    osal_mem_attr_t memattr = 0;

    if (phy_mem != 0ULL) {
        ret = osal_get_mem_attr(phy_mem, len, &memattr);
        if (ret != OSAL_SUCCESS) {
            ret = TE_ERROR_GENERIC;
            goto fin;
        }
        if (OSAL_MEM_ATTR_UNKNOWN == memattr) {
            ret = TE_ERROR_GENERIC;
            goto fin;
        } else if (OSAL_MEM_ATTR_NONSECURE == memattr) {
            *attr |= (0x1U << ns_bit);
        } else {
        }
    }
fin:
    return ret;
}

int te_com_set_llst_nsec_attr(link_list_t *list)
{
    int ret = TE_SUCCESS;
    osal_mem_attr_t nsec_attr = 0;
    link_list_t *local_list = list;

    if (NULL == list) {
        return TE_ERROR_BAD_PARAMS;
    }

    while (local_list) {
        /* break when reach last entry */
        if (0x0000000000000000ULL == local_list->addr &&
            0xFFFFFFFFFFFFFFFFULL == local_list->val) {
            break;
        }

        /* set nsec attribute, take case of the len, for linklist of Merak
           '0' denotes '1' ... 'n' denotes 'n + 1` */
        ret = osal_get_mem_attr(local_list->addr,
                                (uint64_t)(local_list->sz_attr.sz + 1ULL),
                                &nsec_attr);
        if ((ret != OSAL_SUCCESS) ||
            (OSAL_MEM_ATTR_UNKNOWN == nsec_attr)) {
            return TE_ERROR_GENERIC;
        }
        if (OSAL_MEM_ATTR_NONSECURE == nsec_attr) {
            local_list->sz_attr.nsec_attr = 1;
        } else {
            local_list->sz_attr.nsec_attr = 0;
        }

        /* set sz_attr endian */
        local_list->val = HTOLE64(local_list->val);
        local_list->addr = HTOLE64(local_list->addr);
        local_list++;
    }

    return TE_SUCCESS;
}