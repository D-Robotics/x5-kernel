//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_LOG_H__
#define __OSAL_LOG_H__

#include "osal_err.h"
#include "osal_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the osal LOG API */
OSAL_API int32_t osal_log_get_level(void);
OSAL_API void osal_log_set_level(int32_t level);
OSAL_API const char *osal_log_get_prefix(void);
OSAL_API void osal_log_set_prefix(const char *prefix);
OSAL_API osal_err_t osal_log_init(void *arg);

#if 0
/* MUST support %s */
OSAL_API int32_t osal_log_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
#else
OSAL_API int32_t osal_log_printf(const char *fmt, ...);
#endif
OSAL_API void osal_log_dump_data(const char *str,
                                 const uint8_t *data,
                                 size_t size);
OSAL_API void osal_log_dump_data_less(const char *str,
                                      const uint8_t *data,
                                      size_t size);
/* OSAL log levels */
#define OSAL_LOG_LEVEL_NULL (-1)
#define OSAL_LOG_LEVEL_ERR (0)
#define OSAL_LOG_LEVEL_WARN (1)
#define OSAL_LOG_LEVEL_INFO (2)
#define OSAL_LOG_LEVEL_DEBUG (3)
#define OSAL_LOG_LEVEL_TRACE (4)

#ifndef OSAL_MAX_LOG_LEVEL
#warning "OSAL_MAX_LOG_LEVEL is not defined, use default: TRACE(4)"
#define OSAL_MAX_LOG_LEVEL 4
#endif /*OSAL_MAX_LOG_LEVEL*/

/* OSAL_LOG_PREFIX_NAME overwrite the osal_log_get_prefix() return */
#ifdef OSAL_LOG_PREFIX_NAME
#define _LOG_PREFIX OSAL_LOG_PREFIX_NAME
#else
#define _LOG_PREFIX osal_log_get_prefix()
#endif

/* Evaluate OSAL_MAX_LOG_LEVEL in case provided by caller */
#define __LOG_LEVEL_EVAL(level) level
#define _MAX_LOG_LEVEL __LOG_LEVEL_EVAL(OSAL_MAX_LOG_LEVEL)

/* Filter logging based on logMask and dispatch to platform specific logging
 * mechanism */
#define __LOG(level, __fmt__, ...)                                             \
    do {                                                                       \
        if (osal_log_get_level() >= (OSAL_LOG_LEVEL_##level)) {                \
            osal_log_printf("%s " __fmt__,_LOG_PREFIX, ##__VA_ARGS__);         \
        }                                                                      \
    } while (0)

#define OSAL_LOG_RAW(__LOG_LEVEL__, __fmt__, ...)                              \
    do {                                                                       \
        if (osal_log_get_level() >= (__LOG_LEVEL__)) {                         \
            osal_log_printf(__fmt__, ##__VA_ARGS__);                           \
        }                                                                      \
    } while (0)

#define OSAL_HEX_DUMP(__LOG_LEVEL__, __msg__, __data__, __size__)              \
    do {                                                                       \
        if (osal_log_get_level() >= (__LOG_LEVEL__)) {                         \
            osal_log_dump_data((const char *)(__msg__),                        \
                               (const uint8_t *)(__data__),                    \
                               (size_t)(__size__));                            \
        }                                                                      \
    } while (0)

#define OSAL_HEX_DUMP_LESS(__LOG_LEVEL__, __msg__, __data__, __size__)         \
    do {                                                                       \
        if (osal_log_get_level() >= (__LOG_LEVEL__)) {                         \
            osal_log_dump_data_less((const char *)(__msg__),                   \
                                    (const uint8_t *)(__data__),               \
                                    (size_t)(__size__));                       \
        }                                                                      \
    } while (0)

#if (_MAX_LOG_LEVEL >= OSAL_LOG_LEVEL_ERR)
#define OSAL_LOG_ERR(__fmt__, ...) __LOG(ERR, __fmt__, ##__VA_ARGS__)
#else
#define OSAL_LOG_ERR(__fmt__, ...)
#endif

#if (_MAX_LOG_LEVEL >= OSAL_LOG_LEVEL_WARN)
#define OSAL_LOG_WARN(__fmt__, ...) __LOG(WARN, __fmt__, ##__VA_ARGS__)
#else
#define OSAL_LOG_WARN(__fmt__, ...)
#endif

#if (_MAX_LOG_LEVEL >= OSAL_LOG_LEVEL_INFO)
#define OSAL_LOG_INFO(__fmt__, ...) __LOG(INFO, __fmt__, ##__VA_ARGS__)
#define OSAL_LOG_INFO_DUMP_DATA(__msg__, __data__, __size__)                   \
    OSAL_HEX_DUMP(OSAL_LOG_LEVEL_INFO, __msg__, __data__, __size__)
#define OSAL_LOG_INFO_DUMP_DATA_LESS(__msg__, __data__, __size__)              \
    OSAL_HEX_DUMP_LESS(OSAL_LOG_LEVEL_INFO, __msg__, __data__, __size__)
#else
#define OSAL_LOG_INFO(__fmt__, ...)
#define OSAL_LOG_INFO_DUMP_DATA(__msg__, __data__, __size__)
#define OSAL_LOG_INFO_DUMP_DATA_LESS(__msg__, __data__, __size__)
#endif

#if (_MAX_LOG_LEVEL >= OSAL_LOG_LEVEL_DEBUG)
#define OSAL_LOG_DEBUG(__fmt__, ...) __LOG(DEBUG, __fmt__, ##__VA_ARGS__)
#define OSAL_LOG_DEBUG_DUMP_DATA(__msg__, __data__, __size__)                  \
    OSAL_HEX_DUMP(OSAL_LOG_LEVEL_DEBUG, __msg__, __data__, __size__)
#define OSAL_LOG_DEBUG_DUMP_DATA_LESS(__msg__, __data__, __size__)             \
    OSAL_HEX_DUMP_LESS(OSAL_LOG_LEVEL_DEBUG, __msg__, __data__, __size__)
#else
#define OSAL_LOG_DEBUG(__fmt__, ...)
#define OSAL_LOG_DEBUG_DUMP_DATA(__msg__, __data__, __size__)
#define OSAL_LOG_DEBUG_DUMP_DATA_LESS(__msg__, __data__, __size__)
#endif

#if (_MAX_LOG_LEVEL >= OSAL_LOG_LEVEL_TRACE)
#define OSAL_LOG_TRACE(__fmt__, ...) __LOG(TRACE, __fmt__, ##__VA_ARGS__)
#else
#define OSAL_LOG_TRACE(__fmt__, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_LOG_H__ */
