/*
 * Copyright (c) 2018 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
* \file avcstreamoutdemo.h
*
* This file contains the decode streamout layout.
*/

#ifndef _AVC_STREAMOUT_DEMO_H_
#define _AVC_STREAMOUT_DEMO_H_

typedef signed dw;

/*
 * avc streamout layout
*/
typedef struct {
    // dw 0
    union {
        struct {
            dw   InterMbMode             : 2;    // Valid only if IntraMbFlag is inter.
            dw   MbSkipFlag              : 1;    // Cuurently always set to 0
            dw                           : 1;    // MBZ
            dw   IntraMbMode             : 2;    // Valid for Inter MB, Used in conjunction with MbType
            dw                           : 1;    // MBZ
            dw   MbPolarity              : 1;    // FieldMB polarity
            dw   MbType5Bits             : 5;    // Matches best MB mode. In H.264 spec: Table 7-11 for Intra; Table 7-14 for Inter.
            dw   IntraMbFlag             : 1;    // Set if MB is intra, unset if MB is inter
            dw   MbFieldFlag             : 1;    // Set if field MB, unset if frame MB
            dw   Transform8x8Flag        : 1;    // Set if current MB uses 8x8 transforms
            dw                           : 1;    // MBZ
            dw   CodedPatternDC          : 3;    // AVC Only. Indicates whether DC coeffs are sent. Y is most significant bit.
            dw   EdgeFilterFlag          : 3;    // AVC.
            dw                           : 1;    // MBZ
            dw   PackedMvNum             : 8;    // Debug only. Specifies number of MVs in packed motion vector form
        };
        struct {
            dw   Value;
        };
    } DW0;

    // dw 1
    union {
        struct {
            dw   MbXCnt                  : 16;   // Horizontal Origin of MB in dest piture in units of MBs
            dw   MbYCnt                  : 16;   // Vertical Origin of MB in dest piture in units of MBs
        };
        struct {
            dw   Value;
        };
    } DW1;

    // dw 2
    union {
        struct {
            dw   CbpAcY                  : 16;   // Coded block pattern for Y.
            dw   CbpAcU                  : 4;    // Coded block pattern for U
            dw   CbpAcV                  : 4;    // Coded block pattern for V
            dw                           : 6;    // Reserved
            dw   LastMBOfSliceFlag       : 1;    // Indicates current MB is last in slice. Data not right
            dw   ConcealMBFlag           : 1;    // Specifies in MB is a conceal MB.
        };
        struct {
            dw   Value;
        };
    } DW2;

    // dw 3
    union {
        struct {
            dw   QpPrimeY                : 7;    // AVC: Per-MB QP for luma.
            dw   QScaleType              : 1;    // MPEG2 only
            dw   MbClock16               : 8;    // MB compute clocks in 16-clock units
            dw   NzCoefCountMB           : 9;    // All coded coefficients in MB
            dw                           : 3;    // Reserved
            dw   Skip8x8Pattern          : 4;    // AVC Only. Indicates which of the 8x8 sub-blocks uses predicted MVs
        };
        struct {
            dw   Value;
        };
    } DW3;


    // dw 4
    union {
        struct {
            dw   LumaIntraPredModes0     : 16;   // AVC only
            dw   LumaIntraPredModes1     : 16;   // AVC only
        } Intra;
        struct {
            dw   SubMbShape              : 8;    // Indicates sub-block partitioning for each 8x8 sub-block
            dw   SubMbPredModes          : 8;    // Indicates prediction mode for each 8x8 sub-block
            dw                           : 16;   // Reserved
        } Inter;
        struct {
            dw   Value;
        };
    } DW4;

    // dw 5
    union {
        struct {
            dw   LumaIntraPredModes2     : 16;   // AVC only
            dw   LumaIntraPredModes3     : 16;   // AVC only
        } Intra;
        struct {
            dw   FrameStorIDL0_0     : 8;
            dw   FrameStorIDL0_1     : 8;
            dw   FrameStorIDL0_2     : 8;
            dw   FrameStorIDL0_3     : 8;
        } Inter;
        struct {
            dw   Value;
        };
    } DW5;

    // dw 6
    union {
        struct {
            dw   MbIntraStruct           : 8;    // Indicates which neighbours can be used for intra-prediction
            dw                           : 24;   // Reserved
        } Intra;
        struct {
            dw   FrameStorIDL1_0     : 8;
            dw   FrameStorIDL1_1     : 8;
            dw   FrameStorIDL1_2     : 8;
            dw   FrameStorIDL1_3     : 8;
        } Inter;
        struct {
            dw   Value;
        };
    } DW6;

    // dw 7
    union {
        struct {
            dw   SubBlockCodeTypeY0      : 2;    // VC-1. Specifies if 8x8, 8x4, 4x8, 4x4
            dw   SubBlockCodeTypeY1      : 2;    // VC-1. Specifies if 8x8, 8x4, 4x8, 4x4
            dw   SubBlockCodeTypeY2      : 2;    // VC-1. Specifies if 8x8, 8x4, 4x8, 4x4
            dw   SubBlockCodeTypeY3      : 2;    // VC-1. Specifies if 8x8, 8x4, 4x8, 4x4
            dw   SubBlockCodeTypeU       : 2;    // VC-1. Specifies if 8x8, 8x4, 4x8, 4x4
            dw   SubBlockCodeTypeV       : 2;    // VC-1. Specifies if 8x8, 8x4, 4x8, 4x4
            dw                           : 8;
            dw   MvFieldSelect           : 4;    // Field polatity for VC-1 and MPEG2
            dw                           : 8;
        };
        struct {
            dw   Value;
        };
    } DW7;

    // dw 8-15 for inter MBs only
    union {
        struct {
            dw   MvFwd_x                : 16;   // x-component of fwd MV for 8x8 or 4x4 sub-block
            dw   MvFwd_y                : 16;   // y-component of fwd MV for 8x8 or 4x4 sub-block
            dw   MvBwd_x                : 16;   // x-component of bwd MV for 8x8 or 4x4 sub-block
            dw   MvBwd_y                : 16;   // y-component of bwd MV for 8x8 or 4x4 sub-block
        };
        struct {
            dw   Value[2];
        };
    } QW8[4];

} VADecStreamOutData;
#endif /*_AVC_STREAMOUT_DEMO_H_*/
