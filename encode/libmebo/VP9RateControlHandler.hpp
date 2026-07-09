#pragma once

#include "libmebo.hpp"

#include "LibMeboControlHandler.hpp"

class LibmeboBrc_VP9 : public LibMeboBrc {
public:
  LibmeboBrc_VP9(LibMeboBrcAlgorithmID algo_id);
  virtual ~LibmeboBrc_VP9() override = default;
  LibMeboRateController *init(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_config) override;
  LibMeboStatus update_config(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rc_cfg) override;
  LibMeboStatus post_encode_update(LibMeboRateController *rc,
                                   uint64_t encoded_frame_size) override;
  LibMeboStatus compute_qp(LibMeboRateController *rc,
                           LibMeboRCFrameParams *rc_frame_params) override;
  LibMeboStatus get_qp(LibMeboRateController *rc, int *qp) override;
  LibMeboStatus get_loop_filter_level(LibMeboRateController *rc,
                                      int *filter_level) override;

private:
  typedef void *BrcCodecEnginePtr;
  BrcCodecEnginePtr brc_codec_handler;
};
