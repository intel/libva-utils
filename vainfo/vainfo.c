/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <getopt.h>
#include <va/va_str.h>

#include "va_display.h"

#ifdef ANDROID

/* Macros generated from configure */
#define LIBVA_VERSION_S "2.0.0"

#endif

#define CHECK_VASTATUS(va_status,func, ret)                             \
if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr,"%s failed with error code %d (%s),exit\n",func, va_status, vaErrorStr(va_status)); \
    ret_val = ret;                                                      \
    goto error;                                                         \
}
static int show_all_opt = 0;

static void
usage_exit(const char *program)
{
    fprintf(stdout, "Show information from VA-API driver\n");
    fprintf(stdout, "Usage: %s --help\n", program);
    fprintf(stdout, "\t--help print this message\n\n");
    fprintf(stdout, "Usage: %s [options]\n", program);
    fprintf(stdout, "  -a, --all                              Show all supported attributes\n");
    va_print_display_options(stdout);

    exit(0);
}

static void
parse_args(const char *name, int argc, char **argv)
{
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"help", no_argument, 0,     'h'},
        {"all",  no_argument, 0,     'a'},
        { NULL,  0,           NULL,   0 }
    };

    va_init_display_args(&argc, argv);

    while ((c = getopt_long(argc, argv,
                            "a",
                            long_options,
                            &option_index)) != -1) {

        switch (c) {
        case 'a':
            show_all_opt = 1;
            break;
        case 'h':
        default:
            usage_exit(name);
            break;
        }
    }
}

static int show_config_attributes(VADisplay va_dpy, VAProfile profile, VAEntrypoint entrypoint)
{
    struct str_format {
        int format;
        char *name;
    };

    VAStatus va_status;
    int i, n;

    VAConfigAttrib attrib_list[VAConfigAttribTypeMax];
    int max_num_attributes = VAConfigAttribTypeMax;

    for (i = 0; i < max_num_attributes; i++) {
        attrib_list[i].type = i;
    }

    va_status = vaGetConfigAttributes(va_dpy,
                                      profile, entrypoint,
                                      attrib_list, max_num_attributes);
    if (VA_STATUS_ERROR_UNSUPPORTED_PROFILE == va_status ||
        VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT == va_status)
        return 0;

    printf("%s/%s\n", vaProfileStr(profile), vaEntrypointStr(entrypoint));

    if (attrib_list[VAConfigAttribRTFormat].value != VA_ATTRIB_NOT_SUPPORTED) {
        static struct str_format list[] = {
            {VA_RT_FORMAT_YUV420,        "VA_RT_FORMAT_YUV420"},
            {VA_RT_FORMAT_YUV422,        "VA_RT_FORMAT_YUV422"},
            {VA_RT_FORMAT_YUV444,        "VA_RT_FORMAT_YUV444"},
            {VA_RT_FORMAT_YUV411,        "VA_RT_FORMAT_YUV411"},
            {VA_RT_FORMAT_YUV400,        "VA_RT_FORMAT_YUV400"},
            {VA_RT_FORMAT_YUV420_10,     "VA_RT_FORMAT_YUV420_10"},
            {VA_RT_FORMAT_YUV422_10,     "VA_RT_FORMAT_YUV422_10"},
            {VA_RT_FORMAT_YUV444_10,     "VA_RT_FORMAT_YUV444_10"},
            {VA_RT_FORMAT_YUV420_12,     "VA_RT_FORMAT_YUV420_12"},
            {VA_RT_FORMAT_YUV422_12,     "VA_RT_FORMAT_YUV422_12"},
            {VA_RT_FORMAT_YUV444_12,     "VA_RT_FORMAT_YUV444_12"},
            {VA_RT_FORMAT_RGB16,         "VA_RT_FORMAT_RGB16"},
            {VA_RT_FORMAT_RGB32,         "VA_RT_FORMAT_RGB32"},
            {VA_RT_FORMAT_RGBP,          "VA_RT_FORMAT_RGBP"},
            {VA_RT_FORMAT_RGB32_10,      "VA_RT_FORMAT_RGB32_10"},
            {VA_RT_FORMAT_RGB32_10BPP,   "VA_RT_FORMAT_RGB32_10BPP"},
            {VA_RT_FORMAT_YUV420_10BPP,  "VA_RT_FORMAT_YUV420_10BPP"},
            {VA_RT_FORMAT_PROTECTED,     "VA_RT_FORMAT_PROTECTED"},
        };

        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribRTFormat].type));
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribRTFormat].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribSpatialResidual].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: %x\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribSpatialResidual].type),
               attrib_list[VAConfigAttribSpatialResidual].value);
    }

    if (attrib_list[VAConfigAttribSpatialClipping].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: %x\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribSpatialClipping].type),
               attrib_list[VAConfigAttribSpatialClipping].value);
    }

    if (attrib_list[VAConfigAttribIntraResidual].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: %x\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribIntraResidual].type),
               attrib_list[VAConfigAttribIntraResidual].value);
    }

    if (attrib_list[VAConfigAttribEncryption].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: %x\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncryption].type),
               attrib_list[VAConfigAttribEncryption].value);
    }

    if (attrib_list[VAConfigAttribRateControl].value != VA_ATTRIB_NOT_SUPPORTED) {
        static struct str_format list[] = {
            {VA_RC_NONE,            "VA_RC_NONE"},
            {VA_RC_CBR,             "VA_RC_CBR"},
            {VA_RC_VBR,             "VA_RC_VBR"},
            {VA_RC_VCM,             "VA_RC_VCM"},
            {VA_RC_CQP,             "VA_RC_CQP"},
            {VA_RC_VBR_CONSTRAINED, "VA_RC_VBR_CONSTRAINED"},
            {VA_RC_ICQ,             "VA_RC_ICQ"},
            {VA_RC_MB,              "VA_RC_MB"},
            {VA_RC_CFS,             "VA_RC_CFS"},
            {VA_RC_PARALLEL,        "VA_RC_PARALLEL"},
            {VA_RC_QVBR,            "VA_RC_QVBR"},
            {VA_RC_AVBR,            "VA_RC_AVBR"},
            {VA_RC_TCBRC,           "VA_RC_TCBRC"},
        };
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribRateControl].type));
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribRateControl].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribDecSliceMode].value != VA_ATTRIB_NOT_SUPPORTED) {
        static struct str_format list[] = {
            {VA_DEC_SLICE_MODE_NORMAL, "VA_DEC_SLICE_MODE_NORMAL"},
            {VA_DEC_SLICE_MODE_BASE,   "VA_DEC_SLICE_MODE_BASE"},
        };
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribDecSliceMode].type));
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribDecSliceMode].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribDecJPEG].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        static struct str_format list[] = {
            {1 << VA_ROTATION_NONE, "VA_ROTATION_NONE"},
            {1 << VA_ROTATION_90,   "VA_ROTATION_90"},
            {1 << VA_ROTATION_180,  "VA_ROTATION_180"},
            {1 << VA_ROTATION_270,  "VA_ROTATION_270"},
        };

        VAConfigAttribValDecJPEG *config = (VAConfigAttribValDecJPEG*)&attrib_list[VAConfigAttribDecJPEG].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribDecJPEG].type));
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (config->bits.rotation & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribDecProcessing].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s:", vaConfigAttribTypeStr(attrib_list[VAConfigAttribDecProcessing].type));
        if (VA_DEC_PROCESSING_NONE == attrib_list[VAConfigAttribDecProcessing].value)
            printf(" VA_DEC_PROCESSING_NONE\n");
        else if (VA_DEC_PROCESSING == attrib_list[VAConfigAttribDecProcessing].value)
            printf(" VA_DEC_PROCESSING\n");
    }

    if (attrib_list[VAConfigAttribEncPackedHeaders].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncPackedHeaders].type));
        if (VA_ENC_PACKED_HEADER_NONE == attrib_list[VAConfigAttribEncPackedHeaders].value)
            printf("VA_ENC_PACKED_HEADER_NONE\n");
        else {
            static struct str_format list[] = {
                {VA_ENC_PACKED_HEADER_SEQUENCE, "VA_ENC_PACKED_HEADER_SEQUENCE"},
                {VA_ENC_PACKED_HEADER_PICTURE,  "VA_ENC_PACKED_HEADER_PICTURE"},
                {VA_ENC_PACKED_HEADER_SLICE,    "VA_ENC_PACKED_HEADER_SLICE"},
                {VA_ENC_PACKED_HEADER_MISC,     "VA_ENC_PACKED_HEADER_MISC"},
                {VA_ENC_PACKED_HEADER_RAW_DATA, "VA_ENC_PACKED_HEADER_RAW_DATA"},
            };
            for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
                if (attrib_list[VAConfigAttribEncPackedHeaders].value & list[i].format) {
                    printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                    n++;
                }
            }
        }
    }

    if (attrib_list[VAConfigAttribEncInterlaced].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncInterlaced].type));
        if (VA_ENC_INTERLACED_NONE == attrib_list[VAConfigAttribEncInterlaced].value)
            printf("VA_ENC_INTERLACED_NONE\n");
        else {
            static struct str_format list[] = {
                {VA_ENC_INTERLACED_FRAME,       "VA_ENC_INTERLACED_FRAME"},
                {VA_ENC_INTERLACED_FIELD,       "VA_ENC_INTERLACED_FIELD"},
                {VA_ENC_INTERLACED_MBAFF,       "VA_ENC_INTERLACED_MBAFF"},
                {VA_ENC_INTERLACED_PAFF,        "VA_ENC_INTERLACED_PAFF"},
                {VA_ENC_PACKED_HEADER_RAW_DATA, "VA_ENC_PACKED_HEADER_RAW_DATA"},
            };
            for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
                if (attrib_list[VAConfigAttribEncInterlaced].value & list[i].format) {
                    printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                    n++;
                }
            }
        }
    }

    if (attrib_list[VAConfigAttribEncMaxRefFrames].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncMaxRefFrames].type));
        printf("l0=%d\n", attrib_list[VAConfigAttribEncMaxRefFrames].value & 0xffff);
        printf("%-*sl1=%d\n", 45, "", (attrib_list[VAConfigAttribEncMaxRefFrames].value >> 16) & 0xffff);
    }

 #if VA_CHECK_VERSION(1, 21, 0)
    if (attrib_list[VAConfigAttribEncMaxTileRows].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncMaxTileRows].type),
               attrib_list[VAConfigAttribEncMaxTileRows].value);
    }

    if (attrib_list[VAConfigAttribEncMaxTileCols].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncMaxTileCols].type),
               attrib_list[VAConfigAttribEncMaxTileCols].value);
    }
#endif

    if (attrib_list[VAConfigAttribEncMaxSlices].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncMaxSlices].type),
               attrib_list[VAConfigAttribEncMaxSlices].value);
    }

    if (attrib_list[VAConfigAttribEncSliceStructure].value != VA_ATTRIB_NOT_SUPPORTED) {
        static struct str_format list[] = {
            {VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS,     "VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS"},
            {VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS, "VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS"},
            {VA_ENC_SLICE_STRUCTURE_EQUAL_ROWS,            "VA_ENC_SLICE_STRUCTURE_EQUAL_ROWS"},
            {VA_ENC_SLICE_STRUCTURE_MAX_SLICE_SIZE,        "VA_ENC_SLICE_STRUCTURE_MAX_SLICE_SIZE"},
            {VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS,        "VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS"},
        };
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncSliceStructure].type));
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribEncSliceStructure].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribEncMacroblockInfo].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: supported\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncMacroblockInfo].type));
    }

    if (attrib_list[VAConfigAttribMaxPictureWidth].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribMaxPictureWidth].type),
               attrib_list[VAConfigAttribMaxPictureWidth].value);
    }

    if (attrib_list[VAConfigAttribMaxPictureHeight].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribMaxPictureHeight].type),
               attrib_list[VAConfigAttribMaxPictureHeight].value);
    }

    if (attrib_list[VAConfigAttribEncJPEG].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        VAConfigAttribValEncJPEG *config = (VAConfigAttribValEncJPEG*)&attrib_list[VAConfigAttribEncJPEG].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncJPEG].type));
        printf("arithmatic_coding_mode=%d\n", config->bits.arithmatic_coding_mode);
        printf("%-*sprogressive_dct_mode=%d\n", 45, "", config->bits.progressive_dct_mode);
        printf("%-*snon_interleaved_mode=%d\n", 45, "", config->bits.non_interleaved_mode);
        printf("%-*sdifferential_mode=%d\n", 45, "", config->bits.differential_mode);
        printf("%-*sdifferential_mode=%d\n", 45, "", config->bits.differential_mode);
        printf("%-*smax_num_components=%d\n", 45, "", config->bits.max_num_components);
        printf("%-*smax_num_scans=%d\n", 45, "", config->bits.max_num_scans);
        printf("%-*smax_num_huffman_tables=%d\n", 45, "", config->bits.max_num_huffman_tables);
        printf("%-*smax_num_quantization_tables=%d\n", 45, "", config->bits.max_num_quantization_tables);
    }

    if (attrib_list[VAConfigAttribEncQualityRange].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: number of supported quality levels is %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncQualityRange].type),
               attrib_list[VAConfigAttribEncQualityRange].value <= 1 ? 1 : attrib_list[VAConfigAttribEncQualityRange].value);
    }

    if (attrib_list[VAConfigAttribEncQuantization].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s:", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncQuantization].type));
        if (VA_ENC_QUANTIZATION_NONE == attrib_list[VAConfigAttribEncQuantization].value)
            printf(" VA_ENC_QUANTIZATION_NONE\n");
        else if (VA_ENC_QUANTIZATION_TRELLIS_SUPPORTED == attrib_list[VAConfigAttribEncQuantization].value)
            printf(" VA_ENC_QUANTIZATION_TRELLIS_SUPPORTED\n");
    }

    if (attrib_list[VAConfigAttribEncIntraRefresh].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncIntraRefresh].type));
        if (VA_ENC_INTRA_REFRESH_NONE == attrib_list[VAConfigAttribEncIntraRefresh].value)
            printf("VA_ENC_INTRA_REFRESH_NONE\n");
        else {
            static struct str_format list[] = {
                {VA_ENC_INTRA_REFRESH_ROLLING_COLUMN, "VA_ENC_INTRA_REFRESH_ROLLING_COLUMN"},
                {VA_ENC_INTRA_REFRESH_ROLLING_ROW,    "VA_ENC_INTRA_REFRESH_ROLLING_ROW"},
                {VA_ENC_INTRA_REFRESH_ADAPTIVE,       "VA_ENC_INTRA_REFRESH_ADAPTIVE"},
                {VA_ENC_INTRA_REFRESH_CYCLIC,         "VA_ENC_INTRA_REFRESH_CYCLIC"},
                {VA_ENC_INTRA_REFRESH_P_FRAME,        "VA_ENC_INTRA_REFRESH_P_FRAME"},
                {VA_ENC_INTRA_REFRESH_B_FRAME,        "VA_ENC_INTRA_REFRESH_B_FRAME"},
                {VA_ENC_INTRA_REFRESH_MULTI_REF,      "VA_ENC_INTRA_REFRESH_MULTI_REF"},
            };
            for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
                if (attrib_list[VAConfigAttribEncIntraRefresh].value & list[i].format) {
                    printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                    n++;
                }
            }
        }
    }

    if (attrib_list[VAConfigAttribEncSkipFrame].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: supported\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncSkipFrame].type));
    }

    if (attrib_list[VAConfigAttribEncROI].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        VAConfigAttribValEncROI *config = (VAConfigAttribValEncROI*)&attrib_list[VAConfigAttribEncROI].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncROI].type));
        printf("num_roi_regions=%d\n", config->bits.num_roi_regions);
        printf("%-*sroi_rc_priority_support=%d\n", 45, "", config->bits.roi_rc_priority_support);
        printf("%-*sroi_rc_qp_delta_support=%d\n", 45, "", config->bits.roi_rc_qp_delta_support);
    }

    if (attrib_list[VAConfigAttribEncRateControlExt].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        VAConfigAttribValEncRateControlExt *config = (VAConfigAttribValEncRateControlExt*)&attrib_list[VAConfigAttribEncRateControlExt].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncRateControlExt].type));
        printf("max_num_temporal_layers_minus1=%d ",       config->bits.max_num_temporal_layers_minus1);
        printf("temporal_layer_bitrate_control_flag=%d\n", config->bits.temporal_layer_bitrate_control_flag);
    }

    if (attrib_list[VAConfigAttribProcessingRate].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribProcessingRate].type));
        static struct str_format list[] = {
            {VA_PROCESSING_RATE_ENCODE, "VA_PROCESSING_RATE_ENCODE"},
            {VA_PROCESSING_RATE_DECODE, "VA_PROCESSING_RATE_DECODE"},
        };
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribProcessingRate].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribEncDirtyRect].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: number of supported regions is %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncDirtyRect].type),
               attrib_list[VAConfigAttribEncDirtyRect].value);
    }

    if (attrib_list[VAConfigAttribEncParallelRateControl].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: number of supported layers is %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncParallelRateControl].type),
               attrib_list[VAConfigAttribEncParallelRateControl].value);
    }

    if (attrib_list[VAConfigAttribEncDynamicScaling].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: supported\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncDynamicScaling].type));
    }

    if (attrib_list[VAConfigAttribFrameSizeToleranceSupport].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribFrameSizeToleranceSupport].type),
               attrib_list[VAConfigAttribFrameSizeToleranceSupport].value);
    }

    if (attrib_list[VAConfigAttribFEIFunctionType].value != VA_ATTRIB_NOT_SUPPORTED) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribFEIFunctionType].type));
        static struct str_format list[] = {
            {VA_FEI_FUNCTION_ENC,     "VA_FEI_FUNCTION_ENC"},
            {VA_FEI_FUNCTION_PAK,     "VA_FEI_FUNCTION_PAK"},
            {VA_FEI_FUNCTION_ENC_PAK, "VA_FEI_FUNCTION_ENC_PAK"},
        };
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribFEIFunctionType].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribFEIMVPredictors].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: number of supported MV predictors is %d\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribFEIMVPredictors].type),
               attrib_list[VAConfigAttribFEIMVPredictors].value);
    }

    if (attrib_list[VAConfigAttribStats].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        VAConfigAttribValStats *config = (VAConfigAttribValStats*)&attrib_list[VAConfigAttribStats].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribStats].type));
        printf("max_num_past_references=%d\n", config->bits.max_num_past_references);
        printf("%-*smax_num_future_references=%d\n", 45, "", config->bits.max_num_future_references);
        printf("%-*snum_outputs=%d\n", 45, "", config->bits.num_outputs);
        printf("%-*sinterlaced=%d\n", 45, "", config->bits.interlaced);
    }

    if (attrib_list[VAConfigAttribEncTileSupport].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: supported\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncTileSupport].type));
    }

    if (attrib_list[VAConfigAttribQPBlockSize].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: supported\n", vaConfigAttribTypeStr(attrib_list[VAConfigAttribQPBlockSize].type));
    }

    if (attrib_list[VAConfigAttribMaxFrameSize].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        VAConfigAttribValMaxFrameSize *config = (VAConfigAttribValMaxFrameSize*)&attrib_list[VAConfigAttribMaxFrameSize].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribMaxFrameSize].type));
        printf("max_frame_size=%d\n", config->bits.max_frame_size);
        printf("%-*smultiple_pass=%d\n", 45, "", config->bits.multiple_pass);
    }

    if (attrib_list[VAConfigAttribPredictionDirection].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribPredictionDirection].type));
        static struct str_format list[] = {
            {VA_PREDICTION_DIRECTION_PREVIOUS, "VA_PREDICTION_DIRECTION_PREVIOUS"},
            {VA_PREDICTION_DIRECTION_FUTURE,  "VA_PREDICTION_DIRECTION_FUTURE"},
        };
        for (i = 0, n = 0; i < sizeof(list) / sizeof(list[0]); i++) {
            if (attrib_list[VAConfigAttribPredictionDirection].value & list[i].format) {
                printf("%-*s%s\n", 0 == n ? 0 : 45, "", list[i].name);
                n++;
            }
        }
    }

    if (attrib_list[VAConfigAttribMultipleFrame].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        VAConfigAttribValMultipleFrame *config = (VAConfigAttribValMultipleFrame*)&attrib_list[VAConfigAttribMultipleFrame].value;
        printf("    %-39s: ", vaConfigAttribTypeStr(attrib_list[VAConfigAttribMultipleFrame].type));
        printf("max_num_concurrent_frames=%d\n", config->bits.max_num_concurrent_frames);
        printf("%-*smixed_quality_level=%d\n", 45, "", config->bits.mixed_quality_level);
    }

    printf("\n");

    return 0;
}

int main(int argc, const char* argv[])
{
    VADisplay va_dpy;
    VAStatus va_status;
    int major_version, minor_version;
    const char *driver;
    const char *name = strrchr(argv[0], '/');
    VAProfile profile, *profile_list = NULL;
    int num_profiles, max_num_profiles, i;
    VAEntrypoint entrypoint, *entrypoints = NULL;
    int num_entrypoint = 0;
    int ret_val = 0;

    if (name)
        name++;
    else
        name = argv[0];

    parse_args(name, argc, (char **)argv);

    va_dpy = va_open_display();
    if (NULL == va_dpy) {
        fprintf(stderr, "%s: vaGetDisplay() failed\n", name);
        return 2;
    }

    va_status = vaInitialize(va_dpy, &major_version, &minor_version);
    CHECK_VASTATUS(va_status, "vaInitialize", 3);

    printf("%s: VA-API version: %d.%d (libva %s)\n",
           name, major_version, minor_version, LIBVA_VERSION_S);

    driver = vaQueryVendorString(va_dpy);
    printf("%s: Driver version: %s\n", name, driver ? driver : "<unknown>");

    num_entrypoint = vaMaxNumEntrypoints(va_dpy);
    entrypoints = malloc(num_entrypoint * sizeof(VAEntrypoint));
    if (!entrypoints) {
        printf("Failed to allocate memory for entrypoint list\n");
        ret_val = -1;
        goto error;
    }

    max_num_profiles = vaMaxNumProfiles(va_dpy);
    profile_list = malloc(max_num_profiles * sizeof(VAProfile));

    if (!profile_list) {
        printf("Failed to allocate memory for profile list\n");
        ret_val = 5;
        goto error;
    }

    va_status = vaQueryConfigProfiles(va_dpy, profile_list, &num_profiles);
    CHECK_VASTATUS(va_status, "vaQueryConfigProfiles", 6);

    if (show_all_opt) {
        printf("%s: Supported config attributes per profile/entrypoint pair\n", name);
        for (i = 0; i < num_profiles; i++) {
            profile = profile_list[i];
            va_status = vaQueryConfigEntrypoints(va_dpy, profile, entrypoints,
                                                 &num_entrypoint);
            if (va_status == VA_STATUS_ERROR_UNSUPPORTED_PROFILE)
                continue;

            CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints", 4);

            for (entrypoint = 0; entrypoint < num_entrypoint; entrypoint++) {
                ret_val = show_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint]);
                if (ret_val) {
                    printf("Failed to get config attributes\n");
                    goto error;
                }
            }
        }
    } else {
        printf("%s: Supported profile and entrypoints\n", name);
        for (i = 0; i < num_profiles; i++) {
            profile = profile_list[i];
            va_status = vaQueryConfigEntrypoints(va_dpy, profile, entrypoints,
                                                 &num_entrypoint);
            if (va_status == VA_STATUS_ERROR_UNSUPPORTED_PROFILE)
                continue;

            CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints", 4);

            for (entrypoint = 0; entrypoint < num_entrypoint; entrypoint++) {
                printf("      %-32s:	%s\n",
                       vaProfileStr(profile),
                       vaEntrypointStr(entrypoints[entrypoint]));
            }
        }
    }

error:
    free(entrypoints);
    free(profile_list);
    vaTerminate(va_dpy);
    va_close_display(va_dpy);

    return ret_val;
}
