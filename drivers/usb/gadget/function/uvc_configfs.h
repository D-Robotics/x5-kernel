/* SPDX-License-Identifier: GPL-2.0 */
/*
 * uvc_configfs.h
 *
 * Configfs support for the uvc function.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzejtp2010@gmail.com>
 */
#ifndef UVC_CONFIGFS_H
#define UVC_CONFIGFS_H

#include <linux/configfs.h>

#include "u_uvc.h"

static inline struct f_uvc_opts *to_f_uvc_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_uvc_opts,
			    func_inst.group);
}

#define UVCG_STREAMING_CONTROL_SIZE	1

DECLARE_UVC_HEADER_DESCRIPTOR(1);

struct uvcg_control_header {
	struct config_item		item;
	struct UVC_HEADER_DESCRIPTOR(1)	desc;
	unsigned			linked;
};

static inline struct uvcg_control_header *to_uvcg_control_header(struct config_item *item)
{
	return container_of(item, struct uvcg_control_header, item);
}

enum uvcg_format_type {
	UVCG_UNCOMPRESSED = 0,
	UVCG_MJPEG,
};

struct uvcg_format {
	struct config_group	group;
	enum uvcg_format_type	type;
	unsigned		linked;
	struct list_head	frames;
	unsigned		num_frames;
	__u8			bmaControls[UVCG_STREAMING_CONTROL_SIZE];
};

struct uvcg_format_ptr {
	struct uvcg_format	*fmt;
	struct list_head	entry;
};

static inline struct uvcg_format *to_uvcg_format(struct config_item *item)
{
	return container_of(to_config_group(item), struct uvcg_format, group);
}

struct uvcg_streaming_header {
	struct config_item				item;
	struct uvc_input_header_descriptor		desc;
	unsigned					linked;
	struct list_head				formats;
	unsigned					num_fmt;
};

static inline struct uvcg_streaming_header *to_uvcg_streaming_header(struct config_item *item)
{
	return container_of(item, struct uvcg_streaming_header, item);
}

struct uvcg_frame_ptr {
	struct uvcg_frame	*frm;
	struct list_head	entry;
};

struct uvcg_frame {
	struct config_item	item;
	enum uvcg_format_type	fmt_type;
	struct {
		u8	b_length;
		u8	b_descriptor_type;
		u8	b_descriptor_subtype;
		u8	b_frame_index;
		u8	bm_capabilities;
		u16	w_width;
		u16	w_height;
		u32	dw_min_bit_rate;
		u32	dw_max_bit_rate;

		/*
		 * The layout of last three members of framebased frame
		 * is different from uncompressed frame.
		 *   Last three members of uncompressed frame are:
		 *     u32	dw_max_video_frame_buffer_size;
		 *     u32	dw_default_frame_interval;
		 *     u8	b_frame_interval_type;
		 *   Last three members of framebased frame are:
		 *     u32	dw_default_frame_interval;
		 *     u8	b_frame_interval_type;
		 *     u32	dw_bytes_perline;
		 */
		u32	dw_max_video_frame_buffer_size;
		u32	dw_default_frame_interval;
		u8	b_frame_interval_type;
#define dw_bytes_perline	dw_max_video_frame_buffer_size
	} __attribute__((packed)) frame;
	u32 *dw_frame_interval;
};

static inline struct uvcg_frame *to_uvcg_frame(struct config_item *item)
{
	return container_of(item, struct uvcg_frame, item);
}

/* -----------------------------------------------------------------------------
 * streaming/uncompressed/<NAME>
 */

struct uvcg_uncompressed {
	struct uvcg_format		fmt;
	struct uvc_format_uncompressed	desc;
};

static inline struct uvcg_uncompressed *to_uvcg_uncompressed(struct config_item *item)
{
	return container_of(to_uvcg_format(item), struct uvcg_uncompressed, fmt);
}

static inline u8 uvcg_uncompressed_subtype(struct uvcg_format *fmt)
{
	struct uvcg_uncompressed *ch = container_of(fmt,
					struct uvcg_uncompressed, fmt);

	return ch->desc.bDescriptorSubType;
}

/* -----------------------------------------------------------------------------
 * streaming/mjpeg/<NAME>
 */

struct uvcg_mjpeg {
	struct uvcg_format		fmt;
	struct uvc_format_mjpeg		desc;
};

static inline struct uvcg_mjpeg *to_uvcg_mjpeg(struct config_item *item)
{
	return container_of(to_uvcg_format(item), struct uvcg_mjpeg, fmt);
}

int uvcg_attach_configfs(struct f_uvc_opts *opts);

#endif /* UVC_CONFIGFS_H */
