
#pragma once
extern "C" {
#include "libmebo.hpp"
}

#define MaxSpatialLayers 3
#define MaxTemporalLayers 3

struct EncParamsLibmebo {
  unsigned int preset;
  unsigned int id;
  unsigned int bitrate; // in kbps
  unsigned int framerate;
  unsigned int width;
  unsigned int height;
  unsigned int framecount;          // Number of Frames to be encoded
  unsigned int num_sl;              // Number of Spatial Layers
  unsigned int num_tl;              // Number of Temporal Layers
  unsigned int dynamicRateChange; // dynamic rate change enablement flag
  int64_t bufOptimalSz;
};

class LibMeboBrc {
public:
  LibMeboBrc(LibMeboCodecType codecType, LibMeboBrcAlgorithmID algo_id);
  virtual ~LibMeboBrc() = default;

  virtual LibMeboRateController *
  init(LibMeboRateController *rc, LibMeboRateControllerConfig *rc_config) = 0;
  virtual LibMeboStatus update_config(LibMeboRateController *rc,
                                      LibMeboRateControllerConfig *rc_cfg) = 0;
  virtual LibMeboStatus post_encode_update(LibMeboRateController *rc,
                                           uint64_t encoded_frame_size) = 0;
  virtual LibMeboStatus compute_qp(LibMeboRateController *rc,
                                   LibMeboRCFrameParams *rc_frame_params) = 0;
  virtual LibMeboStatus get_qp(LibMeboRateController *rc, int *qp) = 0;
  virtual LibMeboStatus get_loop_filter_level(LibMeboRateController *rc,
                                              int *filter_level) = 0;

  uint32_t MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
                                         uint32_t max_framerate);
  int GetBitratekBps_l(int sl_id, int tl_id);
  void InitLayeredFramerate(int num_tl, int framerate, int *tsRateDecimator);
  void InitLayeredBitrateAlloc(int num_sl, int num_tl, int bitrate);

  LibMeboCodecType getCodecType() const { return codec_type; }

protected:
  LibMeboCodecType codec_type;
  LibMeboBrcAlgorithmID algo_id;
  EncParamsLibmebo encParamsLibMebo;
  int layeredFrameRate[MaxTemporalLayers];
  int layered_bitrates[MaxSpatialLayers][MaxTemporalLayers];
};
