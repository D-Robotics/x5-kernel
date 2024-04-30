//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "osal_log.h"
#include "osal_utils.h"

#if defined(OSAL_ENV_LINUX_KERNEL)
#define LOG_DEFAULT_PREFIX "Linux Kernel"
#elif defined(OSAL_ENV_LINUX_USER)
#define LOG_DEFAULT_PREFIX "User Space"
#elif defined(OSAL_ENV_OPTEE_TA)
#define LOG_DEFAULT_PREFIX "OPTEE TA"
#elif defined(OSAL_ENV_OPTEE_OS)
#define LOG_DEFAULT_PREFIX "OPTEE OS"
#elif defined(OSAL_ENV_UBL)
#define LOG_DEFAULT_PREFIX "UBL"
#elif defined(OSAL_ENV_UBOOT)
#define LOG_DEFAULT_PREFIX "UBOOT"
#elif defined(OSAL_ENV_LK)
#define LOG_DEFAULT_PREFIX "LK"
#else
#error "Bad OSAL_ENV!"
#endif

#define LOG_PREFIX_MAX_SIZE (64)
static int _g_log_level                            = 4;
static char _g_log_prefix_buf[LOG_PREFIX_MAX_SIZE] = LOG_DEFAULT_PREFIX;

OSAL_API int32_t osal_log_get_level(void)
{
    return _g_log_level;
}

OSAL_API void osal_log_set_level(int32_t level)
{
    _g_log_level = level;
}

const char *osal_log_get_prefix(void)
{
    return (const char *)(_g_log_prefix_buf);
}

void osal_log_set_prefix(const char *prefix)
{
    size_t i     = 0;
    bool str_end = false;

    if (!prefix) {
        return;
    }

    for (i = 0; i < LOG_PREFIX_MAX_SIZE; i++) {
        if (!prefix[i]) {
            str_end = true;
        }
        if (str_end) {
            _g_log_prefix_buf[i] = 0;
        } else {
            _g_log_prefix_buf[i] = prefix[i];
        }
    }
    /* Set string ender */
    _g_log_prefix_buf[LOG_PREFIX_MAX_SIZE - 1] = 0;
}

#define _DUMP_PRINT_STR(__str__) osal_log_printf("%s", __str__)
#define _INT_TO_CHAR(__d__)                                                    \
    (((__d__) >= (0x0A)) ? ((__d__) - (0x0A) + 'A') : ((__d__) + '0'))
#define _BYTES_PER_LINE (16)
#define _HEAD_TAIL_LINES (2)
#define _STRING_LEN_PER_LINE (8 + 2 + _BYTES_PER_LINE * 4 + 4)
#define _LINE_NUM_TO_STR(__line_num__, __p__)                                  \
    do {                                                                       \
        (__p__)[0] = (_INT_TO_CHAR(((__line_num__) >> 28) & 0xF));             \
        (__p__)[1] = (_INT_TO_CHAR(((__line_num__) >> 24) & 0xF));             \
        (__p__)[2] = (_INT_TO_CHAR(((__line_num__) >> 20) & 0xF));             \
        (__p__)[3] = (_INT_TO_CHAR(((__line_num__) >> 16) & 0xF));             \
        (__p__)[4] = (_INT_TO_CHAR(((__line_num__) >> 12) & 0xF));             \
        (__p__)[5] = (_INT_TO_CHAR(((__line_num__) >> 8) & 0xF));              \
        (__p__)[6] = (_INT_TO_CHAR(((__line_num__) >> 4) & 0xF));              \
        (__p__)[7] = (_INT_TO_CHAR(((__line_num__) >> 0) & 0xF));              \
        (__p__) += 8;                                                          \
    } while (0)

#define _BYTE_DATA_TO_STR(__data__, __p__)                                     \
    do {                                                                       \
        (__p__)[0] = (_INT_TO_CHAR(((__data__) >> 4) & 0xF));                  \
        (__p__)[1] = (_INT_TO_CHAR(((__data__) >> 0) & 0xF));                  \
        (__p__) += 2;                                                          \
    } while (0)

#define _PRINT_BYTE(__data__, __p__)                                           \
    do {                                                                       \
        (__p__)[0] =                                                           \
            ((((__data__) >= 0x20) && ((__data__) < 0x7F)) ? (__data__)        \
                                                           : ('.'));           \
        (__p__)++;                                                             \
    } while (0)

#define __GET_DATA_IDX(__i__, __j__) (((__i__)*_BYTES_PER_LINE) + (__j__))

#define _FEED_LINE                                                             \
    do {                                                                       \
        _DUMP_PRINT_STR("\n");                                                 \
    } while (0)

static void _osal_log_dump_data(const char *str,
                                const uint8_t *data,
                                size_t size,
                                int32_t start_line_num)
{
    size_t lines                             = 0;
    size_t i                                 = 0;
    size_t j                                 = 0;
    char line_data[_STRING_LEN_PER_LINE + 2] = {0};
    char *p                                  = NULL;

    lines = (size + _BYTES_PER_LINE - 1) / _BYTES_PER_LINE;

    if (str) {
        _DUMP_PRINT_STR(str);
        _FEED_LINE;
    }
    for (i = 0; i < lines; i++) {
        p = line_data;
        _LINE_NUM_TO_STR(i * _BYTES_PER_LINE + start_line_num, p);
        *p++ = ' ';
        *p++ = ' ';
        for (j = 0; j < _BYTES_PER_LINE; j++) {
            if (__GET_DATA_IDX(i, j) < size) {
                _BYTE_DATA_TO_STR(data[__GET_DATA_IDX(i, j)], p);
            } else {
                *p++ = ' ';
                *p++ = ' ';
            }
            *p++ = ' ';
            if (j == (_BYTES_PER_LINE / 2) - 1) {
                *p++ = ' ';
            }
        }
        *p++ = ' ';
        *p++ = '|';
        /* convert to char */
        for (j = 0; j < _BYTES_PER_LINE; j++) {
            if (__GET_DATA_IDX(i, j) < size) {
                _PRINT_BYTE(data[__GET_DATA_IDX(i, j)], p);
            } else {
                *p++ = ' ';
            }
        }
        *p++ = '|';
        *p++ = 0;

        /* print the data */
        _DUMP_PRINT_STR(line_data);
        _FEED_LINE;
    }
}

/**
 * This function dump one line:
                      .. .. .. ..  .. .. .. ..
*/
static void _osal_log_dump_ellipsis(void)
{
    size_t i                                 = 0;
    char line_data[_STRING_LEN_PER_LINE + 2] = {0};
    char *p                                  = NULL;

    p = line_data;
    for (i = 0; i < 22; i++) {
        *p++ = ' ';
    }
    // clang-format off
    *p++ = '.'; *p++ = '.'; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' '; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' ';
    *p++ = '.'; *p++ = '.'; *p++ = ' '; *p++ = ' ';
    // clang-format on
    *p++ = '\0';
    /* print the data */
    _DUMP_PRINT_STR(line_data);
    _FEED_LINE;
}

void osal_log_dump_data_less(const char *str, const uint8_t *data, size_t size)
{
    int32_t start_line_num = 0;

    if (size <= 2 * _HEAD_TAIL_LINES * _BYTES_PER_LINE) {
        _osal_log_dump_data(str, data, size, 0);
    } else {
        _osal_log_dump_data(str, data, _HEAD_TAIL_LINES * _BYTES_PER_LINE, 0);
        data += _HEAD_TAIL_LINES * _BYTES_PER_LINE;
        size -= _HEAD_TAIL_LINES * _BYTES_PER_LINE;
        start_line_num += _HEAD_TAIL_LINES * _BYTES_PER_LINE;
        _osal_log_dump_ellipsis();
        _osal_log_dump_ellipsis();
        while (size > _HEAD_TAIL_LINES * _BYTES_PER_LINE) {
            data += _BYTES_PER_LINE;
            size -= _BYTES_PER_LINE;
            start_line_num += _BYTES_PER_LINE;
        }
        _osal_log_dump_data(NULL, data, size, start_line_num);
    }
    return;
}

void osal_log_dump_data(const char *str, const uint8_t *data, size_t size)
{
    return _osal_log_dump_data(str, data, size, 0);
}
