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
#include "nano2D_user_buffer.h"
#include "nano2D_user_hardware.h"
#include "nano2D_user_os.h"
#include "nano2D_user_base.h"


static n2d_error_t gcFreeCmdBuffer(
    IN n2d_user_hardware_t                *hardware,
    IN n2d_user_cmd_buf_t                 *cmd_buf
    )
{
    n2d_error_t error;
    if (cmd_buf != N2D_NULL)
    {
        if (cmd_buf->logical != 0)
        {

            N2D_ON_ERROR(gcUnmapMemory(hardware, cmd_buf->handle));
            N2D_ON_ERROR(gcFreeGpuMemory(hardware, cmd_buf->handle));

            /* Reset the buffer pointer. */
            cmd_buf->logical = 0;
            cmd_buf->handle = N2D_INVALID_HANDLE;
        }
    }

    return N2D_SUCCESS;

on_error:
    return error;

}


/*******************************************************************************
**
**  gcDestroyCmdBuf
**
**  Destroy a n2d_user_cmd_buf_t * object.
**
**  INPUT:
**
**      n2d_user_cmd_buf_t * CommandBuffer
**          Pointer to an n2d_user_cmd_buf_t * object.
**
**  OUTPUT:
**
**      None.
*/
static n2d_error_t gcDestroyCmdBuf(
    IN n2d_user_hardware_t                *hardware,
    IN n2d_user_cmd_buf_t                 *cmd_buf
    )
{
    n2d_error_t error;
    /* Destroy command buffer allocations. */
    N2D_ON_ERROR(gcFreeCmdBuffer(hardware, cmd_buf));

    /* Destroy signal. */
    if (cmd_buf->stall_signal != N2D_NULL)
    {
        N2D_ON_ERROR(n2d_user_os_signal_destroy(hardware, cmd_buf->stall_signal));
        cmd_buf->stall_signal = N2D_NULL;
    }

    /* Free the n2d_user_cmd_buf_t * object. */
    n2d_user_os_free(cmd_buf);
    cmd_buf = N2D_NULL;

    return N2D_SUCCESS;

on_error:
    return error;

}

/*******************************************************************************
**
**  gcDestroySurfaceCmdBuf
**
**  Destroy an n2d_user_cmd_location_t object.
**
**  INPUT:
**
**      n2d_user_cmd_location_t Buffer
**          Pointer to an n2d_user_cmd_location_t object to delete.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcDestroySurfaceCmdBuf(
    IN n2d_user_hardware_t     *hardware,
    IN n2d_user_buffer_t       *Surface
    )
{
    n2d_user_cmd_buf_t *cmd_buf = {0};

    /* Destroy all command buffers. */
    while (N2D_NULL != Surface->command_buffer_list)
    {
        /* Get the head of the list. */
        cmd_buf = Surface->command_buffer_list;

        if(Surface->command_buffer_tail == cmd_buf)
        {
            Surface->command_buffer_tail = N2D_NULL;
        }

        /* Remove the head Surface from the list. */
        if (cmd_buf->next == cmd_buf)
        {
            Surface->command_buffer_list = N2D_NULL;
        }
        else
        {
            cmd_buf->prev->next =
            Surface->command_buffer_list = cmd_buf->next;
            cmd_buf->next->prev = cmd_buf->prev;
        }

        /* Destroy command Surface. */
        gcDestroyCmdBuf(hardware, cmd_buf);
    }

    return N2D_SUCCESS;

}

/*******************************************************************************
**
**  gcQueryCmdBuf
**
**  Query the command buffer alignment and number of reserved bytes.
**
**
**  OUTPUT:
**
**      n2d_size_t * alignment
**          Pointer to a variable receiving the alignment for each command.
**
**      n2d_size_t * reserved_tail
**          Pointer to a variable receiving the number of bytes reserved at the
**          tail of each command buffer.
*/
n2d_error_t gcQueryCmdBuf(
    OUT n2d_uint32_t *alignment,
    OUT n2d_uint32_t *reserved_tail
    )
{
    if(N2D_NULL != alignment)
    {
        /* Align every 8 Bytes. */
        *alignment = 8;
    }

    if(N2D_NULL != reserved_tail)
    {
        /* Reserve space for Link(). */
        *reserved_tail = 8;
    }

        /* Success. */
    return N2D_SUCCESS;

}


/*******************************************************************************
**
**  gcConstructCmdBuf
**
**  Construct a new n2d_user_cmd_buf_t * object.
**
**  INPUT:
**
**      gcoOS Os
**          Pointer to a gcoOS object.
**
**      gcohardware hardware
**          Pointer to a gcohardware object.
**
**      n2d_size_t Bytes
**          Number of bytes for the buffer.
**
**      gcsCOMMAND_BUFFER_PTR Info
**          Alignment and head/tail information.
**
**  OUTPUT:
**
**      n2d_user_cmd_buf_t * * CommandBuffer
**          Pointer to a variable that will hold the the n2d_user_cmd_buf_t * object
**          pointer.
*/
static n2d_error_t gcConstructCmdBuf(
    IN n2d_user_hardware_t                  *hardware,
    IN n2d_size_t                           bytes,
    IN n2d_user_cmd_buf_reserved_info_t     *info,
    OUT n2d_user_cmd_buf_t                  **command_buffer
    )
{
    n2d_error_t             error = N2D_SUCCESS;
    n2d_ioctl_interface_t   iface = {0};
    n2d_user_cmd_buf_t      *commandBuffer = N2D_NULL;
    n2d_pointer             pointer = N2D_NULL;

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(N2D_NULL != hardware);
    gcmVERIFY_ARGUMENT(bytes > 0);

    N2D_ON_ERROR(n2d_user_os_allocate(N2D_SIZEOF(n2d_user_cmd_buf_t), &pointer));
    commandBuffer = pointer;
    n2d_user_os_memory_fill(commandBuffer,0,N2D_SIZEOF(n2d_user_cmd_buf_t));

    /* Create the signal. */
    N2D_ON_ERROR(n2d_user_os_signal_create(hardware, N2D_FALSE, &commandBuffer->stall_signal));

    /* Mark the buffer as available. */
    N2D_ON_ERROR(n2d_user_os_signal_signal(hardware, commandBuffer->stall_signal, N2D_TRUE));

    N2D_ON_ERROR(gcAllocateGpuMemory(hardware, bytes, N2D_TRUE, &iface));

    commandBuffer->handle = iface.u.command_allocate.handle;
    n2d_user_os_memory_fill(&iface, 0, N2D_SIZEOF(n2d_ioctl_interface_t));

    N2D_ON_ERROR(gcMapMemory(hardware,commandBuffer->handle,&iface));

    commandBuffer->logical      = iface.u.command_map.logical;
    commandBuffer->address      = (n2d_uintptr_t)iface.u.command_map.address;
    commandBuffer->bytes        = bytes;

    commandBuffer->reserved_tail = info->reserved_tail;

    /* Return pointer to the n2d_user_cmd_buf_t * object. */
    *command_buffer = commandBuffer;

    /* Success. */
    return N2D_SUCCESS;

on_error:

    /* Roll back. */
    if (commandBuffer)
    {
        gcDestroyCmdBuf(hardware, commandBuffer);
    }

    return error;

}

static n2d_error_t gcGetCmdBuf(
    IN n2d_user_hardware_t      *hardware,
    IN n2d_user_buffer_t        *buffer
    )
{
    n2d_error_t             error = N2D_SUCCESS;
    n2d_user_cmd_buf_t      *cmd_buf = N2D_NULL;

    /* Determine the next command cmd_buf. */
    if (N2D_NULL == buffer->command_buffer_tail)
    {
        /* Get the head of the list. */
        cmd_buf = buffer->command_buffer_list;
    }
    else
    {
        /* Get the next command cmd_buf. */
        cmd_buf = buffer->command_buffer_tail->next;
    }

    if (N2D_NULL != cmd_buf)
    {
        /* Test if command cmd_buf is available. */
        error = n2d_user_os_signal_wait(hardware, cmd_buf->stall_signal, N2D_INFINITE);
    }

    /* Not available? */
    if (error == N2D_TIMEOUT || N2D_NULL == cmd_buf)
    {
        /* Construct new command cmd_buf. */
        if (0 == buffer->max_count || buffer->count < buffer->max_count)
        {
            n2d_user_cmd_buf_t *temp = N2D_NULL;
            N2D_ON_ERROR(gcConstructCmdBuf(hardware, buffer->bytes,
                                               &buffer->info,
                                               &temp));

            if (N2D_NULL == cmd_buf)
            {
                buffer->command_buffer_list = temp->next = temp->prev = temp;
            }
            else
            {
                N2D_ASSERT(buffer->command_buffer_tail != N2D_NULL);
                /* Insert into the list after command_buffer_tail, maybe not the list tail. */
                temp->prev = buffer->command_buffer_tail;
                temp->next = buffer->command_buffer_tail->next;
                buffer->command_buffer_tail->next->prev = temp;
                buffer->command_buffer_tail->next = temp;
                buffer->command_buffer_tail = temp;
            }

            buffer->count += 1;

            cmd_buf = temp;

        }

        if (N2D_NULL == cmd_buf)
        {
            N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
        }

        /* Wait for cmd_buf to become available. */
        N2D_ON_ERROR(n2d_user_os_signal_wait(hardware, cmd_buf->stall_signal, N2D_INFINITE));

        buffer->command_buffer_tail = cmd_buf;
    }
    else
    {
        N2D_ON_ERROR(error);
        buffer->command_buffer_tail = cmd_buf;
    }

    /* Reset command cmd_buf. */
    cmd_buf->start_offset = 0;
    cmd_buf->offset      = 0;
    cmd_buf->free        = cmd_buf->bytes - buffer->total_reserved;

    return N2D_SUCCESS;

on_error:

    return error;


}


/*
    Get command buffer
*/
n2d_error_t gcInitCmdBuf(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_size_t           max_size
    )
{
    n2d_error_t                 error = N2D_SUCCESS;
    n2d_uint32_t                i = 0;
    n2d_user_buffer_t           *buffer = N2D_NULL;
    n2d_user_cmd_buf_t          *command_buffer = N2D_NULL;

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(N2D_NULL != hardware);

    buffer = &hardware->buffer;

    buffer->command_buffer_list = N2D_NULL;
    buffer->command_buffer_tail = N2D_NULL;

    /** Query alignment.*/
    N2D_ON_ERROR(gcQueryCmdBuf(
        &buffer->info.alignment,
        &buffer->info.reserved_tail
        ));

    buffer->total_reserved
        = buffer->info.reserved_tail
        + buffer->info.alignment;

    buffer->count    = NANO2D_CMDBUFFERS_COUNT;

    for( i = 0; i < buffer->count; i++)
    {

        N2D_ON_ERROR(gcConstructCmdBuf(
            hardware,
            max_size,
            &buffer->info,
            &command_buffer));


        if (N2D_NULL == buffer->command_buffer_list)
        {
            buffer->command_buffer_list = command_buffer;
            command_buffer->prev =
            command_buffer->next = command_buffer;
        }
        else
        {
            /* Add to the tail. */
            command_buffer->prev = buffer->command_buffer_list->prev;
            command_buffer->next = buffer->command_buffer_list;
            buffer->command_buffer_list->prev->next = command_buffer;
            buffer->command_buffer_list->prev = command_buffer;
        }

    }

    /*ToDo: expand this value*/
    buffer->max_count = NANO2D_CMDBUFFERS_COUNT_MAX;
    /* Get the current command buffer. */
    N2D_ON_ERROR(gcGetCmdBuf(hardware,buffer));

    return N2D_SUCCESS;

on_error:

    /* Roll back. */
    if(buffer)
    {
        gcDestroySurfaceCmdBuf(hardware, buffer);
    }

    return error;


}


/*******************************************************************************
**
**  gcReserveCmdBuf
**
**  Reserve a number of bytes in the buffer.
**
**  INPUT:
**
**      n2d_user_cmd_location_t Buffer
**          Pointer to an n2d_user_cmd_location_t object.
**
**      n2d_size_t Bytes
**          Number of bytes to reserve.
**
**      n2d_bool_t Aligned
**          N2D_TRUE if the data needs to be aligned to 64-bit.
**
**  OUTPUT:
**
*/
n2d_error_t gcReserveCmdBuf(
    IN n2d_user_hardware_t  *hardware,
    IN n2d_size_t           bytes,
    OUT n2d_user_cmd_buf_t  **reserve
    )
{
    n2d_error_t         error = N2D_SUCCESS;
    n2d_size_t          align_bytes = 0, offset = 0;
    n2d_size_t          aligned_bytes = 0, final_bytes = 0;
    n2d_user_cmd_buf_t  *cmd_buf;
    n2d_user_buffer_t   *buffer = &hardware->buffer;
    n2d_ioctl_interface_t   iface = {0};

    /* Get the current command buffer. */
    cmd_buf = buffer->command_buffer_tail;

    if (cmd_buf == N2D_NULL)
    {
        /* Grab a new command buffer. */
        N2D_ON_ERROR(gcGetCmdBuf(hardware,buffer));

        /* Get the new buffer. */
        cmd_buf = buffer->command_buffer_tail;
    }

    N2D_ASSERT(cmd_buf != N2D_NULL);
    /* Compute the number of aligned bytes. */
    align_bytes = gcmALIGN(cmd_buf->offset, buffer->info.alignment) - cmd_buf->offset;

    /* Compute the number of required bytes. */
    aligned_bytes = bytes + align_bytes;

    if (aligned_bytes > cmd_buf->free)
    {
        /* Sent event to signal when command buffer completes. */
        iface.command                       = N2D_KERNEL_COMMAND_SIGNAL;
        iface.u.command_signal.handle       = (n2d_uintptr_t)(cmd_buf->stall_signal);
        iface.u.command_signal.process      = 0;
        iface.u.command_signal.from_kernel  = N2D_FALSE;

        N2D_ON_ERROR(gcCommitCmdBuf(hardware));

        /* Grab a new command buffer. */
        N2D_ON_ERROR(gcGetCmdBuf(hardware, buffer));

        /* Get new buffer. */
        cmd_buf = buffer->command_buffer_tail;

        if (bytes > cmd_buf->free)
        {
            /* This just won't fit! */
            gcTrace("Command of %lu original bytes + %lu resume bytes is too big!", cmd_buf->free, bytes);
            N2D_ON_ERROR(N2D_OUT_OF_MEMORY);
        }
        /* Calculate total bytes again. */
        align_bytes = 0;
        final_bytes = bytes;
    }
    else
    {
        final_bytes = aligned_bytes;
    }

    N2D_ASSERT(N2D_NULL != cmd_buf);
    N2D_ASSERT(final_bytes <= cmd_buf->free);

    /* Determine the data offset. */
    offset = cmd_buf->offset + align_bytes;

    /* Update the last reserved location. */
    cmd_buf->last_reserve = (n2d_uintptr_t)((n2d_uint8_t*)(cmd_buf->logical) + offset);
    cmd_buf->last_offset  = offset;

    /* Adjust command buffer size. */
    cmd_buf->offset += final_bytes;
    cmd_buf->free   -= final_bytes;

    *reserve = cmd_buf;

    return N2D_SUCCESS;

on_error:

    return error;

}

/*******************************************************************************
**
**  gcCommitCmdBuf
**
**  Commit the command buffer to the hardware.
**
**  INPUT:
**
**      n2d_user_cmd_location_t Buffer
**          Pointer to a n2d_user_cmd_location_t object.
**
**      gcePIPE_SELECT CurrentPipe
**          Current graphics pipe.
**
**      gcsSTATE_DELTA_PTR StateDelta
**          Pointer to the state delta.
**
**      gcoQUEUE Queue
**          Pointer to a gcoQUEUE object.
**
**  OUTPUT:
**
**      Nothing.
*/
n2d_error_t gcCommitCmdBuf(
    IN n2d_user_hardware_t          *hardware
    )
{
    n2d_error_t error;
    n2d_user_cmd_buf_t *cmd_buf;

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(N2D_NULL != hardware);

    cmd_buf = hardware->buffer.command_buffer_tail;

    if (cmd_buf != N2D_NULL)
    {
        n2d_user_subcommit_t    subcommit = {0};

        subcommit.cmd_buf.address = cmd_buf->address;
        subcommit.cmd_buf.logical = (n2d_pointer)(cmd_buf->logical);
        subcommit.cmd_buf.offset  = (n2d_uint32_t)cmd_buf->start_offset;
        subcommit.cmd_buf.size    = (n2d_uint32_t)(cmd_buf->offset - cmd_buf->start_offset);
        subcommit.cmd_buf.handle  = cmd_buf->handle;

        /* When the current cmdbuf size is 0, the commit buf phase will be skipped, only commit event. */
        if(subcommit.cmd_buf.size != 0)
            N2D_ON_ERROR(gcSubcommitCmd(hardware, &subcommit));

        /* update to new offset location*/
        cmd_buf->start_offset = cmd_buf->offset;
    }
    else
    {
        N2D_ON_ERROR(N2D_INVALID_ARGUMENT);
    }

    return N2D_SUCCESS;

on_error:
    /* Return the status. */
    return error;
}
