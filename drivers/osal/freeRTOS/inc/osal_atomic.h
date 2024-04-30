/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_ATOMIC_H__
#define __OSAL_ATOMIC_H__

#include "osal_common_atomic.h"

static inline void osal_atomic_set(osal_atomic_t *p, int32_t v)
{
    osal_cmn_atomic_set(p, v);
}

static inline void osal_atomic_inc(osal_atomic_t *p)
{
    osal_cmn_atomic_inc (p);
}

static inline void osal_atomic_dec(osal_atomic_t *p)
{
    osal_cmn_atomic_dec (p);
}

static inline void osal_atomic_add(int32_t value, osal_atomic_t *p)
{
    osal_cmn_atomic_add (p, value);
}

static inline int32_t osal_atomic_read(osal_atomic_t *p)
{
    return osal_cmn_atomic_read(p);
}

static inline int32_t osal_atomic_dec_return(osal_atomic_t *p)
{
    return osal_cmn_atomic_dec_return(p);
}

static inline int32_t osal_atomic_inc_return(osal_atomic_t *p)
{
    return osal_cmn_atomic_inc_return(p);
}

#endif //__OSAL_ATOMIC_H__
