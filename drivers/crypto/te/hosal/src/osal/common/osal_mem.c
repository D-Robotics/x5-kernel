//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */
#include "osal_utils.h"
#include "osal_mem.h"
#include "osal_assert.h"
#include "osal_internal.h"

#ifdef CFG_OSAL_MEM_DEBUG
#include <sqlist.h>
typedef struct osal_mem_track {
    sqlist_t     list;
    osal_mem_hdr_t *hdr;
} osal_mem_track_t;

static sqlist_t g_allocated = SQLIST_INIT(&g_allocated);

static void osal_mem_debug_track(osal_mem_hdr_t *hdr)
{
    unsigned long     flags = 0;
    osal_mem_track_t *track = osal_env_alloc(sizeof(osal_mem_track_t));
    OSAL_ASSERT(track != NULL);
    hdr->track = track;
    track->hdr = hdr;
    flags      = osal_mem_debug_lock();
    sqlist_insert_tail(&g_allocated, &track->list);
    osal_mem_debug_unlock(flags);
    return;
}

static void osal_mem_debug_untrack(osal_mem_hdr_t *hdr)
{
    unsigned long     flags = 0;
    osal_mem_track_t *track = hdr->track;

    flags = osal_mem_debug_lock();
    sqlist_remove(&track->list);
    osal_mem_debug_unlock(flags);

    hdr->track = NULL;
    osal_env_free(track);
    return;
}

void osal_mem_usage_report(void)
{
    unsigned long     flags = 0;
    osal_mem_hdr_t *  hdr   = NULL;
    osal_mem_track_t *cur = NULL, *next = NULL;
    size_t            total = 0;
    OSAL_LOG_ERR("OSAL Memory usage report:\n");
    flags = osal_mem_debug_lock();
    SQLIST_FOR_EACH_CONTAINER_SAFE(&g_allocated, cur, next, list)
    {
        hdr = cur->hdr;
        OSAL_LOG_ERR("\tptr(%lx), size(%d), owner(file:%s, line:%d)\n",
                     hdr->ptr, (size_t)hdr->sz, hdr->fname, (int)hdr->lineno);
        total += (size_t)hdr->sz;
    }
    osal_mem_debug_unlock(flags);
    OSAL_LOG_ERR("\ttotal: %d\n", total);
}
#endif /* CFG_OSAL_MEM_DEBUG */

static void *__osal_malloc(size_t size, const char *fname, int lineno)
{
    void *          p   = NULL;
    osal_mem_hdr_t *hdr = NULL;
    size_t          size_align;

    if (size == 0) {
        return NULL;
    }

    size_align = UTILS_ROUND_UP(size, sizeof(uintptr_t));
    p = osal_env_alloc(size_align + sizeof(osal_mem_hdr_t) + sizeof(uint32_t));
    if (!p) {
        return NULL;
    }

    hdr        = (osal_mem_hdr_t *)p;
    p          = (void *)((uintptr_t)p + sizeof(osal_mem_hdr_t));
    hdr->magic = OSAL_MEM_MAGIC;
    hdr->ptr   = (void *)hdr;
    hdr->sz    = size;
#ifdef CFG_OSAL_MEM_DEBUG
    hdr->fname  = fname;
    hdr->lineno = lineno;
    osal_mem_debug_track(hdr);
#else
    (void)(fname);
    (void)(lineno);
#endif

    hdr->canary =
        (uint32_t *)((uintptr_t)hdr + size_align + sizeof(osal_mem_hdr_t));
    *(hdr->canary) = OSAL_MEM_CANARY;

    return p;
}

static void *__osal_calloc(size_t nmemb, size_t size, const char *fname,
                           int lineno)
{
    void *          p     = NULL;
    osal_mem_hdr_t *hdr   = NULL;
    size_t          total = nmemb * size;
    size_t          size_align;

    if (total == 0) {
        return NULL;
    }

    size_align = UTILS_ROUND_UP(total, sizeof(uintptr_t));
    p = osal_env_zalloc(size_align + sizeof(osal_mem_hdr_t) + sizeof(uint32_t));
    if (!p) {
        return NULL;
    }
    memset(p, 0, total + sizeof(osal_mem_hdr_t));

    hdr        = (osal_mem_hdr_t *)p;
    p          = (void *)((uintptr_t)p + sizeof(osal_mem_hdr_t));
    hdr->magic = OSAL_MEM_MAGIC;
    hdr->ptr   = (void *)hdr;
    hdr->sz    = total;
#ifdef CFG_OSAL_MEM_DEBUG
    hdr->fname  = fname;
    hdr->lineno = lineno;
    osal_mem_debug_track(hdr);
#else
    (void)(fname);
    (void)(lineno);
#endif

    hdr->canary =
        (uint32_t *)((uintptr_t)hdr + size_align + sizeof(osal_mem_hdr_t));
    *(hdr->canary) = OSAL_MEM_CANARY;

    return p;
}

static void __osal_free(void *ptr, const char *fname, int lineno)
{
    if (ptr) {
        osal_mem_hdr_t *hdr =
            (osal_mem_hdr_t *)((uintptr_t)ptr - sizeof(osal_mem_hdr_t));

        if (hdr->magic != OSAL_MEM_MAGIC) {
            OSAL_LOG_ERR("osal_free: Invalid magic number(ptr:%p)\n", ptr);
            OSAL_LOG_ERR("osal_free: Invalid magic number(ptr:%p)\n", ptr);
            OSAL_LOG_ERR("Possible cause:\n");
            OSAL_LOG_ERR("\t1. ptr not be allocated by OSAL interface\n");
            OSAL_LOG_ERR("\t2. ptr buffer overwritten by others\n");
#ifdef CFG_OSAL_MEM_DEBUG
            OSAL_LOG_ERR("Magic error, free by: file:%s line:%d\n", fname,
                         lineno);
#endif
            OSAL_ASSERT(hdr->magic == OSAL_MEM_MAGIC);
        }

#ifdef CFG_OSAL_MEM_DEBUG
        osal_mem_debug_untrack(hdr);
#endif
        if (*(hdr->canary) != OSAL_MEM_CANARY) {
            OSAL_LOG_ERR("Memroy overwritten detected\n");
#ifdef CFG_OSAL_MEM_DEBUG
            OSAL_LOG_ERR("allocated by: file:%s line:%d\n", hdr->fname,
                         hdr->lineno);
            OSAL_LOG_ERR("free by: file:%s line:%d\n", fname, lineno);
#else
            (void)(fname);
            (void)(lineno);
#endif
            OSAL_ASSERT((*(hdr->canary)) == OSAL_MEM_CANARY);
        }

        OSAL_ASSERT(hdr->magic == OSAL_MEM_MAGIC);
        OSAL_ASSERT(hdr->ptr != NULL);
        hdr->magic = 0;
        osal_env_free(hdr->ptr);
    }
    return;
}

static void *__osal_realloc(void *ptr, size_t size, const char *fname,
                            int lineno)
{
    void *          p    = NULL;
    uintptr_t       cpsz = 0;
    size_t          size_align;
    osal_mem_hdr_t *hdr =
        (osal_mem_hdr_t *)((uintptr_t)ptr - sizeof(osal_mem_hdr_t));
    osal_mem_hdr_t *new = NULL;

    /* Behave as malloc */
    if (ptr) {
        OSAL_ASSERT(hdr->magic == OSAL_MEM_MAGIC);
        OSAL_ASSERT(hdr->ptr != NULL);
    }

    /* Behave as dealloc */
    if (size == 0) {
        goto out;
    }

    size_align = UTILS_ROUND_UP(size, sizeof(uintptr_t));
    p = osal_env_alloc(size_align + sizeof(osal_mem_hdr_t) + sizeof(uint32_t));
    if (!p) {
        return NULL;
    }

    new        = (osal_mem_hdr_t *)p;
    p          = (void *)((uintptr_t)p + sizeof(osal_mem_hdr_t));
    new->magic = OSAL_MEM_MAGIC;
    new->ptr   = (void *)new;
    new->sz    = size;
#ifdef CFG_OSAL_MEM_DEBUG
    new->fname  = fname;
    new->lineno = lineno;
    osal_mem_debug_track(new);
#else
    (void)(fname);
    (void)(lineno);
#endif

    new->canary =
        (uint32_t *)((uintptr_t) new + size_align + sizeof(osal_mem_hdr_t));
    *(new->canary) = OSAL_MEM_CANARY;

    if (ptr) {
        /* Copy to new buffer */
        cpsz = ((new->sz > hdr->sz) ? hdr->sz : new->sz);
        memcpy(p, ptr, cpsz);
    }

out:
    osal_free(ptr);
    return p;
}

static osal_err_t __osal_safe_realloc(void *ptr, size_t size, void **new_ptr,
                                      const char *fname, int lineno)
{
    void *tmp = NULL;

    tmp = __osal_realloc(ptr, size, fname, lineno);
    if (tmp) {
        *new_ptr = tmp;
        return OSAL_SUCCESS;
    } else {
        return OSAL_ERROR_OUT_OF_MEMORY;
    }
}

static void *__osal_malloc_aligned(size_t size, size_t alignment,
                                   const char *fname, int lineno)
{
    void *          p      = NULL;
    void *          aptr   = NULL;
    osal_mem_hdr_t *hdr    = NULL;
    int             offset = alignment - 1 + sizeof(osal_mem_hdr_t);
    size_t          size_align;

    if (size == 0) {
        return NULL;
    }

    OSAL_ASSERT(!(alignment & (alignment - 1)));
    size_align = UTILS_ROUND_UP(size, sizeof(uintptr_t));
    p          = osal_env_alloc(size_align + offset + sizeof(uint32_t));
    if (!p) {
        return NULL;
    }

    aptr = (void *)(((uintptr_t)p + offset) & ~(alignment - 1));
    hdr  = (osal_mem_hdr_t *)((uintptr_t)aptr - sizeof(osal_mem_hdr_t));

    hdr->magic = OSAL_MEM_MAGIC;
    hdr->ptr   = p;
    hdr->sz    = size;
#ifdef CFG_OSAL_MEM_DEBUG
    hdr->fname  = fname;
    hdr->lineno = lineno;
    osal_mem_debug_track(hdr);
#else
    (void)(fname);
    (void)(lineno);
#endif

    hdr->canary =
        (uint32_t *)((uintptr_t)hdr + size_align + sizeof(osal_mem_hdr_t));
    *(hdr->canary) = OSAL_MEM_CANARY;

    return aptr;
}

#ifdef CFG_OSAL_MEM_DEBUG
void *osal_malloc_debug(size_t size, const char *fname, int lineno)
{
    return __osal_malloc(size, fname, lineno);
}

void *osal_malloc_aligned_debug(size_t size, size_t alignment,
                                const char *fname, int lineno)
{
    return __osal_malloc_aligned(size, alignment, fname, lineno);
}

void osal_free_debug(void *ptr, const char *fname, int lineno)
{
    __osal_free(ptr, fname, lineno);
    return;
}

void *osal_calloc_debug(size_t nmemb, size_t size, const char *fname,
                        int lineno)
{
    return __osal_calloc(nmemb, size, fname, lineno);
}

void *osal_realloc_debug(void *ptr, size_t size, const char *fname, int lineno)
{
    return __osal_realloc(ptr, size, fname, lineno);
}

osal_err_t osal_safe_realloc_debug(void *ptr, size_t size, void **new_ptr,
                                   const char *fname, int lineno)
{
    return __osal_safe_realloc(ptr, size, new_ptr, fname, lineno);
}
#else  /* CFG_OSAL_MEM_DEBUG */
void *osal_malloc(size_t size)
{
    return __osal_malloc(size, NULL, -1);
}

void *osal_malloc_aligned(size_t size, size_t alignment)
{
    return __osal_malloc_aligned(size, alignment, NULL, -1);
}

void osal_free(void *ptr)
{
    __osal_free(ptr, NULL, -1);
    return;
}

void *osal_calloc(size_t nmemb, size_t size)
{
    return __osal_calloc(nmemb, size, NULL, -1);
}

void *osal_realloc(void *ptr, size_t size)
{
    return __osal_realloc(ptr, size, NULL, -1);
}

osal_err_t osal_safe_realloc(void *ptr, size_t size, void **new_ptr)
{
    return __osal_safe_realloc(ptr, size, new_ptr, NULL, -1);
}
#endif /* !CFG_OSAL_MEM_DEBUG */
