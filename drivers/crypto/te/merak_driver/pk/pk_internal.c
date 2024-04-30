
//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include "pk_internal.h"

void te_pk_lock(const te_drv_handle hdl)
{
    int ret                   = TE_SUCCESS;
    const te_crypt_drv_t *drv = NULL;

    TE_ASSERT(hdl);

    drv = (te_crypt_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(drv);
    ret = te_drv_put(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(TE_SUCCESS == ret);

    ret = te_aca_lock(drv);
    TE_ASSERT(TE_SUCCESS == ret);
}

void te_pk_unlock(const te_drv_handle hdl)
{
    int ret                   = TE_SUCCESS;
    const te_crypt_drv_t *drv = NULL;

    TE_ASSERT(hdl);

    drv = (te_crypt_drv_t *)te_drv_get(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(drv);
    ret = te_drv_put(hdl, TE_DRV_TYPE_ACA);
    TE_ASSERT(TE_SUCCESS == ret);

    ret = te_aca_unlock(drv);
    TE_ASSERT(TE_SUCCESS == ret);
}

int te_pk_submit_req(void *req)
{
    TE_ASSERT(req);

    return te_aca_submit_req(req);
}
