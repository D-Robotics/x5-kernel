//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __UTILS_EXT_H__
#define __UTILS_EXT_H__

#include "osal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UTILS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define UTILS_MAX(a, b) ((a) >= (b) ? (a) : (b))

#define UTILS_ARRAY_SIZE(__x__) (sizeof(__x__) / sizeof(__x__[0]))
#define UTILS_CONTAINER_OF(__ptr__, __Type__, __field__)                        \
    ((__Type__ *)(((char *)(__ptr__)) - offsetof(__Type__, __field__)))

/* round "x" up/down to next multiple of "align" (which must be a power of 2) */
#define UTILS_ROUND_UP(__x__, __align__)                                        \
    (((unsigned long)(__x__) + ((unsigned long)(__align__)-1)) &               \
     ~((unsigned long)(__align__)-1))
#define UTILS_ROUND_DOWN(__x__, __align__)                                      \
    ((unsigned long)(__x__) & ~((unsigned long)(__align__)-1))
/* check if "x" is align with "align"(must be a power of 2) */
#define UTILS_IS_ALIGNED(__x__, __align__)                                      \
    (!(((unsigned long)(__x__)) & ((unsigned long)(__align__)-1)))

#define UTILS_WEAK_IMP __attribute__((weak))
#define UTILS_UNUSED(__arg__) (void)(__arg__)

#define UTILS_CHECK_RET(__fmt__, ...)                                           \
    do {                                                                       \
        if ((OSAL_SUCCESS) != (ret)) {                                         \
            OSAL_LOG_ERR("[FAIL] %s:%d. Ret is 0x%x \n", __func__, __LINE__,   \
                         ret);                                                 \
            OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__);                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define UTILS_CHECK_CONDITION(__true_condition__, __ret_code__, __fmt__, ...)   \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            OSAL_LOG_ERR("[FAIL] %s:%d. Condition is  " #__true_condition__    \
                         "\n",                                                 \
                         __func__, __LINE__);                                  \
            OSAL_LOG_ERR(__fmt__, ##__VA_ARGS__);                              \
            ret = (__ret_code__);                                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define UTILS_CHECK_RET_QUIET                                                   \
    do {                                                                       \
        if ((OSAL_SUCCESS) != (ret)) {                                         \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#define UTILS_CHECK_CONDITION_QUIET(__true_condition__, __ret_code__)           \
    do {                                                                       \
        if (!(__true_condition__)) {                                           \
            ret = (__ret_code__);                                              \
            goto finish;                                                       \
        }                                                                      \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_EXT_H__ */
