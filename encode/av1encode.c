#define LIBVA_UTILS_UPLOAD_DOWNLOAD_YUV_SURFACE 1

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include <va/va.h>
#include "va_display.h"

#define ALIGN16(x)  ((x+15)&~15)
#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        exit(1);                                                        \
    }

#define MIN(a, b) ((a)>(b)?(b):(a))
#define MAX(a, b) ((a)>(b)?(a):(b))

#define CHECK_NULL(p)                                           \
    if(!(p))                                                      \
    {                                                           \
        fprintf(stderr, "Null pointer at:%s:%d\n", __func__, __LINE__); \
        exit(1);                                                \
    }
    
#define CHECK_CONDITION(cond)                                    \
    if(!(cond))                                                      \
    {                                                           \
        fprintf(stderr, "Unexpected condition: %s:%d\n", __func__, __LINE__); \
        exit(1);                                                \
    }

#define CHECK_BS_NULL(p)         \
    CHECK_NULL(p)        \
    CHECK_NULL(p->buffer)      

#include "loadsurface.h"

/*****
 * 
 * Bit stream management
 * 
 * 
 * */
#define BITSTREAM_ALLOCATE_STEPPING 1024 // in byte

struct __bitstream {
    uint8_t *buffer; // managed by u8 to avoid swap every 4byte
    int bit_offset;
    int max_size_in_byte;
};
typedef struct __bitstream bitstream;

static void
bitstream_start(bitstream *bs)
{
    CHECK_NULL(bs);
    bs->max_size_in_byte = BITSTREAM_ALLOCATE_STEPPING;
    bs->buffer = calloc(bs->max_size_in_byte * sizeof(uint8_t), 1);
    CHECK_NULL(bs->buffer);
    bs->bit_offset = 0;
}

static void
put_ui(bitstream* bs, uint32_t val, int size_in_bits)
{
    CHECK_BS_NULL(bs);
    CHECK_CONDITION((size_in_bits + bs->bit_offset) <= (8 * bs->max_size_in_byte));
    int remain_bits = 8 - (bs->bit_offset % 8);

    // make sure val does not overflow size_in_bits
    val &= (0xffffffff >> (32 - size_in_bits));

    if(size_in_bits <= remain_bits)
    {
        bs->buffer[bs->bit_offset / 8] |= val << (remain_bits - size_in_bits);
        bs->bit_offset += size_in_bits;
    }
    else
    {
        
        put_ui(bs, val >> (size_in_bits - remain_bits), remain_bits);
        put_ui(bs, val & (~(0xffffffff << (size_in_bits - remain_bits))), size_in_bits - remain_bits);

    }

}

static void
put_aligning_bits(bitstream* bs)
{
    CHECK_BS_NULL(bs);
    while (bs->bit_offset & 7)
        put_ui(bs, 0, 1);; //trailing_zero_bit
}

static void
put_trailing_bits(bitstream* bs)
{
    CHECK_BS_NULL(bs);
    put_ui(bs, 1, 1); //trailing_one_bit
    while (bs->bit_offset & 7)
        put_ui(bs, 0, 1);; //trailing_zero_bit
}

static void
bitstream_free(bitstream* bs)
{
    CHECK_BS_NULL(bs);
    free(bs->buffer);
    bs->bit_offset = 0;
    bs->max_size_in_byte = 0;
}

static void
bitstream_cat(bitstream *bs1, bitstream *bs2)
{
    CHECK_NULL(bs1);
    CHECK_NULL(bs2);
    CHECK_CONDITION(! (bs1->bit_offset & 7));
    if(! (bs1->bit_offset & 7)) //byte aligned
    {
        memcpy(bs1->buffer + (bs1->bit_offset / 8), bs2->buffer, bs2->bit_offset/8);
        bs1->bit_offset += bs2->bit_offset;
        bitstream_free(bs2);
    }
    else
    {
        //when call this function to concat 2 bitstreams, the first bitstream should always be byte aligned
        CHECK_CONDITION(0);
    }
}

/******
 * definition of para set structure
 * 
 * 
 * 
 * */
#define PRIMARY_REF_BITS              3
#define PRIMARY_REF_NONE              7

#define REF_FRAMES_LOG2               3
#define NUM_REF_FRAMES                (1 << REF_FRAMES_LOG2)
#define REFS_PER_FRAME                7
#define TOTAL_REFS_PER_FRAME          8
#define MAX_MODE_LF_DELTAS            2
#define MAX_MB_PLANE                  3

#define CDEF_MAX_STRENGTHS            8
#define CDEF_STRENGTH_BITS            6
#define CDEF_STRENGTH_DIVISOR         4
#define AV1_MAX_NUM_TILE_COLS         64
#define AV1_MAX_NUM_TILE_ROWS         64
#define MAX_NUM_OPERATING_POINTS      32

#define SURFACE_NUM 16 /* 16 surfaces for source YUV */

enum {
    SINGLE_REFERENCE      = 0,
    COMPOUND_REFERENCE    = 1,
    REFERENCE_MODE_SELECT = 2,
    REFERENCE_MODES       = 3,
} REFERENCE_MODE;

enum FRAME_TYPE
{
    KEY_FRAME = 0,
    INTER_FRAME = 1,
    INTRA_ONLY_FRAME = 2,
    SWITCH_FRAME = 3,
    NUM_FRAME_TYPES,
};

enum INTERP_FILTER{
    EIGHTTAP_REGULAR,
    EIGHTTAP_SMOOTH,
    EIGHTTAP_SHARP,
    BILINEAR,
    SWITCHABLE,
    INTERP_FILTERS_ALL
};

enum AV1_OBU_TYPE
{
    OBU_SEQUENCE_HEADER = 1,
    OBU_TEMPORAL_DELIMITER = 2,
    OBU_FRAME_HEADER = 3,
    OBU_TILE_GROUP = 4,
    OBU_METADATA = 5,
    OBU_FRAME = 6,
    OBU_REDUNDANT_FRAME_HEADER = 7,
    OBU_PADDING = 15,
};

struct LoopFilterParams
{
    int32_t loop_filter_level[4];
    int32_t loop_filter_sharpness;
    uint8_t loop_filter_delta_enabled;
    uint8_t loop_filter_delta_update;
    // 0 = Intra, Last, Last2, Last3, GF, BWD, ARF
    int8_t loop_filter_ref_deltas[TOTAL_REFS_PER_FRAME];
    // 0 = ZERO_MV, MV
    int8_t loop_filter_mode_deltas[MAX_MODE_LF_DELTAS];
};

struct TileInfoAv1
{
    uint32_t uniform_tile_spacing_flag;
    uint32_t TileColsLog2;
    uint32_t TileRowsLog2;
    uint32_t TileCols;
    uint32_t TileRows;
    uint32_t TileWidthInSB[AV1_MAX_NUM_TILE_COLS];  // valid for 0 <= i < TileCols
    uint32_t TileHeightInSB[AV1_MAX_NUM_TILE_ROWS]; // valid for 0 <= i < TileRows
    uint32_t context_update_tile_id;
    uint32_t TileSizeBytes;
};

struct QuantizationParams
{
    uint32_t base_q_idx;
    int32_t DeltaQYDc;
    int32_t DeltaQUDc;
    int32_t DeltaQUAc;
    int32_t DeltaQVDc;
    int32_t DeltaQVAc;
    uint32_t using_qmatrix;
    uint32_t qm_y;
    uint32_t qm_u;
    uint32_t qm_v;
};

struct CdefParams
{
    uint32_t cdef_damping;
    uint32_t cdef_bits;
    uint32_t cdef_y_pri_strength[CDEF_MAX_STRENGTHS];
    uint32_t cdef_y_sec_strength[CDEF_MAX_STRENGTHS];
    uint32_t cdef_uv_pri_strength[CDEF_MAX_STRENGTHS];
    uint32_t cdef_uv_sec_strength[CDEF_MAX_STRENGTHS];
};

enum RestorationType
{
    RESTORE_NONE,
    RESTORE_SWITCHABLE,
    RESTORE_WIENER,
    RESTORE_SGRPROJ,
    RESTORE_TYPES = 4,
};

enum {
    BITDEPTH_8 = 8,
    BITDEPTH_10 = 10
};

enum {
    INTRA_FRAME     = 0,
    LAST_FRAME      = 1,
    LAST2_FRAME     = 2,
    LAST3_FRAME     = 3,
    GOLDEN_FRAME    = 4,
    BWDREF_FRAME    = 5,
    ALTREF2_FRAME   = 6,
    ALTREF_FRAME    = 7,
    MAX_REF_FRAMES  = 8
};

struct LRParams
{
    enum RestorationType lr_type[MAX_MB_PLANE];
    uint32_t lr_unit_shift;
    uint32_t lr_uv_shift;
    uint32_t lr_unit_extra_shift;
};

enum TX_MODE{
    ONLY_4X4 = 0,     // only 4x4 transform used
    TX_MODE_LARGEST,  // transform size is the largest possible for pu size
    TX_MODE_SELECT,   // transform specified for each block
    TX_MODES,
};

struct ColorConfig
{
    uint32_t BitDepth                      ;
    uint32_t mono_chrome                   ;
    uint32_t color_primaries               ;
    uint32_t transfer_characteristics      ;
    uint32_t matrix_coefficients           ;
    uint32_t color_description_present_flag;
    uint32_t color_range                   ;
    uint32_t chroma_sample_position        ;
    uint32_t subsampling_x                 ;
    uint32_t subsampling_y                 ;
    uint32_t separate_uv_delta_q           ;
};

typedef struct FrameHeader
{
    uint32_t show_existing_frame;
    uint32_t frame_to_show_map_idx;
    uint64_t frame_presentation_time;
    uint32_t display_frame_id;
    enum FRAME_TYPE frame_type;
    uint32_t show_frame;
    uint32_t showable_frame;
    uint32_t error_resilient_mode;
    uint32_t disable_cdf_update;
    uint32_t allow_screen_content_tools;
    uint32_t force_integer_mv;
    uint32_t frame_size_override_flag;
    uint32_t order_hint;
    uint32_t primary_ref_frame;

    uint8_t refresh_frame_flags;

    uint32_t FrameWidth;//input
    uint32_t FrameHeight;//input
    uint32_t use_superres;
    uint32_t SuperresDenom;
    uint32_t UpscaledWidth;
    uint32_t RenderWidth;
    uint32_t RenderHeight;

    uint32_t allow_intrabc;
    int32_t ref_frame_idx[REFS_PER_FRAME];
    uint32_t allow_high_precision_mv;
    enum INTERP_FILTER interpolation_filter;
    //uint32_t is_motion_mode_switchable;
    uint32_t use_ref_frame_mvs; 
    uint32_t disable_frame_end_update_cdf;

    uint32_t sbCols; 
    uint32_t sbRows;
    uint32_t sbSize; //64 by default

    struct TileInfoAv1 tile_info;
    struct QuantizationParams quantization_params; 

    uint32_t delta_q_present;
    uint32_t delta_q_res;

    uint32_t delta_lf_present; 
    uint32_t delta_lf_res; 
    uint32_t delta_lf_multi;

    uint32_t CodedLossless;
    uint32_t AllLossless;

    struct LoopFilterParams loop_filter_params;
    struct CdefParams cdef_params;
    struct LRParams lr_params;

    enum TX_MODE TxMode;
    uint32_t reference_select;
    uint32_t skipModeAllowed;
    uint32_t skipModeFrame[2];
    uint32_t skip_mode_present;
    uint32_t allow_warped_motion;
    uint32_t reduced_tx_set;

} FH;

typedef struct SequenceHeader
{
    uint32_t seq_profile                      ;
    uint32_t still_picture                    ;
    uint32_t reduced_still_picture_header     ;

    uint32_t timing_info_present_flag         ;

    uint32_t decoder_model_info_present_flag  ;

    uint32_t operating_points_cnt_minus_1     ; 
    uint32_t operating_point_idc[MAX_NUM_OPERATING_POINTS]                                   ;
    uint32_t seq_level_idx[MAX_NUM_OPERATING_POINTS]                                         ;
    uint32_t seq_tier[MAX_NUM_OPERATING_POINTS]                                              ;
    uint32_t decoder_model_present_for_this_op[MAX_NUM_OPERATING_POINTS]                     ;
    uint32_t initial_display_delay_minus_1[MAX_NUM_OPERATING_POINTS]                         ;

    uint32_t frame_width_bits                   ; //15 as default value
    uint32_t frame_height_bits                  ; //15 as default value

    uint32_t frame_id_numbers_present_flag      ; // default 0

    uint32_t sbSize                             ; //default 64
    uint32_t enable_filter_intra                ; 
    uint32_t enable_intra_edge_filter           ;
    uint32_t enable_interintra_compound         ;
    uint32_t enable_masked_compound             ;
    uint32_t enable_warped_motion               ; 
    uint32_t enable_dual_filter                 ; 
    uint32_t enable_order_hint                  ;//default set to 1
    uint32_t enable_jnt_comp                    ;
    uint32_t enable_ref_frame_mvs               ; 
    uint32_t seq_force_screen_content_tools     ; 
    uint32_t seq_force_integer_mv               ; 
    uint32_t order_hint_bits_minus1             ; //default 8 - 1
    uint32_t enable_superres                    ; 
    uint32_t enable_cdef                        ;
    uint32_t enable_restoration                 ;

    struct ColorConfig color_config; //default

    uint32_t film_grain_param_present           ;

    uint32_t frame_rate_extN;
    uint32_t frame_rate_extD;
} SH;

struct ObuExtensionHeader {
    uint32_t temporal_id;
    uint32_t spatial_id;
};

struct BitOffsets
{
    uint32_t QIndexBitOffset;
    uint32_t SegmentationBitOffset;
    uint32_t SegmentationBitSize; 
    uint32_t LoopFilterParamsBitOffset;
    uint32_t FrameHdrOBUSizeInBits;
    uint32_t FrameHdrOBUSizeByteOffset;
    uint32_t UncompressedHeaderByteOffset;
    uint32_t CDEFParamsBitOffset;
    uint32_t CDEFParamsSizeInBits;
};

struct Av1InputParameters
{
    char* srcyuv;
    char* recyuv;
    char* output;
    uint32_t profile;
    
    uint32_t order_hint_bits;
    uint32_t enable_cdef;
    uint32_t width;
    uint32_t height;
    uint32_t LDB;
    uint32_t frame_rate_extN;
    uint32_t frame_rate_extD;
    uint32_t level;

    // for brc
    uint32_t bit_rate;
    uint8_t MinBaseQIndex;
    uint8_t MaxBaseQIndex;

    uint32_t intra_period;
    uint32_t ip_period;
    uint32_t RateControlMethod;
    uint32_t BRefType;
    int encode_syncmode;
    int calc_psnr;
    int frame_count;
    int frame_width_aligned;
    int frame_height_aligned;
    uint32_t base_qindex;
    int bit_depth;
    int target_bitrate;
    int vbr_max_bitrate;
    int buffer_size;
    int initial_buffer_fullness;
};


static  VADisplay va_dpy;
static  VAProfile av1_profile;
static  VAEntrypoint entryPoint;
static  VAConfigAttrib attrib[VAConfigAttribTypeMax];
static  VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
static  int config_attrib_num = 0;
static  VASurfaceID src_surface[SURFACE_NUM];
static  VABufferID  coded_buf[SURFACE_NUM];
static  VASurfaceID ref_surface[SURFACE_NUM];
static  VAConfigID config_id;
static  VAContextID context_id;

// buffer 
static  VAEncSequenceParameterBufferAV1 seq_param;
static  VAEncPictureParameterBufferAV1 pic_param;
static  VAEncTileGroupBufferAV1 tile_group_param;

// sh fh ips
static  struct Av1InputParameters ips;
static  FH fh;
static  SH sh;
struct BitOffsets offsets;

//Default entrypoint for Encode
static VAEntrypoint requested_entrypoint = -1;

static  unsigned long long current_frame_encoding = 0;
static  unsigned long long current_frame_display = 0;
static  int current_frame_type;
#define current_slot (current_frame_display % SURFACE_NUM)

/* thread to save coded data/upload source YUV */
struct storage_task_t {
    void *next;
    unsigned long long display_order;
    unsigned long long encode_order;
};

static  struct storage_task_t *storage_task_header = NULL, *storage_task_tail = NULL;
#define SRC_SURFACE_IN_ENCODING 0
#define SRC_SURFACE_IN_STORAGE  1
static  int srcsurface_status[SURFACE_NUM];
//static  int encode_syncmode = 0; moved to input pars
static  pthread_mutex_t encode_mutex = PTHREAD_MUTEX_INITIALIZER;
static  pthread_cond_t  encode_cond = PTHREAD_COND_INITIALIZER;
static  pthread_t encode_thread;

static  FILE *coded_fp = NULL, *srcyuv_fp = NULL, *recyuv_fp = NULL;
static  unsigned long long srcyuv_frames = 0;
static  int srcyuv_fourcc = VA_FOURCC_IYUV;

static  uint64_t frame_size = 0;

/* for performance profiling */
static unsigned int UploadPictureTicks = 0;
static unsigned int BeginPictureTicks = 0;
static unsigned int RenderPictureTicks = 0;
static unsigned int EndPictureTicks = 0;
static unsigned int SyncPictureTicks = 0;
static unsigned int SavePictureTicks = 0;
static unsigned int TotalTicks = 0;

static  unsigned int frame_coded = 0;

static  int rc_default_modes[] = {
    VA_RC_VBR,
    VA_RC_CQP,
    VA_RC_VBR_CONSTRAINED,
    VA_RC_CBR,
    VA_RC_VCM,
    VA_RC_NONE,
};

static int len_ivf_header;
static int len_seq_header;
static int len_pic_header;

/*
 * Helper function for profiling purposes
 */
static unsigned int GetTickCount()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

static int string_to_fourcc(char *str)
{
    CHECK_NULL(str);
    int fourcc;

    if (!strncmp(str, "NV12", 4))
        fourcc = VA_FOURCC_NV12;
    else if (!strncmp(str, "IYUV", 4))
        fourcc = VA_FOURCC_IYUV;
    else if (!strncmp(str, "YV12", 4))
        fourcc = VA_FOURCC_YV12;
    else if (!strncmp(str, "UYVY", 4))
        fourcc = VA_FOURCC_UYVY;
    else {
        printf("Unknow FOURCC\n");
        fourcc = -1;
    }
    return fourcc;
}

static int string_to_rc(char *str)
{
    CHECK_NULL(str);
    int rc_mode;

    if (!strncmp(str, "NONE", 4))
        rc_mode = VA_RC_NONE;
    else if (!strncmp(str, "CBR", 3))
        rc_mode = VA_RC_CBR;
    else if (!strncmp(str, "VBR", 3))
        rc_mode = VA_RC_VBR;
    else if (!strncmp(str, "VCM", 3))
        rc_mode = VA_RC_VCM;
    else if (!strncmp(str, "CQP", 3))
        rc_mode = VA_RC_CQP;
    else if (!strncmp(str, "VBR_CONSTRAINED", 15))
        rc_mode = VA_RC_VBR_CONSTRAINED;
    else {
        printf("Unknown RC mode\n");
        rc_mode = -1;
    }
    return rc_mode;
}

static void print_help()
{
    printf("./av1encode <options>\n");
    printf("   -n <frames> -f <frame rate> -o <output>\n");
    printf("   --intra_period <number>\n");
    printf("   --ip_period <number>\n");
    printf("   --rcmode <16 for CQP>\n");
    printf("   --srcyuv <filename> load YUV from a file\n");
    printf("   --fourcc <NV12|IYUV|YV12> source YUV fourcc\n");
    printf("   --recyuv <filename> save reconstructed YUV into a file\n");
    printf("   --enablePSNR calculate PSNR of recyuv vs. srcyuv\n");
    printf("   --level\n");
    printf("   --height <number>\n");
    printf("   --width <number>\n");
    printf("   --base_q_idx <number> 1-255\n");
    printf("   --normal_mode select VAEntrypointEncSlice as entrypoint\n");
    printf("   --low_power_mode select VAEntrypointEncSliceLP as entrypoint\n");

    printf(" sample usage:\n");
    printf("./av1encode -n 8 -f 30 --intra_period 4 --ip_period 1 --rcmode CQP --srcyuv ./input.yuv --recyuv ./rec.yuv --fourcc IYUV --level 8 --width 1920 --height 1080 --base_q_idx 128  -o ./out.av1 --LDB --low_power_mode\n"
           "./av1encode -n 8 -f 30 --intra_period 4 --ip_period 1 --rcmode CBR --srcyuv ./input.yuv --recyuv ./rec.yuv --fourcc IYUV --level 8 --width 1920 --height 1080 --target_bitrate 3360000 -o ./out.av1 --LDB --low_power_mode\n"
           "./av1encode -n 8 -f 30 --intra_period 4 --ip_period 1 --rcmode VBR --srcyuv ./input.yuv --recyuv ./rec.yuv --fourcc IYUV --level 8 --width 1920 --height 1080 --vbr_max_bitrate 3360000 -o ./out.av1 --LDB --low_power_mode\n");

}

static void process_cmdline(int argc, char *argv[])
{
    int c;
    const struct option long_opts[] = {
        {"help",            no_argument,        NULL, 0 },
        {"intra_period",    required_argument,  NULL, 1 },
        {"ip_period",       required_argument,  NULL, 2 },
        {"rcmode",          required_argument,  NULL, 3 },
        {"srcyuv",          required_argument,  NULL, 4 },
        {"recyuv",          required_argument,  NULL, 5 },
        {"fourcc",          required_argument,  NULL, 6 },
        {"syncmode",        no_argument,        NULL, 7 },
        {"enablePSNR",      no_argument,        NULL, 8 },
        {"level",           required_argument,  NULL, 9 },
        {"height",          required_argument,  NULL, 10 },
        {"width",           required_argument,  NULL, 11 },
        {"base_q_idx",      required_argument,  NULL, 12},
        {"LDB",             no_argument,        NULL, 13},
        {"normal_mode",     no_argument,        NULL, 14},
        {"low_power_mode",  no_argument,        NULL, 15},
        {"target_bitrate",  required_argument,  NULL, 16},
        {"vbr_max_bitrate", required_argument,  NULL, 17},
        {NULL,              no_argument,        NULL, 0 }
    };

    int long_index;
    while ((c = getopt_long_only(argc, argv, "n:f:o:t:m:u:d:?", long_opts, &long_index)) != EOF)
    {
        switch (c) 
        {
            case 'n':
                ips.frame_count = atoi(optarg);
                break;
            case 'f':
                ips.frame_rate_extN = (int)(atoi(optarg) * 100);
                ips.frame_rate_extD = 100;
                break;
            case 'o':
                ips.output = strdup(optarg);
                break;
            case 1:
                ips.intra_period = atoi(optarg);
                break;
            case 2:
                ips.ip_period = atoi(optarg);
                break;
            case 3:
                ips.RateControlMethod = string_to_rc(optarg); //16:cqp 2:CBR 4:VBR
                break;
            case 4:
                ips.srcyuv = strdup(optarg);
                break;
            case 5:
                ips.recyuv = strdup(optarg);
                break;
            case 6:
                srcyuv_fourcc = string_to_fourcc(optarg);
                break;
            case 7:
                ips.encode_syncmode = 1;
                break;
            case 8:
                ips.calc_psnr = 1;
                break;
            case 9:
                ips.level = atoi(optarg);
                break;
            case 10:
                ips.height = atoi(optarg);
                ips.frame_height_aligned = (ips.height + 63) & (~63);
                break;
            case 11:
                ips.width = atoi(optarg);
                ips.frame_width_aligned = (ips.width + 63) & (~63);
                break;
            case 12:
                ips.base_qindex = atoi(optarg);
                break;
            case 13:
                ips.LDB = 1;
                break;
            case 14:
                requested_entrypoint = VAEntrypointEncSlice;
                break;
            case 15:
                requested_entrypoint = VAEntrypointEncSliceLP;
                break;
            case 't':
            case 16:
                ips.target_bitrate = atoi(optarg);
                break;
            case 'm':
            case 17:
                ips.vbr_max_bitrate = atoi(optarg);
                break;
            case 'u':
                ips.buffer_size = atoi(optarg) * 8000;
                break;
            case 'd':
                ips.initial_buffer_fullness = atoi(optarg) * 8000;
                break;
            case 0:
            case ':':
            case '?':
                print_help();
                exit(0);
        }
    }

    // init other input parameters as default value
    ips.MaxBaseQIndex = 255;
    ips.MinBaseQIndex = 1;
    ips.bit_depth = 8;

    if (ips.frame_rate_extD == 0)
    {
        ips.frame_rate_extN = 3000;
        ips.frame_rate_extD = 100;
    }

    int default_bitrate = (long long int) ips.height * ips.width * 12 * ips.frame_rate_extN / ips.frame_rate_extD / 50;
    // For CBR, target bitrate should be set
    if(ips.RateControlMethod == VA_RC_CBR)
    {
        if (ips.target_bitrate == 0)
        {
            ips.target_bitrate = default_bitrate;
        }
    }
    // For VBR, max bitrate should be set
    else if (ips.RateControlMethod == VA_RC_VBR)
    {
        if (ips.target_bitrate == 0 && ips.vbr_max_bitrate == 0)
        {
            ips.vbr_max_bitrate = default_bitrate;
        }
        else if (ips.vbr_max_bitrate == 0)
        {
            ips.vbr_max_bitrate = ips.target_bitrate;
        }
    }

    // interface with IO
    /* open source file */
    if (ips.srcyuv) {
        srcyuv_fp = fopen(ips.srcyuv, "r");

        if (srcyuv_fp == NULL)
            printf("Open source YUV file %s failed, use auto-generated YUV data\n", ips.srcyuv);
        else {
            struct stat tmp;

            int ret = fstat(fileno(srcyuv_fp), &tmp);
            CHECK_CONDITION(ret == 0);
            srcyuv_frames = tmp.st_size / (ips.width * ips.height * 1.5);
            printf("Source YUV file %s with %llu frames\n", ips.srcyuv, srcyuv_frames);

            if (ips.frame_count == 0)
                ips.frame_count = srcyuv_frames;
        }
    }

    /* open source file */
    if (ips.recyuv) {
        recyuv_fp = fopen(ips.recyuv, "w+");

        if (recyuv_fp == NULL)
            printf("Open reconstructed YUV file %s failed\n", ips.recyuv);
    }

    if (ips.output == NULL) {
        struct stat buf;
        if (stat("/tmp", &buf) == 0)
            ips.output = strdup("/tmp/test.av1");
        else if (stat("/sdcard", &buf) == 0)
            ips.output = strdup("/sdcard/test.av1");
        else
            ips.output = strdup("./test.av1");
    }

    /* store coded data into a file */
    if (ips.output) {
        coded_fp = fopen(ips.output, "w+");
    } else {
        printf("Copy file string failed");
        exit(1);
    }
    if (coded_fp == NULL) {
        printf("Open file %s failed, exit\n", ips.output);
        exit(1);
    }

}

static char *rc_to_string(int rcmode)
{
    switch (rcmode) {
    case VA_RC_NONE:
        return "NONE";
    case VA_RC_CBR:
        return "CBR";
    case VA_RC_VBR:
        return "VBR";
    case VA_RC_VCM:
        return "VCM";
    case VA_RC_CQP:
        return "CQP";
    case VA_RC_VBR_CONSTRAINED:
        return "VBR_CONSTRAINED";
    default:
        return "Unknown";
    }
}

static int print_input()
{
    printf("frame count: %d \n", ips.frame_count);
    printf("frame rate: %d \n", ips.frame_rate_extN / ips.frame_rate_extD);
    printf("Intra period: %d \n", ips.intra_period);
    printf("Gop ref dist: %d \n", ips.ip_period);
    printf("rcmode: %s \n", rc_to_string(ips.RateControlMethod));
    printf("source yuv: %s \n", ips.srcyuv);
    printf("recon yuv: %s \n", ips.recyuv);
    printf("output bitstream: %s \n", ips.output);
    printf("level index: %d \n", ips.level);
    printf("frame height: %d \n", ips.height);
    printf("frame width: %d \n", ips.width);
    printf("base_q_index: %d \n", ips.base_qindex);
    printf("target_bitrate: %d bps\n", ips.target_bitrate);
    printf("vbr_max_bitrate: %d bps\n", ips.vbr_max_bitrate);
    return 0;
}

static int init_va(void)
{
    va_dpy = va_open_display();
    VAStatus va_status;
    int major_ver, minor_ver;
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    CHECK_VASTATUS(va_status, "vaInitialize");
    av1_profile = VAProfileAV1Profile0;

    // select entrypoint

    int num_entrypoints = vaMaxNumEntrypoints(va_dpy);
    VAEntrypoint* entrypoints = malloc(num_entrypoints * sizeof(*entrypoints));
    if (!entrypoints) {
        fprintf(stderr, "error: failed to initialize VA entrypoints array\n");
        exit(1);
    }

    vaQueryConfigEntrypoints(va_dpy, av1_profile, entrypoints, &num_entrypoints);
    int support_encode = 0;
    for (int slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) 
    {
        if (requested_entrypoint == -1) {
            //Select the entry point based on what is avaiable
            if ((entrypoints[slice_entrypoint] == VAEntrypointEncSlice) ||
                (entrypoints[slice_entrypoint] == VAEntrypointEncSliceLP)) {
                support_encode = 1;
                entryPoint = entrypoints[slice_entrypoint];
                break;
            }
        } else if ((entrypoints[slice_entrypoint] == requested_entrypoint)) {
            //Select the entry point based on what was requested in cmd line option
            support_encode = 1;
            entryPoint = entrypoints[slice_entrypoint];
            break;
        }
    }

    if(entrypoints)
        free(entrypoints);

    if (support_encode == 0) {
        printf("Can't find avaiable or requested entrypoints for AV1 profiles\n");
        exit(1);
    }

    unsigned int i;
    for (i = 0; i < VAConfigAttribTypeMax; i++)
    attrib[i].type = i;


    va_status = vaGetConfigAttributes(va_dpy, av1_profile, entryPoint,
                                      &attrib[0], VAConfigAttribTypeMax);
    CHECK_VASTATUS(va_status, "vaGetConfigAttributes");
    /* check the interested configattrib */
    if ((attrib[VAConfigAttribRTFormat].value & VA_RT_FORMAT_YUV420) == 0) {
        printf("Not find desired YUV420 RT format\n");
        exit(1);
    } else {
        config_attrib[config_attrib_num].type = VAConfigAttribRTFormat;
        config_attrib[config_attrib_num].value = VA_RT_FORMAT_YUV420;
        config_attrib_num++;
    }

    if (attrib[VAConfigAttribRateControl].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribRateControl].value;

        printf("Support rate control mode (0x%x):", tmp);

        if (tmp & VA_RC_NONE)
            printf("NONE ");
        if (tmp & VA_RC_CBR)
            printf("CBR ");
        if (tmp & VA_RC_VBR)
            printf("VBR ");
        if (tmp & VA_RC_VCM)
            printf("VCM ");
        if (tmp & VA_RC_CQP)
            printf("CQP ");
        if (tmp & VA_RC_VBR_CONSTRAINED)
            printf("VBR_CONSTRAINED ");

        printf("\n");

        if (ips.RateControlMethod == -1 || !(ips.RateControlMethod & tmp))  {
            if (ips.RateControlMethod != -1) {
                printf("Warning: Don't support the specified RateControl mode: %s!!!, switch to ", rc_to_string(ips.RateControlMethod));
            }

            for (i = 0; i < sizeof(rc_default_modes) / sizeof(rc_default_modes[0]); i++) {
                if (rc_default_modes[i] & tmp) {
                    ips.RateControlMethod = rc_default_modes[i];
                    break;
                }
            }

            printf("RateControl mode: %s\n", rc_to_string(ips.RateControlMethod));
        }

        config_attrib[config_attrib_num].type = VAConfigAttribRateControl;
        config_attrib[config_attrib_num].value = ips.RateControlMethod;
        config_attrib_num++;
    }


    if (attrib[VAConfigAttribEncPackedHeaders].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncPackedHeaders].value;

        printf("Support VAConfigAttribEncPackedHeaders\n");

        config_attrib[config_attrib_num].type = VAConfigAttribEncPackedHeaders;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;

        if (tmp & VA_ENC_PACKED_HEADER_SEQUENCE) {
            printf("Support packed sequence headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SEQUENCE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_PICTURE) {
            printf("Support packed picture headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_PICTURE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_SLICE) {
            printf("Support packed slice headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_SLICE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_MISC) {
            printf("Support packed misc headers\n");
            config_attrib[config_attrib_num].value |= VA_ENC_PACKED_HEADER_MISC;
        }

        config_attrib_num++;
    }

    if (attrib[VAConfigAttribEncInterlaced].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncInterlaced].value;

        printf("Support VAConfigAttribEncInterlaced\n");

        if (tmp & VA_ENC_INTERLACED_FRAME)
            printf("support VA_ENC_INTERLACED_FRAME\n");
        if (tmp & VA_ENC_INTERLACED_FIELD)
            printf("Support VA_ENC_INTERLACED_FIELD\n");
        if (tmp & VA_ENC_INTERLACED_MBAFF)
            printf("Support VA_ENC_INTERLACED_MBAFF\n");
        if (tmp & VA_ENC_INTERLACED_PAFF)
            printf("Support VA_ENC_INTERLACED_PAFF\n");

        config_attrib[config_attrib_num].type = VAConfigAttribEncInterlaced;
        config_attrib[config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        config_attrib_num++;
    }

    return 0;
}

static int setup_encode()
{
    VAStatus va_status;
    VASurfaceID *tmp_surfaceid;
    int codedbuf_size, i;

    va_status = vaCreateConfig(va_dpy, av1_profile, entryPoint,
                               &config_attrib[0], config_attrib_num, &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    /* create source surfaces */
    va_status = vaCreateSurfaces(va_dpy,
                                 VA_RT_FORMAT_YUV420, ips.frame_width_aligned, ips.frame_height_aligned,
                                 &src_surface[0], SURFACE_NUM,
                                 NULL, 0);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    /* create reference surfaces */
    va_status = vaCreateSurfaces(
                    va_dpy,
                    VA_RT_FORMAT_YUV420, ips.frame_width_aligned, ips.frame_height_aligned,
                    &ref_surface[0], SURFACE_NUM,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    tmp_surfaceid = calloc(2 * SURFACE_NUM, sizeof(VASurfaceID));
    if (tmp_surfaceid) {
        memcpy(tmp_surfaceid, src_surface, SURFACE_NUM * sizeof(VASurfaceID));
        memcpy(tmp_surfaceid + SURFACE_NUM, ref_surface, SURFACE_NUM * sizeof(VASurfaceID));
    }

    /* Create a context for this encode pipe */
    va_status = vaCreateContext(va_dpy, config_id,
                                ips.frame_width_aligned, ips.frame_height_aligned,
                                VA_PROGRESSIVE,
                                tmp_surfaceid, 2 * SURFACE_NUM,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");
    free(tmp_surfaceid);

    codedbuf_size = ((long long int) ips.frame_width_aligned * ips.frame_height_aligned * 400) / (16 * 16);

    for (i = 0; i < SURFACE_NUM; i++) {
        /* create coded buffer once for all
         * other VA buffers which won't be used again after vaRenderPicture.
         * so APP can always vaCreateBuffer for every frame
         * but coded buffer need to be mapped and accessed after vaRenderPicture/vaEndPicture
         * so VA won't maintain the coded buffer
         */
        va_status = vaCreateBuffer(va_dpy, context_id, VAEncCodedBufferType,
                                   codedbuf_size, 1, NULL, &coded_buf[i]);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
    }

    return 0;
}

static int release_encode()
{
    int i;

    vaDestroySurfaces(va_dpy, &src_surface[0], SURFACE_NUM);
    vaDestroySurfaces(va_dpy, &ref_surface[0], SURFACE_NUM);

    for (i = 0; i < SURFACE_NUM; i++)
        vaDestroyBuffer(va_dpy, coded_buf[i]);

    vaDestroyContext(va_dpy, context_id);
    vaDestroyConfig(va_dpy, config_id);

    return 0;
}

static int deinit_va()
{
    vaTerminate(va_dpy);

    va_close_display(va_dpy);

    return 0;
}

/*
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type
 */
void encoding2display_order(
    unsigned long long encoding_order, int intra_period,
    unsigned long long *displaying_order,
    int *frame_type)
{

    // simple case for IPPP av1

    *displaying_order = encoding_order;
    // all KEY FRAME *frame_type = KEY_FRAME;
    *frame_type = (encoding_order % intra_period) ? INTER_FRAME : KEY_FRAME;
    return;

}

static void
fill_sps_header()
{
    memset(&sh, 0, sizeof(sh));

    sh.seq_level_idx[0] = ips.level;

    sh.frame_width_bits = 15;
    sh.frame_height_bits = 15;
    sh.sbSize = 64;
    sh.order_hint_bits_minus1 = 8 - 1;
    sh.enable_cdef = 1;
    sh.enable_order_hint = 1;
    sh.color_config.separate_uv_delta_q = 1;

}

static void
fill_pps_header(uint64_t displaying_order)
{
    fh.show_existing_frame = 0;
    fh.frame_to_show_map_idx = 0;
    fh.frame_presentation_time = 0;
    fh.display_frame_id = 0;
    fh.frame_type = current_frame_type;
    fh.show_frame = 1;
    fh.showable_frame = 1;
    fh.error_resilient_mode = (fh.frame_type == KEY_FRAME) ? 1 : 0;
    fh.disable_cdf_update = 0;
    fh.allow_screen_content_tools = 0;
    fh.force_integer_mv = 0;
    fh.frame_size_override_flag = 0;
    fh.order_hint = displaying_order; //for I/P frame
    fh.primary_ref_frame = (fh.frame_type == KEY_FRAME || fh.frame_type == INTRA_ONLY_FRAME) ? PRIMARY_REF_NONE : 0;
    // depends on reference frames
    // by default, always refresh idx0
    fh.refresh_frame_flags = (fh.frame_type == KEY_FRAME || fh.frame_type == INTRA_ONLY_FRAME) ? 0xff : 0x01;
    fh.FrameWidth = ips.width;
    fh.FrameHeight = ips.height;
    fh.use_superres = 0;
    fh.SuperresDenom = 0;
    fh.UpscaledWidth = fh.FrameWidth;
    fh.RenderWidth = fh.FrameWidth;
    fh.RenderHeight = fh.FrameHeight;
    fh.allow_intrabc = 0;
    for(uint8_t i = 0; i < REFS_PER_FRAME; i++)
        fh.ref_frame_idx[i] = 0;
    fh.allow_high_precision_mv = 0;
    fh.interpolation_filter = 0; 
    fh.use_ref_frame_mvs = 0;
    fh.disable_frame_end_update_cdf = 0;
    fh.sbCols = ((fh.FrameWidth+63)&(~63))/64;
    fh.sbRows = ((fh.FrameHeight+63)&(~63))/64;
    fh.sbSize = 64;
    fh.tile_info.uniform_tile_spacing_flag = 1;
    fh.tile_info.context_update_tile_id = 0;
    fh.quantization_params.base_q_idx = ips.base_qindex;
    fh.quantization_params.DeltaQYDc = 0;
    fh.quantization_params.DeltaQUDc = 0;
    fh.quantization_params.DeltaQUAc = 0;
    fh.quantization_params.DeltaQVDc = 0;
    fh.quantization_params.DeltaQVAc = 0;
    fh.quantization_params.using_qmatrix = 0;
    fh.quantization_params.qm_y = 15;
    fh.quantization_params.qm_u = 15;
    fh.quantization_params.qm_v = 15;
    fh.delta_q_present = 0;
    fh.delta_q_res = 0;
    fh.delta_lf_present = 1;
    fh.delta_lf_res = 0;
    fh.delta_lf_multi = 1;
    fh.CodedLossless = 0;
    fh.AllLossless = 0;
    fh.cdef_params.cdef_damping = 5;
    fh.cdef_params.cdef_bits = 3;
    fh.cdef_params.cdef_y_pri_strength[0] = 9;
    fh.cdef_params.cdef_y_sec_strength[0] = 0;
    fh.cdef_params.cdef_uv_pri_strength[0] = 9;
    fh.cdef_params.cdef_uv_sec_strength[0] = 0;

    fh.cdef_params.cdef_y_pri_strength[1] = 12;
    fh.cdef_params.cdef_y_sec_strength[1] = 2;
    fh.cdef_params.cdef_uv_pri_strength[1] = 12;
    fh.cdef_params.cdef_uv_sec_strength[1] = 2;

    fh.cdef_params.cdef_y_pri_strength[2] = 0;
    fh.cdef_params.cdef_y_sec_strength[2] = 0;
    fh.cdef_params.cdef_uv_pri_strength[2] = 0;
    fh.cdef_params.cdef_uv_sec_strength[2] = 0;

    fh.cdef_params.cdef_y_pri_strength[3] = 6;
    fh.cdef_params.cdef_y_sec_strength[3] = 0;
    fh.cdef_params.cdef_uv_pri_strength[3] = 6;
    fh.cdef_params.cdef_uv_sec_strength[3] = 0;

    fh.cdef_params.cdef_y_pri_strength[4] = 2;
    fh.cdef_params.cdef_y_sec_strength[4] = 0;
    fh.cdef_params.cdef_uv_pri_strength[4] = 2;
    fh.cdef_params.cdef_uv_sec_strength[4] = 0;

    fh.cdef_params.cdef_y_pri_strength[5] = 4;
    fh.cdef_params.cdef_y_sec_strength[5] = 1;
    fh.cdef_params.cdef_uv_pri_strength[5] = 4;
    fh.cdef_params.cdef_uv_sec_strength[5] = 1;

    fh.cdef_params.cdef_y_pri_strength[6] = 1;
    fh.cdef_params.cdef_y_sec_strength[6] = 0;
    fh.cdef_params.cdef_uv_pri_strength[6] = 1;
    fh.cdef_params.cdef_uv_sec_strength[6] = 0;

    fh.cdef_params.cdef_y_pri_strength[7] = 2;
    fh.cdef_params.cdef_y_sec_strength[7] = 1;
    fh.cdef_params.cdef_uv_pri_strength[7] = 2;
    fh.cdef_params.cdef_uv_sec_strength[7] = 1;

    fh.loop_filter_params.loop_filter_level[0] = 15;
    fh.loop_filter_params.loop_filter_level[1] = 15;
    fh.loop_filter_params.loop_filter_level[2] = 8;
    fh.loop_filter_params.loop_filter_level[3] = 8;

    fh.loop_filter_params.loop_filter_sharpness = 0;
    fh.loop_filter_params.loop_filter_delta_enabled = 0;
    fh.loop_filter_params.loop_filter_delta_update = 0;

    fh.TxMode = TX_MODE_SELECT;
    fh.reference_select = ips.LDB ? 1 : 0;
    fh.skipModeAllowed = 0;
    fh.skipModeFrame[0] = 0;
    fh.skipModeFrame[1] = 0;
    fh.allow_warped_motion = 0;
    fh.reduced_tx_set = 1;
}

// brief interface with va, render bitstream
static void
va_render_packed_data(bitstream* bs)
{
    CHECK_BS_NULL(bs);
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packed_para_bufid = VA_INVALID_ID;
    VABufferID packed_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits = bs->bit_offset;

    unsigned char *packedpic_buffer = bs->buffer;
    VAStatus va_status;

    packedheader_param_buffer.type = VAEncPackedHeaderPicture; 
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packed_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(va_dpy,
                               context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedpic_buffer,
                               &packed_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packed_para_bufid;
    render_id[1] = packed_data_bufid;
    va_status = vaRenderPicture(va_dpy, context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    //free(packedpic_buffer); free outside this function call

    if (packed_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packed_para_bufid);
        packed_para_bufid = VA_INVALID_ID;
    }
    if (packed_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, packed_data_bufid);
        packed_data_bufid = VA_INVALID_ID;
    }
}

static void
render_ivf_frame_header()
{
    // write 12 empty byte to driver
    // IVF frame header is filled after encoding
    // first 4 byte is u32 for bit stream length
    // last 8 byte is u64 for display order
    bitstream bs;
    bitstream_start(&bs);

    for (size_t i = 0; i < 12; i++)
    {
        put_ui(&bs, 0x00, 8);
    }

    va_render_packed_data(&bs);

    bitstream_free(&bs);
}

static void
render_ivf_header()
{
    bitstream bs;
    bitstream_start(&bs);
    uint32_t ivfSeqHeader[11] = {0x46494B44, 0x00200000, 0x31305641,
    (uint32_t)(fh.UpscaledWidth + (fh.FrameHeight << 16)),
    ips.frame_rate_extN,// FrameRateExtN
    ips.frame_rate_extD,// FrameRateExtD
    ips.frame_count/*numFramesInFile*/,
    0x00000000,
    0,0,0 };

    // 
    uint8_t* hdr = (uint8_t*) ivfSeqHeader;
    for (size_t i = 0; i < 44; i++)
    {
        put_ui(&bs, hdr[i], 8);
    }

    va_render_packed_data(&bs);

    bitstream_free(&bs);
}

static void
render_TD()
{
    // OBU_TEMPORAL_DELIMITER
    bitstream bs;
    bitstream_start(&bs);
    put_ui(&bs, 0x12, 8);
    put_ui(&bs, 0x00, 8);

    va_render_packed_data(&bs);

    bitstream_free(&bs);
}

static void
build_sps_buffer(VAEncSequenceParameterBufferAV1* sps)
{
    CHECK_NULL(sps);

    sps->seq_profile = (uint8_t)sh.seq_profile;
    sps->seq_level_idx = (uint8_t)(sh.seq_level_idx[0]);

    sps->intra_period = ips.intra_period;
    sps->ip_period    = ips.ip_period;

    if (ips.RateControlMethod == VA_RC_CBR)
    {
        sps->bits_per_second = ips.target_bitrate;
    }
    else if (ips.RateControlMethod == VA_RC_VBR)
    {
        sps->bits_per_second = ips.vbr_max_bitrate;
    }

    sps->order_hint_bits_minus_1 = (uint8_t)(sh.order_hint_bits_minus1);

    sps->seq_fields.bits.still_picture              = sh.still_picture;
    sps->seq_fields.bits.enable_filter_intra        = sh.enable_filter_intra;
    sps->seq_fields.bits.enable_intra_edge_filter   = sh.enable_intra_edge_filter;
    sps->seq_fields.bits.enable_interintra_compound = sh.enable_interintra_compound;
    sps->seq_fields.bits.enable_masked_compound     = sh.enable_masked_compound;
    sps->seq_fields.bits.enable_warped_motion       = sh.enable_warped_motion;
    sps->seq_fields.bits.enable_dual_filter         = sh.enable_dual_filter;
    sps->seq_fields.bits.enable_order_hint          = sh.enable_order_hint;
    sps->seq_fields.bits.enable_jnt_comp            = sh.enable_jnt_comp;
    sps->seq_fields.bits.enable_ref_frame_mvs       = sh.enable_ref_frame_mvs;
    sps->seq_fields.bits.enable_superres            = sh.enable_superres;
    sps->seq_fields.bits.enable_cdef                = sh.enable_cdef;
    sps->seq_fields.bits.enable_restoration         = sh.enable_restoration;
    sps->seq_fields.bits.bit_depth_minus8           = ips.bit_depth - 8;
    sps->seq_fields.bits.subsampling_x              = 0;
    sps->seq_fields.bits.subsampling_y              = 0;
}

static void
render_sequence()
{
    VABufferID seq_param_buf_id = VA_INVALID_ID;
    VAStatus va_status;

    VAEncSequenceParameterBufferAV1 sps_buffer;
    memset(&sps_buffer, 0, sizeof(sps_buffer));
    build_sps_buffer(&sps_buffer);

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncSequenceParameterBufferType,
                               sizeof(sps_buffer), 1, &sps_buffer, &seq_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &seq_param_buf_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (seq_param_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, seq_param_buf_id);
        seq_param_buf_id = VA_INVALID_ID;
    }
}

static void
render_rc_buffer()
{
    VABufferID rc_param_buf = VA_INVALID_ID;
    VAStatus va_status;
    VABufferID render_id = VA_INVALID_ID;

    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *misc_rate_ctrl;

    va_status = vaCreateBuffer(va_dpy, context_id,
                            VAEncMiscParameterBufferType,
                            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                            1, NULL, &rc_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(va_dpy, rc_param_buf, (void **)&misc_param);
    misc_param->type = VAEncMiscParameterTypeRateControl;
    misc_rate_ctrl = (VAEncMiscParameterRateControl *)misc_param->data;
    memset(misc_rate_ctrl, 0, sizeof(*misc_rate_ctrl));
    if (ips.RateControlMethod == VA_RC_CBR)
    {
        misc_rate_ctrl->bits_per_second = ips.target_bitrate;
    }
    else if (ips.RateControlMethod == VA_RC_VBR)
    {
        misc_rate_ctrl->bits_per_second = ips.vbr_max_bitrate;
        if (ips.target_bitrate != 0)
        {
            misc_rate_ctrl->target_percentage = MIN(100, (uint32_t) (100.0 * ips.target_bitrate / ips.vbr_max_bitrate));
        }
    }

    vaUnmapBuffer(va_dpy, rc_param_buf);

    render_id = rc_param_buf;

    va_status = vaRenderPicture(va_dpy, context_id, &render_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (rc_param_buf != VA_INVALID_ID) 
    {
        vaDestroyBuffer(va_dpy, rc_param_buf);
        rc_param_buf = VA_INVALID_ID;
    }
}

static void
render_hrd_buffer()
{
    VABufferID param_buf = VA_INVALID_ID;
    VAStatus va_status;
    VABufferID render_id = VA_INVALID_ID;

    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *misc_hrd;

    va_status = vaCreateBuffer(va_dpy, context_id,
                            VAEncMiscParameterBufferType,
                            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
                            1, NULL, &param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(va_dpy, param_buf, (void **)&misc_param);
    misc_param->type = VAEncMiscParameterTypeHRD;
    misc_hrd = (VAEncMiscParameterHRD *)misc_param->data;
    memset(misc_hrd, 0, sizeof(*misc_hrd));

    misc_hrd->initial_buffer_fullness = ips.initial_buffer_fullness;
    misc_hrd->buffer_size = ips.buffer_size;

    vaUnmapBuffer(va_dpy, param_buf);

    render_id = param_buf;

    va_status = vaRenderPicture(va_dpy, context_id, &render_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (param_buf != VA_INVALID_ID) 
    {
        vaDestroyBuffer(va_dpy, param_buf);
        param_buf = VA_INVALID_ID;
    }
}

static void
render_fr_buffer()
{
    VABufferID param_buf = VA_INVALID_ID;
    VAStatus va_status;
    VABufferID render_id = VA_INVALID_ID;

    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *misc_fr;

    va_status = vaCreateBuffer(va_dpy, context_id,
                            VAEncMiscParameterBufferType,
                            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                            1, NULL, &param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(va_dpy, param_buf, (void **)&misc_param);
    misc_param->type = VAEncMiscParameterTypeFrameRate;
    misc_fr = (VAEncMiscParameterFrameRate *)misc_param->data;
    memset(misc_fr, 0, sizeof(*misc_fr));

    misc_fr->framerate = ips.frame_rate_extN | (ips.frame_rate_extD << 16);

    vaUnmapBuffer(va_dpy, param_buf);

    render_id = param_buf;

    va_status = vaRenderPicture(va_dpy, context_id, &render_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (param_buf != VA_INVALID_ID) 
    {
        vaDestroyBuffer(va_dpy, param_buf);
        param_buf = VA_INVALID_ID;
    }
}

static void
render_misc_buffer()
{
    render_rc_buffer();
    if (ips.buffer_size != 0 || ips.initial_buffer_fullness != 0)
    {
        render_hrd_buffer();
    }
    render_fr_buffer();
}

static void
render_tile_group()
{
    VABufferID tile_param_buf_id = VA_INVALID_ID;
    VAStatus va_status;

    VAEncTileGroupBufferAV1 tile_group_buffer = {0}; //default setting

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncSliceParameterBufferType,
                               sizeof(tile_group_buffer), 1, &tile_group_buffer, &tile_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &tile_param_buf_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (tile_param_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, tile_param_buf_id);
        tile_param_buf_id = VA_INVALID_ID;
    }
}

static void
pack_obu_header(bitstream *bs, int obu_type, uint32_t obu_extension_flag)
{
    put_ui(bs, 0, 1); //obu_forbidden_bit
    put_ui(bs, obu_type, 4); //type
    put_ui(bs, obu_extension_flag, 1);
    put_ui(bs, 1, 1); //obu_has_size_field
    put_ui(bs, 0, 1); //reserved

    if (obu_extension_flag) {
        //  Obu Extension Header
        // not written to bitstream by default
    }
}

static void
pack_obu_header_size(bitstream *bs, 
                    uint32_t value,
                    uint8_t fixed_output_len)
{
    // pack leb128
    uint64_t leb128_buf = 0;
    uint8_t leb128_size = 0;

    uint8_t* buf = (uint8_t*)(&leb128_buf);

    if (!fixed_output_len)
    {
        // general encoding
        do {
            buf[leb128_size] = value & 0x7fU;
            if (value >>= 7)
            {
                buf[leb128_size] |= 0x80U;
            }
            leb128_size++;
        } while (value);
    }
    else
    {
        // get fixed len of output, in this case, LEB128 will have fixed length
        uint8_t value_byte_count = 0;
        do {
            buf[value_byte_count++] = value & 0x7fU;
            value >>= 7;
        } while (value);

        for (int i = 0; i < fixed_output_len - 1; i++)
        {
            buf[i] |= 0x80U;
            leb128_size++;
        }
        leb128_size++;
    }

    // write to bitstream
    const uint8_t* ptr = (uint8_t*)(&leb128_buf);
    for (uint8_t i = 0; i < leb128_size; i++)
        put_ui(bs, ptr[i], 8);
}

static void
pack_operating_points(bitstream* bs)
{
    put_ui(bs, sh.operating_points_cnt_minus_1, 5);
    
    for(uint8_t i = 0;i <= sh.operating_points_cnt_minus_1;i++)
    {
        //put_ui(bs, sh.operating_point_idc[i], 12);
        put_ui(bs, sh.operating_point_idc[i] >> 4, 8);
        put_ui(bs, sh.operating_point_idc[i] & 0x9f, 4);
        
        put_ui(bs, sh.seq_level_idx[i], 5);
        if(sh.seq_level_idx[i]>7)
            put_ui(bs, sh.seq_tier[i], 1);
    }
}

static void
pack_frame_size_info(bitstream* bs)
{
    //pack frame size info
    put_ui(bs, 15, 4);//frame_width_bits_minus_1
    put_ui(bs, 15, 4);//frame_height_bits_minus_1
    put_ui(bs, fh.UpscaledWidth - 1, 16);//max_frame_width_minus_1
    put_ui(bs, fh.FrameHeight - 1, 16);//max_frame_height_minus_1
    // end of pack frame size info
}

static void
pack_seq_data(bitstream *bs)
{
    put_ui(bs, sh.seq_profile, 3);
    put_ui(bs, sh.still_picture, 1);
    put_ui(bs, 0, 1);//reduced_still_picture_header
    put_ui(bs, 0, 1);//timing_info_present_flag
    put_ui(bs, 0, 1);//initial_display_delay_present_flag
    pack_operating_points(bs);

    pack_frame_size_info(bs);


    put_ui(bs, 0, 1);//frame_id_numbers_present_flag (affects FH)

    put_ui(bs, 0, 1);//use_128x128_superblock
    put_ui(bs, sh.enable_filter_intra, 1);//enable_filter_intra
    put_ui(bs, sh.enable_intra_edge_filter, 1);//enable_intra_edge_filter
    put_ui(bs, sh.enable_interintra_compound, 1);//enable_interintra_compound
    put_ui(bs, sh.enable_masked_compound, 1);//enable_masked_compound
    put_ui(bs, sh.enable_warped_motion, 1);//enable_warped_motion
    put_ui(bs, sh.enable_dual_filter, 1);//enable_dual_filter
    put_ui(bs, sh.enable_order_hint, 1);//enable_order_hint

    if (sh.enable_order_hint)
    {
        put_ui(bs, 0, 1); //enable_jnt_comp
        put_ui(bs, fh.use_ref_frame_mvs, 1);//enable_ref_frame_mvs
    }

    put_ui(bs, 1, 1);//seq_choose_screen_content_tools
    put_ui(bs, sh.seq_force_integer_mv, 1);//seq_choose_integer_mv

    if (!sh.seq_force_integer_mv)
    {
        put_ui(bs, 0, 1); //seq_force_integer_mv
    }

    if (sh.enable_order_hint)
    {
        put_ui(bs, sh.order_hint_bits_minus1, 3);//affects FH
    }

    put_ui(bs, sh.enable_superres, 1);//enable_superres
    put_ui(bs, sh.enable_cdef, 1);//enable_cdef
    put_ui(bs, sh.enable_restoration, 1);//enable_restoration

    // pack color config
    put_ui(bs, sh.color_config.BitDepth == BITDEPTH_10 ? 1 : 0, 1);
    if (sh.seq_profile != 1)
        put_ui(bs, 0, 1);; //mono_chrome
        
    put_ui(bs, sh.color_config.color_description_present_flag, 1);

    if (sh.color_config.color_description_present_flag)
    {
        put_ui(bs, sh.color_config.color_primaries, 8);
        put_ui(bs, sh.color_config.transfer_characteristics, 8);
        put_ui(bs, sh.color_config.matrix_coefficients, 8);
    }

    put_ui(bs, sh.color_config.color_range, 1);//color_range

    if (sh.seq_profile == 0)
        put_ui(bs, 0, 2); //chroma_sample_position

    put_ui(bs, sh.color_config.separate_uv_delta_q, 1); //separate_uv_delta_q

    put_ui(bs, 0, 1);//film_grain_params_present

    put_trailing_bits(bs);
}

static void
build_packed_seq_header(bitstream* bs)
{
    CHECK_BS_NULL(bs);
    CHECK_CONDITION(bs->bit_offset == 0);
    // handle vairable length
    // seq obu data
    bitstream obu_data;
    bitstream_start(&obu_data);
    pack_seq_data(&obu_data);

    uint32_t obu_extension_flag = sh.operating_points_cnt_minus_1 ? 1 : 0;
    pack_obu_header(bs, OBU_SEQUENCE_HEADER, obu_extension_flag);

    //calculate data size
    uint32_t obu_size_in_bytes = (obu_data.bit_offset + 7) / 8;
    pack_obu_header_size(bs, obu_size_in_bytes, 0);
    
    bitstream_cat(bs, &obu_data);
}

static int
render_packedsequence()
{
    int len;
    bitstream bs;
    bitstream_start(&bs);

    build_packed_seq_header(&bs);

    len = (bs.bit_offset + 7) / 8;

    va_render_packed_data(&bs);

    bitstream_free(&bs);

    return len;
}

static void
pack_show_existing_frame(bitstream* bs)
{
    return; //only for B frame, not enable by default
}

static void
pack_show_frame(bitstream* bs)
{
    put_ui(bs, fh.show_frame, 1);
    if(!fh.show_frame)
        put_ui(bs, fh.showable_frame, 1);
}

static void
pack_error_resilient(bitstream* bs)
{
    if (!(fh.frame_type == SWITCH_FRAME || (fh.frame_type == KEY_FRAME && fh.show_frame)))
        put_ui(bs, 0, 1); //error_resilient_mode
}

static void
pack_ref_frame_flags(bitstream* bs, uint8_t error_resilient_mode, uint8_t isI)
{
    if(!(isI || error_resilient_mode))
        put_ui(bs, 0, 3); //primary_ref_frame
    if (!(fh.frame_type == SWITCH_FRAME || (fh.frame_type == KEY_FRAME && fh.show_frame)))
        put_ui(bs, fh.refresh_frame_flags, NUM_REF_FRAMES);
}

static void
pack_interpolation_filter(bitstream* bs)
{
    const uint8_t is_filter_switchable = (fh.interpolation_filter == 4 ? 1 : 0);
    put_ui(bs, is_filter_switchable, 1);//is_filter_switchable
    if (!is_filter_switchable)
    {
        put_ui(bs, fh.interpolation_filter, 2);//interpolation_filter
    }
}

static void
pack_render_size(bitstream* bs)
{
    uint32_t render_and_frame_size_different = 0;

    put_ui(bs, render_and_frame_size_different, 1);//render_and_frame_size_different
}

static void
pack_frame_size(bitstream *bs)
{
    if (fh.frame_size_override_flag)
    {
        put_ui(bs, fh.UpscaledWidth - 1, sh.frame_width_bits + 1); //frame_width_minus_1
        put_ui(bs, fh.FrameHeight - 1, sh.frame_height_bits + 1); //frame_height_minus_1
    }

}

static void
pack_frame_size_with_refs(bitstream* bs)
{
    CHECK_BS_NULL(bs);
    uint32_t found_ref = 0;

    for (int8_t ref = 0; ref < REFS_PER_FRAME; ref++)
        put_ui(bs, found_ref, 1);//found_ref

    // if found_ref == 9
    pack_frame_size(bs);
    pack_render_size(bs);


}

static void
pack_frame_ref_info(bitstream* bs, uint8_t error_resilient_mode)
{
    if (sh.enable_order_hint)
        put_ui(bs, 0, 1); //frame_refs_short_signaling

    for (uint8_t ref = 0; ref < REFS_PER_FRAME; ref++)
        put_ui(bs, fh.ref_frame_idx[ref], REF_FRAMES_LOG2);

    if (fh.frame_size_override_flag && !error_resilient_mode)
    {
        pack_frame_size_with_refs(bs);
    }
    else
    {
        pack_frame_size(bs);
        pack_render_size(bs);
    }

    put_ui(bs, fh.allow_high_precision_mv, 1); //allow_high_precision_mv

    //PackInterpolationFilter(bs, fh);
    pack_interpolation_filter(bs);

    put_ui(bs, 0, 1);//is_motion_switchable

    if (fh.use_ref_frame_mvs)
        put_ui(bs, 1, 1); //use_ref_frame_mvs
}

static void
pack_tile_info(bitstream* bs)
{
    // use single tile by default
    put_ui(bs, 1, 1);//uniform_tile_spacing_flag
    put_ui(bs, 0, 1);//increment_tile_cols_log2
    put_ui(bs, 0, 1);//increment_tile_rows_log2
}

static uint16_t
write_SU(int32_t value, uint16_t n)
{
    int16_t signMask = 1 << (n - 1);
    if (value & signMask)
    {
        value = value - 2 * signMask;
    }
    return value;
}

static void
pack_delta_q_value(bitstream* bs, int32_t deltaQ)
{
    if (deltaQ)
    {
        put_ui(bs, 1, 1);
        put_ui(bs, write_SU(deltaQ, 7), 7);
    }
    else
        put_ui(bs, 0, 1);
}

static void
pack_quantization_params(bitstream* bs)
{
    put_ui(bs, fh.quantization_params.base_q_idx, 8); //base_q_idx

    pack_delta_q_value(bs, fh.quantization_params.DeltaQYDc);

    bool diff_uv_delta = false;
    if (fh.quantization_params.DeltaQUDc != fh.quantization_params.DeltaQVDc
        || fh.quantization_params.DeltaQUAc != fh.quantization_params.DeltaQVAc)
        diff_uv_delta = true;

    if (sh.color_config.separate_uv_delta_q)
        put_ui(bs, diff_uv_delta, 1);

    pack_delta_q_value(bs, fh.quantization_params.DeltaQUDc);
    pack_delta_q_value(bs, fh.quantization_params.DeltaQUAc);

    if (diff_uv_delta)
    {
        pack_delta_q_value(bs, fh.quantization_params.DeltaQVDc);
        pack_delta_q_value(bs, fh.quantization_params.DeltaQVAc);
    }

    put_ui(bs, fh.quantization_params.using_qmatrix, 1);//using_qmatrix
    if (fh.quantization_params.using_qmatrix)
    {
        put_ui(bs, fh.quantization_params.qm_y, 4);
        put_ui(bs, fh.quantization_params.qm_u, 4);
        if (sh.color_config.separate_uv_delta_q)
            put_ui(bs, fh.quantization_params.qm_v, 4);
    }
}

static void
pack_loop_filter_params(bitstream* bs)
{
    if (fh.CodedLossless || fh.allow_intrabc)
        return;
    
    put_ui(bs, fh.loop_filter_params.loop_filter_level[0], 6);//loop_filter_level[0]
    put_ui(bs, fh.loop_filter_params.loop_filter_level[1], 6);//loop_filter_level[1]

    if (fh.loop_filter_params.loop_filter_level[0] || fh.loop_filter_params.loop_filter_level[1])
    {

        put_ui(bs, fh.loop_filter_params.loop_filter_level[2], 6);//loop_filter_level[2]
        put_ui(bs, fh.loop_filter_params.loop_filter_level[3], 6);//loop_filter_level[3]
    }

    put_ui(bs, fh.loop_filter_params.loop_filter_sharpness, 3); //loop_filter_sharpness
    put_ui(bs, 0, 1); //loop_filter_delta_enabled

}

static void
pack_cdef_params(bitstream* bs)
{
    if (!sh.enable_cdef || fh.CodedLossless || fh.allow_intrabc)
        return;

    uint16_t num_planes = sh.color_config.mono_chrome ? 1 : 3;

    put_ui(bs, fh.cdef_params.cdef_damping - 3, 2);//cdef_damping_minus_3
    put_ui(bs, fh.cdef_params.cdef_bits, 2);//cdef_bits

    for (uint16_t i = 0; i < (1 << fh.cdef_params.cdef_bits); ++i)
    {
        put_ui(bs, fh.cdef_params.cdef_y_pri_strength[i], 4);//cdef_y_pri_strength[0]
        put_ui(bs, fh.cdef_params.cdef_y_sec_strength[i], 2);//cdef_y_sec_strength[0]

        if (num_planes > 1)
        {
            put_ui(bs, fh.cdef_params.cdef_uv_pri_strength[i], 4);//cdef_uv_pri_strength[0]
            put_ui(bs, fh.cdef_params.cdef_uv_sec_strength[i], 2);//cdef_uv_sec_strength[0]
        }
    }
}

static void
pack_lr_params(bitstream* bs)
{
    if (fh.AllLossless || fh.allow_intrabc || !sh.enable_restoration)
        return;

    bool usesLR = false;
    bool usesChromaLR = false;

    for (int i = 0; i < MAX_MB_PLANE; i++)
    {
        put_ui(bs, fh.lr_params.lr_type[i], 2);
        if (fh.lr_params.lr_type[i] != RESTORE_NONE)
        {
            usesLR = true;
            if (i > 0)
            {
                usesChromaLR = true;
            }
        }
    }

    if (usesLR)
    {
        put_ui(bs, fh.lr_params.lr_unit_shift, 1);

        if (sh.sbSize != 1 && fh.lr_params.lr_unit_shift) 
        {
            put_ui(bs, fh.lr_params.lr_unit_extra_shift, 1);
        }

        if (sh.color_config.subsampling_x && sh.color_config.subsampling_y && usesChromaLR)
        {
            put_ui(bs, fh.lr_params.lr_uv_shift, 1);
        }
    }
}

static void
pack_delta_q_params(bitstream* bs)
{
    if (fh.quantization_params.base_q_idx)
        put_ui(bs, fh.delta_q_present, 1); //delta_q_present
    if (fh.delta_q_present)
    {
        put_ui(bs, 0, 2); //delta_q_res
        put_ui(bs, fh.delta_lf_present, 1); //delta_lf_present
        put_ui(bs, 0, 2); //delta_lf_res
        put_ui(bs, fh.delta_lf_multi, 1); //delta_lf_multi
    }
}

static void
pack_frame_reference_mode(bitstream* bs, bool frameIsIntra)
{
    if (frameIsIntra)
        return;
    put_ui(bs, fh.reference_select, 1); //reference_select
}

static void
pack_skip_mode_params(bitstream* bs)
{
    if (fh.skipModeAllowed)
        put_ui(bs, fh.skip_mode_present, 1); //skip_mode_present
}

static void
pack_wrapped_motion(bitstream* bs, bool frameIsIntra)
{
    if (frameIsIntra)
        return;

    if (sh.enable_warped_motion)
        put_ui(bs, 0, 1); //allow_warped_motion
}

static void
pack_global_motion_params(bitstream* bs, bool frameIsIntra)
{
    if (frameIsIntra)
        return;

    for (uint8_t i = LAST_FRAME; i <= ALTREF_FRAME; i++)
        put_ui(bs, 0, 1); //is_global[7]
}

static void
pack_frame_header(bitstream* bs)
{
    const uint8_t isI = (fh.frame_type == INTRA_ONLY_FRAME || fh.frame_type == KEY_FRAME);

    put_ui(bs, fh.frame_type, 2);

    pack_show_frame(bs);

    uint8_t error_resilient_mode = 0;
    pack_error_resilient(bs);
    
    put_ui(bs, fh.disable_cdf_update, 1);
    put_ui(bs, fh.allow_screen_content_tools, 1);
    put_ui(bs, fh.frame_size_override_flag, 1);

    // pack order hint
    if(sh.enable_order_hint)
        put_ui(bs, fh.order_hint, sh.order_hint_bits_minus1 + 1);

    // PackRefFrameFlags
    pack_ref_frame_flags(bs, error_resilient_mode, isI);

    if (!isI)
    {
        pack_frame_ref_info(bs, error_resilient_mode);
    }
    else
    {
        pack_frame_size(bs);
        pack_render_size(bs);
        if (fh.allow_screen_content_tools && fh.UpscaledWidth == fh.FrameWidth)
            put_ui(bs, fh.allow_intrabc, 1);
    }

    if (!fh.disable_cdf_update)
        put_ui(bs, fh.disable_frame_end_update_cdf, 1); //disable_frame_end_update_cdf

    pack_tile_info(bs);

    //quantization_params
    offsets.QIndexBitOffset = bs->bit_offset;
    pack_quantization_params(bs);

    //segmentation_params
    offsets.SegmentationBitOffset = bs->bit_offset;
    put_ui(bs, 0, 1); //segmentation_enabled

    offsets.SegmentationBitSize = bs->bit_offset - offsets.SegmentationBitOffset;

    pack_delta_q_params(bs);

    offsets.LoopFilterParamsBitOffset = bs->bit_offset;
    pack_loop_filter_params(bs);

    offsets.CDEFParamsBitOffset = bs->bit_offset;
    pack_cdef_params(bs);
    offsets.CDEFParamsSizeInBits = bs->bit_offset - offsets.CDEFParamsBitOffset;

    pack_lr_params(bs);

    const uint8_t tx_mode_select = fh.TxMode ? 1 : 0;
   
    if (!fh.CodedLossless)
    put_ui(bs, tx_mode_select, 1); //tx_mode_select

    pack_frame_reference_mode(bs, isI);
    pack_skip_mode_params(bs);
    pack_wrapped_motion(bs, isI);

    put_ui(bs, fh.reduced_tx_set, 1); //reduced_tx_set

    pack_global_motion_params(bs, isI);
}

static void
build_packed_pic_header(bitstream* bs)
{
    // handle vairable length
    // pack obu payload and then put header

    // pic obu data
    bitstream tmp;
    bitstream_start(&tmp);

    put_ui(&tmp, fh.show_existing_frame, 1); //show_existing_frame
    if (fh.show_existing_frame)
        pack_show_existing_frame(&tmp); // only for B frame
    else
        pack_frame_header(&tmp);

    offsets.FrameHdrOBUSizeInBits = tmp.bit_offset;

    const uint32_t obu_extension_flag = sh.operating_points_cnt_minus_1 ? 1 : 0;
    const uint32_t obu_header_offset  = bs->bit_offset;

    put_aligning_bits(&tmp);

    pack_obu_header(bs, OBU_FRAME, obu_extension_flag);

    offsets.FrameHdrOBUSizeByteOffset = (bs->bit_offset >> 3) + len_ivf_header + 2 + len_seq_header; // first frame with IVF header

    const uint32_t obu_size_in_bytes = (tmp.bit_offset + 7) / 8;
    pack_obu_header_size(bs, obu_size_in_bytes, fh.show_existing_frame? 0: 4);

    if (!fh.show_existing_frame)
    {
        // The offset is related to frame or frame header OBU. IVF, sequence, and other headers should not be counted.
        const uint32_t obuPayloadOffset     = bs->bit_offset - obu_header_offset;
        offsets.QIndexBitOffset           += obuPayloadOffset;
        offsets.SegmentationBitOffset     += obuPayloadOffset;
        offsets.LoopFilterParamsBitOffset += obuPayloadOffset;
        offsets.CDEFParamsBitOffset       += obuPayloadOffset;
        //offsets.CDEFParamsSizeInBits is not needed to be updated.
        offsets.FrameHdrOBUSizeInBits     += obuPayloadOffset;
    }

    bitstream_cat(bs, &tmp);
}

static void
render_packedpicture()
{
    bitstream bs;
    bitstream_start(&bs);

    build_packed_pic_header(&bs);

    va_render_packed_data(&bs);

    bitstream_free(&bs);
}

static void
fill_ref_params(VAEncPictureParameterBufferAV1* pps)
{
    CHECK_NULL(pps);
    if(fh.frame_type == KEY_FRAME)
    {
        return;
    }
    else if(fh.frame_type == INTER_FRAME)
    {
        // use last frame as reference
        pps->reference_frames[0] = ref_surface[((current_frame_display - 1) % SURFACE_NUM)];
        pps->ref_frame_ctrl_l0.fields.search_idx0 = LAST_FRAME;

        // for Low delay B
        pps->ref_frame_ctrl_l1.fields.search_idx0 = ips.LDB ? BWDREF_FRAME : INTRA_FRAME;
    }
}

static void
build_pps_buffer(VAEncPictureParameterBufferAV1* pps)
{
    CHECK_NULL(pps);
    // InitPPS
    memset(&(pps->reference_frames), VA_INVALID_ID, 8);

    //frame size
    pps->frame_height_minus_1  =(uint16_t)(fh.FrameHeight - 1);
    pps->frame_width_minus_1 = (uint16_t)(fh.UpscaledWidth - 1);

    //bitstream
    pps->coded_buf = coded_buf[current_slot];
    
    for(int k = 0; k < REFS_PER_FRAME; k++)
        pps->ref_frame_idx[k] = 0;

    pps->picture_flags.bits.use_ref_frame_mvs = fh.use_ref_frame_mvs;
    pps->picture_flags.bits.long_term_reference = 0;
    pps->picture_flags.bits.allow_intrabc = fh.allow_intrabc;

    pps->seg_id_block_size = 0;

    //quantizer
    pps->y_dc_delta_q = (uint8_t)(fh.quantization_params.DeltaQYDc);
    pps->u_dc_delta_q = (uint8_t)(fh.quantization_params.DeltaQUDc);
    pps->u_ac_delta_q = (uint8_t)(fh.quantization_params.DeltaQUAc);
    pps->v_dc_delta_q = (uint8_t)(fh.quantization_params.DeltaQVDc);
    pps->v_ac_delta_q = (uint8_t)(fh.quantization_params.DeltaQVAc);

    //other params
    pps->picture_flags.bits.error_resilient_mode = fh.error_resilient_mode;
    pps->interpolation_filter = (uint8_t)(fh.interpolation_filter);
    pps->picture_flags.bits.use_superres = fh.use_superres;
    pps->picture_flags.bits.allow_high_precision_mv = fh.allow_high_precision_mv;
    pps->picture_flags.bits.reduced_tx_set = fh.reduced_tx_set;
    pps->picture_flags.bits.palette_mode_enable = fh.allow_screen_content_tools;

    //tx_mod
    pps->mode_control_flags.bits.tx_mode = fh.TxMode;
    pps->temporal_id = 0;
    pps->superres_scale_denominator = (uint8_t)(fh.SuperresDenom);

    //q_matrix
    pps->qmatrix_flags.bits.using_qmatrix = fh.quantization_params.using_qmatrix;
    pps->qmatrix_flags.bits.qm_y = fh.quantization_params.qm_y;
    pps->qmatrix_flags.bits.qm_u = fh.quantization_params.qm_u;
    pps->qmatrix_flags.bits.qm_v = fh.quantization_params.qm_v;

    pps->picture_flags.bits.frame_type = fh.frame_type;
    pps->base_qindex     = (uint8_t)fh.quantization_params.base_q_idx;
    pps->min_base_qindex = ips.MinBaseQIndex;
    pps->max_base_qindex = ips.MaxBaseQIndex;

    pps->order_hint = (uint8_t)(fh.order_hint);

    //reference frame
    pps->primary_ref_frame = (uint8_t)(fh.primary_ref_frame);
    pps->ref_frame_ctrl_l0.value = 0;
    pps->ref_frame_ctrl_l1.value = 0;
    fill_ref_params(pps);
    pps->refresh_frame_flags = fh.refresh_frame_flags;

    // //loop filter  
    // auto& lf = fh.loop_filter_params;
    pps->filter_level[0] = (uint8_t)(fh.loop_filter_params.loop_filter_level[0]);
    pps->filter_level[1] = (uint8_t)(fh.loop_filter_params.loop_filter_level[1]);
    pps->filter_level_u  = (uint8_t)(fh.loop_filter_params.loop_filter_level[2]);
    pps->filter_level_v  = (uint8_t)(fh.loop_filter_params.loop_filter_level[3]);
    pps->loop_filter_flags.bits.sharpness_level        = fh.loop_filter_params.loop_filter_sharpness;
    pps->loop_filter_flags.bits.mode_ref_delta_enabled = fh.loop_filter_params.loop_filter_delta_enabled;
    pps->loop_filter_flags.bits.mode_ref_delta_update  = fh.loop_filter_params.loop_filter_delta_update;

    for(int k = 0;k < 8;k++)
        pps->ref_deltas[k] = 0;

    //block-level deltas
    pps->mode_control_flags.bits.delta_q_present  = fh.delta_q_present;
    pps->mode_control_flags.bits.delta_q_res  = fh.delta_q_res;
    pps->mode_control_flags.bits.delta_lf_res  = fh.delta_lf_res;
    pps->mode_control_flags.bits.delta_lf_present = fh.delta_lf_present;
    pps->mode_control_flags.bits.delta_lf_multi   = fh.delta_lf_multi;

    pps->mode_control_flags.bits.reference_mode = fh.reference_select ?
        REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
    pps->mode_control_flags.bits.skip_mode_present = fh.skip_mode_present;

    //tile
    pps->tile_cols = 1; 
    pps->width_in_sbs_minus_1[0] = (uint16_t)(((fh.UpscaledWidth+63)&(~63))/64 - 1);


    pps->tile_rows = 1;
    pps->height_in_sbs_minus_1[0] = (uint16_t)(((fh.FrameHeight+63)&(~63))/64 - 1);

    pps->context_update_tile_id = (uint8_t)(fh.tile_info.context_update_tile_id);

    //cdef
    if(sh.enable_cdef)
    {

        pps->cdef_damping_minus_3 = (uint8_t)(fh.cdef_params.cdef_damping - 3);
        pps->cdef_bits = (uint8_t)(fh.cdef_params.cdef_bits);

        for (uint8_t i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            pps->cdef_y_strengths[i]  = (uint8_t)(fh.cdef_params.cdef_y_pri_strength[i] * CDEF_STRENGTH_DIVISOR + fh.cdef_params.cdef_y_sec_strength[i]);
            pps->cdef_uv_strengths[i] = (uint8_t)(fh.cdef_params.cdef_uv_pri_strength[i] * CDEF_STRENGTH_DIVISOR + fh.cdef_params.cdef_uv_sec_strength[i]);
        }
    }

    //loop restoration
    pps->loop_restoration_flags.bits.yframe_restoration_type  = (fh.lr_params.lr_type[0] == RESTORE_WIENER) ? 1 : 0;
    pps->loop_restoration_flags.bits.cbframe_restoration_type = (fh.lr_params.lr_type[1] == RESTORE_WIENER) ? 1 : 0;
    pps->loop_restoration_flags.bits.crframe_restoration_type = (fh.lr_params.lr_type[2] == RESTORE_WIENER) ? 1 : 0;
    pps->loop_restoration_flags.bits.lr_unit_shift            = fh.lr_params.lr_unit_shift;
    pps->loop_restoration_flags.bits.lr_uv_shift              = fh.lr_params.lr_uv_shift;

    //context
    pps->picture_flags.bits.disable_cdf_update = fh.disable_cdf_update;
    pps->picture_flags.bits.disable_frame_end_update_cdf = fh.disable_frame_end_update_cdf;
    pps->picture_flags.bits.disable_frame_recon = (fh.refresh_frame_flags == 0);

    pps->num_tile_groups_minus1 = 0;

    pps->tile_group_obu_hdr_info.bits.obu_extension_flag = 0;
    pps->tile_group_obu_hdr_info.bits.obu_has_size_field = 1;
    pps->tile_group_obu_hdr_info.bits.temporal_id = 0;
    pps->tile_group_obu_hdr_info.bits.spatial_id  = 0;

    //other params
    pps->picture_flags.bits.error_resilient_mode = fh.error_resilient_mode;
    pps->picture_flags.bits.enable_frame_obu     = 1;
    pps->reconstructed_frame = ref_surface[current_slot];

    //offsets need update
    pps->size_in_bits_frame_hdr_obu     = offsets.FrameHdrOBUSizeInBits;
    pps->byte_offset_frame_hdr_obu_size = offsets.FrameHdrOBUSizeByteOffset;
    pps->bit_offset_loopfilter_params   = offsets.LoopFilterParamsBitOffset;
    pps->bit_offset_qindex              = offsets.QIndexBitOffset;
    pps->bit_offset_segmentation        = offsets.SegmentationBitOffset;
    pps->bit_offset_cdef_params         = offsets.CDEFParamsBitOffset;
    pps->size_in_bits_cdef_params       = offsets.CDEFParamsSizeInBits;

    //q_matrix
    pps->qmatrix_flags.bits.using_qmatrix = fh.quantization_params.using_qmatrix;
    pps->qmatrix_flags.bits.qm_y = fh.quantization_params.qm_y;
    pps->qmatrix_flags.bits.qm_u = fh.quantization_params.qm_u;
    pps->qmatrix_flags.bits.qm_v = fh.quantization_params.qm_v;

    pps->skip_frames_reduced_size = 0;
}

static void
render_picture()
{
    VABufferID pic_param_buf_id = VA_INVALID_ID;
    VAStatus va_status;

    VAEncPictureParameterBufferAV1 pps_buffer;
    memset(&pps_buffer, 0, sizeof(pps_buffer));
    build_pps_buffer(&pps_buffer);

    va_status = vaCreateBuffer(va_dpy, context_id, VAEncPictureParameterBufferType,
                               sizeof(pps_buffer), 1, &pps_buffer, &pic_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(va_dpy, context_id, &pic_param_buf_id, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (pic_param_buf_id != VA_INVALID_ID) {
        vaDestroyBuffer(va_dpy, pic_param_buf_id);
        pic_param_buf_id = VA_INVALID_ID;
    }
}

static int upload_source_YUV_once_for_all()
{
    int box_width = 8;
    int row_shift = 0;
    int i;

    for (i = 0; i < SURFACE_NUM; i++) {
        printf("\rLoading data into surface %d.....", i);
        upload_surface(va_dpy, src_surface[i], box_width, row_shift, 0);

        row_shift++;
        if (row_shift == (2 * box_width)) row_shift = 0;
    }
    printf("Complete surface loading\n");

    return 0;
}

static struct storage_task_t * storage_task_dequeue(void)
{
    struct storage_task_t *header;

    pthread_mutex_lock(&encode_mutex);

    header = storage_task_header;
    if (storage_task_header != NULL) {
        if (storage_task_tail == storage_task_header)
            storage_task_tail = NULL;
        storage_task_header = header->next;
    }

    pthread_mutex_unlock(&encode_mutex);

    return header;
}

static int load_surface(VASurfaceID surface_id, unsigned long long display_order)
{
    unsigned char *srcyuv_ptr = NULL, *src_Y = NULL, *src_U = NULL, *src_V = NULL;
    unsigned long long frame_start, mmap_start;
    char *mmap_ptr = NULL;
    int frame_size, mmap_size;

    if (srcyuv_fp == NULL)
        return 0;

    /* allow encoding more than srcyuv_frames */
    display_order = display_order % srcyuv_frames;
    frame_size = ips.width * ips.height * 3 / 2; /* for YUV420 */
    frame_start = display_order * frame_size;

    mmap_start = frame_start & (~0xfff);
    mmap_size = (frame_size + (frame_start & 0xfff) + 0xfff) & (~0xfff);
    mmap_ptr = mmap(0, mmap_size, PROT_READ, MAP_SHARED,
                    fileno(srcyuv_fp), mmap_start);
    if (mmap_ptr == MAP_FAILED) {
        printf("Failed to mmap YUV file (%s)\n", strerror(errno));
        return 1;
    }
    srcyuv_ptr = (unsigned char *)mmap_ptr + (frame_start & 0xfff);
    if (srcyuv_fourcc == VA_FOURCC_NV12) {
        src_Y = srcyuv_ptr;
        src_U = src_Y + ips.width * ips.height;
        src_V = NULL;
    } else if (srcyuv_fourcc == VA_FOURCC_IYUV ||
               srcyuv_fourcc == VA_FOURCC_YV12) {
        src_Y = srcyuv_ptr;
        if (srcyuv_fourcc == VA_FOURCC_IYUV) {
            src_U = src_Y + ips.width * ips.height;
            src_V = src_U + (ips.width / 2) * (ips.height / 2);
        } else { /* YV12 */
            src_V = src_Y + ips.width * ips.height;
            src_U = src_V + (ips.width / 2) * (ips.height / 2);
        }
    } else {
        printf("Unsupported source YUV format\n");
        exit(1);
    }

    upload_surface_yuv(va_dpy, surface_id,
                       srcyuv_fourcc, ips.width, ips.height,
                       src_Y, src_U, src_V);
    if (mmap_ptr)
        munmap(mmap_ptr, mmap_size);

    return 0;
}

static int save_codeddata(unsigned long long display_order, unsigned long long encode_order)
{
    VACodedBufferSegment *buf_list = NULL;
    VAStatus va_status;
    int ret;
    unsigned int coded_size = 0;

    va_status = vaMapBuffer(va_dpy, coded_buf[display_order % SURFACE_NUM], (void **)(&buf_list));
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    long frame_start = ftell(coded_fp);

    while (buf_list != NULL) {
        coded_size += fwrite(buf_list->buf, 1, buf_list->size, coded_fp);
        buf_list = (VACodedBufferSegment *) buf_list->next;

        frame_size += coded_size;
    }

    long frame_end = ftell(coded_fp);
    vaUnmapBuffer(va_dpy, coded_buf[display_order % SURFACE_NUM]);

    CHECK_CONDITION(frame_start >= 0 && frame_end >= 0);
    if(encode_order == 0)
    {
        //first frame
        unsigned int ivf_size = coded_size - 32 - 12;
        ret = fseek(coded_fp, frame_start + 32, SEEK_SET);
        CHECK_CONDITION(ret == 0);
        fwrite(&ivf_size, 4, 1, coded_fp);
        fwrite(&display_order, 8, 1, coded_fp);
        ret = fseek(coded_fp, frame_end, SEEK_SET);
        CHECK_CONDITION(ret == 0);
    }
    else
    {
        //other frames
        unsigned int ivf_size = coded_size - 12;
        ret = fseek(coded_fp, frame_start, SEEK_SET);
        CHECK_CONDITION(ret == 0);
        fwrite(&ivf_size, 4, 1, coded_fp);
        fwrite(&display_order, 8, 1, coded_fp);
        ret = fseek(coded_fp, frame_end, SEEK_SET);
        CHECK_CONDITION(ret == 0);
    }

    printf("\n      "); /* return back to startpoint */
    switch (encode_order % 4) {
    case 0:
        printf("|");
        break;
    case 1:
        printf("/");
        break;
    case 2:
        printf("-");
        break;
    case 3:
        printf("\\");
        break;
    }
    printf("%08lld", encode_order);
    printf("(%06d bytes coded)\n", coded_size);

    fflush(coded_fp);

    return 0;
}

static int save_recyuv(VASurfaceID surface_id,
                       unsigned long long display_order,
                       unsigned long long encode_order)
{
    unsigned char *dst_Y = NULL, *dst_U = NULL, *dst_V = NULL;

    if (recyuv_fp == NULL)
        return 0;

    if (srcyuv_fourcc == VA_FOURCC_NV12) {
        int uv_size = 2 * (ips.width / 2) * (ips.height / 2);
        dst_Y = malloc(2 * uv_size);
        if (dst_Y == NULL) {
            printf("Failed to allocate memory for dst_Y\n");
            exit(1);
        }

        dst_U = malloc(uv_size);
        if (dst_U == NULL) {
            printf("Failed to allocate memory for dst_U\n");
            free(dst_Y);
            exit(1);
        }

        memset(dst_Y, 0, 2 * uv_size);
        memset(dst_U, 0, uv_size);
    } else if (srcyuv_fourcc == VA_FOURCC_IYUV ||
               srcyuv_fourcc == VA_FOURCC_YV12) {
        int uv_size = (ips.width / 2) * (ips.height / 2);
        dst_Y = malloc(4 * uv_size);
        if (dst_Y == NULL) {
            printf("Failed to allocate memory for dst_Y\n");
            exit(1);
        }

        dst_U = malloc(uv_size);
        if (dst_U == NULL) {
            printf("Failed to allocate memory for dst_U\n");
            free(dst_Y);
            exit(1);
        }

        dst_V = malloc(uv_size);
        if (dst_V == NULL) {
            printf("Failed to allocate memory for dst_V\n");
            free(dst_Y);
            free(dst_U);
            exit(1);
        }

        memset(dst_Y, 0, 4 * uv_size);
        memset(dst_U, 0, uv_size);
        memset(dst_V, 0, uv_size);
    } else {
        printf("Unsupported source YUV format\n");
        exit(1);
    }

    download_surface_yuv(va_dpy, surface_id,
                         srcyuv_fourcc, ips.width, ips.height,
                         dst_Y, dst_U, dst_V);
    fseek(recyuv_fp, display_order * ips.width * ips.height * 1.5, SEEK_SET);

    if (srcyuv_fourcc == VA_FOURCC_NV12) {
        int uv_size = 2 * (ips.width / 2) * (ips.height / 2);
        fwrite(dst_Y, uv_size * 2, 1, recyuv_fp);
        fwrite(dst_U, uv_size, 1, recyuv_fp);
    } else if (srcyuv_fourcc == VA_FOURCC_IYUV ||
               srcyuv_fourcc == VA_FOURCC_YV12) {
        int uv_size = (ips.width / 2) * (ips.height / 2);
        fwrite(dst_Y, uv_size * 4, 1, recyuv_fp);

        if (srcyuv_fourcc == VA_FOURCC_IYUV) {
            fwrite(dst_U, uv_size, 1, recyuv_fp);
            fwrite(dst_V, uv_size, 1, recyuv_fp);
        } else {
            fwrite(dst_V, uv_size, 1, recyuv_fp);
            fwrite(dst_U, uv_size, 1, recyuv_fp);
        }
    }

    if (dst_Y)
        free(dst_Y);
    if (dst_U)
        free(dst_U);
    if (dst_V)
        free(dst_V);

    fflush(recyuv_fp);

    return 0;
}

static void storage_task(unsigned long long display_order, unsigned long long encode_order)
{
    unsigned int tmp;
    VAStatus va_status;

    tmp = GetTickCount();
    va_status = vaSyncSurface(va_dpy, src_surface[display_order % SURFACE_NUM]);
    CHECK_VASTATUS(va_status, "vaSyncSurface");
    SyncPictureTicks += GetTickCount() - tmp;
    tmp = GetTickCount();
    save_codeddata(display_order, encode_order);
    SavePictureTicks += GetTickCount() - tmp;

    save_recyuv(ref_surface[display_order % SURFACE_NUM], display_order, encode_order);

    /* reload a new frame data */
    tmp = GetTickCount();
    if (srcyuv_fp != NULL)
        load_surface(src_surface[display_order % SURFACE_NUM], display_order + SURFACE_NUM);
    UploadPictureTicks += GetTickCount() - tmp;

    pthread_mutex_lock(&encode_mutex);
    srcsurface_status[display_order % SURFACE_NUM] = SRC_SURFACE_IN_ENCODING;
    pthread_mutex_unlock(&encode_mutex);
}

static void * storage_task_thread(void *t)
{
    while (1) {
        struct storage_task_t *current;

        current = storage_task_dequeue();
        if (current == NULL) {
            pthread_mutex_lock(&encode_mutex);
            pthread_cond_wait(&encode_cond, &encode_mutex);
            pthread_mutex_unlock(&encode_mutex);
            continue;
        }

        storage_task(current->display_order, current->encode_order);

        free(current);

        /* all frames are saved, exit the thread */
        if (++frame_coded >= ips.frame_count)
            break;
    }

    return 0;
}

static int storage_task_queue(unsigned long long display_order, unsigned long long encode_order)
{
    struct storage_task_t *tmp;

    tmp = calloc(1, sizeof(struct storage_task_t));
    if (tmp) {
        tmp->display_order = display_order;
        tmp->encode_order = encode_order;
    }

    pthread_mutex_lock(&encode_mutex);

    if (storage_task_header == NULL) {
        storage_task_header = tmp;
        storage_task_tail = tmp;
    } else {
        storage_task_tail->next = tmp;
        storage_task_tail = tmp;
    }

    srcsurface_status[display_order % SURFACE_NUM] = SRC_SURFACE_IN_STORAGE;
    pthread_cond_signal(&encode_cond);

    pthread_mutex_unlock(&encode_mutex);

    return 0;
}

static int encode_frames(void)
{
    unsigned int i, tmp;
    VAStatus va_status;
    //VASurfaceStatus surface_status;

    /* upload RAW YUV data into all surfaces */
    tmp = GetTickCount();
    if (srcyuv_fp != NULL) {
        for (i = 0; i < SURFACE_NUM; i++)
            load_surface(src_surface[i], i);
    } else
        upload_source_YUV_once_for_all();
    UploadPictureTicks += GetTickCount() - tmp;

    /* ready for encoding */
    memset(srcsurface_status, SRC_SURFACE_IN_ENCODING, sizeof(srcsurface_status));

    memset(&seq_param, 0, sizeof(seq_param));
    memset(&pic_param, 0, sizeof(pic_param));
    memset(&tile_group_param, 0, sizeof(tile_group_param));

    if (ips.encode_syncmode == 0)
        pthread_create(&encode_thread, NULL, storage_task_thread, NULL);

    for (current_frame_encoding = 0; current_frame_encoding < ips.frame_count; current_frame_encoding++) {
        encoding2display_order(current_frame_encoding, ips.intra_period, 
                               &current_frame_display, &current_frame_type);

        printf("%s : %lld %s : %lld type : %d\n", "encoding order", current_frame_encoding, "Display order", current_frame_display, current_frame_type);
        /* check if the source frame is ready */
        while (srcsurface_status[current_slot] != SRC_SURFACE_IN_ENCODING) {
            usleep(1);
        }

        tmp = GetTickCount();
        va_status = vaBeginPicture(va_dpy, context_id, src_surface[current_slot]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");
        BeginPictureTicks += GetTickCount() - tmp;

        tmp = GetTickCount(); //start of render process

        // prepare parameters used for sequence and frame
        fill_sps_header();
        fill_pps_header(current_frame_display);

        // init length of packed headers
        len_ivf_header = 0;
        len_seq_header = 0;
        len_pic_header = 0;

        // render headers
        // first frame send IVF sequence header + frame header
        if(current_frame_encoding == 0) 
        {
            render_ivf_header(); //44 byte, 32byte sequence ivf header, 12 byte frame ivf header
            len_ivf_header = 44;
        }
        else 
        {
            render_ivf_frame_header(); //12 byte frame ivf header
            len_ivf_header = 12;
        }

        render_TD();//render OBU_TEMPORAL_DELIMITER

        if (current_frame_type == KEY_FRAME) {
            if(current_frame_encoding == 0)  render_sequence(); //render SPS only needed in first frame
            len_seq_header = render_packedsequence(); //render packed sequence header
        }
        else
        {
            len_seq_header = 0;
        }

        if((ips.RateControlMethod == 2 || ips.RateControlMethod == 4) && current_frame_encoding == 0)
        {
            // misc buffer are not need in CQP case
            // only needed in first frame
            render_misc_buffer();
        }


        render_packedpicture(); //render packed frame header 
        render_picture(); //render frame PPS buffer
        render_tile_group(); //render tile group buffer
        RenderPictureTicks += GetTickCount() - tmp;

        tmp = GetTickCount();
        va_status = vaEndPicture(va_dpy, context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");
        EndPictureTicks += GetTickCount() - tmp;

        if (ips.encode_syncmode)
            storage_task(current_frame_display, current_frame_encoding);
        else /* queue the storage task queue */
            storage_task_queue(current_frame_display, current_frame_encoding);

    }

    if (ips.encode_syncmode == 0) {
        int ret;
        pthread_join(encode_thread, (void **)&ret);
    }

    return 0;
}

static int calc_PSNR(double *psnr)
{
    char *srcyuv_ptr = NULL, *recyuv_ptr = NULL, tmp;
    unsigned long long min_size;
    unsigned long long i, sse = 0;
    double ssemean;
    int fourM = 0x400000; /* 4M */

    min_size = MIN(srcyuv_frames, ips.frame_count) * ips.width * ips.height * 1.5;
    for (i = 0; i < min_size; i++) {
        unsigned long long j = i % fourM;

        if ((i % fourM) == 0) {
            if (srcyuv_ptr)
                munmap(srcyuv_ptr, fourM);
            if (recyuv_ptr)
                munmap(recyuv_ptr, fourM);

            srcyuv_ptr = mmap(0, fourM, PROT_READ, MAP_SHARED, fileno(srcyuv_fp), i);
            recyuv_ptr = mmap(0, fourM, PROT_READ, MAP_SHARED, fileno(recyuv_fp), i);
            if ((srcyuv_ptr == MAP_FAILED) || (recyuv_ptr == MAP_FAILED)) {
                if (srcyuv_ptr != MAP_FAILED) 
                    munmap(srcyuv_ptr, fourM);
                if (recyuv_ptr != MAP_FAILED) 
                    munmap(recyuv_ptr, fourM);
                printf("Failed to mmap YUV files\n");
                return 1;
            }
        }
        tmp = srcyuv_ptr[j] - recyuv_ptr[j];
        sse += tmp * tmp;
    }
    ssemean = (double)sse / (double)min_size;
    *psnr = 20.0 * log10(255) - 10.0 * log10(ssemean);

    if (srcyuv_ptr)
        munmap(srcyuv_ptr, fourM);
    if (recyuv_ptr)
        munmap(recyuv_ptr, fourM);

    return 0;
}

static int print_performance(unsigned int PictureCount)
{
    unsigned int psnr_ret = 1, others = 0;
    double psnr = 0, total_size = ips.width * ips.height * 1.5 * ips.frame_count;

    if (ips.calc_psnr && srcyuv_fp && recyuv_fp)
        psnr_ret = calc_PSNR(&psnr);

    others = TotalTicks - UploadPictureTicks - BeginPictureTicks
             - RenderPictureTicks - EndPictureTicks - SyncPictureTicks - SavePictureTicks;

    printf("\n\n");

    printf("PERFORMANCE:   Frame Rate           : %.2f fps (%d frames, %d ms (%.2f ms per frame))\n",
           (double) 1000 * PictureCount / TotalTicks, PictureCount,
           TotalTicks, ((double)  TotalTicks) / (double) PictureCount);
    printf("PERFORMANCE:   Compression ratio    : %d:1\n", (unsigned int)(total_size / frame_size));
    if (psnr_ret == 0)
        printf("PERFORMANCE:   PSNR                 : %.2f (%lld frames calculated)\n",
               psnr, MIN(ips.frame_count, srcyuv_frames));

    printf("PERFORMANCE:     UploadPicture      : %d ms (%.2f, %.2f%% percent)\n",
           (int) UploadPictureTicks, ((double)  UploadPictureTicks) / (double) PictureCount,
           UploadPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaBeginPicture     : %d ms (%.2f, %.2f%% percent)\n",
           (int) BeginPictureTicks, ((double)  BeginPictureTicks) / (double) PictureCount,
           BeginPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaRenderHeader     : %d ms (%.2f, %.2f%% percent)\n",
           (int) RenderPictureTicks, ((double)  RenderPictureTicks) / (double) PictureCount,
           RenderPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaEndPicture       : %d ms (%.2f, %.2f%% percent)\n",
           (int) EndPictureTicks, ((double)  EndPictureTicks) / (double) PictureCount,
           EndPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     vaSyncSurface      : %d ms (%.2f, %.2f%% percent)\n",
           (int) SyncPictureTicks, ((double) SyncPictureTicks) / (double) PictureCount,
           SyncPictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     SavePicture        : %d ms (%.2f, %.2f%% percent)\n",
           (int) SavePictureTicks, ((double)  SavePictureTicks) / (double) PictureCount,
           SavePictureTicks / (double) TotalTicks / 0.01);
    printf("PERFORMANCE:     Others             : %d ms (%.2f, %.2f%% percent)\n",
           (int) others, ((double) others) / (double) PictureCount,
           others / (double) TotalTicks / 0.01);

    if (ips.encode_syncmode == 0)
        printf("(Multithread enabled, the timing is only for reference)\n");

    return 0;
}

int main(int argc, char **argv)
{
    unsigned int start;

    process_cmdline(argc, argv);

    print_input();

    start = GetTickCount();

    init_va();
    setup_encode();

    encode_frames();

    release_encode();
    deinit_va();

    //free memory
    if(ips.output) free(ips.output);
    if(ips.srcyuv) free(ips.srcyuv);
    if(ips.recyuv) free(ips.recyuv);

    TotalTicks += GetTickCount() - start;
    print_performance(ips.frame_count);

    return 0;
}
