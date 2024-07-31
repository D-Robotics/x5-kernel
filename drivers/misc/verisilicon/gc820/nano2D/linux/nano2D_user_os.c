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

// #include <fcntl.h>
// #include <math.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/mman.h>
// #include <sys/ioctl.h>
// #include <sys/time.h>
// #include <unistd.h>
// #include "stdarg.h"
// #include "pthread.h"
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/printk.h>
#include <asm/io.h>

#include "nano2D.h"
#include "nano2D_types.h"
#include "shared/nano2D_dispatch.h"
#include "nano2D_user_os.h"
#include "nano2D_user_linux.h"
#include "nano2D_user_hardware.h"
#include "nano2D_kernel_driver.h"

#if defined(CONFIG_X86) && (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
#include <asm/set_memory.h>
#endif
// static pthread_key_t __tlsKey;

// static n2d_error_t __attribute__((constructor)) _n2d_user_os_thread_constructor(n2d_void);
// static n2d_error_t __attribute__((destructor)) _n2d_user_os_thread_destructor(n2d_void);

n2d_error_t
n2d_user_os_allocate(
    n2d_uint32_t size,
    n2d_pointer *memory)
{
    n2d_error_t error;

    /* Check the argument. */
    if ((size == 0) || (memory == N2D_NULL))
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Allocate the memory. */
    *memory = n2d_kmalloc(size, GFP_KERNEL);

    if (*memory == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_OUT_OF_MEMORY);
    }

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

n2d_error_t
n2d_user_os_free(
    n2d_pointer memory)
{
    n2d_error_t error;

    /* Check the argument. */
    if (memory == N2D_NULL)
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    /* Free the memory. */
    n2d_kfree(memory);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

void
n2d_user_os_memory_fill(
    n2d_pointer memory,
    n2d_uint8_t filler,
    n2d_uint32_t bytes)
{
    /* Fill the memory. */
    if ((memory != N2D_NULL) && (bytes != 0))
    {
        memset(memory, filler, bytes);
    }
}

void
n2d_user_os_memory_copy(
    n2d_pointer dst,
    n2d_pointer src,
    n2d_uint32_t size)
{
    /* Copy the memory. */
    if ((N2D_NULL != dst) && (N2D_NULL != src) && (size > 0))
    {
        memcpy(dst, src, size);
    }
}

// n2d_float_t
// n2d_user_os_math_sine(
//     n2d_float_t x)
// {
//     return sinf(x);
// }

void
n2d_user_os_trace(
    char *format,
    ...)
{
    static char buffer[256];
    va_list args;
    va_start(args, format);

    vsnprintf(buffer, 255, format, args);;
    buffer[255] = 0;

    pr_info("%s", buffer);
    // fflush(stdout);
    va_end(args);
}

n2d_error_t
n2d_user_os_ioctl(
    n2d_ioctl_interface_t *Data)
{
    // static int device = 0;
    // static unsigned int length;
    // static n2d_pointer mapped;

    n2d_error_t error;
    n2d_ioctl_interface_t *iface = Data;
    // if (device == 0)
    // {
    //     device = open("/dev/nano2d", O_RDWR);

    //     if (device == -1)
    //     {
    //         N2D_ON_ERROR(N2D_GENERIC_IO);
    //     }

    //     mapped = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, device, 0);

    //     if (mapped == NULL)
    //     {
    //         close(device);
    //         device = 0;

    //         N2D_ON_ERROR(N2D_GENERIC_IO);
    //     }
    // }

    /* Close is handled separately to avoid that when the device is not 0, the garbage collection mechanism of the OS causes
     the kernel driver close to be called when the program exits, which may cause the close call twice */
    // if(iface->command == N2D_KERNEL_COMMAND_CLOSE)
    // {
    //     // close(device);
    //     device = 0;
    //     /* Success. */
    //     return N2D_SUCCESS;
    // }

    iface->error   = N2D_SUCCESS;

    if (drv_ioctl_internal(iface) < 0)
    {
        N2D_ON_ERROR(N2D_GENERIC_IO);
    }

    /* Signal waiting timeout is normal */
    if(iface->error != N2D_TIMEOUT)
        N2D_ON_ERROR(iface->error);
    /* There will be a timeout error code only command is waiting for the signal. */
    else if(iface->error == N2D_TIMEOUT && iface->command != N2D_KERNEL_COMMAND_USER_SIGNAL)
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;
}

/* Create a signal. */
n2d_error_t
n2d_user_os_signal_create(
    IN n2d_user_hardware_t       *Hardware,
    IN n2d_bool_t           ManualReset,
    OUT n2d_pointer          *Signal
    )
{

    n2d_error_t error;
    n2d_ioctl_interface_t iface = {0};

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Signal != N2D_NULL);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command     = N2D_KERNEL_COMMAND_USER_SIGNAL;
    iface.core        = Hardware->coreIndex;
    iface.u.command_user_signal.command = N2D_USER_SIGNAL_CREATE;
    iface.u.command_user_signal.manual_reset = ManualReset;

    /* Call kernel driver. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    *Signal = (n2d_pointer)(n2d_uintptr_t) iface.u.command_user_signal.handle;

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

};

/* Destroy a signal. */
n2d_error_t
n2d_user_os_signal_destroy(
    IN n2d_user_hardware_t   *Hardware,
    IN n2d_pointer       Signal
    )
{
    n2d_error_t error;
    n2d_ioctl_interface_t iface = {0};

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Signal != N2D_NULL);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command     = N2D_KERNEL_COMMAND_USER_SIGNAL;
    iface.core        = Hardware->coreIndex;
    iface.u.command_user_signal.command = N2D_USER_SIGNAL_DESTROY;
    iface.u.command_user_signal.handle = (n2d_int32_t)(n2d_uintptr_t)Signal;

    /* Call kernel driver. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

};

/* Signal a signal. */
n2d_error_t
n2d_user_os_signal_signal(
    IN n2d_user_hardware_t *Hardware,
    IN n2d_pointer Signal,
    IN n2d_bool_t State
    )
{
    n2d_error_t error;
    n2d_ioctl_interface_t iface = {0};

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Signal != N2D_NULL);


    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command     = N2D_KERNEL_COMMAND_USER_SIGNAL;
    iface.core        = Hardware->coreIndex;
    iface.u.command_user_signal.command = N2D_USER_SIGNAL_SIGNAL;
    iface.u.command_user_signal.handle = (n2d_int32_t)(n2d_uintptr_t)Signal;
    iface.u.command_user_signal.state = State;

    /* Call kernel driver. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

};

/* Wait for a signal. */
n2d_error_t
n2d_user_os_signal_wait(
    IN n2d_user_hardware_t   *Hardware,
    IN n2d_pointer       Signal,
    IN n2d_uint32_t     Wait
    )
{

    n2d_error_t error;
    n2d_ioctl_interface_t iface = {0};

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Signal != N2D_NULL);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command     = N2D_KERNEL_COMMAND_USER_SIGNAL;
    iface.core        = Hardware->coreIndex;
    iface.u.command_user_signal.command = N2D_USER_SIGNAL_WAIT;
    iface.u.command_user_signal.handle = (n2d_int32_t)(n2d_uintptr_t)Signal;
    iface.u.command_user_signal.wait = Wait;

    /* Call kernel driver. */
    N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

    if(iface.error == N2D_TIMEOUT)
        N2D_ON_ERROR(N2D_TIMEOUT);

    /* Success. */
    return N2D_SUCCESS;

on_error:
    return error;

};

// n2d_uint64_t n2d_user_os_get_ticks(
//     IN n2d_void)
// {
//     struct timeval tv;
//     gettimeofday(&tv, 0);
//     return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
// }

static gcoTLS tls = N2D_NULL;
n2d_error_t n2d_user_os_get_tls(gcoTLS *TLS)
{
    n2d_error_t error = N2D_SUCCESS;

    // tls = (gcoTLS)pthread_getspecific(__tlsKey);

    if (tls == N2D_NULL)
    {
        N2D_ON_ERROR(gcTlsConstructor(&tls));
        // if (pthread_setspecific(__tlsKey, tls) != 0)
        // {
        //     N2D_ON_ERROR(N2D_GENERIC_IO);
        // }
    }

    *TLS = tls;

on_error:
    return error;
}

// static n2d_error_t _n2d_user_os_thread_constructor(n2d_void)
// {
//     n2d_error_t error = N2D_SUCCESS;
//     n2d_uint32_t result;

//     /* Create key. */
//     result = pthread_key_create(&__tlsKey, N2D_NULL);

//     if (result != 0)
//     {
//         gcTrace("%s(%d): TlsAlloc failed\n", __FUNCTION__, __LINE__);
//         N2D_ON_ERROR(N2D_OUT_OF_MEMORY);
//     }

// on_error:
//     return error;
// }

// static n2d_error_t _n2d_user_os_thread_destructor(n2d_void)
// {
//     n2d_error_t error = N2D_SUCCESS;
//     n2d_ioctl_interface_t iface = { 0 };

//     pthread_key_delete(__tlsKey);

//     iface.command = N2D_KERNEL_COMMAND_CLOSE;
//     /* default use device 0 and core 0 */
//     iface.dev_id = N2D_DEVICE_0;
//     iface.core = N2D_CORE_0;

//     /* Call the kernel. */
//     N2D_ON_ERROR(n2d_user_os_ioctl(&iface));

// on_error:
//     return error;
// }

n2d_error_t n2d_user_os_free_tls(gcoTLS* TLS)
{
    n2d_error_t error = N2D_SUCCESS;

    if (*TLS != N2D_NULL)
    {
        N2D_ON_ERROR(n2d_user_os_free(*TLS));
        *TLS = N2D_NULL;
        // pthread_setspecific(__tlsKey, N2D_NULL);
	    tls = N2D_NULL;
    }

on_error:
    return error;
}
