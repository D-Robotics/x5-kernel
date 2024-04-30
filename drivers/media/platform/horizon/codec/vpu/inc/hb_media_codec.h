/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/
#ifndef __HB_MEDIA_CODEC_H__
#define __HB_MEDIA_CODEC_H__

#include "hb_media_basic_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
* Define the video and audio profile.
**/
#define MC_AV_PROFILE_UNKNOWN                          -99
#define MC_AV_PROFILE_RESERVED                         -100

/**
* H265 Profile
**/
#define MC_AV_PROFILE_HEVC_MAIN                        1
#define MC_AV_PROFILE_HEVC_MAIN_10                     2
#define MC_AV_PROFILE_HEVC_MAIN_STILL_PICTURE          3

/**
* H264 Profile
**/
#define MC_AV_PROFILE_H264_CONSTRAINED                 (1<<9)
#define MC_AV_PROFILE_H264_BASELINE                    66
#define MC_AV_PROFILE_H264_CONSTRAINED_BASELINE        \
                                (66|MC_AV_PROFILE_H264_CONSTRAINED)
#define MC_AV_PROFILE_H264_MAIN                        77
#define MC_AV_PROFILE_H264_HIGH                        100
#define MC_AV_PROFILE_H264_HIGH_10                     110

/**
* AAC Profile
**/
#define MC_AV_PROFILE_AAC_MAIN                            0
#define MC_AV_PROFILE_AAC_LOW                             1
#define MC_AV_PROFILE_AAC_SSR                             2
#define MC_AV_PROFILE_AAC_LTP                             3
#define MC_AV_PROFILE_AAC_HE                              4
#define MC_AV_PROFILE_AAC_HE_V2                           28
#define MC_AV_PROFILE_AAC_LD                              22
#define MC_AV_PROFILE_AAC_ELD                             38

/**
* Define the CTU number for max resolution = 8192x8192/(64x64).
**/
#define MC_VIDEO_MAX_CTU_NUM               0x4000

/**
* Define the SubCTU number for max resolution = 8192x8192/(32x32).
**/
#define MC_VIDEO_MAX_SUB_CTU_NUM	        0x10000

/**
* Define the MB number for max resolution = 8192x8192/(16x16).
**/
#define MC_VIDEO_MAX_MB_NUM                0x40000

/**
* Define the maximum GOP size.
**/
#define MC_MAX_GOP_NUM       8

/**
* Define the media codec state.
**/
typedef enum _media_codec_state {
	MEDIA_CODEC_STATE_NONE = -1,
	MEDIA_CODEC_STATE_UNINITIALIZED,
	MEDIA_CODEC_STATE_INITIALIZED,
	MEDIA_CODEC_STATE_CONFIGURED,
	MEDIA_CODEC_STATE_STARTED,
	MEDIA_CODEC_STATE_PAUSED,
	MEDIA_CODEC_STATE_FLUSHING,
	MEDIA_CODEC_STATE_ERROR,
	MEDIA_CODEC_STATE_TOTAL,
} media_codec_state_t;

/**
* Define the supported video and audio codecs.
**/
typedef enum _media_codec_id {
	MEDIA_CODEC_ID_NONE = -1,

	/* Video Codecs */
	MEDIA_CODEC_ID_H264,
	MEDIA_CODEC_ID_H265,
	MEDIA_CODEC_ID_MJPEG,
	MEDIA_CODEC_ID_JPEG,

	/* Audio Codecs */
	MEDIA_CODEC_ID_FLAC,
	MEDIA_CODEC_ID_PCM_MULAW,
	MEDIA_CODEC_ID_PCM_ALAW,
	MEDIA_CODEC_ID_ADPCM_G726,
	MEDIA_CODEC_ID_ADPCM,
	MEDIA_CODEC_ID_AAC,
	MEDIA_CODEC_ID_MP3,
	MEDIA_CODEC_ID_MP2,
	MEDIA_CODEC_ID_TAK,
	MEDIA_CODEC_ID_AC3,
	MEDIA_CODEC_ID_WMA,
	MEDIA_CODEC_ID_AMR,
	MEDIA_CODEC_ID_APE,
	MEDIA_CODEC_ID_G729,
	MEDIA_CODEC_ID_G723,
	MEDIA_CODEC_ID_G722,
	MEDIA_CODEC_ID_IAC,
	MEDIA_CODEC_ID_RALF,
	MEDIA_CODEC_ID_QDMC,
	MEDIA_CODEC_ID_DTS,
	MEDIA_CODEC_ID_GSM,
	MEDIA_CODEC_ID_TTA,
	MEDIA_CODEC_ID_QCELP,
	MEDIA_CODEC_ID_MLP,
	MEDIA_CODEC_ID_ATRAC1,
	MEDIA_CODEC_ID_IMC,
	MEDIA_CODEC_ID_EAC,
	MEDIA_CODEC_ID_MP1,
	MEDIA_CODEC_ID_SIPR,
	MEDIA_CODEC_ID_OPUS,
	MEDIA_CODEC_ID_CELT,

	/* Subtitle Codecs */
	MEDIA_CODEC_ID_MOV_TEXT,
	MEDIA_CODEC_ID_TOTAL,
} media_codec_id_t;

/**
* Define the work mode of codecs.
**/
typedef enum _media_codec_mode {
	MC_SOFTWARE = 0,
	MC_HARDWARE,
} media_codec_mode_t;

/**
* Define the av profile.
*/
typedef struct _media_codec_profile {
/**
 * Profile type.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: -1
 */
	hb_s32 profile;

/**
 * Short name for the profile.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: NULL
 */
	const hb_string name;
} media_codec_profile_t;

/**
* Define the descriptor of codecs.
**/
typedef struct _media_codec_descriptor {
/**
 * Codec ID.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: MEDIA_CODEC_ID_NONE
 */
	media_codec_id_t id;

/**
 * Codec mode.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: MC_SOFTWARE
 */
	media_codec_mode_t mode;

/**
 * Unique name.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: NULL
 */
	const hb_string name;

/**
 * Detail description of name.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: NULL
 */
	const hb_string long_name;

/**
 * MIME type associated with the codec.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: NULL
 */
	const hb_string mime_types;
/**
 * AV profile.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: NULL
 */
	const media_codec_profile_t *profiles;
} media_codec_descriptor_t;

/**
* Define the pixel format types.
**/
typedef enum _mc_pixel_format {
	MC_PIXEL_FORMAT_NONE = -1,
	/* planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples) */
	MC_PIXEL_FORMAT_YUV420P,
	/*
	 * planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components,
	 * which are interleaved (first byte U and the following byte V).
	 */
	MC_PIXEL_FORMAT_NV12,
	/* as above, but U and V bytes are swapped */
	MC_PIXEL_FORMAT_NV21,
	/* planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples) */
	MC_PIXEL_FORMAT_YUV422P,
	/*
	 * interleaved chroma (first byte U and the following byte V)
	 * YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
	 */
	MC_PIXEL_FORMAT_NV16,
	/*
	 * interleaved chroma (first byte V and the following byte U)
	 * YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
	 */
	MC_PIXEL_FORMAT_NV61,
	/* packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr */
	MC_PIXEL_FORMAT_YUYV422,
	/* packed YUV 4:2:2, 16bpp, Y0 Cr Y1 Cb */
	MC_PIXEL_FORMAT_YVYU422,
	/* packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1 */
	MC_PIXEL_FORMAT_UYVY422,
	/* packed YUV 4:2:2, 16bpp, Cr Y0 Cb Y1 */
	MC_PIXEL_FORMAT_VYUY422,
	/* packed YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples) */
	MC_PIXEL_FORMAT_YUV444,
	/* planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples) */
	MC_PIXEL_FORMAT_YUV444P,
	/*
	 * interleaved chroma (first byte U and the following byte V)
	 * YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
	 */
	MC_PIXEL_FORMAT_NV24,
	/*
	 * interleaved chroma (first byte V and the following byte U)
	 * YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
	 */
	MC_PIXEL_FORMAT_NV42,
	/* planar YUV 4:4:0 (1 Cr & Cb sample per 1x2 Y samples) */
	MC_PIXEL_FORMAT_YUV440P,
	/* Gray Y, YUV 4:0:0 */
	MC_PIXEL_FORMAT_YUV400,

	MC_PIXEL_FORMAT_TOTAL,
} mc_pixel_format_t;

/**
* Define the audio sample format.
**/
typedef enum _mc_audio_sample_rate {
	MC_AV_SAMPLE_RATE_NONE = -1,
	MC_AV_SAMPLE_RATE_7350 = 7350,
	/* 8k */
	MC_AV_SAMPLE_RATE_8000 = 8000,
	/* 11.025k */
	MC_AV_SAMPLE_RATE_11025 = 11025,
	/* 12k */
	MC_AV_SAMPLE_RATE_12000 = 12000,
	/* 16k */
	MC_AV_SAMPLE_RATE_16000 = 16000,
	/* 22.05k */
	MC_AV_SAMPLE_RATE_22050 = 22050,
	/* 24k */
	MC_AV_SAMPLE_RATE_24000 = 24000,
	/* 32k */
	MC_AV_SAMPLE_RATE_32000 = 32000,
	/* 44.1k */
	MC_AV_SAMPLE_RATE_44100 = 44100,
	/* 48k */
	MC_AV_SAMPLE_RATE_48000 = 48000,
	/* 64k */
	MC_AV_SAMPLE_RATE_64000 = 64000,
	/* 88.2k */
	MC_AV_SAMPLE_RATE_88200 = 88200,
	/* 96k */
	MC_AV_SAMPLE_RATE_96000 = 96000,

	/* Number of sample rate. */
	MC_AV_SAMPLE_RATE_TOTAL,
} mc_audio_sample_rate_t;

/**
* FFMPEG: channel_masks Audio channel masks
*
* A channel layout is a 64-bits integer with a bit set for every channel.
* The number of bits set must be equal to the number of channels.
* The value 0 means that the channel layout is not known.
* @note this data structure is not powerful enough to handle channels
* combinations that have the same channel multiple times, such as
* dual-mono.
*
*/
#define MC_AV_CH_FRONT_LEFT             0x00000001
#define MC_AV_CH_FRONT_RIGHT            0x00000002
#define MC_AV_CH_FRONT_CENTER           0x00000004

/**
* Define the audio channel layout.
**/
typedef enum _mc_audio_channel_layout {
	MC_AV_CHANNEL_LAYOUT_NONE = -1,
	MC_AV_CHANNEL_LAYOUT_STEREO =
	    MC_AV_CH_FRONT_LEFT | MC_AV_CH_FRONT_RIGHT,
	MC_AV_CHANNEL_LAYOUT_MONO = MC_AV_CH_FRONT_CENTER,

	/* Number of channel layout. */
	MC_AV_CHANNEL_LAYOUT_TOTAL,
} mc_audio_channel_layout_t;

/**
* Define the audio sample rate.
**/
typedef enum _mc_audio_sample_format {
	MC_AV_SAMPLE_FMT_NONE = -1,
	/* unsigned 8 bits */
	MC_AV_SAMPLE_FMT_U8,
	/* signed 16 bits */
	MC_AV_SAMPLE_FMT_S16,
	/* signed 32 bits */
	MC_AV_SAMPLE_FMT_S32,
	/* float */
	MC_AV_SAMPLE_FMT_FLT,
	/* double */
	MC_AV_SAMPLE_FMT_DBL,

	/* unsigned 8 bits, planar */
	MC_AV_SAMPLE_FMT_U8P,
	/* signed 16 bits, planar */
	MC_AV_SAMPLE_FMT_S16P,
	/* signed 32 bits, planar */
	MC_AV_SAMPLE_FMT_S32P,
	/* float, planar */
	MC_AV_SAMPLE_FMT_FLTP,
	/* double, planar */
	MC_AV_SAMPLE_FMT_DBLP,
	/* signed 64 bits */
	MC_AV_SAMPLE_FMT_S64,
	/* signed 64 bits, planar */
	MC_AV_SAMPLE_FMT_S64P,

	/* Number of sample formats. */
	MC_AV_SAMPLE_FMT_TOTAL,
} mc_audio_sample_format_t;

/**
* Define the H264 NALU type.
**/
typedef enum _mc_h264_nal_unit_type {
	MC_H264_NALU_TYPE_NONE = -1,
	MC_H264_NALU_TYPE_I = 0,
	MC_H264_NALU_TYPE_P = 1,
	MC_H264_NALU_TYPE_B = 2,
	MC_H264_NALU_TYPE_IDR = 5,
	MC_H264_NALU_TYPE_SEI = 6,
	MC_H264_NALU_TYPE_SPS = 7,
	MC_H264_NALU_TYPE_PPS = 8,

	MC_H264_NALU_TYPE_TOTAL
} mc_h264_nal_unit_type_t;

/**
* Define the H265 NALU type.
**/
typedef enum _mc_h265_nal_unit_type {
	MC_H265_NALU_TYPE_NONE = -1,
	MC_H265_NALU_TYPE_I = 0,
	MC_H265_NALU_TYPE_P = 1,
	MC_H265_NALU_TYPE_B = 2,
	MC_H265_NALU_TYPE_IDR = 19,
	MC_H265_NALU_TYPE_VPS = 32,
	MC_H265_NALU_TYPE_SPS = 33,
	MC_H265_NALU_TYPE_PPS = 34,
	MC_H265_NALU_TYPE_SEI = 39,

	MC_H265_NALU_TYPE_TOTAL
} mc_h265_nal_unit_type_t;

/**
* Define the H264 profile.
**/
typedef enum _mc_h264_profile {
	MC_H264_PROFILE_UNSPECIFIED,
	MC_H264_PROFILE_BP,
	MC_H264_PROFILE_MP,
	MC_H264_PROFILE_EXTENDED,
	MC_H264_PROFILE_HP,
	MC_H264_PROFILE_HIGH10,
	MC_H264_PROFILE_HIGH422,
	MC_H264_PROFILE_HIGH444
} mc_h264_profile_t;

/**
* Define the H264 level.
**/
typedef enum _mc_h264_level {
	MC_H264_LEVEL_UNSPECIFIED,
	MC_H264_LEVEL1 = 10,
	MC_H264_LEVEL1b = 9,
	MC_H264_LEVEL1_1 = 11,
	MC_H264_LEVEL1_2 = 12,
	MC_H264_LEVEL1_3 = 13,
	MC_H264_LEVEL2 = 20,
	MC_H264_LEVEL2_1 = 21,
	MC_H264_LEVEL2_2 = 22,
	MC_H264_LEVEL3 = 30,
	MC_H264_LEVEL3_1 = 31,
	MC_H264_LEVEL3_2 = 32,
	MC_H264_LEVEL4 = 40,
	MC_H264_LEVEL4_1 = 41,
	MC_H264_LEVEL4_2 = 42,
	MC_H264_LEVEL5 = 50,
	MC_H264_LEVEL5_1 = 51,
	MC_H264_LEVEL5_2 = 52,
} mc_h264_level_t;

/**
* Define the H265 level.
**/
typedef enum _mc_h265_level {
	MC_H265_LEVEL_UNSPECIFIED,
	MC_H265_LEVEL1 = 30,
	MC_H265_LEVEL2 = 60,
	MC_H265_LEVEL2_1 = 63,
	MC_H265_LEVEL3 = 90,
	MC_H265_LEVEL3_1 = 93,
	MC_H265_LEVEL4 = 120,
	MC_H265_LEVEL4_1 = 123,
	MC_H265_LEVEL5 = 150,
	MC_H265_LEVEL5_1 = 153,
} mc_h265_level_t;

/**
* Define the codec command priority.
**/
typedef enum {
	_PRIO_0,
	_PRIO_1,
	_PRIO_2,
	_PRIO_3,
	_PRIO_4,
	_PRIO_5,
	_PRIO_6,
	_PRIO_7,
	_PRIO_8,
	_PRIO_9,
	_PRIO_10,
	_PRIO_11,
	_PRIO_12,
	_PRIO_13,
	_PRIO_14,
	_PRIO_15,
	_PRIO_16,
	_PRIO_17,
	_PRIO_18,
	_PRIO_19,
	_PRIO_20,
	_PRIO_21,
	_PRIO_22,
	_PRIO_23,
	_PRIO_24,
	_PRIO_25,
	_PRIO_26,
	_PRIO_27,
	_PRIO_28,
	_PRIO_29,
	_PRIO_30,
	_PRIO_31,
	_MAX_PRIO_32,
} _mc_video_cmd_prio_t;

/**
* Define the aac profile.
**/
typedef enum _mc_aac_profile {
	MC_AAC_PROFILE_MAIN = MC_AV_PROFILE_AAC_MAIN,
	MC_AAC_PROFILE_LOW = MC_AV_PROFILE_AAC_LOW,
	MC_AAC_PROFILE_SSR = MC_AV_PROFILE_AAC_SSR,
	MC_AAC_PROFILE_LTP = MC_AV_PROFILE_AAC_LTP,
	MC_AAC_PROFILE_HE = MC_AV_PROFILE_AAC_HE,
	MC_AAC_PROFILE_HE_V2 = MC_AV_PROFILE_AAC_HE_V2,
	MC_AAC_PROFILE_LD = MC_AV_PROFILE_AAC_LD,
	MC_AAC_PROFILE_ELD = MC_AV_PROFILE_AAC_ELD,
	MC_AAC_PROFILE_TOTAL,
} mc_aac_profile_t;

/**
* Define the flac lpc type.
**/
typedef enum _mc_flac_lpc_type {
	/* use the codec default LPC type  */
	MC_FLAC_LPC_TYPE_DEFAULT = -1,
	/* do not use LPC prediction or use all zero coefficients */
	MC_FLAC_LPC_TYPE_NONE = 0,
	/* fixed LPC coefficients */
	MC_FLAC_LPC_TYPE_FIXED = 1,
	/* Levinson-Durbin recursion */
	MC_FLAC_LPC_TYPE_LEVINSON = 2,
	/* Cholesky factorization */
	MC_FLAC_LPC_TYPE_CHOLESKY = 3,
	MC_FLAC_LPC_TYPE_TOTAOL,
} mc_flac_lpc_type_t;

/**
* Define the encapsulated data type.
**/
typedef enum _mc_aac_data_type {
	MC_AAC_DATA_TYPE_ADTS = 0,
	MC_AAC_DATA_TYPE_LOAS = 1,
	MC_AAC_DATA_TYPE_LATM = 2,
	MC_AAC_DATA_TYPE_TOTAOL,
} mc_aac_data_type_t;

/**
* Define the g726 BPS
**/
typedef enum _mc_g726_bps_t {
	MC_G726_BPS_16K = 16000,
	MC_G726_BPS_24K = 24000,
	MC_G726_BPS_32K = 32000,
	MC_G726_BPS_40K = 40000,
	MC_G726_BPS_TOTAL,
} mc_g726_bps_t;

/**
* Define rotate counter clock wise degree.
**/
typedef enum _mc_rotate_degree {
	MC_CCW_0,
	MC_CCW_90,
	MC_CCW_180,
	MC_CCW_270,
} mc_rotate_degree_t;

/**
* Define mirror direction.
**/
typedef enum _mc_mirror_direction {
	MC_DIRECTION_NONE,
	MC_VERTICAL,
	MC_HORIZONTAL,
	MC_HOR_VER,
} mc_mirror_direction_t;

/**
* Specify a display window. Each value means an offset from the start point of
* a frame
**/
typedef struct _mc_av_codec_rect {
/**
 * A horizontal pixel offset of top-left corner of rectangle.
 * Values[0, 8192], Should be equal to multiple of 2
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 x_pos;

/**
 * A vertical pixel offset of top-left corner of rectangle.
 * Values[0, 8192], Should be equal to multiple of 2
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 y_pos;

/**
 * The width of the crop rect.
 * Values(0, 8192], Should be equal to multiple of 2
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 width;

/**
 * The height of the crop rect.
 * Values(0, 8192], Should be equal to multiple of 2
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 height;
} mc_av_codec_rect_t;

/**
* Define the rate control mode.
**/
typedef enum _mc_video_rate_control_mode {
	MC_AV_RC_MODE_NONE = -1,
	MC_AV_RC_MODE_H264CBR,
	MC_AV_RC_MODE_H264VBR,
	MC_AV_RC_MODE_H264AVBR,
	MC_AV_RC_MODE_H264FIXQP,
	MC_AV_RC_MODE_H264QPMAP,
	MC_AV_RC_MODE_H265CBR,
	MC_AV_RC_MODE_H265VBR,
	MC_AV_RC_MODE_H265AVBR,
	MC_AV_RC_MODE_H265FIXQP,
	MC_AV_RC_MODE_H265QPMAP,
	MC_AV_RC_MODE_MJPEGFIXQP,
	MC_AV_RC_MODE_TOTAL,
} mc_video_rate_control_mode_t;

/**
* Define the parameter of h264 Constant Bit Rate(CBR).
**/
typedef struct _mc_h264_cbr_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * A quantization parameter of intra picture.
 * Values[0,51]
 *
 * - Note: It's changeable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 30
 */
	hb_u32 intra_qp;

/**
 * The target average bitrate of the encoded data in kbps.
 * Values[0,700000]kbps
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 bit_rate;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Specifies the initial QP by user. If this value is smaller than 0 or
 * larger than 51, the initial QP is decided by F/W.
 * Values[0~63]
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 63
 */
	hb_u32 initial_rc_qp;

/**
 * Specifies the size of the VBV buffer in msec (10 ~ 3000).
 * For example, 3000 should be set for 3 seconds. This value is valid
 * when RateControl is 1. VBV buffer size in bits is
 * bit_rate * vbv_buffer_size / 1000.
 * vbv_buffer_size has relevance to picture quality and bitrate accuracy.
 * As vbv_buffer_size is shorter, encoder can reach target bitrate accurately
 * with worse quality. On the other hand, as vbv_buffer_size is longer,
 * it can achieve rather better quality under rate control. This parameter
 * is also used for deriving an RC bitrate error by the following equation.
 * max bitrate error = (VbvBufferSize / 1000) / (FramesTo-
 * BeEncoded / FrameRate) * 100 (%)
 * In other words, bitrate error decreases as VbvBufferSize decreases
 * or FramesToBeEncoded increases. For example, when VbvBufferSize is
 * 3000msec (default value) and FrameRate is 30,
 * • If FramesToBeEncoded is 30(=1sec), max bitrate error is 300%.
 * • If FramesToBeEncoded is 300(=10sec), max bitrate error is 30%.
 * • If FramesToBeEncoded is 3000(=100sec), max bitrate error is 3%.
 * • If FramesToBeEncoded is 30000(=1000sec), max bitrate error is 0.3%.
 * Values[10,3000]ms
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10
 */
	hb_s32 vbv_buffer_size;

/**
 * The rate control can work in frame level and MB level.
 * VPU works defaultly in frame level. If ROI encoding is enabled, the MB
 * level rate control is turned off automatically. Enable Mb level where
 * bitrate error should be severely low like in broadcasting application.
 * MB Level rate control can do bitrate control better than picture level
 * rate control, while MB level rate control can have a slight quality drop.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's unchangeable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 mb_level_rc_enalbe;

/**
 * A minimum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_I;

/**
 * A maximum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_I;

/**
 * A minimum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_P;

/**
 * A maximum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_P;

/**
 * A minimum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_B;

/**
 * A maximum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_B;

/**
 * Enables or disables CU QP derivation based on CU variance. It can
 * enable CU QP adjustment for subjective quality enhancement.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 hvs_qp_enable;

/**
 * A QP scaling factor for subCTU QP adjustment when hvs_qp_enable is 1.
 * Values[0,4]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 2
 */
	hb_s32 hvs_qp_scale;

/**
 * Specifies maximum delta QP of HVS QP.
 * Values[0,12]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10
 */
	hb_u32 max_delta_qp;

/**
 * Enables or disables QP map.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: FALSE
 */
	hb_bool qp_map_enable;
} mc_h264_cbr_params_t;

/**
* Define the parameter of h264 Variable Bit Rate(VBR).
**/
typedef struct _mc_h264_vbr_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * A quantization parameter of intra picture.
 * Values[0,51]
 *
 * - Note: It's changeable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 intra_qp;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Enables or disables QP map.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool qp_map_enable;
} mc_h264_vbr_params_t;

/**
* Define the parameter of h264 Average Variable Bit Rate(AVBR).
**/
typedef struct mc_h264_avbr_params_t {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * A quantization parameter of intra picture.
 * Values[0,51]
 *
 * - Note: It's changeable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 intra_qp;

/**
 * The target average bitrate of the encoded data in kbps.
 * Values[0,700000]kbps
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 bit_rate;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Specifies the initial QP by user. If this value is smaller than 0 or
 * larger than 51, the initial QP is decided by F/W.
 * Values[0~51]
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 63
 */
	hb_u32 initial_rc_qp;

/**
 * Specifies the size of the VBV buffer in msec (10 ~ 3000).
 * For example, 3000 should be set for 3 seconds. This value is valid
 * when RateControl is 1. VBV buffer size in bits is
 * bit_rate * vbv_buffer_size / 1000.
 * vbv_buffer_size has relevance to picture quality and bitrate accuracy.
 * As vbv_buffer_size is shorter, encoder can reach target bitrate accurately
 * with worse quality. On the other hand, as vbv_buffer_size is longer,
 * it can achieve rather better quality under rate control. This parameter
 * is also used for deriving an RC bitrate error by the following equation.
 * max bitrate error = (VbvBufferSize / 1000) / (FramesTo-
 * BeEncoded / FrameRate) * 100 (%)
 * In other words, bitrate error decreases as VbvBufferSize decreases
 * or FramesToBeEncoded increases. For example, when VbvBufferSize is
 * 3000msec (default value) and FrameRate is 30,
 * • If FramesToBeEncoded is 30(=1sec), max bitrate error is 300%.
 * • If FramesToBeEncoded is 300(=10sec), max bitrate error is 30%.
 * • If FramesToBeEncoded is 3000(=100sec), max bitrate error is 3%.
 * • If FramesToBeEncoded is 30000(=1000sec), max bitrate error is 0.3%.
 * Values[10,3000]ms
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 3000
 */
	hb_s32 vbv_buffer_size;

/**
 * The rate control can work in frame level and MB level.
 * VPU works defaultly in frame level. If ROI encoding is enabled, the MB
 * level rate control is turned off automatically. Enable Mb level where
 * bitrate error should be severely low like in broadcasting application.
 * MB Level rate control can do bitrate control better than picture level
 * rate control, while MB level rate control can have a slight quality drop.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's unchangeable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 mb_level_rc_enalbe;

/**
 * A minimum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_I;

/**
 * A maximum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_I;

/**
 * A minimum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_P;

/**
 * A maximum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_P;

/**
 * A minimum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_B;

/**
 * A maximum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_B;

/**
 * Enables or disables CU QP derivation based on CU variance. It can
 * enable CU QP adjustment for subjective quality enhancement.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 hvs_qp_enable;

/**
 * A QP scaling factor for subCTU QP adjustment when hvs_qp_enable is 1.
 * Values[0,4]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 2
 */
	hb_s32 hvs_qp_scale;

/**
 * Specifies maximum delta QP of HVS QP. (0 ~ 51) This value is valid
 * when hvs_qp_enable is 1.
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10
 */
	hb_u32 max_delta_qp;

/**
 * Enables or disables QP map.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool qp_map_enable;
} mc_h264_avbr_params_t;

/**
* Define the parameter of h264 Fix Qp.
**/
typedef struct _mc_h264_fix_qp_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * A force picture quantization parameter for I picture.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 force_qp_I;

/**
 * A force picture quantization parameter for P picture.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 force_qp_P;

/**
 * A force picture quantization parameter for B picture.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 force_qp_B;
} mc_h264_fix_qp_params_t;

/**
* Define the parameter of h264 Qp Map.
**/
typedef struct _mc_h264_qp_map_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Specify the qp map. The QP map array should be written a series
 * of 1 byte QP values for each Macroblock in raster scan order.
 * The H264 Macroblock size is 16x16.
 * QP Values[0~51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: NULL
 */
	hb_byte qp_map_array;

/**
 * Specify the qp map number. It's related with the picture width
 * and height. The size should be (ALIGN16(picWidth)>>4)*(ALIGN16(picHeight)>>4)
 * Values[0, MC_VIDEO_MAX_MB_NUM]
 *
 * - Note: It's changable parameter in the same sequence
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 qp_map_array_count;
} mc_h264_qp_map_params_t;

/**
* Define the parameter of h265 Constant Bit Rate(CBR).
**/
typedef struct _mc_h265_cbr_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * A quantization parameter of intra picture.
 * Values[0,51]
 *
 * - Note: It's changeable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 intra_qp;

/**
 * The target average bitrate of the encoded data in kbps.
 * Values[0,700000]kbps
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 bit_rate;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Specifies the initial QP by user. If this value is smaller than 0 or
 * larger than 51, the initial QP is decided by F/W.
 * Values[0~51]
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 63
 */
	hb_u32 initial_rc_qp;

/**
 * Specifies the size of the VBV buffer in msec (10 ~ 3000).
 * For example, 3000 should be set for 3 seconds. This value is valid
 * when RateControl is 1. VBV buffer size in bits is
 * bit_rate * vbv_buffer_size / 1000.
 * vbv_buffer_size has relevance to picture quality and bitrate accuracy.
 * As vbv_buffer_size is shorter, encoder can reach target bitrate accurately
 * with worse quality. On the other hand, as vbv_buffer_size is longer,
 * it can achieve rather better quality under rate control. This parameter
 * is also used for deriving an RC bitrate error by the following equation.
 * max bitrate error = (VbvBufferSize / 1000) / (FramesTo-
 * BeEncoded / FrameRate) * 100 (%)
 * In other words, bitrate error decreases as VbvBufferSize decreases
 * or FramesToBeEncoded increases. For example, when VbvBufferSize is
 * 3000msec (default value) and FrameRate is 30,
 * • If FramesToBeEncoded is 30(=1sec), max bitrate error is 300%.
 * • If FramesToBeEncoded is 300(=10sec), max bitrate error is 30%.
 * • If FramesToBeEncoded is 3000(=100sec), max bitrate error is 3%.
 * • If FramesToBeEncoded is 30000(=1000sec), max bitrate error is 0.3%.
 * Values[10,3000]ms
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10
 */
	hb_s32 vbv_buffer_size;

/**
 * The rate control can work in frame level and ctu level.
 * VPU works defaultly in frame level. If ROI encoding is enabled, the ctu
 * level rate control is turned off automatically. Enable ctu level where
 * bitrate error should be severely low like in broadcasting application.
 * ctu Level rate control can do bitrate control better than picture level
 * rate control, while ctu level rate control can have a slight quality drop.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangeable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 ctu_level_rc_enalbe;

/**
 * A minimum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_I;

/**
 * A maximum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_I;

/**
 * A minimum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_P;

/**
 * A maximum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_P;

/**
 * A minimum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_B;

/**
 * A maximum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_B;

/**
 * Enables or disables CU QP derivation based on CU variance. It can
 * enable CU QP adjustment for subjective quality enhancement.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 hvs_qp_enable;

/**
 * A QP scaling factor for subCTU QP adjustment when hvs_qp_enable is 1.
 * Values[0,4]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 2
 */
	hb_s32 hvs_qp_scale;

/**
 * Specifies maximum delta QP of HVS QP. (0 ~ 51) This value is valid
 * when hvs_qp_enable is 1.
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10
 */
	hb_u32 max_delta_qp;

/**
 * Enables or disables QP map.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool qp_map_enable;
} mc_h265_cbr_params_t;

/**
* Define the parameter of h265 Variable Bit Rate(VBR).
**/
typedef struct _mc_h265_vbr_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * A quantization parameter of intra picture.
 * Values[0,51]
 *
 * - Note: It's changeable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 intra_qp;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Enables or disables QP map.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool qp_map_enable;
} mc_h265_vbr_params_t;

/**
* Define the parameter of h265 Average Variable Bit Rate(AVBR).
**/
typedef struct _mc_h265_avbr_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * A quantization parameter of intra picture.
 * Values[0,51]
 *
 * - Note: It's changeable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 intra_qp;

/**
 * The target average bitrate of the encoded data in kbps.
 * Values[0,700000]kbps
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 bit_rate;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Specifies the initial QP by user. If this value is smaller than 0 or
 * larger than 51, the initial QP is decided by F/W.
 * Values[0~63]
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 63
 */
	hb_u32 initial_rc_qp;

/**
 * Specifies the size of the VBV buffer in msec (10 ~ 3000).
 * For example, 3000 should be set for 3 seconds. This value is valid
 * when RateControl is 1. VBV buffer size in bits is
 * bit_rate * vbv_buffer_size / 1000.
 * vbv_buffer_size has relevance to picture quality and bitrate accuracy.
 * As vbv_buffer_size is shorter, encoder can reach target bitrate accurately
 * with worse quality. On the other hand, as vbv_buffer_size is longer,
 * it can achieve rather better quality under rate control. This parameter
 * is also used for deriving an RC bitrate error by the following equation.
 * max bitrate error = (VbvBufferSize / 1000) / (FramesTo-
 * BeEncoded / FrameRate) * 100 (%)
 * In other words, bitrate error decreases as VbvBufferSize decreases
 * or FramesToBeEncoded increases. For example, when VbvBufferSize is
 * 3000msec (default value) and FrameRate is 30,
 * • If FramesToBeEncoded is 30(=1sec), max bitrate error is 300%.
 * • If FramesToBeEncoded is 300(=10sec), max bitrate error is 30%.
 * • If FramesToBeEncoded is 3000(=100sec), max bitrate error is 3%.
 * • If FramesToBeEncoded is 30000(=1000sec), max bitrate error is 0.3%.
 * Values[10,3000]ms
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 3000
 */
	hb_s32 vbv_buffer_size;

/**
 * The rate control can work in frame level and ctu level.
 * VPU works defaultly in frame level. If ROI encoding is enabled, the ctu
 * level rate control is turned off automatically. Enable ctu level where
 * bitrate error should be severely low like in broadcasting application.
 * ctu Level rate control can do bitrate control better than picture level
 * rate control, while ctu level rate control can have a slight quality drop.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangeable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 ctu_level_rc_enalbe;

/**
 * A minimum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_I;

/**
 * A maximum QP of I picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_I;

/**
 * A minimum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_P;

/**
 * A maximum QP of P picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_P;

/**
 * A minimum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_u32 min_qp_B;

/**
 * A maximum QP of B picture for rate control
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 51
 */
	hb_u32 max_qp_B;

/**
 * Enables or disables CU QP derivation based on CU variance. It can
 * enable CU QP adjustment for subjective quality enhancement.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 hvs_qp_enable;

/**
 * A QP scaling factor for subCTU QP adjustment when hvs_qp_enable is 1.
 * Values[0,4]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 2
 */
	hb_s32 hvs_qp_scale;

/**
 * Specifies maximum delta QP of HVS QP. (0 ~ 51) This value is valid
 * when hvs_qp_enable is 1.
 * Values[0,51]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10
 */
	hb_u32 max_delta_qp;

/**
 * Enables or disables QP map.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool qp_map_enable;
} mc_h265_avbr_params_t;

/**
* Define the parameter of h265 Fix Qp.
**/
typedef struct _mc_h265_fix_qp_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * A force picture quantization parameter for I picture.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 force_qp_I;

/**
 * A force picture quantization parameter for P picture.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 force_qp_P;

/**
 * A force picture quantization parameter for B picture.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 force_qp_B;
} mc_h265_fix_qp_params_t;

/**
* Define the parameter of h265 Qp Map.
**/
typedef struct _mc_h265_qp_map_params {
/**
 * I frame interval.
 * Values[0,2047]
 *
 * - Note: It's changeable parameter in the same sequence.
 *         It's related with Gop size.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 28
 */
	hb_u32 intra_period;

/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Specify the qp map. The QP map array should be written a series
 * of 1 byte QP values for each subCTU in raster scan order.
 * The subCTU block size is 32x32.
 * Values[0~51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_byte qp_map_array;

/**
 * Specify the qp map number.  It's related with the picture width
 * and height. The size should be (ALIGN64(picWidth)>>5)*(ALIGN64(picHeight)>>5)
 * Values[0, MC_VIDEO_MAX_SUB_CTU_NUM]
 *
 * - Note: It's changable parameter in the same sequence
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 qp_map_array_count;
} mc_h265_qp_map_params_t;

/**
* Define the parameter of mjpeg Fix Qp.
**/
typedef struct _mc_mjpeg_fix_qp_params {
/**
 * The target frame rate of the encoded data in fps.
 * Values[1,240]fps
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_rate;

/**
 * Quality factor. Qualities 50..100 are converted to scaling percentage
 * 200 - 2*Q. Note that at Q=100 the scaling is 0, it will cause minimum
 * quantization loss and low compressibility. Qualities 1..50 are converted
 * to scaling percentage 5000/Q. Note that at Q=1 the scaling is 5000,
 * it will cause maximun quantization loss and high compressibility.
 * Values[0,100]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 50;
 */
	hb_u32 quality_factor;
} mc_mjpeg_fix_qp_params_t;

/**
* Define the parameters of H264 rate control.
**/
typedef struct _mc_h264_rate_control_params {
/**
 * Rate control mode.
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_RC_MODE_NONE
 */
	mc_video_rate_control_mode_t mode;

/**
 * The union member is decided according to the
 * @see mc_h264_rate_control_params_t.mode.
 */
	union {
		mc_h264_cbr_params_t cbr_params;
		mc_h264_vbr_params_t vbr_params;
		mc_h264_avbr_params_t avbr_params;
		mc_h264_fix_qp_params_t fix_qp_params;
		mc_h264_qp_map_params_t qp_map_params;
	};
} mc_h264_rate_control_params_t;

/**
* Define the parameters of H265 rate control.
**/
typedef struct _mc_h265_rate_control_params {
/**
 * Rate control mode.
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_RC_MODE_NONE
 */
	mc_video_rate_control_mode_t mode;

/**
 * The union member is decided according to the
 * @see mc_h265_rate_control_params_t.mode.
 *
 */
	union {
		mc_h265_cbr_params_t cbr_params;
		mc_h265_vbr_params_t vbr_params;
		mc_h265_avbr_params_t avbr_params;
		mc_h265_fix_qp_params_t fix_qp_params;
		mc_h265_qp_map_params_t qp_map_params;
	};
} mc_h265_rate_control_params_t;

/**
* Define the parameters of MJPEG rate control.
**/
typedef struct _mc_mjpeg_rate_control_params {
/**
 * Rate control mode.
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_RC_MODE_NONE
 */
	mc_video_rate_control_mode_t mode;

/**
 * The union member is decided according to the
 * @see mc_mjpeg_rate_control_params_t.mode.
 *
 */
	union {
		mc_mjpeg_fix_qp_params_t fix_qp_params;
	};
} mc_mjpeg_rate_control_params_t;

/**
* Define the parameter of rate control.
**/
typedef struct _mc_rate_control_params {
/**
 * Rate control mode.
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_RC_MODE_NONE
 */
	mc_video_rate_control_mode_t mode;
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 *
 */
	union {
		mc_h264_cbr_params_t h264_cbr_params;
		mc_h264_vbr_params_t h264_vbr_params;
		mc_h264_avbr_params_t h264_avbr_params;
		mc_h264_fix_qp_params_t h264_fixqp_params;
		mc_h264_qp_map_params_t h264_qpmap_params;
		mc_h265_cbr_params_t h265_cbr_params;
		mc_h265_vbr_params_t h265_vbr_params;
		mc_h265_avbr_params_t h265_avbr_params;
		mc_h265_fix_qp_params_t h265_fixqp_params;
		mc_h265_qp_map_params_t h265_qpmap_params;
		mc_mjpeg_fix_qp_params_t mjpeg_fixqp_params;
	};
} mc_rate_control_params_t;

/**
* This is a data structure for custom GOP parameters of the given picture.
**/
typedef struct _mc_video_custom_gop_pic_params {
/**
 * A picture type of Nth picture in the custom GOP.
 * The valid numbers are as follows.
 *     0 : I picture
 *     1 : P picture
 *     2 : B picture
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 pic_type;

/**
 * A POC of Nth picture in the custom GOP.
 * Values[1,custom_gop_size]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 poc_offset;

/**
 * A quantization parameter of Nth picture in the custom GOP.
 * Values[0,51]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 30
 */
	hb_u32 pic_qp;

/**
 * The number of reference L0 of Nth picture in the custom GOP.
 * Flag to use multi reference picture for P picture.
 * It is valid only if PIC_TYPE is P.
 * Values[0,1] for XJ3
 * Values[0] for J5
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 num_ref_picL0;

/**
 * A POC of reference L0 of Nth picture in the custom GOP.
 * Values[-custom_gop_size, custom_gop_size]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 ref_pocL0;

/**
 * A POC of reference L1 of Nth picture in the custom GOP.
 * Values[-custom_gop_size, custom_gop_size]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 ref_pocL1;

/**
 * A temporal ID of Nth picture in the custom GOP.
 * Values[0,6]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 temporal_id;
} mc_video_custom_gop_pic_params_t;

/**
* @Deprecated Define the parameters of custom GOP structure.
**/
typedef struct _mc_video_custom_gop_params {
/**
 * The size of custom GOP.
 * Values[0,8]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u32 custom_gop_size;

/**
 * Picture parameters of Nth picture in custom GOP.
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: @see mc_video_custom_gop_pic_params_t
 */
	mc_video_custom_gop_pic_params_t
	    custom_gop_pic_param[MC_MAX_GOP_NUM];
} mc_video_custom_gop_params_t;

/**
* Define the parameters of GOP structure.
**/
typedef struct _mc_video_gop_params {
/**
 * The type of I picture to be inserted at every intra_period.
 * The valid numbers are as follows.
 *     0 : Non-IRAP(I picture, not a clean random access point)
 *     1 : CRA(non-IDR clean random access point)
 *     2 : IDR
 *
 * - Note: It's unchangable parameters in the same sequence.
 *         It's only applied in H265.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: IDR.
 */
	hb_s32 decoding_refresh_type;

/**
 * A GOP structure preset option.
 * The valid numbers are as follows.
 *     0: Custom GOP
 *     1 : I-I-I-I,..I (all intra, gop_size=1)
 *     2 : I-P-P-P,… P (consecutive P, gop_size=1)
 *     3 : I-B-B-B,…B (consecutive B, gop_size=1)
 *     4 : I-B-P-B-P,… (gop_size=2)
 *     5 : I-B-B-B-P,… (gop_size=4)
 *     6 : I-P-P-P-P,… (consecutive P, gop_size=4)
 *     7 : I-B-B-B-B,… (consecutive B, gop_size=4)
 *     8 : I-B-B-B-B-B-B-B-B,… (random access, gop_size=8)
 *     9 : I-P-P-P,… P (consecutive P, gop_size = 1, with single reference)
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 2
 */
	hb_u32 gop_preset_idx;

/**
 * Custom gop parameters. It's valid only if gop_preset_idx = 0.
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: @see mc_video_custom_gop_params_t
 */
	//mc_video_custom_gop_params_t custom_gop_param;
/**
 * The size of custom GOP.
 * Values[1,8]
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_s32 custom_gop_size;

/**
 * Picture parameters of Nth picture in custom GOP.
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: @see mc_video_custom_gop_pic_params_t
 */
	mc_video_custom_gop_pic_params_t
	    custom_gop_pic_param[MC_MAX_GOP_NUM];
} mc_video_gop_params_t;

/**
* Define the encoding parameters of h264 video codec.
**/
typedef struct _mc_h264_enc_config {
/**
 * User add profile information to SPS by setting the profile register.
 * However, if you set 0 or have done nothing to the register, VPU
 * automatically encodes a profile by using the bit depth of source picture.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Unsupport.
 * - Default: MC_H264_PROFILE_UNSPECIFIED;
 */
	mc_h264_profile_t h264_profile;

/**
 * H.264/AVC level_idc in SPS. Please refer to H.264 document.
 * If 'level=0', FW calculates level instead of host setting value.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_H264_LEVEL_UNSPECIFIED
 */
	mc_h264_level_t h264_level;
} mc_h264_enc_config_t;

/**
* Define the encoding parameters of h265 video codec.
**/
typedef struct _mc_h265_enc_config {
/**
 * User add profile information to SPS by setting the profile register.
 * However, if you set 0 or have done nothing to the register, VPU
 * automatically encodes a profile by using the bit depth of source picture.
 * The valid numbers are as follows.
 *     0 : disable main still picture profile
 *     1 : enable main still picture profile
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_bool main_still_picture_profile_enable;

/**
 * H.265/HEVC general_level_idc. Please refer to H.265 document.
 * If 'level=0', FW calculates level instead of host setting value.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_H265_LEVEL_UNSPECIFIED
 */
	mc_h265_level_t h265_level;

/**
 * VPU is able to support up to main and high tier. The tier information
 * can also be given to SPS by setting the tier register.
 * If you set 'h265_level=0', FW calculate level & tier instead of host
 * setting value.
 * The valid numbers are as follows.
 *     0 : Main tier
 *     1 : High tier
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_s32 h265_tier;

/**
 * Please refer to H.265 document.
 * It enables transform skip for intra CU.
 *		  0 : Disable intra transform skip.
 *		  1 : Enable intra transform skip for chroma of CU_8x8
 *			  and luma/chroma of IntraNxN.
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 transform_skip_enabled_flag;

/**
 * VPU can encode in lossless mode where no distortion is allowed in
 * reconstructed frames.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable parameters in the same sequence.
 *         H265 only.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 lossless_mode;

/**
 * It enables temporal motion vector prediction. H265 only.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 tmvp_enable;

/**
 * It enables wave-front parallel processing. H265 only.
 * It enables the use of specific CABAC probabilities synchronization
 * at the beginning of each line of CTBs in order to produce a bitstream
 * that can be decoded using one or more cores.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 wpp_enable;
} mc_h265_enc_config_t;

/**
* Define the encoding parameters of Mjpeg video codec.
**/
typedef struct _mc_mjpeg_enc_config {
/**
 * Specify the number of MCU in the restart interval.
 * Values [0~((picwidth+15)>>4) * ((picheight+15)>>4) * 2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 restart_interval;

/**
 * Specify the validation of huffman table.
 * The Huffman table consists of luminance and chrominance tables.
 * Both luminance and chroma has a DC table and an AC table. The DC table
 * includes 16 BitLength values and 12(baseline)/13(extended) HuffValue values.
 * And the AC table includes 16 BitLength values and 162(baseline)/256(extended)
 * HuffValue values.
 * The valid numbers are as follows.
 *	 0 : invalid
 *	 1 : valid
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_bool huff_table_valid;

/**
 * The Huffman luma DC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */

	hb_u8 huff_luma_dc_bits[16];

/**
 * The Huffman luma DC table for HuffValue values.
 * It has 12(baseline)/13(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_luma_dc_val[16];

/**
 * The Huffman luma AC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_luma_ac_bits[16];

/**
 * The Huffman luma AC table for HuffValue values.
 * It has 162(baseline)/256(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_luma_ac_val[256];

/**
 * The Huffman chroma DC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_dc_bits[16];


/**
 * The Huffman chroma AC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_ac_bits[16];

/**
 * The Huffman chroma DC table for HuffValue values.
 * It has 12(baseline)/13(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_dc_val[16];


/**
 * The Huffman chroma AC table for HuffValue values.
 * It has 162(baseline)/256(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_ac_val[256];

/**
 * Set the 12bit mode.
 * The valid numbers are as follows.
 *  0 : 8bit
 *  1 : 12bit
 *
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_bool extended_sequential;
} mc_mjpeg_enc_config_t;

/**
* Define the encoding parameters of jpeg codec.
**/
typedef struct _mc_jpeg_enc_config {
/* Enable DCF.
 * Unsupported feature
 * The valid numbers are as follows.
 * 	0 : disable
 * 	1  : enable
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool dcf_enable;

/**
 * Specify the number of MCU in the restart interval.
 * Values [0~((picwidth+15)>>4) * ((picheight+15)>>4) * 2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 restart_interval;

/**
 * Quality factor. Qualities 50..100 are converted to scaling percentage
 * 200 - 2*Q. Note that at Q=100 the scaling is 0, it will cause minimum
 * quantization loss and low compressibility. Qualities 1..50 are converted
 * to scaling percentage 5000/Q. Note that at Q=1 the scaling is 5000,
 * it will cause maximum quantization loss and high compressibility.
 * Values[0~100]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 50;
 */
	hb_u32 quality_factor;

/**
 * Specify the validation of huffman table.
 * The Huffman table consists of luminance and chrominance tables.
 * Both luminance and chroma has a DC table and an AC table. The DC table
 * includes 16 BitLength values and 12(baseline)/13(extended) HuffValue values.
 * And the AC table includes 16 BitLength values and 162(baseline)/256(extended)
 * HuffValue values.
 * The valid numbers are as follows.
 *	 0 : invalid
 *	 1 : valid
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_bool huff_table_valid;

/**
 * The Huffman luma DC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */

	hb_u8 huff_luma_dc_bits[16];

/**
 * The Huffman luma DC table for HuffValue values.
 * It has 12(baseline)/13(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_luma_dc_val[16];

/**
 * The Huffman luma AC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_luma_ac_bits[16];

/**
 * The Huffman luma AC table for HuffValue values.
 * It has 162(baseline)/256(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_luma_ac_val[256];

/**
 * The Huffman chroma DC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_dc_bits[16];


/**
 * The Huffman chroma AC table for BitLength values.
 * It has 16 BitLength values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_ac_bits[16];

/**
 * The Huffman chroma DC table for HuffValue values.
 * It has 12(baseline)/13(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_dc_val[16];


/**
 * The Huffman chroma AC table for HuffValue values.
 * It has 162(baseline)/256(extended) HuffValue values.
 * Values[0~255]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 huff_chroma_ac_val[256];

/**
 * Set the 12bit mode.
 * The valid numbers are as follows.
 *  0 : 8bit
 *  1 : 12bit
 *
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_bool extended_sequential;
} mc_jpeg_enc_config_t;

/**
* Define the video codec encoding parameters.
**/
typedef struct _mc_video_codec_enc_params {
/**
 * Specify the width and height of input video in luma samples.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 width, height;

/**
 * Input Pixel format.
 * H264/H265 Values : [MC_PIXEL_FORMAT_YUV420P, MC_PIXEL_FORMAT_NV21].
 * MJPEG/JPEG Values : [MC_PIXEL_FORMAT_YUV420P, MC_PIXEL_FORMAT_VYUY422] and
 *              MC_PIXEL_FORMAT_YUV400
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	mc_pixel_format_t pix_fmt;

/**
 * The number of input FrameBuffer.
 * Values[1,65536]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 5
 */
	hb_u32 frame_buf_count;

/**
 * The value specifies that MediaCodec should using exteranl input frame
 * buffer.
 * The valid numbers are as follows.
 *     0 : use internal frame buffer
 *     1  : use external frame buffer
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool external_frame_buf;

/**
 * Specify the count of bitstream buffers.
 * Values[1,65536]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 5
 */
	hb_u32 bitstream_buf_count;

/**
 * Specify the size of bitstream buffers.
 * Values[64*1024, 2^31-1] for H264/H265, align with 1024
 * Values[8*1024, 2^31-1] for MJPEG/JPEG, align with 4096
 * Values 0 means codec calculates the size.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 10*1024*1024
 */
	hb_u32 bitstream_buf_size;

/**
 * Set the rate control parameters.
 *
 * - Note: It's changable RC parameters.
 *         It's only useful for H264, H265 and MJPEG
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: @see mc_rate_control_params_t;
 */
	mc_rate_control_params_t rc_params;

/**
 * Set the gop parameters.
 *
 * - Note: It's unchangable parameters in the same sequence.
 *         It's only useful for H264 and H265
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: @see mc_video_gop_params_t;
 */
	mc_video_gop_params_t gop_params;

/**
 * VPU/JPU can rotate counterclockwise incoming pictures before starting
 * the ecode process. The decode process doesn't support rotation.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_CCW_0;
 */
	mc_rotate_degree_t rot_degree;

/**
 * VPU/JPU can mirror incoming pictures before starting the ecode process.
 * The decode process doesn't support mirror operation.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_DIRECTION_NONE;
 */
	mc_mirror_direction_t mir_direction;

/* VPU generates frame_cropping_flag syntax in the SPS header.
 * See the frame_crop_*_offset syntax in H.264/AVC SPS tabular form.
 * JPU can crop incoming pictures before starting the ecode process.
 * The valid numbers are as follows.
 *		  0 : disable
 *		  1 : enable
 *
 * - Note: It's unchangable parameters in the same sequence.
 *		   VPU will not crop the input frame, and it just set information
 *		   to SPS.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u32 frame_cropping_flag;

/* This field is valid only when frame_crop_flag = 1.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	mc_av_codec_rect_t crop_rect;

/**
 * Set user pts. Using user frame buffer pts to set the stream pts.
 *
 * - Note: It's unchangable RC parameters.
 *		   It's only useful for H264, H265
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: FALSE
 */
	hb_bool enable_user_pts;
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_enc_config_t h264_enc_config;
		mc_h265_enc_config_t h265_enc_config;
		mc_mjpeg_enc_config_t mjpeg_enc_config;
		mc_jpeg_enc_config_t jpeg_enc_config;
	};
} mc_video_codec_enc_params_t;

/**
* Define the feeding mode of stream during video/audio decoding.
**/
typedef enum _mc_video_stream_feeding_mode {
	MC_FEEDING_MODE_NONE = -1,
	/* Feed the video/audio stream in stream size */
	MC_FEEDING_MODE_STREAM_SIZE,
	/* Feed the video/audio stream in frame size */
	MC_FEEDING_MODE_FRAME_SIZE,
	MC_FEEDING_MODE_TOTAL,
} mc_av_stream_feeding_mode_t;

/**
* Define the decoding parameters of h264 video codec.
**/
typedef struct _mc_h264_dec_config {
/**
 * Support frame reordering. That is, the coded order may be different
 * from the presentation order of the corresponding frames.
 * The valid numbers are as follows.
 *     0 : disable reordering
 *     1 : enable reordering
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 1;
 */
	hb_bool reorder_enable;

/**
 * Enable skip frame mode.
 * The valid numbers are as follows.
 *     0x00: normal DEC_PIC.
 *     0x01: skip non-IRAP.
 *     0x02: skip non-reference picture. It skips to decode non-reference
 *           pictures which correspond to sub-layer non-reference picture
 *           with MAX_DEC_TEMP_ID. (The sub-layer non-reference picture is
 *           the one whose nal_unit_type equal to TRAIL_N, TSA_N, STSA_N,
 *           RADL_N, RASL_N, RSV_VCL_N10, RSV_VCL_N12, or RSV_VCL_N14. )
 *     0x03: thumbnail mode. It skips non-IRAP pictures w/o registering
 *           reference DPB.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 skip_mode;

/**
 * Support bandwidth optimization feature which allows VPU to skip writing
 * compressed format of non-reference pictures or linear format of
 * non-display pictures to the frame buffer for BW saving reason.
 * The valid numbers are as follows.
 *     0 : disable bandwidth optimization
 *     1  : enable bandwidth optimization
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 1
 */
	hb_bool bandwidth_Opt;
} mc_h264_dec_config_t;

/**
* Define the decoding parameters of h265 video codec.
**/
typedef struct _mc_h265_dec_config {
/**
 * Support frame reordering. That is, the coded order may be different
 * from the presentation order of the corresponding frames.
 * The valid numbers are as follows.
 *     0 : disable reordering
 *     1 : enable reordering
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 1;
 */
	hb_bool reorder_enable;

/**
 * Skip frame function enable and operation mode
 * The valid numbers are as follows.
 *     0x00: normal DEC_PIC.
 *     0x01: skip non-IRAP.
 *     0x02: skip non-reference picture.
 *     0x03: thumbnail mode. It skips non-IRAP pictures w/o registering
 *           reference DPB. It's only for XJ3.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 skip_mode;

/**
 * Handle CRA picture as BLA. It skips RASL pictures followd by
 * CRA pictures.
 * The valid numbers are as follows.
 *     0 : Don't handle CRA as BLA
 *     1  : Handle CRA as BLA
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0;
 */
	hb_bool cra_as_bla;

/**
 * Support bandwidth optimization feature which allows VPU to skip writing
 * compressed format of non-reference pictures or linear format of
 * non-display pictures to the frame buffer for BW saving reason.
 * The valid numbers are as follows.
 *     0 : disable bandwidth optimization
 *     1 : enable bandwidth optimization
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 1
 */
	hb_bool bandwidth_Opt;

/**
 * Set the mode of temporal ID selection.
 * The valid numbers are as follows.
 *     0 : use the target temporal_id as absolute value.
 *         TARGET_DEC_TEMP_ID = TARGET_DEC_TEMP_ID_PLUS1 - 1
 *         When use of absolute value for temporal target, decoder can
 *         keep the decoding layer ID. If the SPS_MAX_SUB_LAYER is changed
 *         in the bitstream, the temporal skip ratio can be changed.
 *     1 : use the targe temporal_id as relative value.
 *         TARGET_DEC_TEMP_ID = SPS_MAX_SUB_LAYER - REL_TARGET_DEC_TEMP_ID
 *         SPS_MAX_SUB_LAYER is signalled from bitstream.
 *         When use of relative value decoder can keep the skip ratio
 *         regardless the cange of SPS_MAX_SUB_LAYER in the bitstream.
 *         It's only for XJ3.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 dec_temporal_id_mode;

/**
 * If temporal_id_dec_mode is 0, thie field is used as an absolute target
 * temporal id(TARGET_DEC_TEMP_ID_PLUS1).
 * TARGET_DEC_TEMP_ID = TARGET_DEC_TEMP_ID_PLUS1 - 1
 * Base on TARGET_DEC_TEMP_ID,
 * - 0x0: it decodes a picture of all ranges of tempral ID, which means
 *        temporal ID decoding constraint off.
 * - 0x1~0x6: it decodes a picture if the temporal id is less than or equal
 *            to TARGET_DEC_TEMP_ID. It discards a pciture when its temporal
 *            id is greater than TARGET_DEC_TEMP_ID.
 *
 * If temporal_id_dec_mode is 1, the field is used as a relative target
 * temporal id(REL_TARGET_DEC_TEMP_ID)
 * TARGET_DEC_TEMP_ID = SPS_MAX_SUB_LAYER - REL_TARGET_DEC_TEMP_ID
 * It discards a picture when its temporal id is greater then
 * TARGET_DEC_TEMP_ID.
 * Values[0,7]?
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 1
 */
	hb_u32 target_dec_temporal_id_plus1;
} mc_h265_dec_config_t;

/**
* Define the decoding parameters of MJPEG codec.
**/
typedef struct _mc_mjpeg_dec_config {
/**
 * JPU can rotate counterclockwise decoded pictures.
 * Values[@see mc_rotate_degree_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_CCW_0;
 */
	mc_rotate_degree_t rot_degree;

/**
 * JPU can mirror decoded pictures.
 * Values[@see mc_mirror_direction_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_DIRECTION_NONE;
 */
	mc_mirror_direction_t mir_direction;

/* JPU can crop decoded picture.
 * The valid numbers are as follows.
 *        0 : disable
 *        1 : enable
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool frame_crop_enable;

/* This crop size.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	mc_av_codec_rect_t crop_rect;
} mc_mjpeg_dec_config_t;

/**
* Define the decoding parameters of JPEG codec.
**/
typedef struct _mc_jpeg_dec_config {
/**
 * JPU can rotate counterclockwise decoded pictures.
 * Values[@see mc_rotate_degree_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_CCW_0;
 */
	mc_rotate_degree_t rot_degree;

/**
 * JPU can mirror decoded pictures.
 * Values[@see mc_mirror_direction_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_DIRECTION_NONE;
 */
	mc_mirror_direction_t mir_direction;

	/* JPU can crop decoded picture.
	 * The valid numbers are as follows.
	 *        0 : disable
	 *        1 : enable
	 *
	 * - Note: It's unchangable parameters in the same sequence.
	 * - Encoding: Support.
	 * - Decoding: Unsupport.
	 * - Default: 0
	 */
	hb_bool frame_crop_enable;

	/* This crop size.
	 *
	 * - Note: It's unchangable parameters in the same sequence.
	 * - Encoding: Support.
	 * - Decoding: Unsupport.
	 * - Default:
	 */
	mc_av_codec_rect_t crop_rect;
} mc_jpeg_dec_config_t;

/**
* Define the parameters of video codec decoding.
**/
typedef struct _mc_video_codec_dec_params {
/**
 * Specify the feeding mode of bitstream.
 * Values @see mc_av_stream_feeding_mode_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_FEEDING_MODE_NONE
 */
	mc_av_stream_feeding_mode_t feed_mode;

/**
 * Output pixel format.
 * H264/H265 Values: [MC_PIXEL_FORMAT_YUV420P, MC_PIXEL_FORMAT_NV21]
 * MJPEG/JPEG Values: [MC_PIXEL_FORMAT_NONE, MC_PIXEL_FORMAT_YUV440P] and
 *                    MC_PIXEL_FORMAT_YUV440P is only for YUV422 rotation 90/270
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_PIXEL_FORMAT_YUV420P
 */
	mc_pixel_format_t pix_fmt;

/**
 * Specify the size of bitstream buffer. The buffers are internally
 * allocated by MediaCodec. It's size should be larger than the feeding
 * size. Usually, it should align with 1024.
 * Values[1024, 2^31-1]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 10*1024*1024
 */
	hb_u32 bitstream_buf_size;

/**
 * Specify the count of bitstream buffers.
 * Values[1,65536] for XJ3
 * Values[2,65536] for J5
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 5
 */
	hb_u32 bitstream_buf_count;

/**
 * The value specifies that MediaCodec should using exteranl input bitstream
 * buffer.
 * The valid numbers are as follows.
 *	   0 : use internal bitstream buffer
 *	   1  : use external bitstream buffer
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_bool external_bitstream_buf;

/**
 * The size of FrameBuffer is decided by the MediaCodec according to the
 * sequence information. But users can specify the count of FrameBuffer
 * buffers. VPU may delay decoded picture display for display reordering
 * when H.264/H.265, pic_order_cnt_type 0 or 1 case and for B-frame
 * handling in VC1 decoder. If the specified count is less then the
 * required count, MediaCodec with H264/H265 will modify the
 * specified count to the value that the maximum display frame buffer
 * delay for buffering decoded picture reorder plus.
 * (extra frame buffer number(1) + 1). MediaCodec with MJPEG/JPEG will
 * choose at least 2 frame buffer.
 * Values[1,31]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 5
 */
	hb_u32 frame_buf_count;

/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_dec_config_t h264_dec_config;
		mc_h265_dec_config_t h265_dec_config;
		mc_mjpeg_dec_config_t mjpeg_dec_config;
		mc_jpeg_dec_config_t jpeg_dec_config;
	};
} mc_video_codec_dec_params_t;

/**
* Define the encoding parameters of aac codec.
**/
typedef struct _mc_aac_enc_config {
/**
 * AAC profile
 * Values @see mc_aac_profile_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AAC_PROFILE_MAIN;
 */
	mc_aac_profile_t profile;

/**
 * AAC data type
 * Values @see mc_aac_data_type_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AAC_DATA_TYPE_ADTS;
 */
	mc_aac_data_type_t type;
} mc_aac_enc_config_t;

/**
* Define the encoding parameters of aac codec.
**/
typedef struct _mc_flac_enc_config {
/**
 * FLAC LPC type.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_FLAC_LPC_TYPE_DEFAULT;
 */
	mc_flac_lpc_type_t profile;

/**
 * Compression level.
 * Values[0~12]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 5;
 */
	hb_s32 compression_level;
} mc_flac_enc_config_t;

/**
* Define the encoding parameters of mulaw codec.
**/
typedef struct _mc_mulaw_enc_config {
	/* Reserverd */
} mc_mulaw_enc_config_t;

/**
* Define the encoding parameters of alaw codec.
**/
typedef struct _mc_alaw_enc_config {
	/* Reserverd */
} mc_alaw_enc_config_t;

/**
* Define the encoding parameters of g726 codec.
**/
typedef struct _mc_g726_enc_config {
/**
 * g726 bit rate
 * Values[@see mc_g726_bps_t]
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 */
	mc_g726_bps_t bit_rate;
} mc_g726_enc_config_t;

/**
* Define the encoding parameters of ADPCM codec.
**/
typedef struct _mc_adpcm_enc_config {
	/* Reserverd */
} mc_adpcm_enc_config_t;

/**
* Define the decoding parameters of aac codec.
**/
typedef struct _mc_aac_dec_config {
/**
 * AAC data type
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AAC_DATA_TYPE_ADTS;
 */
	mc_aac_data_type_t type;
/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;
/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;
/**
 * Number of audio channels.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: 0
 */
	hb_u32 channels;
} mc_aac_dec_config_t;

/**
* Define the decoding parameters of aac codec.
**/
typedef struct _mc_flac_dec_config {
/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;
/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;
/**
 * Number of audio channels.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: 0
 */
	hb_u32 channels;
} mc_flac_dec_config_t;

/**
* Define the decoding parameters of mulaw codec.
**/
typedef struct _mc_mulaw_dec_config {
/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;
/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;
/**
 * Number of audio channels.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: 0
 */
	hb_u32 channels;
} mc_mulaw_dec_config_t;

/**
* Define the decoding parameters of alaw codec.
**/
typedef struct _mc_alaw_dec_config {
/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;
/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;
/**
 * Number of audio channels.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: 0
 */
	hb_u32 channels;
} mc_alaw_dec_config_t;

/**
* Define the decoding parameters of g726 codec.
**/
typedef struct _mc_g726_dec_config {
/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;
/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;
/**
 * Number of audio channels.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: 0
 */
	hb_u32 channels;
/**
 * g726 bit rate
 * Values[@see mc_g726_bps_t]
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 */
	mc_g726_bps_t bit_rate;
} mc_g726_dec_config_t;

/**
* Define the decoding parameters of ADPCM codec.
**/
typedef struct _mc_adpcm_dec_config {
/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;
/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: UnSupport.
 * - Decoding: Support. Only setting by user when doing resample
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;
/**
 * Number of audio channels.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 channels;
} mc_adpcm_dec_config_t;

/**
* Define the parameters of audio codec encoding.
**/
typedef struct _mc_audio_codec_enc_params {
/**
 * The average bitrate
 * Values[0,320000]bps
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 16000
 */
	hb_u32 bit_rate;

/**
 * Number of samples per channel in an audio frame.
 * Users must not modify this value!!! And users can read this value after
 * starting the codec. Each submitted frame except the last must contain
 * exactly frame_size samples per channel. May be 0 when the codec has
 * AV_CODEC_CAP_VARIABLE_FRAME_SIZE set, then the frame size is not
 * restricted.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 frame_size;

/**
 * Number of frame buffer. It's used as a input buffer cache.
 * Values[1,200]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 5
 */
	hb_s32 frame_buf_count;

/**
 * Number of encoded audio packet. It's used as a output buffer cache.
 * Values[1,200]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 5
 */
	hb_s32 packet_count;

/**
 * Audio sample format.
 * Values@see mc_audio_sample_format_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;

/**
 * Audio sample rate.
 * Values @see mc_audio_sample_rate_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;

/**
 * Audio channel layout.
 * Values @see mc_audio_channel_layout_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: MC_AV_CHANNEL_LAYOUT_NONE
 */
	mc_audio_channel_layout_t channel_layout;

/**
 * Number of audio channels. It's related with channel layout.
 * Values[0,2]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 channels;

/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	hb_ptr enc_config;
} mc_audio_codec_enc_params_t;

/**
* Define the parameters of audio codec decoding.
**/
typedef struct _mc_audio_codec_dec_params {
/**
 * Specify the feeding mode of stream.
 * Values @see mc_av_stream_feeding_mode_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_FEEDING_MODE_NONE
 */
	mc_av_stream_feeding_mode_t feed_mode;

/**
 * The size of audio packet.
 * Values[0,?]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 20480
 */
	hb_s32 packet_buf_size;

/**
 * Number of encoded audio packet. It's used as a input buffer cache.
 * Values[1,200]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 5
 */
	hb_s32 packet_count;

/**
 * It's the output buffer size in internal_frame_size unit.
 * Values[1,200]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 frame_cache_size;

/**
 * It's the fixed frame size and it may be not equal to real frame size.
 * Users must not modify this value!!! And users can read this value after
 * starting the codec.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 internal_frame_size;

/**
 * Number of frame buffer. It's used as a output buffer cache.
 * Values[1,200]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 5
 */
	hb_s32 frame_buf_count;

/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	hb_ptr dec_config;
} mc_audio_codec_dec_params_t;

/**
* Define the parameters of media codec context.
**/
typedef struct _media_codec_context {
/**
 * Specify the codec type.
 * Vaules @see media_codec_id_t
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: MEDIA_CODEC_ID_NONE
 */
	media_codec_id_t codec_id;

/**
 * Whether the codec is encoder?
 * The valid numbers are as follows.
 *     0 : decoder
 *     1 : encoder
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 1
 */
	hb_bool encoder;

/**
 * Private data. Users must not modify this value!!!
 * Values[0,31]
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: -1
 */
	hb_s32 instance_index;

/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id and media_codec_context_t.encoder.
 */
	union {
		mc_video_codec_enc_params_t video_enc_params;
		mc_video_codec_dec_params_t video_dec_params;
		mc_audio_codec_enc_params_t audio_enc_params;
		mc_audio_codec_dec_params_t audio_dec_params;
	};

/**
 * Private data. Users must not modify this value!!!
 * Values
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: -1
 */
	hb_ptr vpf_context;

/**
 * video command priority
 * see @mc_video_cmd_prio_t
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: NON_PRIO
 */
	_mc_video_cmd_prio_t priority;
} media_codec_context_t;

/**
* Define the encoding startup information of video codec.
**/
typedef struct _mc_video_enc_startup_params {
/**
 * Specify the frame number that MediaCodec should receive.
 * Values[]
 *     <=0: infinite number
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 receive_frame_number;
} mc_video_enc_startup_params_t;

/**
* Define the decoding startup information of video codec.
**/
typedef struct _mc_video_dec_startup_params {
	/* Reserverd */
} mc_video_dec_startup_params_t;

/**
* Define the encoding startup information of audio codec.
**/
typedef struct _mc_audio_enc_startup_params {
	/* Reserverd */
} mc_audio_enc_startup_params_t;

/**
* Define the decoding startup information of audio codec.
**/
typedef struct _mc_audio_dec_startup_params {
	/* Reserverd */
} mc_audio_dec_startup_params_t;

/**
* Define the startup information of media codec.
**/
typedef struct _mc_av_codec_startup_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id and media_codec_context_t.encoder.
 */
	union {
		mc_video_enc_startup_params_t video_enc_startup_params;
		mc_video_dec_startup_params_t video_dec_startup_params;
		mc_audio_enc_startup_params_t audio_enc_startup_params;
		mc_audio_dec_startup_params_t audio_dec_startup_params;
	};
} mc_av_codec_startup_params_t;

/**
* Define the meida codec buffers type.
**/
typedef enum _media_codec_buffer_type {
	MC_VIDEO_FRAME_BUFFER = 0,
	MC_VIDEO_STREAM_BUFFER,
	MC_AUDIO_FRAME_BUFFER,
	MC_AUDIO_STREAM_BUFFER,
} media_codec_buffer_type_t;

/**
* Define the H264/H265 output frame information.
**/
typedef struct _mc_h264_h265_output_frame_info {
/**
 * It indicates that decoding result for enqueued decode command
 * The valid numbers are as follows.
 *     0x00: FAIL
 *     0x01: SUCCESS
 *     0x10: SUCCESS_WITH_WARNING (success but there exist some warning)
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 decode_result;

/**
 * This is a frame buffer index for the picture to be displayed at the
 * moment among frame buffers which are registered using
 * VPU_DecRegisterFrameBuffer(). Frame data to be displayed are stored into
 * the frame buffer with this index.
 * When there is no display delay, this index is always the same with
 * frame_decoded_index. However, if display delay does exist for display
 * reordering in AVC or B-frames in VC1), this index might be different
 * with frame_decoded_index. By checking this index, HOST application can
 * easily know whether sequence decoding has been finished or not.
 * The valid numbers are as follows.
 *     -3(0xFFFD) or -2(0xFFFE) : a display output cannot be given due to
 *                                picture reordering or skip option.
 *     -1(0xFFFF) : there is no more output for display
 *                  at the end of sequence decoding.
 *      > 0 : Normal display index.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 frame_display_index;

/**
 * This is a frame buffer index of decoded picture among frame buffers
 * which were registered using VPU_DecRegisterFrameBuffer(). The currently
 * decoded frame is stored into the frame buffer specified by this index.
 * The valid numbers are as follows.
 *     -2 : it indicates that no decoded output is generated because
 *          decoder meets EOS (End Of Sequence) or skip.
 *     -1 : it indicates that decoder fails to decode a picture because
 *          there is no available frame buffer.
 *     > 0 : Normal decode index.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 frame_decoded_index;

/**
 * This is the physical start address of corresponding stream buffer.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_u64 stream_start_addr;

/**
 * This is the size of corresponding stream buffer.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 stream_size;

/**
 * This is the picture type of decoded picture.
 * @see mc_h264_nal_unit_type_t
 * @see mc_h265_nal_unit_type_t
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 nalu_type;

/**
 * This is the number of error coded unit in a decoded picture (
 * frame_decoded_index)
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 err_mb_in_frame_decoded;

/**
 * This is the number of coded unit in a decoded picture.(
 * frame_decoded_index)
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 total_mb_in_frame_decoded;


/**
 * This is the number of error coded unit in a picture mapped to
 * frame_display_index.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 err_mb_in_frame_display;


/**
 * This is the number of coded unit in a picture
 * mapped to frame_display_index.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 total_mb_in_frame_display;


/**
 * This field reports the display rectangular region in pixel unit.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	mc_av_codec_rect_t display_rect;

/**
 * This field reports the width of a picture to be displayed in pixel unit.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 display_width;

/**
 * This field reports the height of a picture to be displayed in pixel unit.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 display_height;

/**
 * This field reports the decoded rectangular region in pixel unit.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	mc_av_codec_rect_t decoded_rect;


/**
 * This is aspect ratio information for each standard.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 aspect_rate_info;

/**
 * The numerator part of frame rate fraction. If frame rate syntax is not
 * decoded in bitstream, the value of frame_rate_numerator is equal to -1.
 * If frame_rate_numerator and frame_rate_denominator are not-zero values,
 * FrameRate is derived as follows.
 * FrameRate =frame_rate_numerator/frame_rate_denominator.
 * Otherwise if frame_rate_numerator or frame_rate_denominator are zero
 * values, the value of FrameRate is invalid.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 frame_rate_numerator;

/**
 * The denominator part of frame rate fraction. If frame rate syntax is not
 * decoded in bitstream, the value of frame_rate_denominator is equal to -1.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 frame_rate_denominator;

/**
 * A POC value of picture with display index. When frame_display_index is
 * -1, it returns -1.
 *
 * - Note: h265 only
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 display_poc;

/**
 * A POC value of picture that has currently been decoded and with decoded
 * index. When frame_decoded_index is -1, it returns -1.
 *
 * - Note: h265 only
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 decoded_poc;

/**
 * This variable reports the error reason that occurs while decoding.
 * For error description, please find the 'Appendix: Error Definition'
 * in the Programmer's Guide.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 error_reason;

/**
 * This variable reports the warning information that occurs while decoding.
 * For warning description, please find the 'Appendix: Error Definition'
 * in the Programmer's Guide.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 warn_info;

/**
 * This variable increases by 1 whenever sequence changes.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 sequence_no;

/**
 * A temporal ID of Nth picture in the custom GOP.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 temporal_id;

/**
 * This variable reports the value of pic_output_flag syntax in HEVC
 * slice_segment_header.
 *
 * - Note:  h265 only
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 output_flag;

/**
 * A CTU size
 *    16 : CTU16x16
 *    32 : CTU32x32
 *    64 : CTU64x64
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 ctu_size;
} mc_h264_h265_output_frame_info_t;

/**
* Define the MJPEG/JPEG output frame information.
**/
typedef struct _mc_mjpeg_jpeg_output_frame_info {
/**
 * It indicates that decoding result for enqueued decode command
 * The valid numbers are as follows.
 *     0x00: FAIL with error block
 *     0x01: SUCCESS
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 decode_result;

/**
 * This is a frame buffer index for the picture to be displayed at the
 * moment among frame buffers which are registered using
 * JPU_DecRegisterFrameBuffer(). Frame data to be displayed are stored into
 * the frame buffer with this index.
 *     -1(0xFFFF) : ?
 *      > 0 : Normal display index.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 frame_display_index;

/**
 * This is the physical start address of corresponding stream buffer.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_u64 stream_start_addr;

/**
 * This is the size of corresponding stream buffer.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 stream_size;

/**
 * JPEG error restart index.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 err_rst_idx;

/**
 * JPEG error MCU position X.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 err_pos_x;

/**
 * JPEG error MCU position Y.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 err_pos_y;

/**
 * This field reports the width of a picture to be displayed in pixel unit.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 display_width;

/**
 * This field reports the height of a picture to be displayed in pixel unit.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_s32 display_height;

/**
 * Set the 12bit mode.
 * The valid numbers are as follows.
 *  0 : 8bit
 *  1 : 12bit
 *
 * - Note: It's unchangable parameters.
 * - Encoding: unupport.
 * - Decoding: Support.
 * - Default: 0;
 */
	hb_bool extended_sequential;
/**
 * This variable increases by 1 whenever sequence changes.
 *
 * - Note:
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 sequence_no;
} mc_mjpeg_jpeg_output_frame_info_t;

/**
* Define the coded H264 stream information.
**/
typedef struct _mc_h264_h265_output_stream_info {
/**
 * The encoded picture number
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_bool frame_index;

/**
 * This is the Y component physical start address of corresponding frame buffer.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u64 frame_start_addr;

/**
 * This is the frame size of  corresponding frame buffer.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_s32 frame_size;

/**
 * This is the picture type of encoded picture.
 * Only report I, P and B type.
 * @see mc_h264_nal_unit_type_t
 * @see mc_h265_nal_unit_type_t
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_s32 nalu_type;

/**
 * The slice idx of the currently being encoded Picture.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 slice_idx;

/**
 * The number of slices of the currently being encoded Picture.
 * For H264.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 slice_num;

/**
 * The number of slices of the currently being encoded Picture.
 * For H265.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 dependent_slice_num;
	hb_u32 independent_slice_num;

/**
 * A flag which represents whether the current encoding has been
 * skipped or not.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 pic_skipped;

/**
 * The number of intra coded block.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 intra_block_num;

/**
 * The number of skip block in 8x8.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 skip_block_num;

/**
 * The average value of MB QPs.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 avg_mb_qp;

/**
 * The number of encoded picture bytes.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 enc_pic_byte;

/**
 * The GOP index of the currently encoded picture.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 enc_gop_pic_idx;

/**
 * The POC(picture order count) value of the currently encoded picture.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 enc_pic_poc;

/**
 * The source buffer index of the currently encoded picture.
 * (-2 : encoding delay)
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 enc_src_idx;

/**
 * Encoded picture number.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 enc_pic_cnt;

/**
 * Encoding error reason.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 enc_error_reason;

/**
 * Encoding warn information.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 enc_warn_info;

/**
 * The parameter for reporting the cycle number of encoding one frame.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_cycle;

/**
 * Temporal ID.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 temporal_id;

/**
 * Longterm reference type and it's valid when longterm reference mode is
 * enabled.
 *
 * - Note: 0: normal frame
 *         1: longterm reference frame
 *         2: using longterm reference frame
 *         3: longterm reference and using longterm reference frame
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 longterm_ref_type;
} mc_h264_h265_output_stream_info_t;

/**
* Define the MJPEG/JPEG output stream information.
**/
typedef struct _mc_mjpeg_jpeg_output_stream_info {
/**
 * This is the Y component physical start address of corresponding frame buffer.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u64 frame_start_addr;

/**
 * This is the frame size of  corresponding frame buffer.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_s32 frame_size;

/**
 * The slice idx of the currently being encoded Picture.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: unSupport.
 * - Default: 0
 */
	hb_u32 slice_idx;

/**
 * The number of slices of the currently being encoded Picture.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 slice_num;

/**
 * The parameter for reporting the cycle number of encoding one frame.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 frame_cycle;
} mc_mjpeg_jpeg_output_stream_info_t;

/**
* Define the audio output frame information.
**/
typedef struct _mc_audio_output_frame_info {
/**
 * The average bitrate
 * Values[>0]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0

hb_s64 bit_rate;     */
} mc_audio_output_frame_info_t;

/**
* Define the audio output stream information.
**/
typedef struct _mc_audio_output_stream_info {
	/* Reservered */
} mc_audio_output_stream_info_t;

/**
* Define the meida codec output buffer information.
**/
typedef struct _media_codec_output_buffer_info {
/**
 * It's decided by the media_codec_buffer_type.
 */
	union {
		mc_h264_h265_output_frame_info_t video_frame_info;
		mc_h264_h265_output_stream_info_t video_stream_info;
		mc_mjpeg_jpeg_output_frame_info_t jpeg_frame_info;
		mc_mjpeg_jpeg_output_stream_info_t jpeg_stream_info;
		mc_audio_output_frame_info_t audio_frame_info;
		mc_audio_output_stream_info_t audio_stream_info;
	};
} media_codec_output_buffer_info_t;

/**
* Define the information of video frame buffer(H264/H265/MJPEG/JPEG).
**/
typedef struct _mc_video_frame_buffer_info {
/**
 * Buffer virtual address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u8 *vir_ptr[3];

/**
 * Buffer physical address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 phy_ptr[3];

/**
 * Buffer size.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 size;

/**
 * Y/Cb/Cr component Buffer size.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 compSize[3];

/**
 * Specify the width and height of input video in luma samples.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s32 width, height;

/**
 * Pixel format.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	mc_pixel_format_t pix_fmt;

/**
 * Luma stride.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s32 stride;

/**
 * Chroma stride.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s32 vstride;

/**
 * Luma and chroma fd. It's for ion operaion.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s32 fd[3];

/**
 * Frame pts
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 pts;

/**
 * Source buffer index.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 src_idx;

/**
 * It indicates the end of frame.
 * The valid numbers are as follows.
 *     0 : False
 *     1 : True
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0.
 */
	hb_bool frame_end;

/**
 *.
 * Specify the validation of qp map.
 *     0 : invalid
 *     1  : valid
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_bool qp_map_valid;

/**
 * Specify the qp map. The QP map array should be written a series
 * of 1 byte QP values for each Macroblock in raster scan order.
 * The H264 Macroblock size is 16x16.
 * QP Values[0~51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: NULL
 */
	hb_byte qp_map_array;

/**
 * Specify the qp map number. It's related with the picture width
 * and height.
 * Values[1, MC_VIDEO_MAX_MB_NUM]
 *
 * - Note: It's changable parameter in the same sequence
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 qp_map_array_count;

/**
 * Reserverd
 */
	hb_s32 flags;
} mc_video_frame_buffer_info_t;

/**
* Define the information of video stream buffers(H264/H265/MJPEG/JPEG).
**/
typedef struct _mc_video_stream_buffer_info {
/**
 * Buffer virtual address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u8 *vir_ptr;

/**
 * Buffer physical address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 phy_ptr;

/**
 * Buffer size.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 size;

/**
 * Stream pts
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 pts;

/**
 * ION fd. It's for ion operaion???
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s32 fd;

/**
 * Source buffer index.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s32 src_idx;

/**
 * It indicates the end of stream.
 * The valid numbers are as follows.
 *     0 : False
 *     1 : True
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0.
 */
	hb_bool stream_end;
} mc_video_stream_buffer_info_t;

/**
* Define the information of audio frame buffers.
**/
typedef struct _mc_audio_frame_buffer_info {
/**
 * Buffer virtual address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u8 *vir_ptr;

/**
 * Buffer physical address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 phy_ptr;

/**
 * Buffer size.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 size;

/**
 * Audio sample format.
 * Values[@see mc_audio_sample_format_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: .
 * - Decoding: Support.
 * - Default: MC_AV_SAMPLE_FMT_NONE
 */
	mc_audio_sample_format_t sample_fmt;

/**
 * Audio sample rate.
 * Values[@see mc_audio_sample_rate_t]
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: MC_AV_SAMPLE_RATE_NONE
 */
	mc_audio_sample_rate_t sample_rate;

/**
 * Audio channel layout.
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Unsupport.
 * - Decoding: Support.
 * - Default: 0
 */
	mc_audio_channel_layout_t channel_layout;

/**
 * Frame pts
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s64 pts;

/**
 * It indicates the end of frame.
 * The valid numbers are as follows.
 *     0 : False
 *     1 : True
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_bool frame_end;
} mc_audio_frame_buffer_info_t;

/**
* Define the information of audio stream buffers.
**/
typedef struct _mc_audio_stream_buffer_info {
/**
 * Buffer virtual address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u8 *vir_ptr;

/**
 * Buffer physical address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 phy_ptr;

/**
 * Buffer size.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 size;

/**
 * Stream pts
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_s64 pts;

/**
 * It indicates the end of stream.
 * The valid numbers are as follows.
 *     0 : False
 *     1 : True
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_bool stream_end;
} mc_audio_stream_buffer_info_t;

/**
* Define the attribute of audio encoder.
**/
typedef struct _audio_encode_param_t {
/**
 * The type of audio encoder
 * @see media_codec_id_t
 */
	media_codec_id_t ff_type;
/**
 * The max length of frame
 */
	hb_s32 ff_max_frm;
/**
 * The codec name of audio encoder
 */
	char ff_codec_name[256];
/**
 * The function pointer of open encoder
 */
	hb_s32 (*ff_audio_open_encoder)(hb_ptr ff_encoder_param,
			hb_ptr *ff_encoder);
/**
 * The function pointer of encode frame
 */
	hb_s32 (*ff_audio_encode_frame)(hb_ptr ff_encoder,
			mc_audio_frame_buffer_info_t *audio_frame,
			hb_u8 *outbuf, hb_s32 *outlen);
/**
 * The function pointer of close encoder
 */
	hb_s32 (*ff_audio_close_encoder)(hb_ptr ff_encoder);
} mc_audio_encode_param_t;

/**
* Define the attribute of audio decoder.
**/
typedef struct _audio_decode_param_t {
/**
 * The type of audio decoder
 * @see media_codec_id_t
 */
	media_codec_id_t ff_type;
/**
 * The max length of frame
 */
	char ff_codec_name[256];
/**
 * The codec name of audio decoder
 */
	hb_s32 (*ff_audio_open_decoder)(hb_ptr ff_decoder_param,
			hb_ptr *ff_decoder);
/**
 * The function pointer of decode frame
 */
	hb_s32 (*ff_audio_decode_frame)(hb_ptr ff_decode, hb_u8 *inbuf,
			hb_s32 inlen, mc_audio_frame_buffer_info_t *outbuf,
			hb_s32 *outlen);
/**
 * The function pointer of close decoder
 */
	hb_s32 (*ff_audio_close_decoder)(hb_ptr ff_decoder);
} mc_audio_decode_param_t;

/**
* Define the meida codec buffers.
**/
typedef struct _media_codec_buffer {
/**
 * Buffer type.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: FRAME_BUFFER
 */
	media_codec_buffer_type_t type;

/**
 * It's decided by the media_codec_buffer_type.
 */
	union {
		mc_video_frame_buffer_info_t vframe_buf;
		mc_video_stream_buffer_info_t vstream_buf;
		mc_audio_frame_buffer_info_t aframe_buf;
		mc_audio_stream_buffer_info_t astream_buf;
	};
} media_codec_buffer_t;

/**
* Define the functions of media callback.
**/
typedef struct _media_codec_callback {
/**
 * Notify users that an input buffer is available.
 *
 * @param[in]       user data
 * @param[in]       media codec buffer
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	void (*on_input_buffer_available)(hb_ptr userdata,
			media_codec_buffer_t * buffer);

/**
 * Notify users that an output buffer is available.
 *
 * @param[in]       user data
 * @param[in]       media codec buffer
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	void (*on_output_buffer_available)(hb_ptr userdata,
			media_codec_buffer_t * buffer,
			media_codec_output_buffer_info_t * info);

/**
 * Notify users that an internal message is triggered.
 *
 * @param[in]       user data
 * @param[in]       error
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	void (*on_media_codec_message)(hb_ptr userdata, hb_s32 error);

/**
 * Notify users that an vlc buffer size message is triggered.
 * And user can modify the size to save memory and guarantee the
 * validity. If the size is too small, encoder may report error during
 * registering buffer or encoding.
 *
 * @param[in]       user data
 * @param[out]      vlc buffer size
 *
 * - Note: It's only useful for encoding.
 * - Encoding: Support.
 * - Decoding: unsupport.
 * - Default: 0
 */
	void (*on_vlc_buffer_message)(hb_ptr userdata, hb_s32 * vlc_buf);
} media_codec_callback_t;

/**
enum H265_PROFILE {
UNSPECIFIED_PROFILE,
MAIN_PROFILE,
MAIN10_PROFILE,
MAIN_STILL_PICTURE_PROFILE,
};
**/

/**
* Define the parameters of longterm reference mode.
**/
typedef struct _mc_video_longterm_ref_mode {
/**
 * It enables long-term reference mode.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 use_longterm;

/**
 * It specifies the period of long-term reference picture.
 * Values[0,2^31-1]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 longterm_pic_period;

/**
 * It specifies the period of using long-term reference picture.
 * Values[0,2^31-1]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 longterm_pic_using_period;
} mc_video_longterm_ref_mode_t;

/**
* Define the parameters of H264/H265 intra refresh.
**/
typedef struct _mc_video_intra_refresh_params {
/**
 * An intra refresh mode, only applied in H264/H265.
 * IntraRefresh can be enabled for error robustness. Host application
 * can specify the number of intra MBs or CTBs in a non-intra picture and
 * IntraRefresh mode.
 * The valid IntraRefresh modes are as follows.
 *     0: no intra refresh
 *     1: row
 *     2: column
 *     3: step size in MB or CTU
 *     4: adaptive intra refresh (only for H265 of XJ3)
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 intra_refresh_mode;

/**
 * It specifies an intra MB or CTU refresh interval. Depending on
 * intra_refresh_mode, it can be one of the followings.
 *     intra_refresh_mode = 1: The number of consecutive MB/CTU rows.
 *         Values(0, Max Mb(16x16) or CTU(64x64) number in row]
 *     intra_refresh_mode = 2: The number of consecutive MB/CTU columns.
 *         Values(0, Max Mb(16x16) or CTU(64x64) number in column]
 *     intra_refresh_mode = 3: A step size in MB/CTU.
 *         Values(0, Max Mb(16x16) or CTU(64x64) number in picture]
 *     intra_refresh_mode = 4: The number of intra MB/CTU to be encoded
 *                             in a picture.
 *        Values(0, Max Mb(16x16) or CTU(64x64) number in picture]
 *
 * Values[0,2^31-1]
 *
 * - Note:It's unchangable parameter in the same sequence.
 *        The intra_refresh_mode 4 can't work with lossless and ROI mode.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u32 intra_refresh_arg;
} mc_video_intra_refresh_params_t;

/**
* Define the parameter of h264 deblocking filter.
**/
typedef struct _mc_h264_deblk_filter_params {
/**
 * Please refer to H.264 document.
 * Values[0~2]
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 disable_deblocking_filter_idc;

/**
 * Please refer to H.264 document.
 * Values[-6~6]
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 slice_alpha_c0_offset_div2;

/**
 * Please refer to H.264 document.
 * Values[-6~6]
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 slice_beta_offset_div2;
} mc_h264_deblk_filter_params_t;

/**
* Define the parameter of h265 deblocking filter.
**/
typedef struct _mc_h265_deblk_filter_params {
/**
 * Please refer to H.265 document.
 * Values[0~1]
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 slice_deblocking_filter_disabled_flag;

/**
 * Please refer to H.265 document.
 * Values[-6~6]
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 slice_beta_offset_div2;

/**
 * Please refer to H.265 document.
 * Values[-6~6]
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 slice_tc_offset_div2;

/**
 * It enables filtering across slice boundaries for in-loop deblocking.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u32 slice_loop_filter_across_slices_enabled_flag;
} mc_h265_deblk_filter_params_t;

/**
* Define the parameter of deblocking filter.
**/
typedef struct _mc_video_deblk_filter_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_deblk_filter_params_t h264_deblk;
		mc_h265_deblk_filter_params_t h265_deblk;
	};
} mc_video_deblk_filter_params_t;

/**
* Define the parameter of h265 sample adaptive offset (SAO).
**/
typedef struct _mc_h265_sao_params {
/**
 * It applies the sample adaptive offset process to the reconstructed
 * picture after the deblocking filter process. It enables both luma and
 * chroma components.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable parameter.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 sample_adaptive_offset_enabled_flag;
} mc_h265_sao_params_t;

/**
* Define the parameter of h264 entropy coding.
**/
typedef struct _mc_h264_entropy_params {
/**
 * It selects the entropy coding method used in the encoding process.
 * The valid numbers are as follows.
 *     0 : CAVLC
 *     1 : CABAC
 *
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_u32 entropy_coding_mode;
} mc_h264_entropy_params_t;

/**
* Define the parameter of h264 timing.
**/
typedef struct _mc_h264_timing_params {
/**
 * It specifies the number of time units of a clock operating at
 * the frequency time_scale Hz. This is used to to calculate
 * frameRate syntax.
 *  Values[>0]
 *
 * - Note: It's unchangable SPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1000
 */
	hb_u32 vui_num_units_in_tick;

/**
 * It specifies the number of time units that pass in one second.
 * This is used to to calculate frameRate syntax.
 * Values[>0]
 *
 * - Note: It's unchangable SPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: framerate*1000
 */
	hb_u32 vui_time_scale;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u32 fixed_frame_rate_flag;
} mc_h264_timing_params_t;

/**
* Define the parameter of h265 timing.
**/
typedef struct _mc_h265_timing_params {
/**
 * It specifies the number of time units of a clock operating at the frequency
 * time_scale Hz. This is used to to calculate frameRate syntax.
 *  Values[0, 2^31 - 1]
 *
 * - Note: It's unchangable SPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1000
 */
	hb_u32 vui_num_units_in_tick;

/**
 * It specifies the number of time units that pass in one second.
 * This is used to to calculate frameRate syntax.
 * Values[0, 2^31 - 1]
 *
 * - Note: It's unchangable SPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: framerate*1000
 */
	hb_u32 vui_time_scale;

/**
 * It specifies the number of clock ticks corresponding to a difference
 * of picture order count values equal to 1. This is used to calculate
 * frameRate syntax.
 * Values[0 ~ (2^31-2)]
 *
 * - Note: It's unchangable SPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 vui_num_ticks_poc_diff_one_minus1;
} mc_h265_timing_params_t;

/**
* Define the parameters of VUI timing.
**/
typedef struct _mc_video_vui_timing_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_timing_params_t h264_timing;
		mc_h265_timing_params_t h265_timing;
	};
} mc_video_vui_timing_params_t;

/**
* Define the parameter of h264 vui.
**/
typedef struct _mc_h264_vui_params {
/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 aspect_ratio_info_present_flag;

/**
 * Please refer to H.264 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u8 aspect_ratio_idc;

/**
 * Please refer to H.264 document.
 * Values (0, 65535]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u16 sar_width;

/**
 * Please refer to H.264 document.
 * Values (0, 65535]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u16 sar_height;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *       0 : disable
 *       1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 overscan_info_present_flag;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *       0 : disable
 *       1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 overscan_appropriate_flag;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *       0 : disable
 *       1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 video_signal_type_present_flag;

/**
 * Please refer to H.264 document.
 * Values [0, 7]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 video_format;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *       0 : disable
 *       1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 video_full_range_flag;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *       0 : disable
 *       1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 colour_description_present_flag;

/**
 * Please refer to H.264 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 colour_primaries;

/**
 * Please refer to H.264 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 transfer_characteristics;

/**
 * Please refer to H.264 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 matrix_coefficients;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *       0 : disable
 *       1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 vui_timing_info_present_flag;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 * Values [0, 2^31 - 1]
 * - Note: It's unchangable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1000.
 */
	hb_u32 vui_num_units_in_tick;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 * Values [0, 2^31 - 1]
 * - Note: It's unchangable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: framerate*1000.
 */
	hb_u32 vui_time_scale;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u8 vui_fixed_frame_rate_flag;

/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 bitstream_restriction_flag;
} mc_h264_vui_params_t;

/**
* Define the parameter of h265 vui.
**/
typedef struct _mc_h265_vui_params {
/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *	   0 : disable
 *	   1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 aspect_ratio_info_present_flag;

/**
 * Please refer to H.265 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u8 aspect_ratio_idc;

/**
 * Please refer to H.265 document.
 * Values (0, 65535]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u16 sar_width;

/**
 * Please refer to H.265 document.
 * Values (0, 65535]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u16 sar_height;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 overscan_info_present_flag;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 overscan_appropriate_flag;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 video_signal_type_present_flag;

/**
 * Please refer to H.265 document.
 * Values [0, 5]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 video_format;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 video_full_range_flag;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 colour_description_present_flag;

/**
 * Please refer to H.265 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 colour_primaries;

/**
 * Please refer to H.265 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 transfer_characteristics;

/**
 * Please refer to H.265 document.
 * Values [0, 255]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 matrix_coefficients;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 vui_timing_info_present_flag;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 * Values [0, 2^31 - 1]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1000.
 */
	hb_u32 vui_num_units_in_tick;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 * Values [0, 2^31 - 1]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: framerate*1000.
 */
	hb_u32 vui_time_scale;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 vui_poc_proportional_to_timing_flag;

/**
 * Please refer to H.265 document.
 * Values[0 ~ (2^31-2)]
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u32 vui_num_ticks_poc_diff_one_minus1;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *		 0 : disable
 *		 1 : enable
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u8 bitstream_restriction_flag;
} mc_h265_vui_params_t;

/**
* Define the parameters of VUI.
**/
typedef struct _mc_video_vui_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_vui_params_t h264_vui;
		mc_h265_vui_params_t h265_vui;
	};
} mc_video_vui_params_t;

/**
* Define the parameters of H264 slice encode.
**/
typedef struct _mc_h264_slice_params {
/**
 * A slice mode for independent slice.
 * The valid numbers are as follows.
 *     0: no multi-slice
 *     1: slice in MB number.
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 h264_slice_mode;

/**
 * The number of MB for a slice when h264_slice_mode is set with 1.
 * Values[0,2^31-1]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 h264_slice_arg;
} mc_h264_slice_params_t;

/**
* Define the parameters of H265 slice encode.
**/
typedef struct _mc_h265_slice_params {
/**
 * A slice mode for independent slice.
 * The valid numbers are as follows.
 *     0: no multi-slice
 *     1: slice in CTU number
 *     2: slice in CTU number with interrupt. It's only for J5
 *
 * - Note: It's changable RDO parameters and the slice interrupt is
 *         unchangable.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 h265_independent_slice_mode;

/**
 * The number of CTU for a slice when independSliceMode is set with 1.
 * Values[0,2^16-1]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 h265_independent_slice_arg;

/**
 * A slice mode for dependent slice.
 * The valid numbers are as follows.
 *     0: no multi-slice
 *     1 : slice in CTU number
 *     2 : slice in number of byte
 *     3 : slice in CTU number with interrupt. It's only for J5
 *     4 : slice in number of byte with interrupt. It's only for J5
 *
 * - Note: It's changable RDO parameters and the slice interrupt is
 *         unchangable.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 h265_dependent_slice_mode;

/**
 * The number of CTU or bytes for a slice when h265_dependent_slice_mode
 * is set with 1 or 2.
 * Values[0,2^16-1]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_s32 h265_dependent_slice_arg;
} mc_h265_slice_params_t;

/**
* Define the parameters of jpeg/mjpeg slice encode.
**/
typedef struct _mc_mjpeg_slice_params {
/**
 * It specifies a slice height for slice encoding.
 * Values[0, picture height]
 *
 * - Note: It's unchangable parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.(no slice)
 */
	hb_u32 mjpeg_slice_height;
} mc_mjpeg_slice_params_t;

/**
* Define the parameters of slice mode.
**/
typedef struct _mc_video_slice_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_slice_params_t h264_slice;
		mc_h265_slice_params_t h265_slice;
		mc_mjpeg_slice_params_t mjpeg_slice;
	};
} mc_video_slice_params_t;

/**
* Define the parameters of h264/h265 smart backgroud encoding.
* It's only for XJ3.
**/
typedef struct _mc_video_smart_bg_enc_params {
/**
 * It enables background detection.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable RDO parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 bg_detect_enable;

/**
 * It specifies the threshold of max difference that is used in s2me block.
 * It is valid when background detection is on.
 * Values[0~255]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 8
 */
	hb_s32 bg_threshold_diff;

/**
 * It specifies the threshold of mean difference that is used in s2me block.
 * It is valid  when background detection is on.
 * Values[0~255]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1
 */
	hb_s32 bg_threshold_mean_diff;

/**
 * It specifies the minimum lambda QP value to be used in the background area.
 * Values[0~51]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 32
 */
	hb_s32 bg_lambda_qp;

/**
 * It specifies the difference between the lambda QP value of background and
 * the lambda QP value of foreground.
 * Values[-16~15]
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 3
 */
	hb_s32 bg_delta_qp;

/**
 * It disables s2me_fme (only for AVC encoder).
 * The valid numbers are as follows.
 *     0 : enable
 *     1 : disable
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 s2fme_disable;
} mc_video_smart_bg_enc_params_t;

/**
* Define the parameters of H264 intra perdiction.
**/
typedef struct _mc_h264_intra_pred_params {
/**
 * Please refer to H.264 document.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u32 constrained_intra_pred_flag;
} mc_h264_intra_pred_params_t;

/**
* Define the parameters of H265 prediction unit.
**/
typedef struct _mc_h265_pred_unit_params {
/**
 * It enables intra NxN PUs in intra CUs.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 1.
 */
	hb_u32 intra_nxn_enable;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 * - Note: It's changable PPS parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u32 constrained_intra_pred_flag;

/**
 * Please refer to H.265 document.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 * - Note: It's unchangable parameters in same sequence.
 *         It's only for XJ3.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0.
 */
	hb_u32 strong_intra_smoothing_enabled_flag;

/**
 * It specifies the number of merge candidates in RDO (0,1 or 2). HEVC only.
 * The valid numbers are as follows.
 *     1: improves encoding performance.
 *     2: offers better quality of encoded picture,
 *
 * - Note: It's changable RDO parameters.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 2.
 */
	hb_u32 max_num_merge;
} mc_h265_pred_unit_params_t;

/**
* Define the parameter of intra prediction.
**/
typedef struct _mc_video_pred_unit_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_intra_pred_params_t h264_intra_pred;
		mc_h265_pred_unit_params_t h265_pred_unit;
	};
} mc_video_pred_unit_params_t;

/**
* Define the user h264/h265 scaling list. User should specify the data.
**/
#define MC_SL_MATRIX_NUM       6

typedef struct _mc_h264_transform_params {
/**
 * It enables 8x8 transform.
 * The valid numbers are as follows.
 *     0 : disable 8x8 transform (BP)
 *     1 : enable 8x8 transform (HP)
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 transform_8x8_enable;

/**
 * The value of chroma(Cb) QP offset.
 * Values[-12~12]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 chroma_cb_qp_offset;

/**
 * The value of chroma(Cr) QP offset.
 * Values[-12~12]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 chroma_cr_qp_offset;

/**
 * Enable the user scaling list. ScalingList should include every scaling
 * list according to INTER/INTRA, prediction mode 4x4/8x8, and LUMA/CHROMA
 * just as H.264/AVC standard defines.
 * The valid numbers are as follows.
 *     0 : disable user scaling list
 *     1 : enable using user defined scaling list
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 user_scaling_list_enable;

/**
 * The element is listed as follows. And each element has 16 coefficients.
 * "INTRA4X4_LUMA, INTRA4X4_CHROMAU, INTRA4X4_CHROMAV,
 * INTER4X4_LUMA, INTER4X4_CHROMAU, INTER4X4_CHROMAV"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_4x4[MC_SL_MATRIX_NUM][16];

/**
 * The element is listed as follows. And each element has 64 coefficients.
 * "INTRA8X8_LUMA,INTER8X8_LUMA"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_8x8[2][64];
} mc_h264_transform_params_t;

typedef struct _mc_h265_transform_params {
/**
 * The value of chroma(Cb) QP offset.
 * Values[-12~12]
 *
 * - Note: It's changable parameter in the same sequence for XJ3.
 *         It's only for XJ3.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 chroma_cb_qp_offset;

/**
 * The value of chroma(Cr) QP offset.
 * Values[-12~12]
 *
 * - Note: It's changable parameter in the same sequence for XJ3.
 *         It's only for XJ3.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 chroma_cr_qp_offset;

/**
 * Enable the user scaling list. ScalingList should include every scaling
 * list according to INTER/INTRA, prediction mode 4x4/8x8/16x16/32x32,
 * and LUMA/CHROMA just as H.265/HEVC standard defines.
 * The valid numbers are as follows.
 *     0 : disable user scaling list
 *     1 : enable using user defined scaling list
 * - Note: It's unchangable parameter in the same sequence.
 *         When user enable this, the J5 uses the default scaling list.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 user_scaling_list_enable;

/**
 * The element is listed as follows. And each element has 16 coefficients.
 * "INTRA4X4_LUMA, INTRA4X4_CHROMAU, INTRA4X4_CHROMAV,
 * INTER4X4_LUMA, INTER4X4_CHROMAU, INTER4X4_CHROMAV"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_4x4[MC_SL_MATRIX_NUM][16];

/**
 * The element is listed as follows. And each element has 64 coefficients.
 * "INTRA8X8_LUMA, INTRA8X8_CHROMAU, INTRA8X8_CHROMAV,
 * INTER8X8_LUMA, INTER8X8_CHROMAU, INTER8X8_CHROMAV"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_8x8[MC_SL_MATRIX_NUM][64];

/**
 * The element is listed as follows. And each element has 64 coefficients.
 * "INTRA16X16_LUMA, INTRA16X16_CHROMAU, INTRA16X16_CHROMAV,
 * INTER16X16_LUMA, INTER16X16_CHROMAU, INTER16X16_CHROMAV"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_16x16[MC_SL_MATRIX_NUM][64];

/**
 * The element is listed as follows. And each element has 64 coefficients.
 * "INTRA32X32_LUMA, INTER32X32_LUMA"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_32x32[2][64];

/**
 * The element is listed as follows. And each element has 1 coefficient.
 * "INTRA16X16_LUMA_DC, INTRA16X16_CHROMAU_DC, INTRA16X16_CHROMAV_DC,
 * INTER16X16_LUMA_DC, INTER16X16_CHROMAU_DC, INTER16X16_CHROMAV_DC"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_dc_16x16[MC_SL_MATRIX_NUM];

/**
 * The element is listed as follows. And each element has 1 coefficient.
 * "INTRA32X32_LUMA_DC, INTER32X32_LUMA_DC"
 * Values[1~255]
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u8 scaling_list_dc_32x32[2];
} mc_h265_transform_params_t;

/**
* Define the transform parameters.
**/
typedef struct _mc_video_transform_params {
/**
 * The union member is decided according to the
 * @see media_codec_context_t.codec_id.
 */
	union {
		mc_h264_transform_params_t h264_transform;
		mc_h265_transform_params_t h265_transform;
	};
} mc_video_transform_params_t;

/**
* Define the parameters of H264/H265 ROI encoding.
**/
typedef struct _mc_video_roi_params {
/**
 * It enable ROI encoding throughout the sequence level.
 * It only support CTU QP map mode.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's unchangable RC parameter in the same sequence.
 *         It's valid when rate control is on.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 roi_enable;

/**
 * Specify the ROI map. The ROI map array should be written a series
 * of 1 byte QP values for each Macroblock in raster scan order.
 * Values[0~51]
 *
 * - Note: It's changable parameter in the same sequence.
 *         It's valid only when roi_enable = 1.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_byte roi_map_array;

/**
 * Specify the ROI map number.
 * Values[1, MC_VIDEO_MAX_MB_NUM] for h264,
 * and the size should be (ALIGN16(picWidth)>>4)*(ALIGN16(picHeight)>>4)
 * Values[1, MC_VIDEO_MAX_SUB_CTU_NUM] for XJ3 h265
 * and the size should be (ALIGN64(picWidth)>>5)*(ALIGN64(picHeight)>>5)
 * Values[1, MC_VIDEO_MAX_CTU_NUM] for J5 h265
 * and the size should be (ALIGN64(picWidth)>>6)*(ALIGN64(picHeight)>>6)
 *
 * - Note: It's unchangable parameter in the same sequence.
 *         It's valid only when roi_enable = 1.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 roi_map_array_count;
} mc_video_roi_params_t;

/**
* Define the parameters of H264/H265 ROI-Ex encoding.
**/
typedef struct _mc_video_roi_params_ex {
/**
 * It specifies ROI mode. ROI supports CTU ROI map and CTU QP map mode.
 * The CTU ROI map mode supports setting importance level(0 ~ 8) of the ROI
 * for the #th CTU. The final QP of the CTUs is
 * QP(importance_level) = QP(non-ROI) - (roi_delta_qp * importance_level).
 * The larger value means the #th CTU (ROI) is more
 * important than other CTUs. It can work with rate control.
 *
 * The CTU QP map mode supports setting QP(0 ~ 51) of the ROI
 * for the #th CTU. The setting QP is just the final QP of the CTUs.
 * The larger value means poor quality. It can't work with
 * rate control.
 * The valid numbers are as follows.
 *     0 : disable roi
 *     1 : CTU ROI map mode
 *     2 : CTU QP map mode
 *
 * - Note: It's unchangable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 roi_mode;

/**
 * It specifies ROI index. Max 64 number roi regions are supported.
 * The index 0 region has the highest priority.
 * Values[0~63]
 *
 * - Note: It's changable RC parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 roi_idx;

/**
 * It enables ROI encoding throughout the sequence level.
 * The valid numbers are as follows.
 *	   0 : disable
 *	   1 : enable
 *
 * - Note: It's changable RC parameter in the same sequence.
 *		   It's valid when rate control is on.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 roi_enable;

/**
 * It specifies ROI values for CTUs in the specified region.
 * Values[0,8] for CTU ROI map mode
 * Values[0,51] for CTU QP map mode
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u8 roi_val;

/**
 * It specifies ROI Delta QP for CTU ROI map mode.
 * Values[0,51]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 roi_delta_qp;

/* It specifies ROI region. The coordinate unit is CTU(64x64 for HEVC).
 * Values[0, ALIGN64(picWidth)>>6] or [0, ALIGN64(picHeight)>>6]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	mc_av_codec_rect_t crop_rect;
} mc_video_roi_params_ex_t;

/**
* Define the parameters of block encoding mode decision.
* It's only for XJ3.
**/
typedef struct _mc_video_mode_decision_params {
/**
 * It enables mode decision.
 * The valid numbers are as follows.
 *	   0 : disable
 *	   1 : enable
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 mode_decision_enable;

/**
 * A value which is added to the total cost of 4x4 blocks
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu04_delta_rate;

/**
 * A value which is added to the total cost of 8x8 blocks
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu08_delta_rate;

/**
 * A value which is added to the total cost of 16x16 blocks
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu16_delta_rate;

/**
 * A value which is added to the total cost of 32x32 blocks
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu32_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 4x4 Planar intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu04_intra_planar_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 4x4 DC intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu04_intra_dc_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 4x4 angular intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu04_intra_angle_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 8x8 Planar intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu08_intra_planar_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 8x8 DC intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu08_intra_dc_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 8x8 Angular intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu08_intra_angle_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 16x16 Planar intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu16_intra_planar_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 16x16 DC intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu16_intra_dc_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 16x16 Angular intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu16_intra_angle_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 32x32 Planar intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu32_intra_planar_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 32x32 DC intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu32_intra_dc_delta_rate;

/**
 * A value which is added to rate when calculating cost(=distortion + rate)
 * in 32x32 Angular intra prediction mode.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 pu32_intra_angle_delta_rate;

/**
 * A value which is added to rate when calculating cost for intra CU8x8
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu08_intra_delta_rate;

/**
 * A value which is added to rate when calculating cost for inter CU8x8
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu08_inter_delta_rate;

/**
 * A value which is added to rate when calculating cost for merge CU8x8
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu08_merge_delta_rate;

/**
 * A value which is added to rate when calculating cost for intra CU16x16
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu16_intra_delta_rate;

/**
 * A value which is added to rate when calculating cost for inter CU16x16
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu16_inter_delta_rate;

/**
 * A value which is added to rate when calculating cost for merge CU16x16
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu16_merge_delta_rate;

/**
 * A value which is added to rate when calculating cost for intra CU32x32
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu32_intra_delta_rate;

/**
 * A value which is added to rate when calculating cost for inter CU32x32
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu32_inter_delta_rate;

/**
 * A value which is added to rate when calculating cost for merge CU32x32
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_s32 cu32_merge_delta_rate;
} mc_video_mode_decision_params_t;

/**
* Define the user data structure
**/
typedef struct _mc_user_data_buffer {
/**
 * Specify the validation of user data.
 *	   0 : invalid
 *	   1 : valid
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0.
 */
	hb_bool user_data_valid;

/**
 * Buffer_size
 * Values[0,]
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u32 size;

/**
 * Physical address.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u64 phys_addr;

/**
 * virtual user space address
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default: 0
 */
	hb_u8 *virt_addr;
} mc_user_data_buffer_t;

/**
* Define the parameters of MJPEG encoding.
**/
typedef struct _mc_mjpeg_enc_params {
/**
 * The set of 64 quantization values of luma component used to
 * quantize the DCT coefficients. It's for baseline sequential encoding(8Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 0.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u8 luma_quant_table[64];

/**
 * The set of 64 quantization values of chroma component used to
 * quantize the DCT coefficients. It's for baseline sequential encoding(8Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 0.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u8 chroma_quant_table[64];

/**
 * The set of 64 quantization values of luma component used to
 * quantize the DCT coefficients. It's for extended sequential encoding(12Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 1.
 * Values[0~65535]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u16 luma_quant_es_table[64];

/**
 * The set of 64 quantization values of chroma component used to
 * quantize the DCT coefficients. It's for extended sequential encoding(12Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 1.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u16 chroma_quant_es_table[64];

/**
 * Specify the number of MCU in the restart interval.
 * Values [0~((picwidth+15)>>4) * ((picheight+15)>>4) * 2]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 restart_interval;
} mc_mjpeg_enc_params_t;

/**
* Define the parameters of JPEG encoding.
**/
typedef struct _mc_jpeg_enc_params {
/**
 * Quality factor. Qualities 50..100 are converted to scaling percentage
 * 200 - 2*Q. Note that at Q=100 the scaling is 0, it will cause minimum
 * quantization loss and low compressibility. Qualities 1..50 are converted
 * to scaling percentage 5000/Q. Note that at Q=1 the scaling is 5000,
 * it will cause maximun quantization loss and high compressibility.
 * When Q=0, 50 are used as the new quality factor.
 * Values[0~100]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 50;
 */
	hb_u32 quality_factor;

/**
 * The set of 64 quantization values of luma component used to
 * quantize the DCT coefficients. It's for baseline sequential encoding(8Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 0.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u8 luma_quant_table[64];

/**
 * The set of 64 quantization values of chroma component used to
 * quantize the DCT coefficients. It's for baseline sequential encoding(8Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 0.
 * Values[0~255]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u8 chroma_quant_table[64];

/**
 * The set of 64 quantization values of luma component used to
 * quantize the DCT coefficients. It's for extended sequential encoding(12Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 1.
 * Values[0~65535]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u16 luma_quant_es_table[64];

/**
 * The set of 64 quantization values of chroma component used to
 * quantize the DCT coefficients. It's for extended sequential encoding(12Bit).
 * And It's valid only when mc_jpeg_enc_config_t.extended_sequential = 1.
 * Values[0~65535]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u16 chroma_quant_es_table[64];

/**
 * Specify the number of MCU in the restart interval.
 * Values [0~((picwidth+15)>>4) * ((picheight+15)>>4) * 2]
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0
 */
	hb_u32 restart_interval;

/**
 * crop flag, to decide crop or not
 * The valid numbers are as follows
 * 0:disable
 * 1:enable
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding:Support
 * - Decoding:Support
 * - Default:0
 */
  hb_bool crop_en;

/**
 *crop rect area
 *valid only when crop_en enable
 *
 * - Note: It's changable parameter in the same sequence.
 * - Encoding:Support
 * - Decoding:Support
 * - Default:
 */
  mc_av_codec_rect_t crop_rect;
} mc_jpeg_enc_params_t;

/**
* Define the status of MediaCodec.
**/
typedef struct _mc_inter_status {
/**
 * Current input buffer count. The input buffer may be frames waiting for
 * encoding or streams waiting for decoding. During decoding,
 * if MC_FEEDING_MODE_FRAME_SIZE is set, the input_buf_cnt also means
 * frame count to be decoded.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 cur_input_buf_cnt;

/**
 * Current input buffer size. The input buffer may be frames waiting for
 * encoding or streams waiting for decoding.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u64 cur_input_buf_size;

/**
 * Current output buffer count. The output buffer may be decoded frames
 * or encoded streams.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 cur_output_buf_cnt;

/**
 * Current output buffer size. The output buffer may be decoded frames
 * or encoded streams.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u64 cur_output_buf_size;

/**
 * Left receiving frames count. It's valid only when receive_frame_number
 * in mc_video_enc_startup_params_t is set. It means the left frames count
 * that should be received.
 * @mc_video_enc_startup_params_t
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u32 left_recv_frame;

/**
 * Left encoding frames count. It's valid only when receive_frame_number
 * in mc_video_enc_startup_params_t is set. It means the left frames count
 * that should be encoded.
 * @mc_video_enc_startup_params_t
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_u32 left_enc_frame;

/**
 * Total received buffer count. It means total received frame or stream buffer
 * count.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 total_input_buf_cnt;

/**
 * Total processed buffer count. It means total encoded frame count or decoded
 * frame count.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 total_output_buf_cnt;

/**
 * Camera pipeline ID. It's valid for video encoding.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_s32 pipeline;

/**
 * Camera pipeline channel port id. It's valid for video encoding.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default:
 */
	hb_s32 channel_port_id;
} mc_inter_status_t;

typedef struct _mc_user_status {
/**
 * Current user output buffer count.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 cur_user_output_buf_cnt;

/**
 * Current output buffer size.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u64 cur_user_output_buf_size;

/**
 * Total processed buffer count.
 *
 * - Note:
 * - Encoding: Support.
 * - Decoding: Support.
 * - Default:
 */
	hb_u32 total_user_output_buf_cnt;
} mc_user_status_t;

/**
* Define the parameters of 3DNR encoding.
**/
typedef struct _mc_video_3dnr_enc_params {
/**
 * It enables noise reduction algorithm to Y component.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_y_enable;

/**
 * It enables noise reduction algorithm to Cb component.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_cb_enable;

/**
 * It enables noise reduction algorithm to Cr component.
 * The valid numbers are as follows.
 *	   0 : disable
 *	   1 : enable
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_cr_enable;

/**
 * It enables noise estimation for noise reduction. When this is disabled,
 * host carries out noise estimation with nr_noise_sigmaY/Cb/Cr.
 * The valid numbers are as follows.
 *     0 : disable
 *     1 : enable
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_est_enable;

/**
 * A weight to Y noise level for intra picture (0 ~ 31).
 * nr_intra_weight/4 is multiplied to the noise level that has been estimated.
 * This weight is put for intra frame to be filtered more strongly or
 * more weakly than just with the estimated noise level.
 * Values[0~31]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 7;
 */
	hb_u32 nr_intra_weightY;

/**
 * A weight to Cb noise level for intra picture (0 ~ 31).
 * Values[0~31]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 7;
 */
	hb_u32 nr_intra_weightCb;

/**
 * A weight to Cr noise level for intra picture (0 ~ 31).
 * Values[0~31]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 7;
 */
	hb_u32 nr_intra_weightCr;

/**
 * A weight to Y noise level for inter picture (0 ~ 31).
 * nr_inter_weightY/4 is multiplied to the noise level that has been estimated.
 * This weight is put for inter frame to be filtered more strongly or
 * more weakly than just with the estimated noise level.
 * Values[0~31]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 4;
 */
	hb_u32 nr_inter_weightY;

/**
 * A weight to Cb noise level for inter picture (0 ~ 31).
 * Values[0~31]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 4;
 */
	hb_u32 nr_inter_weightCb;

/**
 * A weight to Cr noise level for inter picture (0 ~ 31).
 * Values[0~31]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 4;
 */
	hb_u32 nr_inter_weightCr;

/**
 * It specifies Y noise standard deviation when nr_est_enable is 0.
 * Values[0~255]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_noise_sigmaY;

/**
 * It specifies Cb noise standard deviation when nr_est_enable is 0.
 * Values[0~255]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_noise_sigmaCb;

/**
 * It specifies Cr noise standard deviation when nr_est_enable is 0.
 * Values[0~255]
 *
 * - Note: It's changable parameters in the same sequence.
 * - Encoding: Support.
 * - Decoding: Unsupport.
 * - Default: 0;
 */
	hb_u32 nr_noise_sigmaCr;
} mc_video_3dnr_enc_params_t;

/**
* Get the descriptor of the specified code id.
*
* @param[in]       codec id
*
* @return NULL means no descriptor
* @see media_codec_id_t
* @see media_codec_descriptor_t
*/
extern const media_codec_descriptor_t *hb_mm_mc_get_descriptor(
										media_codec_id_t codec_id);
/**
* Get the default media codec context.
*
* @param[out]       muxer context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_get_default_context(media_codec_id_t codec_id,
				hb_bool encoder, media_codec_context_t *context);

/**
* Initialize the codec and codec context. If success, MediaCodec will
* enter into MEDIA_CODEC_STATE_INITIALIZED state. And it's invalid to initialize
* the media codec if the MediaCodec's state isn't MEDIA_CODEC_STATE_UNINITIALIZED.
*
* @param[out]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_initialize(media_codec_context_t *context);

/**
* Configure the codec using the specified parameters. If success, MediaCodec
* will enter into MEDIA_CODEC_STATE_CONFIGURED state.
*
* @param[in]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_configure(media_codec_context_t *context);

/**
* Set callback to media codec. And Encoder/Decoder will work in async mode.
* The function should be called before MediaCodec is started.
*
* @param[in]       codec context
* @param[in]       media codec callback @see media_codec_callback_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_callback_t
*/
extern hb_s32 hb_mm_mc_set_callback(media_codec_context_t *context,
				const media_codec_callback_t *callback, hb_ptr userdata);

/**
* Set VLC buffer size listener to media codec. And user can modify the
* size of VLC buffer size.
* The function should be called before MediaCodec is started.
*
* @param[in]	   codec context
* @param[in]	   media codec callback @see media_codec_callback_t
*				   only on_vlc_buffer_message is useful.
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_callback_t
*/
extern hb_s32 hb_mm_mc_set_vlc_buffer_listener(
				media_codec_context_t *context,
				const media_codec_callback_t *callback, hb_ptr userdata);

/**
* It can specify the channel information of camera.
*
* @param[in]       codec context
* @param[in]       pipeline
* @param[in]       IPU channel port id
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_set_camera(media_codec_context_t *context,
				hb_s32 pipeline, hb_s32 channel_port_id);

/**
* Start the codec processing. VPU will create encoder instance, decode stream,
* register framebuffer, encoder header and so on. If success, MediaCodec
* will enter into MEDIA_CODEC_STATE_STARTED state.
*
* @param[in]       codec context
* @param[in]       startup information
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_av_codec_startup_params_t
*/
extern hb_s32 hb_mm_mc_start(media_codec_context_t *context,
				const mc_av_codec_startup_params_t * info);

/**
* Stop the codec processing. If success, MediaCodec will be reset and
* enter into MEDIA_CODEC_STATE_INITIALIZED state.
*
* @param[in]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_stop(media_codec_context_t *context);

/**
* Pause the codec processing. If success, MediaCodec will be paused and
* enter into MEDIA_CODEC_STATE_PAUSED state.
*
* @param[in]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_pause(media_codec_context_t *context);

/**
* Flush the input and output buffers of the codec. And MediaCodec will enter
* into MEDIA_CODEC_STATE_FLUSHING state. If success, the input and output
* bufffers of MediaCodec will be flushed and MediaCodec will enter into
* MEDIA_CODEC_STATE_STARTED state.
*
* @param[in]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_flush(media_codec_context_t *context);

/**
* Release the codec. MediaCodec will be released and go back to
* MEDIA_CODEC_STATE_UNINITIALIZED state.
*
* @param[in]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_release(media_codec_context_t *context);

/**
* Get the state of media codec. The value is enum media_codec_state_t.
*
* @param[in]       codec context
* @param[out]      av codec state @see media_codec_state_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_state_t
*/
extern hb_s32 hb_mm_mc_get_state(media_codec_context_t *context,
				media_codec_state_t *state);

/**
* Get the status of media codec.
*
* @param[in]	   codec context
* @param[out]	   av codec status @see mc_inter_status_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_inter_status_t
*/
extern hb_s32 hb_mm_mc_get_status(media_codec_context_t *context,
				mc_inter_status_t *status);

/**
* Queue the input buffer into MediaCodec. The operation is valid only if
* MediaCodec's state is MEDIA_CODEC_STATE_STARTED.
*
* @param[in]       codec context
* @param[out]      media codec buffer @see media_codec_buffer_t
* @param[in]      timeout in ms
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_buffer_t
*/
extern hb_s32 hb_mm_mc_queue_input_buffer(
				media_codec_context_t *context,
				media_codec_buffer_t *buffer,
				hb_s32 timeout);

/**
* Dequeue the input buffer from MediaCodec. The operation is valid only if
* MediaCodec's state is MEDIA_CODEC_STATE_STARTED.
*
* @param[in]       codec context
* @param[out]      media codec buffer @see media_codec_buffer_t
* @param[in]       timeout in ms
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_buffer_t
*/
extern hb_s32 hb_mm_mc_dequeue_input_buffer(
				media_codec_context_t *context,
				media_codec_buffer_t *buffer,
				hb_s32 timeout);

/**
* Queue the output buffer into MediaCodec. The operation is valid only if
* MediaCodec's state is MEDIA_CODEC_STATE_STARTED.
*
* @param[in]       codec context
* @param[out]      media codec buffer @see media_codec_buffer_t
* @param[in]      timeout in ms
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_buffer_t
*/
extern hb_s32 hb_mm_mc_queue_output_buffer(
				media_codec_context_t *context,
				media_codec_buffer_t *buffer,
				hb_s32 timeout);

/**
* Dequeue the output buffer from MediaCodec. The operation is valid only if
* MediaCodec's state is MEDIA_CODEC_STATE_STARTED.
*
* @param[in]       codec context
* @param[out]      media codec buffer @see media_codec_buffer_t
* @param[out]      stream information
*                  @see mc_h264_h265_output_frame_info_t
*                  @see mc_mjpeg_jpeg_output_frame_info_t
*                  @see mc_h264_h265_output_stream_info_t
*                  @see mc_mjpeg_jpeg_output_stream_info_t
*                  @see mc_audio_output_frame_info_t
*                  @see mc_audio_output_stream_info_t
* @param[in]       timeout in ms
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see media_codec_buffer_t
* @see mc_h264_h265_output_frame_info_t
*/
extern hb_s32 hb_mm_mc_dequeue_output_buffer(
				media_codec_context_t *context,
				media_codec_buffer_t *buffer,
				media_codec_output_buffer_info_t*info,
				hb_s32 timeout);

/**
* Get the parameters of long-term reference mode.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      long-term reference parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_longterm_ref_mode_t
*/
extern hb_s32 hb_mm_mc_get_longterm_ref_mode(
				media_codec_context_t *context,
				mc_video_longterm_ref_mode_t *params);

/**
* Set the parameters of long-term reference mode.
*
* Only applied in H264 and H265 codec.
* Support dynamic setting.
*
* @param[in]       codec context
* @param[in]       long-term reference parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_intra_refresh_params_t
*/
extern hb_s32 hb_mm_mc_set_longterm_ref_mode(
				media_codec_context_t *context,
				const mc_video_longterm_ref_mode_t *params);

/**
* Get the parameters of the intra refresh.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      intra refresh parameters @see mc_video_intra_refresh_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_intra_refresh_params_t
*/
extern hb_s32 hb_mm_mc_get_intra_refresh_config(
				media_codec_context_t *context,
				mc_video_intra_refresh_params_t *params);

/**
* Set the parameters of the intra refresh.
*
* Intra refresh mode can be enabled for error robustness.
* Host application can specify the number of intra CTBs in a
* non-intra picture and intra refresh mode.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       intra refresh parameters @see mc_video_intra_refresh_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_intra_refresh_params_t
*/
extern hb_s32 hb_mm_mc_set_intra_refresh_config(
				media_codec_context_t *context,
				const mc_video_intra_refresh_params_t *params);

/**
* Get the parameters of rate control.
*
* Only applied in H264, H265 and MJPEG codec.
*
* @param[in]       codec context
* @param[out]      rate control parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_rate_control_params_t
*/
extern hb_s32 hb_mm_mc_get_rate_control_config(
				media_codec_context_t *context,
				mc_rate_control_params_t *params);

/**
* Set the parameters of rate control.
*
* Only applied in H264, H265 and MJPEG codec.
* Support dynamic setting.
*
* @param[in]       codec context
* @param[in]       rate control parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_rate_control_params_t
*/
extern hb_s32 hb_mm_mc_set_rate_control_config(
				media_codec_context_t *context,
				const mc_rate_control_params_t *params);

/**
* Get the max bit rate of rate control. It's only useful for AVBR.
*
* Only applied in H265 codec of J5.
*
* @param[in]	   codec context
* @param[out]	   max bitrate
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_get_max_bit_rate_config(
				media_codec_context_t *context,
				hb_u32 *params);

/**
* Set the max bit rate of AVBR rate control.
* The max bitrate of the encoded data in kbps. When max bit rate is < bit_rate,
* the peak transmission bitrate is supposed to be infinitely great.
* Values[0,700000]kbps
*
* Only applied in H265 codec of J5.
* Support dynamic setting.
*
* @param[in]	   codec context
* @param[in]	   rate control parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_set_max_bit_rate_config(
				media_codec_context_t *context,
				hb_u32 params);

/**
* Get the parameters of deblock filter.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      deblock filter parameters @see mc_video_deblk_filter_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_deblk_filter_params_t
*/
extern hb_s32 hb_mm_mc_get_deblk_filter_config(
				media_codec_context_t *context,
				mc_video_deblk_filter_params_t *params);

/**
* Set the parameters of deblock filter.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       deblock filter parameters @see mc_video_deblk_filter_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_deblk_filter_params_t
*/
extern hb_s32 hb_mm_mc_set_deblk_filter_config(
				media_codec_context_t *context,
				const mc_video_deblk_filter_params_t *params);

/**
* Get the parameters of sample adaptive offset.
*
* Only applied in H265 codec.
*
* @param[in]       codec context
* @param[out]      SAO parameters @see mc_h265_sao_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_h265_sao_params_t
*/
extern hb_s32 hb_mm_mc_get_sao_config(media_codec_context_t *context,
				mc_h265_sao_params_t *params);

/**
* Set the parameters of sample adaptive offset.
*
* Only applied in H265 codec.
*
* @param[in]       codec context
* @param[in]       SAO parameters @see mc_h265_sao_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_h265_sao_params_t
*/
extern hb_s32 hb_mm_mc_set_sao_config(media_codec_context_t *context,
				const mc_h265_sao_params_t *params);

/**
* Get the parameters of entropy coding.
*
* Only applied in H264 codec.
*
* @param[in]       codec context
* @param[out]      entropy coding parameters @see mc_h264_entropy_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_h264_entropy_params_t
*/
extern hb_s32 hb_mm_mc_get_entropy_config(
				media_codec_context_t *context,
				mc_h264_entropy_params_t *params);

/**
* Set the parameters of entropy coding mode.
*
* Only applied in H264 codec?
*
* @param[in]       codec context
* @param[in]       entropy coding parameters @see mc_h264_entropy_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_h264_entropy_params_t
*/
extern hb_s32 hb_mm_mc_set_entropy_config(
				media_codec_context_t *context,
				const mc_h264_entropy_params_t *params);

/**
* Get the timing parameters of VUI.
*
* Only applied in H264 and H265 codec
*
* @param[in]       codec context
* @param[out]      VUI timing parameters @see mc_video_vui_timing_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_vui_timing_params_t
*/
extern hb_s32 hb_mm_mc_get_vui_timing_config(
				media_codec_context_t *context,
				mc_video_vui_timing_params_t *params);

/**
* Set the timing parameters of VUI.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       VUI timing parameters @see mc_video_vui_timing_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_vui_timing_params_t
*/
extern hb_s32 hb_mm_mc_set_vui_timing_config(
				media_codec_context_t *context,
				const mc_video_vui_timing_params_t *params);

/**
* Get the parameters of VUI.
*
* Only applied in H264 and H265 codec
*
* @param[in]	   codec context
* @param[out]	   VUI parameters @see mc_video_vui_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_vui_params_t
*/
extern hb_s32 hb_mm_mc_get_vui_config(
				media_codec_context_t *context,
				mc_video_vui_params_t *params);

/**
* Set the parameters of VUI.
*
* Only applied in H264 and H265 codec.
*
* @param[in]	   codec context
* @param[in]	   VUI parameters @see mc_video_vui_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_vui_params_t
*/
extern hb_s32 hb_mm_mc_set_vui_config(
				media_codec_context_t *context,
				const mc_video_vui_params_t *params);

/**
* Get the slice parameters.
*
* Applied in H264/H265/MJPEG/JPEG codec.
*
* @param[in]       codec context
* @param[out]      slice parameters @see mc_video_slice_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_slice_params_t
*/
extern hb_s32 hb_mm_mc_get_slice_config(media_codec_context_t *context,
				mc_video_slice_params_t *params);

/**
* Set the slice parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       slice parameters @see mc_video_slice_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_slice_params_t
*/
extern hb_s32 hb_mm_mc_set_slice_config(media_codec_context_t *context,
				const mc_video_slice_params_t *params);

/**
* Insert user data.
*
* Only applied in H264/H265/JPEG/MJPEG codec.
*
* @param[in]       codec context
* @param[in]       data, must be "UUID+string"
* @param[in]       length (0, 1024]
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_insert_user_data(media_codec_context_t * context,
			hb_u8 *data, hb_u32 length);

/**
* Request the IDR Frame.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_request_idr_frame(media_codec_context_t *context);

/**
* Request the VPS/SPS/PPS header.
*
* Only applied in H264 and H265 codec.
* Support dynamic setting for XJ3.
*
* @param[in]       codec context
* @param[in]       force header mode
*                  The valid numbers are as follows.
*                  0 : No froced header(VPS/SPS/PPS)
*                  1 : Forced header before IDR frame
*                  2 : Forced header before I frame for H264
*                      or forced header before CRA and IDR frame for H265
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_request_idr_header(
				media_codec_context_t *context, hb_u32 force_header);

/**
* Enable the IDR Frame.
*
* Only applied in H264 and H265 codec.
*
* @param[in]	   codec context
* @param[in]	   enalbe/diable idr frame, default enable
*                  The valid numbers are as follows.
*                  0 : Disable
*                  1 : Enable
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_enable_idr_frame(
				media_codec_context_t *context, hb_bool enable);

/**
* Request skip the picture. The encoder ignores sourceFrame and generates
* a skipped picture. In this case, the reconstructed image at decoder side
* is a duplication of the previous picture. The skipped picture is
* encoded as P-type regardless of the GOP size.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       source buffer index
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_skip_pic(media_codec_context_t * context,
				hb_s32 src_idx);

/**
* Get the parameters of 3DNR (3D Noise Reduction).
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]       3dnr encoding parameters
*                  @see mc_video_3dnr_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_3dnr_enc_params_t
*/
extern hb_s32 hb_mm_mc_get_3dnr_enc_config(
				media_codec_context_t *context,
				mc_video_3dnr_enc_params_t *params);

/**
* Set the parameters of 3DNR (3D Noise Reduction) for better quality of image
* under low light.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       smart background encoding parameters
*                  @see mc_video_3dnr_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_3dnr_enc_params_t
*/
extern hb_s32 hb_mm_mc_set_3dnr_enc_config(
				media_codec_context_t *context,
				const mc_video_3dnr_enc_params_t *params);

/**
* Get the parameters of smart background encoding.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      smart background encoding parameters
*                  @see mc_video_smart_bg_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_smart_bg_enc_params_t
*/
extern hb_s32 hb_mm_mc_get_smart_bg_enc_config(
				media_codec_context_t *context,
				mc_video_smart_bg_enc_params_t *params);

/**
* Set the parameters of smart background encoding.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       smart background encoding parameters
*                  @see mc_video_smart_bg_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_smart_bg_enc_params_t
*/
extern hb_s32 hb_mm_mc_set_smart_bg_enc_config(
				media_codec_context_t *context,
				const mc_video_smart_bg_enc_params_t *params);

/**
* Get the intra prediction parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      intra prediction @see mc_video_pred_unit_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_pred_unit_params_t
*/
extern hb_s32 hb_mm_mc_get_pred_unit_config(
				media_codec_context_t *context,
				mc_video_pred_unit_params_t *params);

/**
* Set the intra prediction parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       intra prediction @see mc_video_pred_unit_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_pred_unit_params_t
*/
extern hb_s32 hb_mm_mc_set_pred_unit_config(
				media_codec_context_t *context,
				const mc_video_pred_unit_params_t *params);

/**
* Get the transform parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      transform parameters @see mc_video_transform_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_transform_params_t
*/
extern hb_s32 hb_mm_mc_get_transform_config(
				media_codec_context_t *context,
				mc_video_transform_params_t *params);

/**
* Set the transform parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       transform parameters @see mc_video_transform_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_transform_params_t
*/
extern hb_s32 hb_mm_mc_set_transform_config(
				media_codec_context_t *context,
				const mc_video_transform_params_t *params);

/**
* Get the ROI parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      ROI parameters @see mc_video_roi_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_roi_params_t
*/
extern hb_s32 hb_mm_mc_get_roi_config(media_codec_context_t * context,
				mc_video_roi_params_t * params);

/**
* Set the ROI parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       ROI parameters @see mc_video_roi_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_roi_params_t
*/
extern hb_s32 hb_mm_mc_set_roi_config(media_codec_context_t * context,
				const mc_video_roi_params_t *params);

/**
* Get the ROI average QP.
*
* Only applied in H264 and H265 codec.
*
* @param[in]	   codec context
* @param[out]	   ROI average QP
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_get_roi_avg_qp(media_codec_context_t * context,
				hb_u32 * params);

/**
* Set the ROI average QP.
*
* Only applied in H264 and H265 codec.
* It's useful only when CBR or AVBR is on.
*
* @param[in]	   codec context
* @param[in]	   ROI average QP [0, 51]
*                  0: using the average qp of QP map.
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_set_roi_avg_qp(media_codec_context_t * context,
				hb_u32 params);

/**
* Get the ROI parameters.
*
* Only applied in J5 H265 codec.
*
* @param[in]	   codec context
* @param[in]	   roi index
* @param[out]	   ROI parameters @see mc_video_roi_params_ex_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_roi_params_ex_t
*/
extern hb_s32 hb_mm_mc_get_roi_config_ex(media_codec_context_t *context,
				hb_u32 roi_idx, mc_video_roi_params_ex_t *params);

/**
* Set the ROI parameters.
*
* Only applied in J5 H265 codec.
*
* @param[in]	   codec context
* @param[in]	   ROI parameters @see mc_video_roi_params_ex_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_roi_params_ex_t
*/
extern hb_s32 hb_mm_mc_set_roi_config_ex(media_codec_context_t *context,
				const mc_video_roi_params_ex_t *params);

/**
* Get the encoding mode decision parameters.
*
* Only applied in H265 codec.
*
* @param[in]       codec context
* @param[out]      mode decision parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_mode_decision_params_t
*/
extern hb_s32 hb_mm_mc_get_mode_decision_config(
				media_codec_context_t *context,
				mc_video_mode_decision_params_t *params);

/**
* Set the mode decision parameters.
*
* Only applied in H265 codec.
*
* @param[in]       codec context
* @param[in]       mode decision parameters
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_video_mode_decision_params_t
*/
extern hb_s32 hb_mm_mc_set_mode_decision_config(
				media_codec_context_t *context,
				const mc_video_mode_decision_params_t *params);

/**
* Get the user data parameters.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      user data parameters @see mc_user_data_buffer_t
* @param[in]       timeout in ms
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_user_data_buffer_t
*/
extern hb_s32 hb_mm_mc_get_user_data(media_codec_context_t * context,
				mc_user_data_buffer_t *params, hb_s32 timeout);

/**
* Release the user data.
*
* Only applied in H264 and H265 codec.
*
* @param[in]	   codec context
* @param[out]	   user data parameters @see mc_user_data_buffer_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_user_data_buffer_t
*/
extern hb_s32 hb_mm_mc_release_user_data(media_codec_context_t * context,
				const mc_user_data_buffer_t * params);

/**
* Get explicit header configuration.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[out]      explicit header configuration
*                  The valid numbers are as follows.
*                  0 : Disable, the header will be encoded into
*                      independent frame
*                  1 : Enable, the header will be encoded into IDR frame if
*                      it exists.
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_get_explicit_header_config(
				media_codec_context_t *context, hb_s32 *status);

/**
* Set explicit header configuration.
*
* Only applied in H264 and H265 codec.
*
* @param[in]       codec context
* @param[in]       enalbe/diable explicit header, default enable
*                  The valid numbers are as follows.
*                  0 : Disable, the header will be encoded into
*                      independent frame
*                  1 : Enable, the header will be encoded into IDR frame if
*                      it exists.
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_set_explicit_header_config(
				media_codec_context_t *context, hb_s32 status);

/**
* Get the mjpeg parameters.
*
* Only applied in mjpeg codec.
*
* @param[in]       codec context
* @param[out]      mjpeg parameters @see mc_mjpeg_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_mjpeg_enc_params_t
*/
extern hb_s32 hb_mm_mc_get_mjpeg_config(media_codec_context_t * context,
				mc_mjpeg_enc_params_t *params);

/**
* Set the mjpeg parameters.
*
* Only applied in mjpeg codec.
*
* @param[in]       codec context
* @param[in]       mjpeg parameters @see mc_mjpeg_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_mjpeg_enc_params_t
*/
extern hb_s32 hb_mm_mc_set_mjpeg_config(media_codec_context_t * context,
				const mc_mjpeg_enc_params_t *params);

/**
* Get the jpeg parameters.
*
* Only applied in jpeg codec.
*
* @param[in]       codec context
* @param[out]      jpeg parameters @see mc_jpeg_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_jpeg_enc_params_t
*/
extern hb_s32 hb_mm_mc_get_jpeg_config(media_codec_context_t * context,
				mc_jpeg_enc_params_t *params);

/**
* Set the jpeg parameters.
*
* Only applied in jpeg codec.
*
* @param[in]       codec context
* @param[in]       jpeg parameters @see mc_jpeg_enc_params_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_jpeg_enc_params_t
*/
extern hb_s32 hb_mm_mc_set_jpeg_config(media_codec_context_t * context,
				const mc_jpeg_enc_params_t *params);

/**
* Get device fd. And user can use it to do select operation.
*
* @param[in]	   codec context
* @param[out]	   device fd
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_get_fd(media_codec_context_t * context,
				hb_s32 *fd);

/**
* Close device fd. User must close the fd which is aquired through hb_mm_mc_get_fd.
*
* @param[in]	   codec context
* @param[out]	   device fd
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
*/
extern hb_s32 hb_mm_mc_close_fd(media_codec_context_t * context,
				hb_s32 fd);

/**
* register audio encoder. User can use it to register external codec.
*
* @param[out]	   register handle
* @param[in]	   audio encoder
*
* @return 0 on success, negative HB_MEDIA_ERROR in case of failure
*/
extern hb_s32 hb_mm_mc_register_audio_encoder(hb_s32 *handle,
	mc_audio_encode_param_t *encoder);

/**
* unregister audio encoder. User can use it to unregister codec.
*
* @param[in]	   register handle
*
* @return 0 on success, negative HB_MEDIA_ERROR in case of failure
*/
extern hb_s32 hb_mm_mc_unregister_audio_encoder(hb_s32 handle);

/**
* register audio decoder. User can use it to register external codec.
*
* @param[out]	   register handle
* @param[in]	   audio decoder
*
* @return 0 on success, negative HB_MEDIA_ERROR in case of failure
*/
extern hb_s32 hb_mm_mc_register_audio_decoder(hb_s32 *handle,
	mc_audio_decode_param_t *decoder);

/**
* unregister audio decoder. User can use it to unregister codec.
*
* @param[in]	   register handle
*
* @return 0 on success, negative HB_MEDIA_ERROR in case of failure
*/
extern hb_s32 hb_mm_mc_unregister_audio_decoder(hb_s32 handle);

/**
* Set the user status. Warning: It's an internal API. Don't use this API.
*
* @param[in]       codec context
* @param[in]       user status @see mc_user_status_t
*
* @return >=0 on success, negative HB_MEDIA_ERROR in case of failure
* @see media_codec_context_t
* @see mc_user_status_t
*/
extern hb_s32 hb_mm_mc_set_status(media_codec_context_t *context,
				mc_user_status_t *status);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __HB_MEDIA_CODEC_H__ */
