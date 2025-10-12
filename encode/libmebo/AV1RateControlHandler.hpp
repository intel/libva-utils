#pragma once
#include <memory>

#include "libmebo.hpp"
#include "ratectrl_rtc.h"
#include "LibMeboControlHandler.hpp"

typedef enum ErrorsLoadingSymbols {
  kNoError = 0,
  kMainHandleLibError = -1,
  kAv1CreateSymbLoadError = -2,
  kAV1RateCtrlInitConfigSymbLoadError = -3,
  kUpdateRateControlSymbLoadError = -4,
  kCompueQPSymbLoadError = -5,
  kPostEncodeSymbLoadError = -6,
  kGetQpSymbLoadError = -7,
  kGetLoopFilterSymbError = -8,
} ErrosLibmeboSymbols;

class LibmeboBrc_AV1 : public LibMeboBrc {
public:
  LibmeboBrc_AV1(LibMeboBrcAlgorithmID algo_id);
  virtual ~LibmeboBrc_AV1() override;
  LibMeboRateController *init(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rcConfig) override;
  LibMeboStatus update_config(LibMeboRateController *rc,
                              LibMeboRateControllerConfig *rcConfig) override;
  LibMeboStatus post_encode_update(LibMeboRateController *rc,
                                   uint64_t encodedFrameSize) override;
  LibMeboStatus compute_qp(LibMeboRateController *rc,
                           LibMeboRCFrameParams *rcFrameParams) override;
  LibMeboStatus get_qp(LibMeboRateController *rc, int *qp) override;
  LibMeboStatus get_loop_filter_level(LibMeboRateController *rc,
                                      int *filterLevel) override;
  int InitSymbolsFromLibrary();

private:
  void *handle;
  typedef void (*InitRateControlConfigFunc_AV1_t)(
      struct AomAV1RateControlRtcConfig *rcConfig);

  typedef AomAV1RateControlRTC *(*CreateRateControl_AV1_t)(
      const struct AomAV1RateControlRtcConfig *rcConfig);

  typedef bool (*UpdateRateControl_AV1_t)(
      void *controller, struct AomAV1RateControlRtcConfig *rcConfig);

  typedef AomFrameDropDecision (*ComputeQP_AV1_t)(
      void *controller, const AomAV1FrameParamsRTC &frameParams);

  typedef int (*GetQP_AV1_t)(void *controller);

  typedef AomAV1LoopfilterLevel (*GetLoopfilterLevel_AV1_t)(void *controller);

  typedef void (*PostEncodeUpdate_AV1_t)(void *controller,
                                         uint64_t encodedFrameSize);

  typedef bool (*GetSegmentationData_AV1_t)(
      void *controller, AomAV1SegmentationData *segmentation_data);

  typedef AomAV1CdefInfo (*GetCdefInfo_AV1_t)(void *controller);

  InitRateControlConfigFunc_AV1_t ptrInitConfigFunc;
  CreateRateControl_AV1_t ptrCreateAV1Controller;
  UpdateRateControl_AV1_t ptrUpdateRateControl_AV1;
  ComputeQP_AV1_t ptrComputeQP_AV1;
  GetQP_AV1_t ptrGetQP_AV1;
  GetLoopfilterLevel_AV1_t ptrGetLoopfilterLevel_AV1;
  PostEncodeUpdate_AV1_t ptrPostEncodeUpdate_AV1;
  GetSegmentationData_AV1_t ptrGetSegmentationData_AV1;
  GetCdefInfo_AV1_t ptrGetCdefInfo_AV1;
  AomAV1RateControlRtcConfig *rcConfig;
};