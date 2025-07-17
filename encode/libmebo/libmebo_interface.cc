#include "libmebo_interface.h"
#include "RateControlFactory.hpp"
#include "av1encode.h"
#include <dlfcn.h>
#include <iostream>

struct FunctionPtrstoLibmebo FnPtrsToLibmebo;
LibMeboRateController *libmebo_brc;

void InitRcCofnig(struct Av1InputParameters ips,
                  LibMeboRateControllerConfig *rc_config) {
  constexpr int kMinQP = 10;
  constexpr int kMaxQP = 56;
  rc_config->width = ips.width;
  rc_config->height = ips.height;
  rc_config->max_quantizer = kMaxQP;
  rc_config->min_quantizer = kMinQP;

  rc_config->buf_initial_sz = 600;
  rc_config->buf_optimal_sz = 500;
  rc_config->target_bandwidth = ips.target_bitrate;
  rc_config->buf_sz = 1000;
  rc_config->undershoot_pct = 25;
  rc_config->overshoot_pct = 50;
  rc_config->max_intra_bitrate_pct = 300;
  rc_config->max_inter_bitrate_pct = 50;
  rc_config->framerate = 60;
  rc_config->layer_target_bitrate[0] = ips.target_bitrate;

  rc_config->ts_rate_decimator[0] = 1;
  rc_config->ss_number_layers = 1;
  rc_config->ts_number_layers = 1;
  rc_config->max_quantizers[0] = kMaxQP;
  rc_config->min_quantizers[0] = kMinQP;
  rc_config->scaling_factor_num[0] = 1;
  rc_config->scaling_factor_den[0] = 1;
}

int InitFuncPtrs(struct Av1InputParameters ips,
                 struct FunctionPtrstoLibmebo *pFnPtrsToLibmebo) {
  
  char path[] = "/usr/local/lib/libmebo.so";
  pFnPtrsToLibmebo->handle = dlopen(path, RTLD_LAZY);

  if (!pFnPtrsToLibmebo->handle) {
    std::cerr << "Cannot open the library given path is :"<<path<<"\n";
    return kMainHandleLibError;
  }

  pFnPtrsToLibmebo->ptrCreateLibmeboController =
      (createLibmeboRateController_t)dlsym(pFnPtrsToLibmebo->handle,
                                           "libmebo_create_rate_controller");
  if (!pFnPtrsToLibmebo->ptrCreateLibmeboController) {
    std::cerr << "Cannot load symbol 'libmebo_create_rate_controller' from "
                 "Libmebo.so\n";
    return kLibMeboControllerSymbolError;
  }

  pFnPtrsToLibmebo->ptrInit_rate_controller = (init_rate_controller_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_init_rate_controller");
  if (!pFnPtrsToLibmebo->ptrInit_rate_controller) {
    std::cerr << "Cannot load symbol 'libmebo_init_rate_controller' from "
                 "Libmebo.so\n";
    return kInitRateControllerSymbolError;
  }

  pFnPtrsToLibmebo->ptrUpdate_rate_control = (update_rate_control_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_update_rate_controller_config");
  if (!pFnPtrsToLibmebo->ptrInit_rate_controller) {
    std::cerr << "Cannot load symbol 'libmebo_update_rate_controller_config' "
                 "from Libmebo.so\n";
    return kUpdateRateControllerSymbolError;
  }

  pFnPtrsToLibmebo->ptrPost_encode_update = (post_encode_update_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_post_encode_update");
  if (!pFnPtrsToLibmebo->ptrPost_encode_update) {
    std::cerr
        << "Cannot load symbol 'libmebo_post_encode_update' from Libmebo.so\n";
    return kPostEncodeSymbolSymbolError;
  }

  pFnPtrsToLibmebo->ptrCompute_qp =
      (compute_qp_t)dlsym(pFnPtrsToLibmebo->handle, "libmebo_compute_qp");
  if (!pFnPtrsToLibmebo->ptrPost_encode_update) {
    std::cerr << "Cannot load symbol 'libmebo_compute_qp' from Libmebo.so\n";
    return kComputeQpSybmolError;
  }

  pFnPtrsToLibmebo->ptrGet_qp =
      (get_qp_t)dlsym(pFnPtrsToLibmebo->handle, "libmebo_get_qp");
  if (!pFnPtrsToLibmebo->ptrGet_qp) {
    std::cerr << "Cannot load symbol 'libmebo_get_qp' from Libmebo.so\n";
    return kGetQPSymbolError;
  }

  pFnPtrsToLibmebo->ptrGet_loop_filter_level = (get_loop_filter_level_t)dlsym(
      pFnPtrsToLibmebo->handle, "libmebo_get_loop_filter_level");
  if (!pFnPtrsToLibmebo->ptrGet_loop_filter_level) {
    std::cerr << "Cannot load symbol 'libmebo_get_loop_filter_level' from "
                 "Libmebo.so\n";
    return kGetLoopFilterSymbolError;
  }
  return kNoError;
}

int CreateInitLibmebo(struct Av1InputParameters ips) {

  LibMeboRateControllerConfig rc_config;
  LibMeboCodecType CodecType = LIBMEBO_CODEC_AV1;
  unsigned int algo_id = 2;

  int result = InitFuncPtrs(ips, &FnPtrsToLibmebo);

  if (result != kNoError) {
    std::cerr << " Cannot load lib or symbol, error code is " << result << "-"
              << dlerror() << std::endl;
  }

  InitRcCofnig(ips, &rc_config); // creating rc config.

  libmebo_brc = reinterpret_cast<LibMeboRateController *>(
      FnPtrsToLibmebo.ptrCreateLibmeboController(
          CodecType, static_cast<LibMeboBrcAlgorithmID>(algo_id)));

  if (nullptr == libmebo_brc) {
    std::cerr << "Libmebo_brc factory object creation failed \n";
  }

  std::cout
      << "till this time, created the constructor to libmebo_brc and av1\n";

  FnPtrsToLibmebo.ptrInit_rate_controller(libmebo_brc, &rc_config);
  return 0;
}

int myfunc_test(struct Av1InputParameters ips) {

  LibMeboRateControllerConfig rc_config;
  LibMeboCodecType CodecType = LIBMEBO_CODEC_AV1;
  unsigned int algo_id = 2;
  uint64_t frame_size_bytes = 600;
  LibMeboRCFrameParams rc_frame_params;
  int qp;
  int filter_level[4];

  rc_frame_params.frame_type = LIBMEBO_KEY_FRAME;

  int result = InitFuncPtrs(ips, &FnPtrsToLibmebo);

  if (result != kNoError) {
    std::cerr << " Cannot load lib or symbol, error code is " << result << "-"
              << dlerror() << std::endl;
  }

  InitRcCofnig(ips, &rc_config); // creating rc config.

  libmebo_brc = reinterpret_cast<LibMeboRateController *>(
      FnPtrsToLibmebo.ptrCreateLibmeboController(
          CodecType, static_cast<LibMeboBrcAlgorithmID>(algo_id)));

  if (nullptr == libmebo_brc) {
    std::cerr << "Libmebo_brc factory object creation failed \n";
  }

  std::cout
      << "till this time, created the construcotr for libmebo_brc and forav1\n";

  FnPtrsToLibmebo.ptrInit_rate_controller(libmebo_brc, &rc_config);
  FnPtrsToLibmebo.ptrUpdate_rate_control(libmebo_brc, &rc_config);
  FnPtrsToLibmebo.ptrPost_encode_update(libmebo_brc, frame_size_bytes);
  FnPtrsToLibmebo.ptrCompute_qp(libmebo_brc, &rc_frame_params);
  FnPtrsToLibmebo.ptrGet_qp(libmebo_brc, &qp);
  FnPtrsToLibmebo.ptrGet_loop_filter_level(libmebo_brc, filter_level);
  return 0;
}

int getQPfromRatectrl(int current_frame_type) {
  LibMeboRCFrameParams rc_frame_params;
  int qp;

  const bool is_keyframe = current_frame_type;

  rc_frame_params.frame_type = LIBMEBO_KEY_FRAME; // default.
  if (rc_frame_params.frame_type == is_keyframe) {
    rc_frame_params.frame_type = LIBMEBO_KEY_FRAME;
  } else {
    rc_frame_params.frame_type = LIBMEBO_INTER_FRAME;
  }
  rc_frame_params.spatial_layer_id = 0;
  rc_frame_params.temporal_layer_id = 0;
  FnPtrsToLibmebo.ptrCompute_qp(libmebo_brc, &rc_frame_params);
  FnPtrsToLibmebo.ptrGet_qp(libmebo_brc, &qp);
  return (qp);
}

void getLoopfilterfromRc(int lfdata[4]) {
  FnPtrsToLibmebo.ptrGet_loop_filter_level(libmebo_brc, lfdata);
}

void post_encode_update(unsigned int consumed_bytes) {
  FnPtrsToLibmebo.ptrPost_encode_update(libmebo_brc, consumed_bytes);
}