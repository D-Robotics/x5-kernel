/****************************************************************************
*
*    Copyright 2012 - 2022 Vivante Corporation, Santa Clara, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#include "nano2D.h"
#include "nano2D_user_hardware.h"
#include "nano2D_user_os.h"

#if NANO2D_DUMP || NANO2D_PERF_TIMER
#ifdef WIN32
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#endif

n2d_uint32_t get_thread_id()
{
#ifdef WIN32
    return GetCurrentThreadId();
#else
    return syscall(SYS_gettid);
#endif
}
#endif

#if NANO2D_DUMP
/*dump file mode*/
typedef enum n2d_dumpfile_mode
{
    N2D_FILE_MODE_GET,      /* if no file,create file*/
    N2D_FILE_MODE_CLOSE,    /*if file exist ,close*/
}
n2d_dumpfile_mode_t;

n2d_error_t n2d_dump_file_operate(n2d_dumpfile_mode_t mode, n2d_user_hardware_t* Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    if (N2D_FILE_MODE_CLOSE == mode)
    {
        if (NULL != Hardware->fp)
        {
            fclose(Hardware->fp);
            Hardware->fp = NULL;
        }
    }
    else if (N2D_FILE_MODE_GET == mode)
    {
        if (NULL == Hardware->fp)
        {
            char name[128] = {0};
            sprintf(name, "n2d_dump_tid-%d.log", get_thread_id());
            Hardware->fp  = fopen(name, "w");
            if (NULL == Hardware->fp)
            {
                N2D_ON_ERROR(N2D_GENERIC_IO);
            }
            Hardware->g_line = 1;
        }
    }
    else
    {
         N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }
    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t n2d_dump_surface(
    n2d_pointer memory,
    n2d_uint32_t physics,
    n2d_uint32_t size,
    n2d_bool_t IsSrc,
    n2d_user_hardware_t* Hardware)
{
    n2d_error_t  error = N2D_SUCCESS;
    n2d_uint32_t index = 0;
    n2d_uint32_t *p = (n2d_uint32_t *)memory;

    if(NULL == p || 0 == physics || size <= 0)
    {
        return N2D_INVALID_ARGUMENT;
    }

    N2D_ON_ERROR(n2d_dump_file_operate(N2D_FILE_MODE_GET, Hardware));

    fprintf(Hardware->fp, "[%6ld] $$[2D %s Surface, GPU Address = 0x%08X, offset = %d, size = %d bytes, coreId = %d]\n",
        Hardware->g_line++, IsSrc ? "Src":"Dst", physics, 0, size, Hardware->coreIndex);
    index = 0;
    size /= 4;
    while (index < size)
    {
        switch (size - index)
        {
        case 1:
            fprintf(Hardware->fp, "[%6ld] $$ 0x%08X\n", Hardware->g_line++, p[index]);
            index += 1;
            break;
        case 2:
            fprintf(Hardware->fp, "[%6ld] $$ 0x%08X 0x%08X\n", Hardware->g_line++, p[index], p[index + 1]);
            index += 2;
            break;
        case 3:
            fprintf(Hardware->fp, "[%6ld] $$ 0x%08X 0x%08X 0x%08X\n",
                Hardware->g_line++, p[index], p[index + 1], p[index + 2]);
            index += 3;
            break;
        default:
            fprintf(Hardware->fp, "[%6ld] $$ 0x%08X 0x%08X 0x%08X 0x%08X\n",
                Hardware->g_line++, p[index], p[index + 1], p[index + 2], p[index + 3]);
            index += 4;
            break;
        }
    }

    fprintf(Hardware->fp, "[%6ld] $$ **********\n", Hardware->g_line++);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t n2d_dump_command(n2d_uint32_t *memory, n2d_uint32_t size, n2d_user_hardware_t* Hardware)
{
    n2d_error_t error = N2D_SUCCESS;
    n2d_uint32_t index = 0;
    n2d_uint32_t *p = memory;

    N2D_ON_ERROR(n2d_dump_file_operate(N2D_FILE_MODE_GET, Hardware));

    fprintf(Hardware->fp, "[%6ld] $$[2D Command Buffer, size = %d bytes, coreId = %d]\n",
        Hardware->g_line++, size * 4, Hardware->coreIndex);

    for(index = 0;index < size;index++)
    {
        fprintf(Hardware->fp, "[%6ld] $$ 0x%08X\n", Hardware->g_line++, p[index]);
    }
    fprintf(Hardware->fp, "[%6ld] $$ **********\n", Hardware->g_line++);
    fflush(Hardware->fp);
    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

}

n2d_error_t n2d_dump_close_file(n2d_user_hardware_t* Hardware)
{
    n2d_error_t error = N2D_SUCCESS;

    N2D_ON_ERROR(n2d_dump_file_operate(N2D_FILE_MODE_CLOSE, Hardware));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

#endif

n2d_void _n2d_printf(n2d_const_string format, ...)
{
// #if !defined(UNDER_CE) && defined(_WIN32)
//     n2d_string      buffer;
//     n2d_int32_t     len;
// #endif

//     va_list args;

//     va_start(args, format);

//     vprintf(format, args);
// #if !defined(UNDER_CE) && defined(_WIN32)
//     len = _vscprintf(format, args) + 1 + 1;
//     buffer = (n2d_string)malloc(len * sizeof(n2d_char));
//     vsprintf(buffer, format, args);

//     OutputDebugStringA(buffer);

//     free(buffer);
// #endif
//     va_end(args);
}

#if NANO2D_PERF_TIMER
n2d_void n2d_time_stamp_trace(n2d_const_string format, ...)
{
    static n2d_char buffer[1024];
    va_list args;
    va_start(args, format);

    vsnprintf(buffer, 1023, format, args);;
    buffer[1023] = 0;

    _n2d_printf(buffer);
}

n2d_void n2d_time_stamp_print(n2d_const_string function_name, n2d_string order)
{
    static n2d_uint32_t line_t = 0;
    n2d_uint32_t thread_id;
    n2d_uint64_t time_stamp;

    thread_id = get_thread_id();
    time_stamp = n2d_user_os_get_ticks();

    n2d_time_stamp_trace("[%lld,%3d] (tid=0x%08X): %s %s\n",
        time_stamp, line_t++, thread_id, function_name, strcmp(order, "START") ? "=>" : "");
}

#endif