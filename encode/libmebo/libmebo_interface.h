#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "av1encode.h"
#include "libmebo.hpp"
int CreateInitLibmebo(struct Av1InputParameters ips);
void getLoopfilterfromRc(int lfdata[4]);
void post_encode_update(unsigned int consumed_bytes);
int getQPfromRatectrl(int current_frame_type);

typedef void *(*createLibmeboRateController_t)(LibMeboCodecType CodecType,
                                               LibMeboBrcAlgorithmID algo_id);
typedef void *(*init_rate_controller_t)(LibMeboRateController *rc,
                                        LibMeboRateControllerConfig *rc_config);
typedef LibMeboStatus (*update_rate_control_t)(
    LibMeboRateController *rc, LibMeboRateControllerConfig *rc_cfg);
typedef LibMeboStatus (*post_encode_update_t)(LibMeboRateController *rc,
                                              uint64_t encoded_frame_size);
typedef LibMeboStatus (*compute_qp_t)(LibMeboRateController *rc,
                                      LibMeboRCFrameParams *rc_frame_params);
typedef LibMeboStatus (*get_qp_t)(LibMeboRateController *rc, int *qp);

typedef LibMeboStatus (*get_loop_filter_level_t)(LibMeboRateController *rc,
                                                 int *filter_level);
struct FunctionPtrstoLibmebo {
  void *handle;
  LibMeboRateController *libmebo_brc;
  createLibmeboRateController_t ptrCreateLibmeboController;
  init_rate_controller_t ptrInit_rate_controller;
  update_rate_control_t ptrUpdate_rate_control;
  post_encode_update_t ptrPost_encode_update;
  compute_qp_t ptrCompute_qp;
  get_qp_t ptrGet_qp;
  get_loop_filter_level_t ptrGet_loop_filter_level;
};

#if 1
typedef enum ErrorsLoadingFunc {
  kMainHandleLibError_interface = -1,
  kLibMeboControllerSymbolError = -2,
  kInitRateControllerSymbolError = -3,
  kUpdateRateControllerSymbolError = -4,
  kPostEncodeSymbolSymbolError = -5,
  kComputeQpSybmolError = -6,
  kGetQPSymbolError = -7,
  kGetLoopFilterSymbolError = -8,
} ErrosLibmeboSymbols_interface;
#endif

#ifdef __cplusplus
}
#endif
