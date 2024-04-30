//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_ASSERT_H__
#define __OSAL_ASSERT_H__

#include "osal_common.h"
#include "osal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSAL_ASSERT_ENABLE 1

OSAL_API void osal_assert_enter(void);
#ifndef OSAL_ASSERT_BREAK
#define OSAL_ASSERT_BREAK                                                      \
    do {                                                                       \
        osal_assert_enter();                                                   \
    } while (0)
#endif /* OSAL_ASSERT_BREAK */

#ifdef OSAL_ASSERT_ENABLE

#define OSAL_ASSERT(__expr__)                                                  \
    do {                                                                       \
        if (!(__expr__)) {                                                     \
            OSAL_LOG_ERR("ASSERT FAILURE:\n");                                 \
            OSAL_LOG_ERR(#__expr__);                                           \
            OSAL_LOG_ERR("[ASSERT] %s (%d): %s\n", __func__, __LINE__,         \
                         __FUNCTION__);                                        \
            OSAL_ASSERT_BREAK;                                                 \
        }                                                                      \
    } while (0)

#define OSAL_ASSERT_MSG(__expr__, __fmt__, ...)                                \
    do {                                                                       \
        if (!(__expr__)) {                                                     \
            OSAL_LOG_ERR("ASSERT FAILURE:\n");                                 \
            OSAL_LOG_ERR(#__expr__);                                           \
            OSAL_LOG_ERR("[ASSERT] %s (%d): %s\n", __func__, __LINE__,         \
                         __FUNCTION__);                                        \
            OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__);                              \
            OSAL_ASSERT_BREAK;                                                 \
        }                                                                      \
    } while (0)

#else /* OSAL_ASSERT_ENABLE */

#define OSAL_ASSERT(__expr__)                                                  \
    do {                                                                       \
    } while (0)
#define OSAL_ASSERT_MSG(__expr__, __fmt__, ...)                                \
    do {                                                                       \
    } while (0)

#endif /* OSAL_ASSERT_ENABLE */

#ifndef OSAL_COMPILE_ASSERT
#define OSAL_COMPILE_ASSERT(__EXPR__)                                          \
    __attribute__(                                                             \
        (unused)) typedef char __build_assert_failure[(__EXPR__) ? 1 : -1]
#endif /* OSAL_COMPILE_ASSERT */

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_ASSERT_H__ */
