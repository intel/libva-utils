/*
 *  Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *  Copyright (c) 2020 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *  Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * LibMebo API Specification
 *
 * Acknowledgements:
 *   Some concepts are borrowed from libvpx:
 *   Jerome Jiang <jianj@google.com> and Marco Paniconi <marpan@google.com>
 *   contributed to various aspects of the VP9 BRC solution.
 */

/**
 * \file libmebo.h
 * \brief APIs exposed in LibMebo
 *
 * This file contains the \ref api_core "LibMebo API".
 */

#ifndef __LIBMEBO_H__
#define __LIBMEBO_H__
#include <stdint.h>

/**
 * \mainpage Library for Media Encode Bitrate-control-algorithm Orchestration
 * (LibMebo)
 *
 * \section intro Introduction
 * LibMebo is an open-source library to orchestrate the bitrate
 * control algorithms for a video encoder pipeline.This library
 * has a collection of algorithms derived from various video encoder
 * libraries.
 *
 * The intention for Libmebo is to allow the video encoder implementations
 * to enable customized bitrate control outside of the core pipeline.
 * The encoder itself could be running on software or hardware.
 *
 * \section api_howto How to use the libmebo apis with an encoder
 *
 * \code
 * #include <libmebo/libmebo.h>
 *
 * LibMeboRateController *rc;
 * LibMeboRateControllerConfig rc_config;
 * LibMeboRCFrameParams rc_frame_param;
 * LibMeboStatus status;
 *
 * //Initialize the rc_config with required parameters
 * __InitializeConfig(rc_cfg);
 *
 * //Create an instance of libmebo
 * rc = libmebo_rate_controller_new (LIBMEBO_CODEC_VP9,
 * Libmebo_brc_ALGORITHM_DEFAULT);
 *
 * //Initialize the libmebo instance with the rc_config
 * status = libmebo_rate_controller_init (rc, &rc_config);
 * assert (status == LIBMEBO_STATUS_SUCCESS);
 *
 * //LibMebo in action
 * while (frame_to_encode) {
 *   if (rc_config_changed) {
 *     status = libmebo_rate_controller_update_config (rc, &rc_config);
 *     assert (status == LIBMEBO_STATUS_SUCCESS);
 *   }
 *
 *   //Initialize the per-frame parameters (frame_type & layer ids)
 *   __InitializeRCFrameParams (rc_frame_param);
 *
 *   //Compute the QP
 *   status = libmebo_rate_controller_compute_qp (rc, rc_frame_param);
 *   assert (status == LIBMEBO_STATUS_SUCCESS);
 *
 *   //Retrieve the QP for the next frame to be encoded
 *   status = libmebo_rate_controller_get_qp (rc, &qp);
 *   assert (status == LIBMEBO_STATUS_SUCCESS);
 *
 *   // Optional:libmebo can also recommend the loop-filter strength
 *   status = libmebo_rate_controller_get_loop_filter_level (rc, &lf);
 *
 *   // Ensure the status == LIBMEBO_STATUS_SUCCESS before using
 *   // the loop filter level since some algos are not supporting this API
 *
 *   //Encoder implementation that running in CQP mode
 *   __EncodeBitStream(qp, [lf])
 *
 *   //Update libmebo instance with encoded frame size
 *   status = libmebo_rate_controller_post_encode_update (rc, EncFrameSize);
 *   assert (status == LIBMEBO_STATUS_SUCCESS);
 * }
 * \endcode
 */

// #include "../src/Handlers/LibMeboControlHandler.hpp"

#ifdef __cplusplus
extern "C" {
#endif

//#define LIBMEBO_ENABLE_AV1 1
#define LIBMEBO_ENABLE_VP9 1
#define LIBMEBO_ENABLE_VP8 1

typedef void *BrcCodecEnginePtr;

/**
 * Codec Types
 */
typedef enum {
  LIBMEBO_CODEC_VP8,
  LIBMEBO_CODEC_VP9,
  LIBMEBO_CODEC_AV1,
  LIBMEBO_CODEC_UNKNOWN,
} LibMeboCodecType;

/**
 * Backend algorithm IDs
 */
typedef enum {
  LIBMEBO_BRC_ALGORITHM_DEFAULT,
  LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP8,
  LIBMEBO_BRC_ALGORITHM_DERIVED_LIBVPX_VP9,
  LIBMEBO_BRC_ALGORITHM_DERIVED_AOM_AV1,
  LIBMEBO_BRC_ALGORITHM_UNKNOWN,
} LibMeboBrcAlgorithmID;

/**
 * Return status type of libmebo APIs
 */
typedef enum {
  LIBMEBO_STATUS_SUCCESS,
  LIBMEBO_STATUS_WARNING,
  LIBMEBO_STATUS_ERROR,
  LIBMEBO_STATUS_FAILED,
  LIBMEBO_STATUS_INVALID_PARAM,
  LIBMEBO_STATUS_UNSUPPORTED_CODEC,
  LIBMEBO_STATUS_UNIMPLEMENTED,
  LIBMEBO_STATUS_UNSUPPORTED_RC_MODE,
  LIBMEBO_STATUS_UNKNOWN,
} LibMeboStatus;

/**
 * Rate Control Modes
 */
typedef enum { LIBMEBO_RC_CBR, LIBMEBO_RC_VBR } LibMeboRateControlMode;

/**
 * Frame prediction types
 */
typedef enum {
  LIBMEBO_KEY_FRAME = 0,
  LIBMEBO_INTER_FRAME = 1,
  LIBMEBO_FRAME_TYPES,
} LibMeboFrameType;

/**
 * \biref Frame parameters
 *
 * This structure conveys frame level parameters and should be sent
 * once per frame
 */
typedef struct _LibMeboRCFrameParams {
  LibMeboFrameType frame_type;
  int spatial_layer_id;
  int temporal_layer_id;
} LibMeboRCFrameParams;

/* Temporal Scalability: Maximum number of coding layers.
 * Not all codecs are supporting the LIBMEBO_TS_MAX_LAYERS. The
 * libmebo_rate_controller_init() will perform the codec specific
 * layer number validation.
 */
#define LIBMEBO_TS_MAX_LAYERS 8

/* Spatial Scalability: Maximum number of Spatial coding layers.
 * Not all codecs are supporting the LIBMEBO_SS_MAX_LAYERS. The
 * libmebo_rate_controller_init() will perform the codec specific
 * layer number validation.
 */
#define LIBMEBO_SS_MAX_LAYERS 4

/**
 * Temporal + Spatial Scalability: Maximum number of coding layers
 * Not all codecs are supporting the LIBMEBO_MAX_LAYERS. The
 * libmebo_rate_controller_init() will perform the codec specific
 * layer number validation.
 */
#define LIBMEBO_MAX_LAYERS 32

/**
 * \biref LibMebo Rate Controller configuration structure
 *
 * This structure conveys the encoding parameters required
 * for libmebo to configure the BRC instance.
 */
typedef struct _LibMeboRateControllerConfig {
  /** \brief pixel width of the bitstream to be encoded */
  int width;

  /** \brief pixel height of the bitstream to be encoded */
  int height;

  /**
   * \brief Maximum (Worst Quality) Quantizer: Range: 0 - 63
   *
   * The quantizer is the most direct control over the quality of the
   * encoded image. The actual range of the valid values for the quantizer
   * is codec specific. Consult the documentation for the codec to determine
   * the values to use(eg: VP8: 0-127, VP9: 0-255).
   *
   * LibMebo scales the max_quantizer between the codec specific limits.
   */
  int max_quantizer;

  /*
   * \brief Minimum (Best Quality) Quantizer: Range: 0 - 63
   *
   * The quantizer is the most direct control over the quality of the
   * encoded image. The actual ange of valid values for the quantizer
   * is codec specific. Consult the documentation for the codec to determine
   * the values to use (eg: VP8: 0-127, VP9: 0-255).
   *
   * LibMebo scales the min_quantizer between the codec specific limits.
   */
  int min_quantizer;

  /** \brief target bandwidth (in kilo bits per second) of the bitstream */
  int64_t target_bandwidth;

  /**
   * \brief Decoder(HRD) Buffer Initial Size
   *
   * This value indicates the amount of data that will be buffered by the
   * decoding application prior to beginning playback for the encoded stream.
   * This value is expressed in units of time (milliseconds).
   */
  int64_t buf_initial_sz;

  /**
   * \brief Decoder(HRD) Buffer Optimal Size
   *
   * This value indicates the amount of data that the encoder should try
   * to maintain in the decoder's buffer. This value is expressed in units
   * of time (milliseconds).
   */
  int64_t buf_optimal_sz;

  /*
   * \brief Decoder(HRD) Buffer Size
   *
   * This value indicates the amount of data that may be buffered by the
   * decoding application. Note that this value is expressed in units of
   * time (milliseconds). For example, a value of 5000 indicates that the
   * client will buffer (at least) 5000ms worth of encoded data.
   */
  int64_t buf_sz;

  /**
   * \brief Rate control adaptation undershoot control
   *
   * Expressed as a percentage of the target bitrate, a threshold
   * undershoot level (current rate vs target) beyond which more aggressive
   * corrective measures are taken.
   *
   * This factor controls the maximum amount of bits that can
   * be subtracted from the target bitrate in order to compensate
   * for prior overshoot.
   *
   * Valid values in the range: 0-100
   */
  int undershoot_pct;

  /**
   * \brief Rate control adaptation overshoot control
   *
   * Expressed as a percentage of the target bitrate, a threshold
   * overshoot level (current rate vs target) beyond which more aggressive
   * corrective measures are taken.
   *
   * This factor controls the maximum amount of bits that can
   * be added to the target bitrate in order to compensate for
   * prior undershoot.
   *
   * Valid values in the range: 0-100
   */
  int overshoot_pct;

  /**
   * \brief Codec control attribute to set max data rate for Intra frames.
   *
   * This value controls additional clamping on the maximum size of a
   * keyframe. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * unlimited, or no additional clamping beyond the codec's built-in
   * algorithm.
   *
   * For example, to allocate no more than 4.5 frames worth of bitrate
   * to a keyframe, set this to 450.
   *
   * It is not guaranteed that all brc algorithms will support this
   * feature. The libmebo_rate_controller_init() is responsible for
   * the codec specific parameter validation.
   *
   */
  int max_intra_bitrate_pct;

  /*\brief Codec control attribute to set max data rate for Inter frames.
   *
   * This value controls additional clamping on the maximum size of an
   * inter frame. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * unlimited, or no additional clamping beyond the codec's built-in
   * algorithm.
   *
   * For example, to allow no more than 4.5 frames worth of bitrate
   * to an inter frame, set this to 450.
   *
   * It is not guaranteed that all brc algorithms will support this
   * feature. The libmebo_rate_controller_init() is responsible for
   * the codec specific parameter validation.
   */
  int max_inter_bitrate_pct;

  /** \brief framerate of the stream */
  double framerate;

  /**
   * \brief Number of spatial coding layers.
   *
   * This value specifies the number of spatial coding layers to be used.
   */
  int ss_number_layers;

  /*
   * \brief Number of temporal coding layers.
   *
   * This value specifies the number of temporal layers to be used.
   */
  int ts_number_layers;

  /**
   * \brief Maximum (Worst Quality) Quantizer for each layer
   */
  int max_quantizers[LIBMEBO_MAX_LAYERS];

  /*
   * \brief Minimum (Best Quality) Quantizer
   */
  int min_quantizers[LIBMEBO_MAX_LAYERS];

  /* \brief Scaling factor numerator for each spatial layer */
  int scaling_factor_num[LIBMEBO_SS_MAX_LAYERS];

  /* \brief Scaling factor denominator for each spatial layer */
  int scaling_factor_den[LIBMEBO_SS_MAX_LAYERS];

  /*
   * \brief Target bitrate for each spatial/temporal layer.
   *
   * These values specify the target coding bitrate to be used for each
   * spatial/temporal layer. (in kbps)
   *
   */
  int layer_target_bitrate[LIBMEBO_MAX_LAYERS];

  /*
   * \brief Frame rate decimation factor for each temporal layer.
   *
   * These values specify the frame rate decimation factors to apply
   * to each temporal layer.
   */
  int ts_rate_decimator[LIBMEBO_TS_MAX_LAYERS];

  /*
   *  \biref Rate control mode
   */
  LibMeboRateControlMode rc_mode;

  /* Reserved bytes for future use, must be zero */
  uint32_t _libmebo_rc_config_reserved[32];
} LibMeboRateControllerConfig;

typedef struct _LibMeboRateController {
  void *priv;

  /* \brief Indicates the codec in use */
  LibMeboCodecType codec_type;

  /* \brief currently configured rate control parameters */
  LibMeboRateControllerConfig rc_config;

  /* Reserved bytes for future use, must be zero */
  uint32_t _libmebo_reserved[32];
} LibMeboRateController;

LibMeboRateController *
libmebo_create_rate_controller(LibMeboCodecType CodecType,
                               LibMeboBrcAlgorithmID algo_id);
void libmebo_release_rate_controller(LibMeboRateController *rc);

LibMeboRateController *
libmebo_init_rate_controller(LibMeboRateController *rc,
                             LibMeboRateControllerConfig *rc_config);

LibMeboStatus
libmebo_update_rate_controller_config(LibMeboRateController *rc,
                                      LibMeboRateControllerConfig *rc_cfg);
LibMeboStatus libmebo_post_encode_update(LibMeboRateController *rc,
                                         uint64_t encoded_frame_size);
LibMeboStatus libmebo_compute_qp(LibMeboRateController *rc,
                                 LibMeboRCFrameParams *rc_frame_params);

LibMeboStatus libmebo_get_qp(LibMeboRateController *rc, int *qp);

LibMeboStatus libmebo_get_loop_filter_level(LibMeboRateController *rc,
                                            int *filter_level);

void *create_brc_factory(unsigned int id);
void destroy_brc_factory(void *brc);
#ifdef __cplusplus
}
#endif

#endif
