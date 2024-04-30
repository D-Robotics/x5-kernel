//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_MEM_H__
#define __OSAL_MEM_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define  OSAL_MEM_MAGIC     0x4d4c534fU     /**< "OSLM" */
#define  OSAL_MEM_CANARY    0x59524e43U     /**< "CNRY" */

struct osal_mem_track;
/* NOTE: keep this struct size is multiple of sizeof(void *) */
typedef struct osal_mem_hdr {
    uintptr_t magic;
    void *ptr;
    uintptr_t sz;
    uint32_t *canary;
} osal_mem_hdr_t;


/* make all memory operation APIs inline */
OSAL_API void *osal_malloc(size_t size);
/*
 * osal_malloc_aligned behaved as posix_memalign().
 * The value of alignment shall be a power of two multiple of sizeof(void *)
 */
OSAL_API void *osal_malloc_aligned(size_t size, size_t alignment);
OSAL_API void osal_free(void *ptr);
OSAL_API void *osal_calloc(size_t nmemb, size_t size);
OSAL_API void *osal_realloc(void *ptr, size_t size);

/* The osal safe realloc function, new potiner returns to *new_ptr */
OSAL_API osal_err_t osal_safe_realloc(void *ptr, size_t size, void **new_ptr);


/* The safe free macro */
#define OSAL_SAFE_FREE(__ptr__)                                                \
    do {                                                                       \
        if (__ptr__) {                                                         \
            osal_free((void *)(__ptr__));                                      \
            __ptr__ = NULL;                                                    \
        }                                                                      \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_MEM_H__ */
