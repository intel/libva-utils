
#ifndef RATE_CTRL_INTERFACE_H_
#define RATE_CTRL_INTERFACE_H_
#include "av1encode.h"
#ifdef __cplusplus
extern "C" {
#endif


 // namespace aom
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned long int uint64_t;



void create_ratectrl(struct Av1InputParameters ips);

int getQPfromRatectrl(int current_frame_type);

void updateRatecontrolAfterEncode(unsigned int consumed_bytes);

void getLoopfilterfromRc(int lfdata[4]);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif

