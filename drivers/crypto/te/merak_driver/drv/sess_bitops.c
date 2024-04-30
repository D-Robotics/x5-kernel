//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "drv_sess_internal.h"

#ifdef __LP64__
#define SESS_BITS_PER_LONG (64)
#else
#define SESS_BITS_PER_LONG (32)
#endif

#define SESS_MIN(a, b)      (((a)<(b)) ? (a) : (b))
#define SESS_BIT_MASK(nr)   (1UL << ((nr) % SESS_BITS_PER_LONG))
#define SESS_BIT_WORD(nr)   ((nr) / SESS_BITS_PER_LONG)

static inline unsigned long __my_ffs(unsigned long word)
{
    return __builtin_ctzl(word);
}

unsigned long te_sess_find_first_bit(unsigned long *addr, unsigned long size)
{
    unsigned long idx;
    for ( idx = 0; (idx * SESS_BITS_PER_LONG) < size; idx++ ) {
        if ( addr[idx] ) {
            return SESS_MIN( idx * SESS_BITS_PER_LONG +
                             __my_ffs(addr[idx]), size );
        }
    }
    return size;
}

void te_sess_set_bit(int nr, unsigned long *addr)
{
    unsigned long mask = SESS_BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + SESS_BIT_WORD(nr);
    *p  |= mask;
    return;
}

void te_sess_clear_bit(int nr, unsigned long *addr)
{
    unsigned long mask = SESS_BIT_MASK(nr);
    unsigned long *p = ((unsigned long *)addr) + SESS_BIT_WORD(nr);
    *p  &= ~mask;
    return;
}

int te_sess_test_bit(int nr, unsigned long *addr)
{
    return 1UL & (addr[SESS_BIT_WORD(nr)] >> (nr & (SESS_BITS_PER_LONG-1)));
}

