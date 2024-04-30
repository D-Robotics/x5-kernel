//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_H__
#define __TRUSTENGINE_HWA_H__

#include "te_hwa_common.h"
#include "te_hwa_stat.h"
#include "te_hwa_otp.h"
#include "te_hwa_aca.h"
#include "te_hwa_sca.h"
#include "te_hwa_hash.h"
#include "te_hwa_trng.h"
#include "te_hwa_ctl.h"
#include "te_hwa_trngctl.h"
#include "te_hwa_otpctl.h"
#include "te_hwa_dbgctl.h"
#include "te_hwa_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine IRQ type
 */
typedef enum te_irq_type {
    TE_IRQ_TYPE_HASH = (1 << 0),
    TE_IRQ_TYPE_SCA  = (1 << 1),
    TE_IRQ_TYPE_ACA  = (1 << 2),
    TE_IRQ_TYPE_RNP  = (1 << 3),
    TE_IRQ_TYPE_TRNG = (1 << 4),         /**< host#0 only */
    TE_IRQ_TYPE_CTX_POOL_ERR = (1 << 5), /**< host#0 only */
} te_irq_type_t;

/**
 * Trust engine IRQ notifier function
 */
typedef int (*te_irq_notifier_t)(const uint32_t type, void *uparam);

/**
 * IRQ notifier block handle
 */
typedef struct __te_irq_nb_handle *te_irq_nb_handle;

/**
 * Trust engine host magic number
 */
#define TE_HOST_MAGIC 0x74536854U /**< "ThSt" */

/**
 * Trust engine host hwa structure
 */
typedef struct te_hwa_host {
    /**
     * Common parts
     */
    te_hwa_crypt_t base;         /**< base class, must be the 1st */
    uint32_t magic;              /**< host magic number */

    /**
     * General HWAs
     */
    te_hwa_stat_t stat;
    te_hwa_otp_t otp;
    te_hwa_aca_t aca;
    te_hwa_sca_t sca;
    te_hwa_hash_t hash;
    te_hwa_trng_t trng;

    /**
     * Host#0 only HWAs
     */
    te_hwa_ctl_t *ctl;
    te_hwa_trngctl_t *trngctl;
    te_hwa_otpctl_t *otpctl;
    te_hwa_dbgctl_t *dbgctl;
    te_hwa_dma_t *dma;
} te_hwa_host_t;

int te_hwa_alloc( te_hwa_host_t **hwa, void* base, int irq, int n );
int te_hwa_free( te_hwa_host_t *hwa );
int te_hwa_init( te_hwa_host_t *hwa, void* base, int irq, int n );
int te_hwa_exit( te_hwa_host_t *hwa );

/**
 * Identifier of this host
 */
int te_hwa_host_id( te_hwa_host_t *hwa );

/**
 * Configuration of this host
 */
int te_hwa_host_conf( te_hwa_host_t *hwa, te_host_conf_t *conf );

/**
 * State of this host
 */
int te_hwa_host_state( te_hwa_host_t *hwa, te_host_int_t *state );

int te_hwa_register_notifier( te_hwa_host_t *hwa,
                              const uint32_t irq_type,
                              te_irq_notifier_t fn,
                              void *uparam,
                              te_irq_nb_handle *h );

int te_hwa_unregister_notifier( te_hwa_host_t *hwa,
                                te_irq_nb_handle h );

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_H__ */
