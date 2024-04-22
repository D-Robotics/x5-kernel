/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright (c) 2012 - 2022 Vivante Corporation
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (C) 2012 - 2022 Vivante Corporation
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices in your
 *    version of this file.
 *
 *****************************************************************************/

#include "nano2D_kernel.h"
#include "nano2D_kernel_hardware.h"
#include "nano2D_kernel_db.h"
#include "nano2D_kernel_event.h"
#include "nano2D_kernel_vidmem.h"
#include "nano2D_feature_database.h"

#define gcmSEMAPHORESTALL(buffer)                                                                \
	do {                                                                                     \
		/* Arm the PE-FE Semaphore. */                                                   \
		*buffer++ =                                                                      \
			gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE) | \
			gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1) |                \
			gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,                   \
				    AQSemaphoreRegAddrs);                                        \
                                                                                                 \
		*buffer++ = gcmSETFIELDVALUE(0, AQ_SEMAPHORE, SOURCE, FRONT_END) |               \
			    gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE);        \
                                                                                                 \
		/* STALL FE until PE is done flushing. */                                        \
		*buffer++ = gcmSETFIELDVALUE(0, STALL_COMMAND, OPCODE, STALL);                   \
                                                                                                 \
		*buffer++ = gcmSETFIELDVALUE(0, STALL_STALL, SOURCE, FRONT_END) |                \
			    gcmSETFIELDVALUE(0, STALL_STALL, DESTINATION, PIXEL_ENGINE);         \
	} while (0)

typedef struct n2d_debug_registers {
	char *module;
	n2d_uint32_t index;
	n2d_uint32_t shift;
	n2d_uint32_t data;
	n2d_uint32_t count;
	n2d_uint32_t signature;
} n2d_debug_registers_t;

static n2d_error_t n2d_kernel_hardware_query_idle(n2d_hardware_t *hardware, n2d_bool_t *idle)
{
	n2d_error_t error   = N2D_SUCCESS;
	n2d_uint32_t reg    = 0;
	n2d_int32_t pending = 0;

	if (!idle)
		return N2D_INVALID_ARGUMENT;

	reg = n2d_kernel_os_peek_with_core(hardware->os, hardware->core, AQ_HI_IDLE_Address);
	/* Skip FE */
	reg |= 0x1;
	if (reg != 0x7FFFFFFF) {
		*idle = N2D_FALSE;
		return error;
	}
	ONERROR(n2d_kernel_os_atom_get(hardware->os, hardware->event_center->pending_event_count,
				       &pending));
	if (pending > 0) {
		*idle = N2D_FALSE;
		return error;
	}
	*idle = N2D_TRUE;
on_error:
	return error;
}

static n2d_error_t n2d_kernel_get_APBRegOffset(n2d_hardware_t *hardware, n2d_uint32_t *APBRegOffset)
{
	*APBRegOffset = hardware->kernel->device->registerAPB;

	return N2D_SUCCESS;
}

static n2d_error_t _dump_buffer(n2d_uint32_t *buf, n2d_uint32_t address, n2d_uint32_t size)
{
	n2d_uint32_t i, line, left;
	n2d_uint32_t *data = buf;

	line = size / 32;
	left = size % 32;

	for (i = 0; i < line; i++) {
		n2d_kernel_os_print("%08X : %08X %08X %08X %08X %08X %08X %08X %08X\n", address,
				    data[0], data[1], data[2], data[3], data[4], data[5], data[6],
				    data[7]);
		data += 8;
		address += 8 * 4;
	}

	switch (left) {
	case 28:
		n2d_kernel_os_print("%08X : %08X %08X %08X %08X %08X %08X %08X\n", address, data[0],
				    data[1], data[2], data[3], data[4], data[5], data[6]);
		break;
	case 24:
		n2d_kernel_os_print("%08X : %08X %08X %08X %08X %08X %08X\n", address, data[0],
				    data[1], data[2], data[3], data[4], data[5]);
		break;
	case 20:
		n2d_kernel_os_print("%08X : %08X %08X %08X %08X %08X\n", address, data[0], data[1],
				    data[2], data[3], data[4]);
		break;
	case 16:
		n2d_kernel_os_print("%08X : %08X %08X %08X %08X\n", address, data[0], data[1],
				    data[2], data[3]);
		break;
	case 12:
		n2d_kernel_os_print("%08X : %08X %08X %08X\n", address, data[0], data[1], data[2]);
		break;
	case 8:
		n2d_kernel_os_print("%08X : %08X %08X\n", address, data[0], data[1]);
		break;
	case 4:
		n2d_kernel_os_print("%08X : %08X\n", address, data[0]);
		break;
	default:
		break;
	}

	return N2D_SUCCESS;
}

static void _verify_dma(n2d_hardware_t *hardware, n2d_uint32_t *Address1, n2d_uint32_t *Address2,
			n2d_uint32_t *State1, n2d_uint32_t *State2)
{
	n2d_uint32_t i;
	n2d_os_t *os	  = hardware->os;
	n2d_uint32_t core = hardware->core;

	*State1	  = n2d_kernel_os_peek_with_core(os, core, 0x660);
	*Address1 = n2d_kernel_os_peek_with_core(os, core, 0x664);

	for (i = 0; i < 500; i++) {
		*State2	  = n2d_kernel_os_peek_with_core(os, core, 0x660);
		*Address2 = n2d_kernel_os_peek_with_core(os, core, 0x664);

		if (*Address1 != *Address2)
			break;

		if (*State1 != *State2)
			break;
	}
}

static void _dump_debug_registers(n2d_hardware_t *hardware, n2d_debug_registers_t *Descriptor)
{
	n2d_uint32_t select;
	n2d_uint32_t data = 0;
	n2d_uint32_t i;
	n2d_os_t *os	  = hardware->os;
	n2d_uint32_t core = hardware->core;

	n2d_kernel_os_print("    %s debug registers:\n", Descriptor->module);

	for (i = 0; i < Descriptor->count; i += 1) {
		select = i << Descriptor->shift;

		n2d_kernel_os_poke_with_core(os, core, Descriptor->index, select);

		data = n2d_kernel_os_peek_with_core(os, core, Descriptor->data);

		n2d_kernel_os_print("      [0x%02X] 0x%08X\n", i, data);
	}

	select = 0xF << Descriptor->shift;

	for (i = 0; i < 500; i += 1) {
		n2d_kernel_os_poke_with_core(os, core, Descriptor->index, select);

		data = n2d_kernel_os_peek_with_core(os, core, Descriptor->data);

		if (data == Descriptor->signature)
			break;
	}

	if (i == 500)
		n2d_kernel_os_print("      failed to obtain the signature (read 0x%08X).\n", data);
	else
		n2d_kernel_os_print("      signature = 0x%08X (%d read attempt(s))\n", data, i + 1);
}

static n2d_bool_t _query_feature_database(n2d_hardware_t *hardware, n2d_feature_t feature)
{
	n2d_bool_t available	       = N2D_FALSE;
	n2d_feature_database *database = (n2d_feature_database *)hardware->featureDatabase;

	if (!database) {
		n2d_kernel_os_print("database is null\n");
		return available;
	}

	switch (feature) {
	case N2D_FEATURE_DEC400_COMPRESSION:
		available = database->G2D_DEC400EX;
		break;

	case N2D_FEATURE_AXI_FE:
		available = database->N2D_FEATURE_AXI_FE;
		break;

	case N2D_FEATURE_MMU_PAGE_DESCRIPTOR:
		available = database->N2D_FEATURE_MMU_PAGE_DESCRIPTOR;
		break;

	case N2D_FEATURE_SECURITY_AHB:
		available = database->N2D_FEATURE_SECURITY_AHB;
		break;

	case N2D_FEATURE_FRAME_DONE_INTR:
		available = database->N2D_FEATURE_FRAME_DONE_INTR;
		break;

	case N2D_FEATURE_64BIT_ADDRESS:
		available = database->N2D_FEATURE_64BIT_ADDRESS;
		break;

	default:
		n2d_kernel_os_print("ERROR: Invalid feature has been requested\n.");
	}
	return available;
}

n2d_error_t n2d_kernel_hardware_notify(n2d_hardware_t *hardware)
{
	return n2d_kernel_event_notify(hardware->event_center);
}

n2d_error_t n2d_kernel_hardware_interrupt(n2d_hardware_t *hardware)
{
	n2d_uint32_t data = 0;
	n2d_error_t error = N2D_SUCCESS;

	/* Read AQIntrAcknowledge register. */
	data = n2d_kernel_os_peek_with_core(hardware->os, hardware->core,
					    AQ_INTR_ACKNOWLEDGE_Address);

	if (N2D_IS_ERROR(error))
		goto on_error;

	if (data == 0) {
		/* Not our interrupt. */
		error = N2D_SUCCESS;
	} else {
		/* Inform event center of the interrupt. */
		error = n2d_kernel_event_interrupt(hardware->event_center, data);
	}

on_error:
	/* Return the error. */
	return error;
}

n2d_error_t n2d_kernel_hardware_query_chip_identity(
	n2d_hardware_t *hardware, n2d_uint32_t *chipModel, n2d_uint32_t *chipRevision,
	n2d_uint32_t *productId, n2d_uint32_t *cid, n2d_uint32_t *chipFeatures,
	n2d_uint32_t *chipMinorFeatures, n2d_uint32_t *chipMinorFeatures1,
	n2d_uint32_t *chipMinorFeatures2, n2d_uint32_t *chipMinorFeatures3,
	n2d_uint32_t *chipMinorFeatures4)
{
	n2d_os_t *os	  = hardware->os;
	n2d_uint32_t core = hardware->core;
	/* Read chip identity register. */
	*chipModel = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_ID_Address);

	/* Read CHIP_REV register. */
	*chipRevision = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_REV_Address);

	*productId = n2d_kernel_os_peek_with_core(os, core, GC_PRODUCT_ID_Address);

	*cid = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_CUSTOMER_Address);

	/* Read chip feature register. */
	*chipFeatures = n2d_kernel_os_peek_with_core(os, core, GC_FEATURES_Address);

	/* Read chip minor feature register #0. */
	*chipMinorFeatures = n2d_kernel_os_peek_with_core(os, core, GC_MINOR_FEATURES0_Address);

#if defined GC_MINOR_FEATURES1_Address
	if (gcmVERIFYFIELDVALUE((*chipMinorFeatures), GC_MINOR_FEATURES0, DEFAULT_REG0,
				AVAILABLE)) {
		/* Read chip minor featuress register #1. */
		*chipMinorFeatures1 =
			n2d_kernel_os_peek_with_core(os, core, GC_MINOR_FEATURES1_Address);

		/* Read chip minor featuress register #2. */
#if defined GC_MINOR_FEATURES2_Address
		*chipMinorFeatures2 =
			n2d_kernel_os_peek_with_core(os, core, GC_MINOR_FEATURES2_Address);
#else
		*chipMinorFeatures2 = 0;
#endif

#if defined GC_MINOR_FEATURES3_Address
		/* Read chip minor featuress register #3. */
		*chipMinorFeatures3 =
			n2d_kernel_os_peek_with_core(os, core, GC_MINOR_FEATURES3_Address);
#else
		*chipMinorFeatures3 = 0;
#endif

#if defined GC_MINOR_FEATURES4_Address
		/* Read chip minor featuress register #4. */
		*chipMinorFeatures4 =
			n2d_kernel_os_peek_with_core(os, core, GC_MINOR_FEATURES4_Address);
#else
		*chipMinorFeatures4 = 0;
#endif
	} else
#endif
	{
		/* Chip doesn't has minor features register #1 or 2 or 3 or 4. */
		*chipMinorFeatures1 = 0;
		*chipMinorFeatures2 = 0;
		*chipMinorFeatures3 = 0;
		*chipMinorFeatures4 = 0;
	}

	/* Success. */
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_hardware_initialize(n2d_hardware_t *hardware)
{
	n2d_error_t error;
	n2d_os_t *os	  = hardware->os;
	n2d_uint32_t core = hardware->core;
	n2d_int32_t data  = 0;
	n2d_uint32_t chipId;
	n2d_uint32_t chipRevision;
	n2d_uint32_t productId;
	n2d_uint32_t cId;

	/* Read chip identity register. */
	chipId = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_ID_Address);
	/* Read CHIP_REV register. */
	chipRevision = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_REV_Address);
	productId    = n2d_kernel_os_peek_with_core(os, core, GC_PRODUCT_ID_Address);
	cId	     = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_CUSTOMER_Address);

	hardware->featureDatabase =
		(n2d_pointer)query_features(chipId, chipRevision, productId, cId);
	if (!hardware->featureDatabase) {
		n2d_kernel_os_print("Cannot find a matching database: \n  chipID:0x%x\n  "
				    "chipV:0x%x\n  productID:0x%x\n  customer:0x%x \n",
				    chipId, chipRevision, productId, cId);
		ONERROR(N2D_NOT_FOUND);
	}

	n2d_kernel_os_poke_with_core(os, core, AQ_HI_CLOCK_CONTROL_Address,
				     AQ_HI_CLOCK_CONTROL_ResetValue);
	/* Disable isolate GPU bit. */
	n2d_kernel_os_poke_with_core(
		os, core, AQ_HI_CLOCK_CONTROL_Address,
		gcmSETFIELD(AQ_HI_CLOCK_CONTROL_ResetValue, AQ_HI_CLOCK_CONTROL, ISOLATE_GPU, 0));

	data = n2d_kernel_os_peek_with_core(os, core, AQ_HI_CLOCK_CONTROL_Address);
	/* Enable debug register. */
	n2d_kernel_os_poke_with_core(
		os, core, AQ_HI_CLOCK_CONTROL_Address,
		gcmSETFIELD(data, AQ_HI_CLOCK_CONTROL, DISABLE_DEBUG_REGISTERS, 0));

	/* AHBDEC400 */
	if (_query_feature_database(hardware, N2D_FEATURE_DEC400_COMPRESSION)) {
		data = GCREG_AHBDEC_CONTROL_ResetValue;
		data = gcmSETFIELDVALUE(data, GCREG_AHBDEC_CONTROL, DISABLE_COMPRESSION, ENABLE);
		n2d_kernel_os_poke_with_core(os, core, GCREG_AHBDEC_CONTROL_Address, data);

		data = GCREG_AHBDEC_CONTROL_EX2_ResetValue;
		data = gcmSETFIELD(data, GCREG_AHBDEC_CONTROL_EX2, TILE_STATUS_READ_ID, 4);
		data = gcmSETFIELD(data, GCREG_AHBDEC_CONTROL_EX2, TILE_STATUS_WRITE_ID, 2);
		n2d_kernel_os_poke_with_core(os, core, GCREG_AHBDEC_CONTROL_EX2_Address, data);
	}

	/*AXI-FE*/
	if (_query_feature_database(hardware, N2D_FEATURE_AXI_FE)) {
		n2d_uint32_t offset = 0;
		ONERROR(n2d_kernel_get_APBRegOffset(hardware, &offset));
		n2d_kernel_os_print("Initialize APB registers, APB offset is 0x%x.\n", offset);

		n2d_kernel_os_poke_with_core(os, core, offset + 0x28, 0x2);
		n2d_kernel_os_poke_with_core(os, core, offset + 0x2C, 0x2);
	}

	if (_query_feature_database(hardware, N2D_FEATURE_64BIT_ADDRESS))
		hardware->support_64bit = N2D_TRUE;

#if NANO2D_MMU_ENABLE
	if (_query_feature_database(hardware, N2D_FEATURE_MMU_PAGE_DESCRIPTOR))
		hardware->mmu_pd_mode = N2D_TRUE;
#endif

	/* Limit 2D outstanding request. */
	chipId	     = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_ID_Address);
	chipRevision = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_REV_Address);
	if ((chipId == 0x320) && ((chipRevision == 0x5007) || (chipRevision == 0x5220))) {
		data = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_TIME_Address);

		n2d_kernel_os_trace("ChipModel=0x%x ChipRevision=0x%x, data=0x%x.\n", chipId,
				    chipRevision, data);

		if (data != 33956864) {
			data = n2d_kernel_os_peek_with_core(os, core, AQ_MEMORY_DEBUG_Address);

			data = gcmSETFIELD(
				data, AQ_MEMORY_DEBUG, MAX_OUTSTANDING_READS,
				chipRevision == 0x5220 ? 8 : (chipRevision == 0x5007 ? 16 : 0));

			n2d_kernel_os_poke_with_core(os, core, AQ_MEMORY_DEBUG_Address, data);
		}
	}

#if NANO2D_MMU_ENABLE
	ONERROR(n2d_mmu_construct(hardware, &hardware->mmu));
	ONERROR(n2d_mmu_enable(&hardware->mmu));
#endif

	/* Success. */
	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_deinitialize(n2d_hardware_t *hardware)
{
	gcmkASSERT(hardware != N2D_NULL);

#if NANO2D_MMU_ENABLE
	n2d_mmu_destroy(&hardware->mmu);
#endif

	/* Success. */
	return N2D_SUCCESS;
}

#if N2D_GPU_TIMEOUT
static void _monitor_timer_func(n2d_pointer data)
{
	n2d_hardware_t *hardware	 = (n2d_hardware_t *)data;
	n2d_event_center_t *event_center = hardware->event_center;
	n2d_int32_t pending		 = 0;
	n2d_bool_t add_time		 = N2D_FALSE;
	n2d_bool_t reset		 = N2D_FALSE;
	n2d_uint32_t cur_total_writes	 = 0;
	n2d_bool_t recovery		 = hardware->kernel->device->recovery;

	if (hardware->monitor_timer_stop)
		/* Stop. */
		return;
	n2d_kernel_os_atom_get(hardware->os, event_center->pending_event_count,
			       (n2d_int32_t *)&pending);

	if (hardware->monitoring == N2D_FALSE) {
		if (pending) {
			/* Begin to monitor GPU state. */
			hardware->monitoring = N2D_TRUE;

			/* Clear timeout. */
			hardware->timer = 0;

			hardware->total_writes =
				n2d_kernel_os_peek_with_core(hardware->os, hardware->core, 0x44);
		}
	} else {
		if (pending) {
			cur_total_writes =
				n2d_kernel_os_peek_with_core(hardware->os, hardware->core, 0x44);

			add_time =
				(cur_total_writes == hardware->total_writes) ? N2D_TRUE : N2D_FALSE;

			if (add_time) {
				/* GPU state is not changed, accumulate timeout. */
				hardware->timer += 1000;

				if (hardware->timer >= hardware->timeout) {
					/* GPU stuck, trigger reset. */
					reset = N2D_TRUE;
				}
			} else {
				/* GPU state changed, cancel current timeout.*/
				hardware->monitoring = N2D_FALSE;
			}
		} else {
			/* GPU finish all jobs, cancel current timeout*/
			hardware->monitoring = N2D_FALSE;
		}
	}

	if (reset) {
		if (recovery)
			n2d_kernel_hardware_recovery(hardware);

		/* Work in this timeout is done. */
		hardware->monitoring = N2D_FALSE;
	}

	n2d_kernel_os_start_timer(hardware->os, hardware->monitor_timer, 1000);
	return;
}

#endif
n2d_error_t n2d_kernel_hardware_start(n2d_hardware_t *hardware)
{
	n2d_error_t error;
	n2d_uint32_t control;
	n2d_os_t *os			    = hardware->os;
	n2d_uint32_t core		    = hardware->core;
	n2d_cmd_buf_t *cmd_buf		    = &hardware->cmd_buf;
	n2d_uint32_t cmd_buf_start_address  = cmd_buf->address;
	n2d_uint32_t *cmd_buf_start_logical = cmd_buf->logical;
	n2d_uint32_t size		    = 0;

	if (hardware->running)
		return N2D_SUCCESS;

	if (hardware->fe_type == HW_FE_WAIT_LINK) {
		/* Init the command buffer in start device . */
		ONERROR(n2d_kernel_hardware_wait_link(hardware, cmd_buf_start_logical,
						      cmd_buf_start_address, &size));

		n2d_kernel_os_memory_barrier(hardware->os, N2D_NULL);

		cmd_buf->wl_current_logical = cmd_buf_start_logical;
		cmd_buf->wl_current_address = cmd_buf_start_address;
		cmd_buf->offset		    = size;

		/*switch to current core*/
		/*n2d_kernel_switch_core(s_cmdbuf.currentCoreId);*/
		/* Start up the command. */
		if (_query_feature_database(hardware, N2D_FEATURE_FRAME_DONE_INTR)) {
			/* Disable interrupt 28bit if support N2D_FEATURE_FRAME_DONE_INTR. */
			n2d_kernel_os_poke_with_core(os, core, AQ_INTR_ENBL_Address,
						     (n2d_uint32_t)0xEFFFFFFF);
			;
		} else {
			n2d_kernel_os_poke_with_core(os, core, AQ_INTR_ENBL_Address,
						     (n2d_uint32_t)~0U);
		}

		n2d_kernel_os_poke_with_core(os, core, AQ_CMD_BUFFER_ADDR_Address,
					     cmd_buf_start_address);

		/* Build control register. */
		control = gcmSETFIELDVALUE(0, AQ_CMD_BUFFER_CTRL, ENABLE, ENABLE) |
			  gcmSETFIELD(0, AQ_CMD_BUFFER_CTRL, PREFETCH, (size + 7) >> 3);

		n2d_kernel_os_poke_with_core(os, core, AQ_CMD_BUFFER_CTRL_Address, control);
	}

#if N2D_GPU_TIMEOUT
	ONERROR(n2d_kernel_os_start_timer(hardware->os, hardware->monitor_timer, 1000));
#endif

	hardware->running = N2D_TRUE;
	/* Success. */
	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_stop(n2d_hardware_t *hardware)
{
	n2d_error_t error;
	n2d_cmd_buf_t *cmd_buf = &hardware->cmd_buf;
	n2d_uint32_t *logical  = cmd_buf->wl_current_logical;

	if (!hardware->running)
		return N2D_SUCCESS;

	if (hardware->fe_type == HW_FE_WAIT_LINK) {
		/*need switch to other core*/
		ONERROR(n2d_kernel_hardware_end(hardware, logical, N2D_NULL));
	}

#if N2D_GPU_TIMEOUT
	/* Stop and monitor timer. */
	if (hardware->monitor_timer)
		ONERROR(n2d_kernel_os_stop_timer(hardware->os, hardware->monitor_timer));
#endif

	hardware->running = N2D_FALSE;
	/* Success. */

	return N2D_SUCCESS;

on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_event(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				      n2d_uint32_t eventid, n2d_uint32_t *size)
{
	n2d_uint32_t *memory = N2D_NULL;

	gcmkASSERT(hardware != N2D_NULL);

	if (logical != N2D_NULL) {
		memory = logical;

		/* Append EVENT(Event, destination). */
		memory[0] =
			gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE) |
			gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS, AQEventRegAddrs) |
			gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

		memory[1] = gcmSETFIELD(gcmSETFIELDVALUE(0, AQ_EVENT, PE_SRC, ENABLE), AQ_EVENT,
					EVENT_ID, eventid);
	}

	if (size != N2D_NULL)
		*size = 8;

	/* Success. */
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_hardware_wait_link(n2d_hardware_t *hardware, n2d_uint32_t *logical,
					  n2d_uint64_t address, n2d_uint32_t *size)
{
	n2d_uint32_t bytes    = 16;
	n2d_uint32_t _address = 0;

	gcmkASSERT(hardware != N2D_NULL);

	N2D_SAFECASTVA(_address, address);

	if (logical != N2D_NULL) {
		/* Append WAIT(count). */
		logical[0] = gcmSETFIELDVALUE(0, AQ_COMMAND_WAIT_COMMAND, OPCODE, WAIT) |
			     gcmSETFIELD(0, AQ_COMMAND_WAIT_COMMAND, DELAY, 20);

		logical[1] = 0;

		/* Append LINK(2, address). */
		logical[2] = gcmSETFIELDVALUE(0, AQ_COMMAND_LINK_COMMAND, OPCODE, LINK) |
			     gcmSETFIELD(0, AQ_COMMAND_LINK_COMMAND, PREFETCH, bytes >> 3);

		logical[3] = _address;
	}

	/* Return the wait link bytes */
	if (size != N2D_NULL)
		*size = 16;

	/* Success. */
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_hardware_link(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				     n2d_uint64_t address, n2d_uint32_t bytes, n2d_uint32_t *size)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_uint32_t data     = 0;
	n2d_uint32_t _address = 0;

	if (logical != N2D_NULL) {
		if (bytes < 8)
			return N2D_INVALID_ARGUMENT;
		/*
		 *The address of the modification in the kernel is aligned with 0xF,
		 *and the address of the modification in the user is aligned with 0x7.
		 *Take the intersection.
		 */
		N2D_SAFECASTVA(_address, address);

		if (_address & 0x7)
			return N2D_NOT_ALIGNED;

		// bytes = gcmALIGN(address + bytes, 64) - address;
		/* if the logical is user logical ?*/
		ONERROR(n2d_kernel_os_memory_write(hardware->os, logical + 1, _address));

		data = gcmSETFIELDVALUE(0, AQ_COMMAND_LINK_COMMAND, OPCODE, LINK) |
		       gcmSETFIELD(0, AQ_COMMAND_LINK_COMMAND, PREFETCH, bytes >> 3);
		ONERROR(n2d_kernel_os_memory_write(hardware->os, logical, data));
	}

	if (size != N2D_NULL)
		*size = 8;

on_error:
	/* Success. */
	return error;
}

n2d_error_t n2d_kernel_hardware_nop(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				    n2d_uint32_t *size)
{
	gcmkASSERT(hardware != N2D_NULL);

	*logical++ = gcmSETFIELDVALUE(0, AQ_COMMAND_NOP_COMMAND, OPCODE, NOP);
	*logical++ = 0;

	if (size)
		*size = 8;

	/* Success. */
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_hardware_end(n2d_hardware_t *hardware, n2d_uint32_t *logical,
				    n2d_uint32_t *size)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t data = 0;

	if (logical != N2D_NULL) {
		/* Append END. */
		data = gcmSETFIELDVALUE(0, AQ_COMMAND_END_COMMAND, OPCODE, END);
		ONERROR(n2d_kernel_os_memory_write(hardware->os, logical, data));
		ONERROR(n2d_kernel_os_memory_write(hardware->os, logical + 1,
						   hardware->event_count));

		hardware->event_count++;
		if (hardware->event_count == 0xFFFFFFFF)
			hardware->event_count = 0;
	}

	if (size != N2D_NULL)
		*size = 8;

	/* Success. */
	return N2D_SUCCESS;
on_error:
	/* Success. */
	return error;
}

static n2d_error_t _execute_once(n2d_hardware_t *hardware, n2d_uint64_t address, n2d_uint32_t size,
				 n2d_bool_t *stuck)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_os_t *os	      = hardware->os;
	n2d_uint32_t core     = hardware->core;
	n2d_uint32_t control  = 0;
	n2d_uint32_t idle     = 0;
	n2d_uint32_t delay    = 0;
	n2d_uint32_t _address = 0;

	N2D_SAFECASTVA(_address, address);

	/* Start command */
	n2d_kernel_os_poke_with_core(os, core, AQ_INTR_ENBL_Address, (n2d_uint32_t)~0U);
	n2d_kernel_os_poke_with_core(os, core, AQ_CMD_BUFFER_ADDR_Address, _address);
	/* build control reg */
	control = gcmSETFIELDVALUE(0, AQ_CMD_BUFFER_CTRL, ENABLE, ENABLE) |
		  gcmSETFIELD(0, AQ_CMD_BUFFER_CTRL, PREFETCH, (size + 7) >> 3);
	/* Start FE */
#if !defined(EMULATOR) && !defined(LINUXEMULATOR)
	n2d_kernel_os_poke_with_core(os, core, AQ_CMD_BUFFER_CTRL_Address, control);
#else
	n2d_kernel_os_poke_with_core(os, core, GCREG_CMD_BUFFER_AHB_CTRL_Address, control);
#endif

	/* Make sure FE is idle. */
	do {
		idle = n2d_kernel_os_peek_with_core(os, core, AQ_HI_IDLE_Address);

		n2d_kernel_os_delay((n2d_os_t *)N2D_NULL, 2);
		/*delay 10ms*/
		delay += 2;

		if (delay > 20000) {
			*stuck = N2D_TRUE;
			ONERROR(N2D_TIMEOUT);
		}

	} while (!gcmGETFIELD(idle, AQ_HI_IDLE, IDLE_FE));

	return N2D_SUCCESS;
on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_commit(n2d_hardware_t *hardware, n2d_uint32_t process,
				       n2d_kernel_command_commit_t *iface)
{
	n2d_error_t error;
	n2d_uint32_t *user_command_tail	  = N2D_NULL;
	n2d_uint32_t user_command_size	  = iface->size;
	n2d_uint64_t user_command_address = iface->address + iface->offset;
	n2d_cmd_buf_t *cmd_buf		  = &hardware->cmd_buf;
	n2d_uint32_t *entry_logical	  = N2D_NULL;
	n2d_uint64_t entry_address	  = 0;
	n2d_user_event_t *queue		  = gcmUINT64_TO_PTR(iface->queue);
	n2d_bool_t get_commit_mutex	  = N2D_FALSE;
	n2d_bool_t execute_stuck	  = N2D_FALSE;
#if NANO2D_MMU_ENABLE
	n2d_bool_t need_flush	       = N2D_FALSE;
	n2d_uint32_t mmu_entry_address = 0, mmu_flush_size = 0;
#endif

	/* flush node cache. */
	n2d_vidmem_node_t *flush_node = N2D_NULL;
	n2d_uintptr_t data	      = 0;

	ONERROR(n2d_kernel_db_get(hardware->kernel->db, process, iface->handle, N2D_NULL, &data));
	flush_node = (n2d_vidmem_node_t *)(n2d_uintptr_t)data;

	ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, cmd_buf->commit_mutex, N2D_INFINITE));
	get_commit_mutex = N2D_TRUE;

#if NANO2D_MMU_ENABLE
	n2d_kernel_hardware_mmu_flush(hardware, N2D_NULL, N2D_NULL, &mmu_flush_size);
#endif

	if (hardware->fe_type == HW_FE_WAIT_LINK) {
		n2d_uint32_t link_size = 0, wait_link_size = 0;

		ONERROR(n2d_kernel_hardware_link(hardware, N2D_NULL, 0, 0, &link_size));
		ONERROR(n2d_kernel_hardware_wait_link(hardware, N2D_NULL, 0, &wait_link_size));
		user_command_tail = (n2d_uint32_t *)(n2d_uintptr_t)(iface->logical + iface->offset +
								    iface->size - link_size);
		/*
		 * If the remaining kernel cmd_buf isn't enough, start from the beginning.
		 * Need to be placed at the end of user cmd_buf before modification, to avoid user
		 * cmd_buf pointing to the wrong address
		 */
#if NANO2D_MMU_ENABLE
		if ((cmd_buf->size - cmd_buf->offset) < (wait_link_size * 2 + mmu_flush_size))
			cmd_buf->offset = 0;

		n2d_kernel_hardware_mmu_flush(hardware, &hardware->mmu, &need_flush, N2D_NULL);
		if (need_flush) {
			mmu_entry_address = cmd_buf->address + cmd_buf->offset;
			cmd_buf->offset += mmu_flush_size;
			entry_logical =
				(n2d_uint32_t *)((n2d_uint8_t *)cmd_buf->logical + cmd_buf->offset);
			entry_address = cmd_buf->address + cmd_buf->offset;
			/* Append a new wait-link */
			ONERROR(n2d_kernel_hardware_wait_link(hardware, entry_logical,
							      entry_address, N2D_NULL));
			ONERROR(n2d_kernel_hardware_link(
				hardware, cmd_buf->wl_current_logical, mmu_entry_address,
				mmu_flush_size + wait_link_size, N2D_NULL));
			cmd_buf->wl_current_logical = entry_logical;
			cmd_buf->wl_current_address = entry_address;
			cmd_buf->offset += wait_link_size;
		}
#else
		if ((cmd_buf->size - cmd_buf->offset) < wait_link_size)
			cmd_buf->offset = 0;
#endif

		entry_logical = (n2d_uint32_t *)((n2d_uint8_t *)cmd_buf->logical + cmd_buf->offset);
		entry_address = cmd_buf->address + cmd_buf->offset;
		/* Append a new wait-link */
		ONERROR(n2d_kernel_hardware_wait_link(hardware, entry_logical, entry_address,
						      N2D_NULL));
		/* Link the user command to new wait-link*/
		ONERROR(n2d_kernel_hardware_link(hardware, user_command_tail, entry_address,
						 wait_link_size, N2D_NULL));
		/* Link to user command */
		ONERROR(n2d_kernel_hardware_link(hardware, cmd_buf->wl_current_logical,
						 user_command_address,
						 user_command_size + link_size, N2D_NULL));

		cmd_buf->wl_current_logical = entry_logical;
		cmd_buf->wl_current_address = entry_address;
		cmd_buf->offset += wait_link_size;

		cmd_buf->executed_cmd_fifo[cmd_buf->fifo_index].address = user_command_address;
		cmd_buf->executed_cmd_fifo[cmd_buf->fifo_index].size	= user_command_size;
		cmd_buf->fifo_index = (cmd_buf->fifo_index + 1) % CMD_FIFO_SIZE;
	} else if (hardware->fe_type == HW_FE_END) {
		n2d_uint32_t end_size = 0;

		n2d_kernel_hardware_end(hardware, N2D_NULL, &end_size);
		user_command_tail = (n2d_uint32_t *)(n2d_uintptr_t)(iface->logical + iface->offset +
								    iface->size - end_size);

#if NANO2D_MMU_ENABLE
		if ((cmd_buf->size - cmd_buf->offset) < (end_size + mmu_flush_size))
			cmd_buf->offset = 0;
		n2d_kernel_hardware_mmu_flush(hardware, &hardware->mmu, &need_flush, N2D_NULL);
		if (need_flush) {
			mmu_entry_address = cmd_buf->address + cmd_buf->offset;
			cmd_buf->offset += mmu_flush_size;
			entry_logical =
				(n2d_uint32_t *)((n2d_uint8_t *)cmd_buf->logical + cmd_buf->offset);
			ONERROR(n2d_kernel_hardware_end(hardware, entry_logical, N2D_NULL));
			cmd_buf->offset += end_size;

			/* execute flush command, then end. */
			ONERROR(_execute_once(hardware, mmu_entry_address,
					      mmu_flush_size + end_size, &execute_stuck));
		}
#endif

		cmd_buf->executed_cmd_fifo[cmd_buf->fifo_index].address = user_command_address;
		cmd_buf->executed_cmd_fifo[cmd_buf->fifo_index].size	= user_command_size;
		cmd_buf->fifo_index = (cmd_buf->fifo_index + 1) % CMD_FIFO_SIZE;

		/* execute user cmd buf, then end. */
		ONERROR(n2d_kernel_hardware_end(hardware, user_command_tail, &end_size));
		/* flush node cache. */
		ONERROR(n2d_kernel_vidmem_cache_op(hardware->kernel, flush_node, 0));

#if NANO2D_PCIE_ORDER
		n2d_kernel_os_memory_read(hardware->os, user_command_tail, N2D_NULL);
#endif
		ONERROR(_execute_once(hardware, user_command_address, user_command_size,
				      &execute_stuck));
	} else {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);

	if (queue)
		ONERROR(n2d_kernel_event_commit(hardware->event_center, queue, N2D_TRUE));

	/* Success. */
	return N2D_SUCCESS;

on_error:
	if (get_commit_mutex)
		n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);

	if (execute_stuck)
		n2d_kernel_hardware_dump_gpu_state(hardware);

	return error;
}

static n2d_error_t _hardware_set_power_off(n2d_hardware_t *hardware, n2d_kernel_power_state_t state)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_int32_t time  = 1000;
	n2d_bool_t idle	  = N2D_FALSE;

	switch (hardware->power_state) {
	case N2D_POWER_ON:
		if (state == N2D_POWER_IDLE)
			break;
		fallthrough;

	case N2D_POWER_IDLE:
		if (state == N2D_POWER_SUSPEND)
			break;
		fallthrough;

	case N2D_POWER_SUSPEND:
		do {
			ONERROR(n2d_kernel_hardware_query_idle(hardware, &idle));
			if (!idle)
				n2d_kernel_os_delay(hardware->os, 2);
			time -= 2;
		} while (!idle && time > 0);

		if (time <= 0 && !idle)
			return N2D_TIMEOUT;

		ONERROR(n2d_kernel_hardware_stop(hardware));
		/* Clock off. */
		ONERROR(n2d_kernel_os_set_gpu_clock(hardware->os, N2D_FALSE));
		/* Power off. */
		ONERROR(n2d_kernel_os_set_gpu_power(hardware->os, N2D_FALSE));
		break;
	default:
		break;
	}
on_error:
	return error;
}

static n2d_error_t _hardware_set_power_on(n2d_hardware_t *hardware, n2d_kernel_power_state_t state)
{
	n2d_error_t error = N2D_SUCCESS;

	switch (hardware->power_state) {
	case N2D_POWER_OFF:
		if (state == N2D_POWER_SUSPEND)
			break;
		fallthrough;

	case N2D_POWER_SUSPEND:
		if (state == N2D_POWER_IDLE)
			break;
		fallthrough;

	case N2D_POWER_IDLE:
		/* Power on. */
		ONERROR(n2d_kernel_os_set_gpu_power(hardware->os, N2D_TRUE));
		/* Clock on. */
		ONERROR(n2d_kernel_os_set_gpu_clock(hardware->os, N2D_TRUE));
		ONERROR(n2d_kernel_hardware_initialize(hardware));
		ONERROR(n2d_kernel_hardware_start(hardware));
		break;
	default:
		break;
	}
on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_set_power(n2d_hardware_t *hardware, n2d_kernel_power_state_t state)
{
	n2d_error_t error	    = N2D_SUCCESS;
	n2d_cmd_buf_t *cmd_buf	    = &hardware->cmd_buf;
	n2d_bool_t get_commit_mutex = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, cmd_buf->commit_mutex, 0));
	get_commit_mutex = N2D_TRUE;

	if (state > hardware->power_state)
		ONERROR(_hardware_set_power_off(hardware, state));
	else if (state < hardware->power_state)
		ONERROR(_hardware_set_power_on(hardware, state));

	hardware->power_state = state;
on_error:
	if (get_commit_mutex)
		n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);
	return error;
}

n2d_error_t n2d_kernel_harware_dump_command(n2d_hardware_t *hardware)
{
	n2d_error_t error	    = N2D_SUCCESS;
	n2d_uint32_t i		    = 0;
	n2d_bool_t get_commit_mutex = N2D_FALSE;
	n2d_cmd_buf_t *cmd_buf	    = &hardware->cmd_buf;
	n2d_uint64_t address;
	n2d_uint32_t size;
	n2d_uint64_t physical = 0;
	n2d_pointer klogical  = N2D_NULL;

	ONERROR(n2d_kernel_os_mutex_acquire(hardware->os, cmd_buf->commit_mutex, 0));
	get_commit_mutex = N2D_TRUE;

	_dump_buffer(cmd_buf->logical, cmd_buf->address, cmd_buf->size);

	n2d_kernel_os_print("GPU user command:\n");
	for (i = 0; i < CMD_FIFO_SIZE; i++) {
		address = cmd_buf->executed_cmd_fifo[i].address;
		size	= cmd_buf->executed_cmd_fifo[i].size;
		if (size > 8) {
			n2d_kernel_os_print("command: [%lX %lX] bytes: 0x%X\n", address,
					    address + size, size);
#if NANO2D_MMU_ENABLE
			ONERROR(n2d_mmu_get_physical(&hardware->mmu, address, &physical));
#else
			physical	= address;
#endif
			ONERROR(n2d_kernel_os_gpu_to_cpu_phy(hardware->os, physical, &physical));
			if (physical) {
				ONERROR(n2d_kernel_os_map_cpu(hardware->os, 0, physical, N2D_NULL,
							      size, N2D_ALLOC_FLAG_CONTIGUOUS,
							      N2D_MAP_TO_KERNEL, &klogical));
				if (klogical)
					_dump_buffer(klogical, address, size);
				ONERROR(n2d_kernel_os_unmap_cpu(hardware->os, 0, klogical, size,
								N2D_MAP_TO_KERNEL));
			}
		}
	}
on_error:
	if (get_commit_mutex)
		n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);
	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_hardware_dump_gpu_state(n2d_hardware_t *hardware)
{
	n2d_uint32_t core = hardware->core;
	n2d_os_t *os	  = hardware->os;

	static const char *_cmdState[] = {
		"PAR_IDLE_ST",	  "PAR_DEC_ST",	      "PAR_ADR0_ST",	   "PAR_LOAD0_ST",
		"PAR_ADR1_ST",	  "PAR_LOAD1_ST",     "PAR_3DADR_ST",	   "PAR_3DCMD_ST",
		"PAR_3DCNTL_ST",  "PAR_3DIDXCNTL_ST", "PAR_INITREQDMA_ST", "PAR_DRAWIDX_ST",
		"PAR_DRAW_ST",	  "PAR_2DRECT0_ST",   "PAR_2DRECT1_ST",	   "PAR_2DDATA0_ST",
		"PAR_2DDATA1_ST", "PAR_WAITFIFO_ST",  "PAR_WAIT_ST",	   "PAR_LINK_ST",
		"PAR_END_ST",	  "PAR_STALL_ST"};

	static const char *_cmdDmaState[] = {"CMD_IDLE_ST", "CMD_START_ST", "CMD_REQ_ST",
					     "CMD_END_ST"};

	static const char *_cmdFetState[] = {"FET_IDLE_ST", "FET_RAMVALID_ST", "FET_VALID_ST"};

	static const char *_reqDmaState[] = {"REQ_IDLE_ST", "REQ_WAITIDX_ST", "REQ_CAL_ST"};

	static const char *_calState[] = {"CAL_IDLE_ST", "CAL_LDADR_ST", "CAL_IDXCALC_ST"};

	static const char *_veReqState[] = {"VER_IDLE_ST", "VER_CKCACHE_ST", "VER_MISS_ST"};

	static n2d_debug_registers_t _dbgRegs[] = {
		{"RA", 0x474, 16, 0x448, 16, 0x12344321}, {"TX", 0x474, 24, 0x44C, 16, 0x12211221},
		{"FE", 0x470, 0, 0x450, 16, 0xBABEF00D},  {"PE", 0x470, 16, 0x454, 16, 0xBABEF00D},
		{"DE", 0x470, 8, 0x458, 16, 0xBABEF00D},  {"SH", 0x470, 24, 0x45C, 16, 0xDEADBEEF},
		{"PA", 0x474, 0, 0x460, 16, 0x0000AAAA},  {"SE", 0x474, 8, 0x464, 16, 0x5E5E5E5E},
		{"MC", 0x478, 0, 0x468, 16, 0x12345678},  {"HI", 0x478, 8, 0x46C, 16, 0xAAAAAAAA}};

	static n2d_uint32_t _otherRegs[] = {
		0x040, 0x044, 0x04C, 0x050, 0x054, 0x058, 0x05C, 0x060, 0x43c, 0x440, 0x444, 0x414,
	};

	n2d_uint32_t idle, axi;
	n2d_uint32_t dmaAddress1, dmaAddress2;
	n2d_uint32_t dmaState1, dmaState2;
	n2d_uint32_t dmaLow, dmaHigh;
	n2d_uint32_t cmdState, cmdDmaState, cmdFetState;
	n2d_uint32_t dmaReqState, calState, veReqState;
	n2d_uint32_t pipe = 0, pixelPipes = 0;
	n2d_uint32_t i;
	n2d_uint32_t control, oldControl;
	n2d_uint32_t chipModel, chipRevision, specs;

	chipModel    = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_ID_Address);
	chipRevision = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_REV_Address);

	n2d_kernel_os_print("ChipModel=0x%x ChipRevision=0x%x:\n", chipModel, chipRevision);

	specs	   = n2d_kernel_os_peek_with_core(os, core, GC_CHIP_SPECS_Address);
	pixelPipes = gcmGETFIELD(specs, GC_CHIP_SPECS, NUM_PIXEL_PIPES);
	/* Default pixelPipes is 1 for 2D. */
	if (0 == pixelPipes)
		pixelPipes = 1;

	/* Reset register values. */
	// idle = axi = dmaState1 = dmaState2 = dmaAddress1 = dmaAddress2 = dmaLow = dmaHigh = 0;

	/* Verify whether DMA is running. */
	_verify_dma(hardware, &dmaAddress1, &dmaAddress2, &dmaState1, &dmaState2);

	cmdState    = dmaState2 & 0x1F;
	cmdDmaState = (dmaState2 >> 8) & 0x03;
	cmdFetState = (dmaState2 >> 10) & 0x03;
	dmaReqState = (dmaState2 >> 12) & 0x03;
	calState    = (dmaState2 >> 14) & 0x03;
	veReqState  = (dmaState2 >> 16) & 0x03;

	idle	= n2d_kernel_os_peek_with_core(os, core, 0x4);
	axi	= n2d_kernel_os_peek_with_core(os, core, 0xc);
	dmaLow	= n2d_kernel_os_peek_with_core(os, core, 0x668);
	dmaHigh = n2d_kernel_os_peek_with_core(os, core, 0x66c);

	n2d_kernel_os_print("**************************\n");
	n2d_kernel_os_print("***   GPU STATE DUMP   ***\n");
	n2d_kernel_os_print("**************************\n");
	n2d_kernel_os_print("*********CORE[%d]*********\n", hardware->core);

	n2d_kernel_os_print("  axi      = 0x%08X\n", axi);

	n2d_kernel_os_print("  idle     = 0x%08X\n", idle);

	if ((idle & 0x00000001) == 0)
		n2d_kernel_os_print("    FE not idle\n");
	if ((idle & 0x00000002) == 0)
		n2d_kernel_os_print("    DE not idle\n");
	if ((idle & 0x00000004) == 0)
		n2d_kernel_os_print("    PE not idle\n");
	if ((idle & 0x00000008) == 0)
		n2d_kernel_os_print("    SH not idle\n");
	if ((idle & 0x00000010) == 0)
		n2d_kernel_os_print("    PA not idle\n");
	if ((idle & 0x00000020) == 0)
		n2d_kernel_os_print("    SE not idle\n");
	if ((idle & 0x00000040) == 0)
		n2d_kernel_os_print("    RA not idle\n");
	if ((idle & 0x00000080) == 0)
		n2d_kernel_os_print("    TX not idle\n");
	if ((idle & 0x00000100) == 0)
		n2d_kernel_os_print("    VG not idle\n");
	if ((idle & 0x00000200) == 0)
		n2d_kernel_os_print("    IM not idle\n");
	if ((idle & 0x00000400) == 0)
		n2d_kernel_os_print("    FP not idle\n");
	if ((idle & 0x00000800) == 0)
		n2d_kernel_os_print("    TS not idle\n");
	if ((idle & 0x80000000) != 0)
		n2d_kernel_os_print("    AXI low power mode\n");

	if ((dmaAddress1 == dmaAddress2) && (dmaState1 == dmaState2)) {
		n2d_kernel_os_print("  DMA appears to be stuck at this address:\n");
		n2d_kernel_os_print("    0x%08X\n", dmaAddress1);
	} else {
		if (dmaAddress1 == dmaAddress2) {
			n2d_kernel_os_print("  DMA address is constant, but state is changing:\n");
			n2d_kernel_os_print("    0x%08X\n", dmaState1);
			n2d_kernel_os_print("    0x%08X\n", dmaState2);
		} else {
			n2d_kernel_os_print("  DMA is running; known addresses are:\n");
			n2d_kernel_os_print("    0x%08X\n", dmaAddress1);
			n2d_kernel_os_print("    0x%08X\n", dmaAddress2);
		}
	}

	n2d_kernel_os_print("  dmaLow   = 0x%08X\n", dmaLow);
	n2d_kernel_os_print("  dmaHigh  = 0x%08X\n", dmaHigh);
	n2d_kernel_os_print("  dmaState = 0x%08X\n", dmaState2);
	n2d_kernel_os_print("    command state       = %d (%s)\n", cmdState, _cmdState[cmdState]);
	n2d_kernel_os_print("    command DMA state   = %d (%s)\n", cmdDmaState,
			    _cmdDmaState[cmdDmaState]);
	n2d_kernel_os_print("    command fetch state = %d (%s)\n", cmdFetState,
			    _cmdFetState[cmdFetState]);
	n2d_kernel_os_print("    DMA request state   = %d (%s)\n", dmaReqState,
			    _reqDmaState[dmaReqState]);
	n2d_kernel_os_print("    cal state           = %d (%s)\n", calState, _calState[calState]);
	n2d_kernel_os_print("    VE request state    = %d (%s)\n", veReqState,
			    _veReqState[veReqState]);

	/* Record control. */
	oldControl = n2d_kernel_os_peek_with_core(os, core, 0x0);

	for (pipe = 0; pipe < pixelPipes; pipe++) {
		n2d_kernel_os_print("  Debug registers of pipe[%d]:\n", pipe);

		/* Switch pipe. */
		control = n2d_kernel_os_peek_with_core(os, core, 0x0);
		control &= ~(0xF << 20);
		control |= (pipe << 20);
		n2d_kernel_os_poke_with_core(os, core, 0x0, control);

		for (i = 0; i < N2D_COUNTOF(_dbgRegs); i += 1)
			_dump_debug_registers(hardware, &_dbgRegs[i]);

		n2d_kernel_os_print("    Other Registers:\n");
		for (i = 0; i < N2D_COUNTOF(_otherRegs); i += 1) {
			n2d_kernel_os_print("      [0x%04X] 0x%08X\n", _otherRegs[i],
					    n2d_kernel_os_peek_with_core(os, core, _otherRegs[i]));
		}
	}

	n2d_kernel_os_print("\n\n");

	/* Restore control. */
	n2d_kernel_os_poke_with_core(os, core, 0x0, oldControl);

	n2d_kernel_event_dump(hardware->event_center);

	n2d_kernel_os_print("GPU kernel command:\n");
	n2d_kernel_harware_dump_command(hardware);

	/* Success. */
	return N2D_SUCCESS;
}

#if NANO2D_MMU_ENABLE
n2d_error_t _program_mmu_states(n2d_mmu_t *mmu, n2d_int32_t secure_mode, n2d_pointer logical,
				n2d_int32_t *bytes)
{
	n2d_error_t error	   = N2D_SUCCESS;
	n2d_uint32_t reserve_bytes = 0;
	n2d_bool_t ace		   = N2D_TRUE;
	n2d_bool_t big_endian	   = N2D_FALSE;
	n2d_uint32_t *buffer	   = (n2d_uint32_t *)logical;
	n2d_uint32_t config, safe_address;
	n2d_uint32_t ext_mtlb, ext_safe_address, config_ex = 0;
	n2d_uint64_t physical;

	if (secure_mode) {
		reserve_bytes = 40;
	} else {
		reserve_bytes = 48;

		if (ace)
			reserve_bytes += 8;
	}

	physical = mmu->config->mtlb_physical;
	config	 = (n2d_uint32_t)(physical & 0xFFFFFFFF);
	ext_mtlb = (n2d_uint32_t)(physical >> 32);

	/* more than 40bit physical address */
	if (ext_mtlb & 0xFFFFFF00)
		ONERROR(N2D_NOT_SUPPORTED);

	physical = mmu->config->safe_page_physical;

	safe_address	 = (n2d_uint32_t)(physical & 0xFFFFFFFF);
	ext_safe_address = (n2d_uint32_t)(physical >> 32);

	if (safe_address & 0x3F)
		ONERROR(N2D_NOT_ALIGNED);

	/* more than 40bit physical address */
	if (ext_safe_address & 0xFFFFFF00)
		ONERROR(N2D_NOT_SUPPORTED);

	if (ace) {
		config_ex = gcmSETFIELD(0, GCREG_MMU_CONFIGURATION_EXT, SAFE_ADDRESS,
					ext_safe_address) |
			    gcmSETFIELD(0, GCREG_MMU_CONFIGURATION_EXT, MASTER_TLB, ext_mtlb);
	}

	if (config & 0xFFF)
		ONERROR(N2D_NOT_ALIGNED);

	config |= gcmSETFIELDVALUE(0, GCREG_MMU_CONFIGURATION, MODE, MODE4_K);

	if (logical) {
		if (secure_mode) {
			n2d_mmu_table_array_entry *entry =
				(n2d_mmu_table_array_entry *)mmu->config->page_table_array_logical;
			/* Setup page table array entry. */
			if (big_endian) {
				entry->low  = gcmBSWAP32(config);
				entry->high = gcmBSWAP32(ext_mtlb);
			} else {
				entry->low  = config;
				entry->high = ext_mtlb;
			}
			/* Setup command buffer to load index 0 of page table array. */
			*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
						     LOAD_STATE) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
						gcregMMUConfigRegAddrs) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);
			*buffer++ = gcmSETMASKEDFIELD(GCREG_MMU_CONFIG, PAGE_TABLE_ID, 0);
		} else {
			*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
						     LOAD_STATE) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
						gcregMMUConfigurationRegAddrs) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

			*buffer++ = config;

			*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
						     LOAD_STATE) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
						gcregMMUSafeAddressRegAddrs) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

			*buffer++ = safe_address;

			if (ace) {
				*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND,
							     OPCODE, LOAD_STATE) |
					    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
							gcregMMUConfigurationExtRegAddrs) |
					    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

				*buffer++ = config_ex;
			}
		} /*end of secure_mode else*/

		/*VIV:[TODO] Enable SH store counters in the right place */
		*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE) |
			    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
					gcregStoreCounterRegAddrs) |
			    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);

		*buffer++ = gcmSETFIELD(0, GCREG_STORE_COUNTER, SOURCE_SH, 1);

		gcmSEMAPHORESTALL(buffer);

		*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_END_COMMAND, OPCODE, END);
		*buffer++ = 0x0;

		if (bytes)
			*bytes = reserve_bytes;
	} /*end of logical*/
on_error:
	return error;
}

n2d_error_t _config_mmu_from_cmd(n2d_mmu_t *mmu, n2d_int32_t secure_mode, n2d_bool_t pd_mode)
{
	n2d_error_t error  = N2D_SUCCESS;
	n2d_uint32_t core  = mmu->hardware->core;
	n2d_os_t *os	   = mmu->hardware->os;
	n2d_int32_t bytes  = 0;
	n2d_uint32_t idle  = 0;
	n2d_uint32_t delay = 0;
	n2d_uint32_t data  = 0;

	if (secure_mode) {
		n2d_uint32_t ext_safe_address, address;
		/* Set up base address of page table array. */

		if (pd_mode) {
			n2d_uint32_t ext_phy =
				(n2d_uint32_t)(mmu->config->mmu_init_buffer_physical >> 32);
			n2d_uint32_t tmp_value;

			tmp_value = gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTROL, MMU, ENABLE);

			/* If mmu init buffer is above 4G, we should enable mmu and configure it
			 * with bypass mode. */
			n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_CONTROL_Address,
						     tmp_value);

			n2d_kernel_os_poke_with_core(
				os, core, GCREG_MMUAHB_CONTEXT_PD_ENTRY0_Address,
				gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTEXT_PD_ENTRY0, MASTER_TLB_MODE,
						 BY_PASS_MODE) |
					gcmSETFIELD(0, GCREG_MMUAHB_CONTEXT_PD_ENTRY0, PD_ADDRESS,
						    (ext_phy << 20)));
		}

		n2d_kernel_os_poke_with_core(
			os, core, GCREG_MMUAHB_TABLE_ARRAY_BASE_ADDRESS_LOW_Address,
			(n2d_uint32_t)(mmu->config->page_table_array_physical & 0xFFFFFFFF));

		n2d_kernel_os_poke_with_core(
			os, core, GCREG_MMUAHB_TABLE_ARRAY_BASE_ADDRESS_HIGH_Address,
			(n2d_uint32_t)((mmu->config->page_table_array_physical >> 32) &
				       0xFFFFFFFF));

		n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_TABLE_ARRAY_SIZE_Address, 1);

		address		 = (n2d_uint32_t)(mmu->config->safe_page_physical & 0xFFFFFFFF);
		ext_safe_address = (n2d_uint32_t)(mmu->config->safe_page_physical >> 32);

		if (address & 0x3F)
			ONERROR(N2D_NOT_ALIGNED);
		/* more than 40bit physical address */
		if (ext_safe_address & 0xFFFFFF00)
			ONERROR(N2D_NOT_SUPPORTED);

		n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_SAFE_SECURE_ADDRESS_Address,
					     address);

		n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_SAFE_NON_SECURE_ADDRESS_Address,
					     address);

		n2d_kernel_os_poke_with_core(
			os, core, GCREG_MMUAHB_SAFE_ADDRESS_EXT_Address,
			gcmSETFIELD(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT, SAFE_ADDRESS_SECURE,
				    (n2d_uint32_t)ext_safe_address) |
				gcmSETFIELDVALUE(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT,
						 MASK_SAFE_ADDRESS_SECURE, ENABLED) |
				gcmSETFIELD(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT,
					    SAFE_ADDRESS_NON_SECURE,
					    (n2d_uint32_t)ext_safe_address) |
				gcmSETFIELDVALUE(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT,
						 MASK_SAFE_ADDRESS_NON_SECURE, ENABLED));
	}

	ONERROR(_program_mmu_states(mmu, secure_mode, mmu->config->mmu_init_buffer_logical,
				    &bytes));

	/* VIV run mmu init command buffer*/
	n2d_kernel_os_poke_with_core(os, core, AQ_INTR_ENBL_Address, (n2d_uint32_t)~0U);
	n2d_kernel_os_poke_with_core(os, core, AQ_CMD_BUFFER_ADDR_Address,
				     mmu->config->mmu_init_buffer_physical);

	/* Build control register. */
	data = gcmSETFIELDVALUE(0, AQ_CMD_BUFFER_CTRL, ENABLE, ENABLE) |
	       gcmSETFIELD(0, AQ_CMD_BUFFER_CTRL, PREFETCH, (bytes + 7) >> 3);
#if !defined(EMULATOR) && !defined(LINUXEMULATOR)
	n2d_kernel_os_poke_with_core(os, core, AQ_CMD_BUFFER_CTRL_Address, data);
#endif
	n2d_kernel_os_poke_with_core(os, core, GCREG_CMD_BUFFER_AHB_CTRL_Address, data);

	do {
		idle = n2d_kernel_os_peek_with_core(os, core, AQ_HI_IDLE_Address);

		n2d_kernel_os_delay((n2d_os_t *)N2D_NULL, 2);
		/*delay 10ms*/
		delay += 2;

		if (delay > 100) {
			n2d_kernel_hardware_dump_gpu_state(mmu->hardware);
			ONERROR(N2D_TIMEOUT);
		}

	} while (!gcmGETFIELD(idle, AQ_HI_IDLE, IDLE_FE));

	if (pd_mode) {
		n2d_kernel_os_poke_with_core(
			os, core, GCREG_MMUAHB_CONTEXT_PD_ENTRY0_Address,
			gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTEXT_PD_ENTRY0, MASTER_TLB_MODE,
					 FOUR_K_MODE) |
				gcmSETFIELD(0, GCREG_MMUAHB_CONTEXT_PD_ENTRY0, PD_ADDRESS, 0));
	}
on_error:
	return error;
}

n2d_error_t _config_mmu_from_reg(n2d_mmu_t *mmu, n2d_int32_t secure_mode, n2d_bool_t pd_mode)
{
	n2d_error_t error	      = N2D_SUCCESS;
	n2d_uint32_t core	      = mmu->hardware->core;
	n2d_os_t *os		      = mmu->hardware->os;
	n2d_uint32_t address	      = 0;
	n2d_uint32_t ext_safe_address = 0;
	n2d_uint32_t mtlb, ext_mtlb;

	mtlb	 = (n2d_uint32_t)(mmu->config->mtlb_physical & 0xFFFFFFFF);
	ext_mtlb = (n2d_uint32_t)(mmu->config->mtlb_physical >> 32);

	if (!pd_mode)
		ONERROR(N2D_NOT_SUPPORTED);
	/* more than 40bit physical address */
	if (ext_mtlb & 0xFFFFFF00)
		ONERROR(N2D_NOT_SUPPORTED);

	/* reserved to do something */
	if (secure_mode)
		;

	n2d_kernel_os_poke_with_core(
		os, core, GCREG_MMUAHB_CONTEXT_PD_ENTRY0_Address,
		gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTEXT_PD_ENTRY0, MASTER_TLB_MODE, FOUR_K_MODE) |
			gcmSETFIELD(0, GCREG_MMUAHB_CONTEXT_PD_ENTRY0, PD_ADDRESS,
				    (ext_mtlb << 20) | (mtlb >> 12)));

	address		 = (n2d_uint32_t)(mmu->config->safe_page_physical & 0xFFFFFFFF);
	ext_safe_address = (n2d_uint32_t)(mmu->config->safe_page_physical >> 32);

	if (address & 0x3F)
		ONERROR(N2D_NOT_ALIGNED);

	/* more than 40bit physical address */
	if (ext_safe_address & 0xFFFFFF00)
		ONERROR(N2D_NOT_SUPPORTED);

	n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_TABLE_ARRAY_SIZE_Address, 1);

	n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_SAFE_SECURE_ADDRESS_Address, address);

	n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_SAFE_NON_SECURE_ADDRESS_Address,
				     address);

	n2d_kernel_os_poke_with_core(
		os, core, GCREG_MMUAHB_SAFE_ADDRESS_EXT_Address,
		gcmSETFIELD(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT, SAFE_ADDRESS_SECURE,
			    (n2d_uint32_t)ext_safe_address) |
			gcmSETFIELDVALUE(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT, MASK_SAFE_ADDRESS_SECURE,
					 ENABLED) |
			gcmSETFIELD(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT, SAFE_ADDRESS_NON_SECURE,
				    (n2d_uint32_t)ext_safe_address) |
			gcmSETFIELDVALUE(0, GCREG_MMUAHB_SAFE_ADDRESS_EXT,
					 MASK_SAFE_ADDRESS_NON_SECURE, ENABLED));

on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_mmu_flush(n2d_hardware_t *hardware, n2d_mmu_t *mmu,
					  n2d_bool_t *need_flush, n2d_uint32_t *size)
{
	n2d_bool_t stall_fe_prefetch = N2D_FALSE;
	n2d_bool_t blt_engine	     = N2D_FALSE;
	n2d_uint32_t semaphore, stall;
	n2d_int32_t dirty      = 0;
	n2d_cmd_buf_t *cmd_buf = &hardware->cmd_buf;
	/*modification for mmu*/
	/*n2d_uint32_t* buffer = cmd_buf->curelogical + cmd_buf->index;*/
	n2d_uint32_t *buffer = (n2d_uint32_t *)((n2d_uint8_t *)cmd_buf->logical + cmd_buf->offset);
	n2d_uint32_t count   = 0;

	if (need_flush)
		*need_flush = N2D_FALSE;

	if (mmu) {
		n2d_kernel_os_atom_get(hardware->os, mmu->config->page_dirty, &dirty);

		if (dirty == 0)
			return N2D_SUCCESS;

		if (need_flush)
			*need_flush = N2D_TRUE;

		n2d_kernel_os_atom_set(hardware->os, mmu->config->page_dirty, 0);

		/* Flush MMU cache. */
		*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE) |
			    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
					gcregMMUConfigurationRegAddrs) |
			    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1);
		*buffer++ = gcmSETMASKEDFIELDVALUE(GCREG_MMU_CONFIGURATION, FLUSH, FLUSH);

		/* Arm the PE-FE Semaphore. */
		*buffer++ =
			gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE, LOAD_STATE) |
			gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1) |
			gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS, AQSemaphoreRegAddrs);
		semaphore = gcmSETFIELDVALUE(0, AQ_SEMAPHORE, SOURCE, FRONT_END);

		if (stall_fe_prefetch)
			semaphore |= gcmSETFIELDVALUE(0, AQ_SEMAPHORE, FRONT_END, PREFETCH);

		if (blt_engine)
			semaphore |= gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, BLT_ENGINE);
		else
			semaphore |= gcmSETFIELDVALUE(0, AQ_SEMAPHORE, DESTINATION, PIXEL_ENGINE);

		*buffer++ = semaphore;

		/* STALL FE until PE is done flushing. */
		*buffer++ = gcmSETFIELDVALUE(0, STALL_COMMAND, OPCODE, STALL);

		stall = gcmSETFIELDVALUE(0, STALL_STALL, SOURCE, FRONT_END);
		if (stall_fe_prefetch)
			stall |= gcmSETFIELDVALUE(0, STALL_STALL, FRONT_END, PREFETCH);

		if (blt_engine)
			stall |= gcmSETFIELDVALUE(0, STALL_STALL, DESTINATION, BLT_ENGINE);
		else
			stall |= gcmSETFIELDVALUE(0, STALL_STALL, DESTINATION, PIXEL_ENGINE);

		*buffer++ = stall;
	}
	count += 4 * 4;
	count += 4 * 2;
	if (blt_engine) {
		if (mmu) {
			/* Blt unlock. */
			*buffer++ = gcmSETFIELDVALUE(0, AQ_COMMAND_LOAD_STATE_COMMAND, OPCODE,
						     LOAD_STATE) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, COUNT, 1) |
				    gcmSETFIELD(0, AQ_COMMAND_LOAD_STATE_COMMAND, ADDRESS,
						gcregBltGeneralControlRegAddrs);
			*buffer++ = gcmSETFIELDVALUE(0, GCREG_BLT_GENERAL_CONTROL, STREAM_CONTROL,
						     UNLOCK);
		}
		count += 4 * 2;
	}
	if (size)
		*size = count;

	return N2D_SUCCESS;
}

n2d_error_t n2d_kernel_hardware_mmu_config(n2d_hardware_t *hardware, n2d_mmu_t *mmu)
{
	n2d_error_t error;
	/*1 has secure feature*/
	n2d_int32_t secure_mode	  = N2D_FALSE;
	n2d_uint32_t reg_mmu_ctrl = 0;
	n2d_uint32_t tmp_value	  = 0;
	n2d_bool_t mmu_disabled	  = N2D_TRUE;
	n2d_uint32_t core	  = hardware->core;
	n2d_os_t *os		  = hardware->os;
	n2d_bool_t pd_mode	  = mmu->hardware->mmu_pd_mode;

	if (_query_feature_database(hardware, N2D_FEATURE_SECURITY_AHB))
		secure_mode = N2D_TRUE;

	if (secure_mode) {
		reg_mmu_ctrl = n2d_kernel_os_peek_with_core(os, core, GCREG_MMUAHB_CONTROL_Address);
		mmu_disabled = (gcmGETFIELD(reg_mmu_ctrl, GCREG_MMUAHB_CONTROL, MMU) ==
				GCREG_MMUAHB_CONTROL_MMU_ENABLE) ?
					     N2D_FALSE :
					     N2D_TRUE;
	} else {
		reg_mmu_ctrl = n2d_kernel_os_peek_with_core(os, core, GCREG_MMU_CONTROL_Address);
		mmu_disabled = (gcmGETFIELD(reg_mmu_ctrl, GCREG_MMU_CONTROL, ENABLE) ==
				GCREG_MMU_CONTROL_ENABLE_ENABLE) ?
					     N2D_FALSE :
					     N2D_TRUE;
	}

	if (mmu_disabled) {
		if (!pd_mode && mmu->config->mmu_init_buffer_physical >> 32)
			ONERROR(N2D_OUT_OF_MEMORY);

		if (mmu->config->mmu_init_buffer_physical & 0x3F)
			ONERROR(N2D_NOT_ALIGNED);

		if (mmu->config->init_mode == n2dMMU_INIT_FROM_CMD)
			ONERROR(_config_mmu_from_cmd(mmu, secure_mode, pd_mode));
		else
			ONERROR(_config_mmu_from_reg(mmu, secure_mode, pd_mode));

		if (secure_mode) {
			if (mmu->config->init_mode == n2dMMU_INIT_FROM_CMD)
				tmp_value = gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTROL, MMU, ENABLE);
			else
				tmp_value = gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTROL, MMU, ENABLE) |
					    gcmSETFIELDVALUE(0, GCREG_MMUAHB_CONTROL, SET_UP_MMU,
							     FROM_REG);

			n2d_kernel_os_poke_with_core(os, core, GCREG_MMUAHB_CONTROL_Address,
						     tmp_value);
		} else {
			tmp_value = gcmSETFIELD(0, GCREG_MMU_CONTROL, ENABLE, N2D_TRUE);
			n2d_kernel_os_poke_with_core(os, core, GCREG_MMU_CONTROL_Address,
						     tmp_value);
		}
	}

	return N2D_SUCCESS;

on_error:
	return error;
}
#endif

/* hardware command start */
static n2d_error_t n2d_kernel_hardware_command_construct(n2d_hardware_t *hardware)
{
	n2d_error_t error	= N2D_SUCCESS;
	n2d_cmd_buf_t *cmd_buf	= &hardware->cmd_buf;
	n2d_pointer pointer	= N2D_NULL;
	n2d_uint32_t flag	= N2D_ALLOC_FLAG_CONTIGUOUS;
	n2d_vidmem_node_t *node = N2D_NULL;
	n2d_map_type_t type	= N2D_MAP_TO_KERNEL | N2D_MAP_TO_GPU;

#if !NANO2D_MMU_ENABLE
	if (!hardware->support_64bit)
		flag |= N2D_ALLOC_FLAG_4G;
#endif
	ONERROR(n2d_kernel_os_memory_fill(hardware->os, cmd_buf, 0, sizeof(n2d_cmd_buf_t)));

	cmd_buf->size = 4096;
	ONERROR(n2d_kernel_vidmem_allocate(hardware->kernel, cmd_buf->size, 4096, flag,
					   N2D_TYPE_COMMAND, 0, &node));
	ONERROR(n2d_kernel_vidmem_node_map(hardware->kernel, 0, type, node));
	cmd_buf->node	 = node;
	cmd_buf->logical = (n2d_uint32_t *)node->klogical;
	ONERROR(n2d_kernel_os_memory_fill(hardware->os, cmd_buf->logical, 0, cmd_buf->size));

	if (!cmd_buf->logical)
		ONERROR(N2D_OUT_OF_MEMORY);

	cmd_buf->address = node->address;
	cmd_buf->offset	 = 0;

	if (hardware->support_64bit) {
		n2d_uint32_t ext_addr = cmd_buf->address >> 32;

		n2d_kernel_os_poke_with_core(hardware->os, hardware->core, AQFE_AXI_ADDRESS_Address,
					     ext_addr);
	}

	ONERROR(n2d_kernel_os_mutex_create(hardware->os, &cmd_buf->commit_mutex));

	ONERROR(n2d_kernel_os_allocate(hardware->os, sizeof(struct n2d_cmd_info) * CMD_FIFO_SIZE,
				       &pointer));
	if (!pointer)
		ONERROR(N2D_OUT_OF_MEMORY);
	ONERROR(n2d_kernel_os_memory_fill(hardware->os, pointer, 0,
					  sizeof(struct n2d_cmd_info) * CMD_FIFO_SIZE));
	cmd_buf->executed_cmd_fifo = (n2d_cmd_info_t *)pointer;
	cmd_buf->fifo_index	   = 0;

on_error:
	return error;
}

static n2d_error_t n2d_kernel_hardware_command_destroy(n2d_hardware_t *hardware)
{
	n2d_error_t error      = N2D_SUCCESS;
	n2d_cmd_buf_t *cmd_buf = &hardware->cmd_buf;

	if (cmd_buf->commit_mutex)
		ONERROR(n2d_kernel_os_mutex_delete(hardware->os, cmd_buf->commit_mutex));
	if (cmd_buf->logical) {
		ONERROR(n2d_kernel_vidmem_node_unmap(hardware->kernel, 0, cmd_buf->node));
		ONERROR(n2d_kernel_vidmem_free(hardware->kernel, cmd_buf->node));
	}
	if (cmd_buf->executed_cmd_fifo)
		ONERROR(n2d_kernel_os_free(hardware->os, cmd_buf->executed_cmd_fifo));

on_error:
	return error;
}

static n2d_error_t _reset_gpu(n2d_hardware_t *hardware, n2d_os_t *os)
{
	n2d_error_t error = N2D_SUCCESS;

	ONERROR(n2d_kernel_os_set_gpu_power(os, N2D_FALSE));

	n2d_kernel_os_delay(os, 10);

	ONERROR(n2d_kernel_os_set_gpu_power(os, N2D_TRUE));

on_error:
	return error;
}

n2d_error_t n2d_kernel_hardware_recovery(n2d_hardware_t *hardware)
{
	n2d_error_t error		 = N2D_SUCCESS;
	n2d_os_t *os			 = hardware->os;
	n2d_cmd_buf_t *cmd_buf		 = &hardware->cmd_buf;
	n2d_event_center_t *event_center = hardware->event_center;
	n2d_bool_t get_commit_mutex	 = N2D_FALSE;
	n2d_device_id_t dev_id		 = 0;
	n2d_core_id_t core_id		 = 0;

	/* Find this core's sub device id and core id. */
	dev_id	= hardware->core / hardware->kernel->dev_core_num;
	core_id = hardware->core % hardware->kernel->dev_core_num;

	ONERROR(n2d_kernel_os_mutex_acquire(os, cmd_buf->commit_mutex, N2D_INFINITE));
	get_commit_mutex = N2D_TRUE;

	n2d_kernel_os_print("[galcore]: GPU[%d] core%d hang, automatic recovery.", dev_id, core_id);

	/* Soft reset is not available for AHB, so need to power it and back on. */
	ONERROR(_reset_gpu(hardware, os));

	/* Initialize hardware. */
	ONERROR(n2d_kernel_hardware_initialize(hardware));

	hardware->running = N2D_FALSE;
	ONERROR(n2d_kernel_hardware_start(hardware));

	/* Handle all outstanding events now. */
	ONERROR(n2d_kernel_event_clear(hardware->event_center));

	n2d_kernel_os_atom_set(os, event_center->pending_event_count, 0);

	n2d_kernel_os_mutex_release(os, cmd_buf->commit_mutex);

	n2d_kernel_os_print("[galcore]: GPU[%d] core%d recovery done.", dev_id, core_id);

	return N2D_SUCCESS;
on_error:
	if (get_commit_mutex)
		n2d_kernel_os_mutex_release(hardware->os, cmd_buf->commit_mutex);

	return error;
}

n2d_error_t n2d_kernel_hardware_construct(n2d_kernel_t *kernel, n2d_uint32_t core,
					  n2d_hardware_t **hardware)
{
	n2d_error_t error	  = N2D_SUCCESS;
	n2d_pointer pointer	  = N2D_NULL;
	n2d_hardware_t *_hardware = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(n2d_hardware_t), &pointer));
	if (!pointer)
		ONERROR(N2D_OUT_OF_MEMORY);
	ONERROR(n2d_kernel_os_memory_fill(kernel->os, pointer, 0, sizeof(n2d_hardware_t)));

	_hardware	       = pointer;
	_hardware->kernel      = kernel;
	_hardware->core	       = core;
	_hardware->os	       = kernel->os;
	_hardware->running     = N2D_FALSE;
	_hardware->event_count = 0;
	_hardware->power_state = N2D_POWER_ON;
#if N2D_GPU_TIMEOUT
	_hardware->timeout = N2D_GPU_TIMEOUT;
#endif
	*hardware = _hardware;

	ONERROR(n2d_kernel_hardware_initialize(_hardware));

#if NANO2D_WAIT_LINK_ONLY
	_hardware->fe_type = HW_FE_WAIT_LINK;
#else
	if (!_query_feature_database(_hardware, N2D_FEATURE_FRAME_DONE_INTR))
		_hardware->fe_type = HW_FE_WAIT_LINK;
	else
		_hardware->fe_type = HW_FE_END;
#endif

	ONERROR(n2d_kernel_hardware_command_construct(_hardware));
	ONERROR(n2d_kernel_event_center_construct(_hardware, &_hardware->event_center));

#if N2D_GPU_TIMEOUT
	ONERROR(n2d_kernel_os_creat_timer(_hardware->os, (n2d_timer_func)_monitor_timer_func,
					  (n2d_pointer)_hardware, &_hardware->monitor_timer));

	_hardware->monitoring	      = N2D_FALSE;
	_hardware->monitor_timer_stop = N2D_FALSE;
#endif

	return error;
on_error:
	if (pointer)
		n2d_kernel_os_free(kernel->os, pointer);
	return error;
}

n2d_error_t n2d_kernel_hardware_destroy(n2d_hardware_t *hardware)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_os_t *os	  = hardware->os;

	if (!hardware)
		return error;

#if N2D_GPU_TIMEOUT
	/* Destroy monitor timer. */
	if (hardware->monitor_timer)
		ONERROR(n2d_kernel_os_destroy_timer(hardware->os, hardware->monitor_timer));
#endif

	ONERROR(n2d_kernel_hardware_command_destroy(hardware));
	ONERROR(n2d_kernel_event_center_destroy(hardware, hardware->event_center));
	ONERROR(n2d_kernel_hardware_deinitialize(hardware));
	ONERROR(n2d_kernel_os_free(os, hardware));

on_error:
	return error;
}
