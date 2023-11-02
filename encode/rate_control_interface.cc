#include <memory>
#include <new>
#include <string>
#include <fstream>
#include <iostream>
#include <stddef.h>
#include "rate_control/lib_header/ratectrl.h"
#include "rate_control/lib_header/ratectrl_rtc.h"
#include "rate_ctrl_interface.h"
#include "av1encode.h"

namespace aom {

class AV1VaapiVideoEncoderDelegate 
{
 public:
   

  void updateRateControlParams(AV1RateControlRtcConfig rc_config);
  void ComputeQp(AV1FrameParamsRTC frame_params);
  int Getqp(void);
  void postencodeupdate(unsigned int consumed_bytes);
  void create_rate_ctrl(AV1RateControlRtcConfig rc_config);
  void getLoopfilter(int lfdata[4]);
 private:
  int level_idx_;
  uint64_t frame_num_ = 0;
  std::unique_ptr<AV1RateControlRTC> rate_ctrl_;
  // TODO(b:274756117): In tuning this encoder, we may decide we want multiple
  // reference frames, not just the most recent.

  
};
}//namespace aom 

aom::AV1VaapiVideoEncoderDelegate *myobj;


void aom::AV1VaapiVideoEncoderDelegate::updateRateControlParams(AV1RateControlRtcConfig rc_config)
{
    rate_ctrl_->UpdateRateControl(rc_config);
}

void aom::AV1VaapiVideoEncoderDelegate::create_rate_ctrl(AV1RateControlRtcConfig rc_config){
    rate_ctrl_ = AV1RateControlRTC::Create(rc_config);
}

void aom::AV1VaapiVideoEncoderDelegate::ComputeQp(AV1FrameParamsRTC frame_params)
{
    rate_ctrl_->ComputeQP(frame_params);
}

int aom::AV1VaapiVideoEncoderDelegate::Getqp()
{
     return  (rate_ctrl_->GetQP());
}

void aom::AV1VaapiVideoEncoderDelegate::postencodeupdate(unsigned int consumed_bytes)
{
    rate_ctrl_->PostEncodeUpdate(consumed_bytes);
}

void aom::AV1VaapiVideoEncoderDelegate::getLoopfilter(int lfdata[4])
{
   aom::AV1LoopfilterLevel loop_filter_level = rate_ctrl_->GetLoopfilterLevel();
   lfdata[0] = loop_filter_level.filter_level[0];
   lfdata[1] = loop_filter_level.filter_level[1];
   lfdata[2] = loop_filter_level.filter_level_u;
   lfdata[3] = loop_filter_level.filter_level_v;
}


int getQPfromRatectrl(int current_frame_type)
{
  aom::AV1FrameParamsRTC frame_params;
  const bool is_keyframe = current_frame_type ;
  frame_params.frame_type = aom::kKeyFrame ; // default.
  if(frame_params.frame_type ==  is_keyframe )
       frame_params.frame_type = aom::kKeyFrame ;   
else 
     frame_params.frame_type =  aom::kInterFrame;
  frame_params.spatial_layer_id = 0;
  frame_params.temporal_layer_id = 0;
  myobj->ComputeQp(frame_params);
  int baseqp =  myobj->Getqp();
 // int baseqp = 45;
  return (baseqp);
}


void updateRatecontrolAfterEncode(unsigned int consumed_bytes)
{
    myobj->postencodeupdate(consumed_bytes);
}

void getLoopfilterfromRc(int lfdata[4])
{
  myobj->getLoopfilter(lfdata);
}


void create_ratectrl(struct Av1InputParameters ips)
{
    myobj = new aom::AV1VaapiVideoEncoderDelegate;
    aom::AV1RateControlRtcConfig rc_config;

    constexpr int kMinQP = 10;
    constexpr int kMaxQP = 56;
    rc_config.width = ips.width;
    rc_config.height = ips.height;
    // third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc
    rc_config.max_quantizer = kMaxQP;
    rc_config.min_quantizer = kMinQP;

    rc_config.buf_initial_sz = 600;
    rc_config.buf_optimal_sz = 500;
    rc_config.target_bandwidth =ips.target_bitrate ;
    rc_config.buf_sz = 1000;
    rc_config.undershoot_pct = 25;
    rc_config.overshoot_pct = 50;
    rc_config.max_intra_bitrate_pct = 300;
    rc_config.max_inter_bitrate_pct = 50;
    rc_config.framerate = 60;
    rc_config.layer_target_bitrate[0] = ips.target_bitrate;

    rc_config.ts_rate_decimator[0] = 1;
    rc_config.aq_mode = 0;
    rc_config.ss_number_layers = 1;
    rc_config.ts_number_layers = 1;
    rc_config.max_quantizers[0] = kMaxQP;
    rc_config.min_quantizers[0] = kMinQP;
    rc_config.scaling_factor_num[0] = 1;
    rc_config.scaling_factor_den[0] = 1;


    myobj->create_rate_ctrl(rc_config);
    myobj->updateRateControlParams(rc_config);
}



