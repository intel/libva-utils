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

/*
 * it is a real program to show how VAAPI decode work,
 * It does VLD decode for a simple AVC clip.
 * The bitstream and VA parameters are hardcoded into avcstreamoutdemo.cpp,
 *
 * ./avcstreamoutdemo  : only do decode
 * ./avcstreamoutdemo <any parameter >: do decode and dump mv info
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <va/va.h>
#include "va_display.h"
#include "avcstreamoutdemo.h"


#define CHECK_VASTATUS(va_status,func)                                  \
if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
    exit(1);                                                            \
}

/* Data dump of a 176x144 AVC video clip,it has one I frame and one P frame
 */
static unsigned int avc_clip[] = {
    0xce20b865, 0xe2201c0f, 0x9a80c737, 0xd10130c0,
    0x73f8f26f, 0xfe2e0af8, 0x04bd8666, 0x333fbfb3,
    0x901da37e, 0x870d050c, 0x5f46568d, 0x2fe68e2c,
    0xbf626f46, 0x7c4802a3, 0x67b0548c, 0x22681c1e,
    0x463bc0f9, 0x815f2161, 0xd6c26c9b, 0xb2c4947a,
    0xf3f7a999, 0x96d4baca, 0x25cd0004, 0x8fbf7436,
    0xfecb7777, 0xecf7effb, 0x0d601808, 0xc86f92ad,
    0x6f696b03, 0x2c7dfec0, 0xd2e474bf, 0x0712e10f,
    0xfd73d1bb, 0xe8ac0192, 0x5402e397, 0xefb811a8,
    0x2fdc9380, 0xed7889f5, 0xffef7fa7, 0x5845ebdd,
    0x6d4a203b, 0xd20170f2, 0xebfb6c61, 0x34a81afd,
    0x67d9621f, 0xf2479fb8, 0x9fb91701, 0x336d9ca9,
    0xc0f33b12, 0x30f5b60d, 0x7d46b674, 0xdc37fa2a,
    0x77f74f31, 0xf9777272, 0xba7f8417, 0xc65e0a90,
    0xdee83735, 0x037fb352, 0x5e8ac257, 0xf09bae14,
    0x23045f14, 0x98f017ef, 0xe89c3716, 0xab6ed611,
    0xe1bd7afe, 0xcb981698, 0x4d2a8484, 0xbbffa532,
    0x01fcf2ef, 0xc7c61893, 0x24fcd3af, 0xc44d6102,
    0xe331fac2, 0x90c48fe4, 0x2afe651f, 0x687883b8,
    0xc1081535, 0x6dd11ffd, 0x34d17afe, 0x4bfb259d,
    0x9d668b08, 0x1ee539b1, 0x702e67e7, 0x3ec8165f,
    0xffc7ffdf, 0xdca08c1b, 0x814dcc2a, 0x558175a4,
    0xce4d0478, 0x4b7edf3c, 0xdc6754ae, 0xb6ee10f9,
    0xe7ccebff, 0x1180fde7, 0x029bcc24, 0x969de939,
    0xc82154b4, 0x8956a0b3, 0x23b417c8, 0x33ad0fc4,
    0xf600c2fa, 0xebc01f2c, 0x75329274, 0x746784bf,
    0x82b7f83f, 0x7bbc6334, 0x9ee79fbf, 0x04803e75,
    0xf768ca7d, 0x2f31e193, 0x863f9705, 0x403723bc,
    0x8f7de430, 0xd952d29f, 0xcb5664b7, 0x2f0b837d,
    0xd1dbf1b1, 0x8aa719e1, 0xaf0f3b6e, 0x33ef4fab,
    0x427dbece, 0x6a411576, 0xb52ab370, 0x117076b1,
    0xc6cd9fb2, 0xdf0d735a, 0xd62dc750, 0x867f7d77,
    0xc0ddd65c, 0x88add368, 0x2f69188b, 0xc3042484,
    0xf51de774, 0xe7f4bccc, 0x09c03d33, 0x3a4892e1,
    0x56e7ed85, 0x10812358, 0xc8c93b1e, 0x4081bd67,
    0x53041322, 0xa768c215, 0xdf387cbb, 0xfcf85fe3,
    0x33c8e4ed, 0xcc6affa6, 0x0e901eef, 0xf23b9d0d,
    0xfdfc7ce6, 0x72c45266, 0xd81f810e, 0xda1ad42d,
    0xc34e8cac, 0xf7d16413, 0x993d01d0, 0x0dc2404a,
    0x0ad3ae3a, 0xff632898, 0x4c99ac02, 0x3cc161e8,
    0x744a1c0a, 0x26af3299, 0x40aeda72, 0xb90b7b6c,
    0x0ee97185, 0x30d4f777, 0xf73fd6dd, 0xe1160a36,
    0x6a97bb07, 0x9e8601ec, 0x6f2db153, 0x8e8f0fb2,
    0xfa296973, 0xfd2fd1f6, 0xab289081, 0x0460f415,
    0x1019999e, 0xe709e77e, 0x09985e94, 0x0eeeb71f,
    0x1716b05f, 0xaf9102e9, 0x10dea4e4, 0xe897ea85,
    0x3964c91b, 0xeef1ff19, 0xff1777ef, 0x6d004ae4,
    0x1d2460b6, 0x097c827f, 0x74486026, 0xb4da67d8,
    0xf10993fe, 0x76becf26, 0xad78198a, 0xeb661d02,
    0xe61d8d6f, 0x4dc6706f, 0xdd0e4844, 0xfae17f21,
    0xd2b13a4a, 0x77ff7ed2, 0x1bfff2c9, 0x40fbd645,
    0xf3d667f9, 0x92bc91ee, 0x0cdfac03, 0xaa7ffa3b,
    0x20568aad, 0x76dcaa3e, 0x5fe050f3, 0x414722f7,
    0x7eff8356, 0xc81a7e3b, 0x04e007d3, 0x2e5df61f,
    0xe06e17ac, 0xe8596902, 0xf16f4262, 0x031c78b9,
    0x019e293c, 0xfffffbeb, 0xc74d76f4, 0xf94cafe0,
    0xfefcbf2f, 0xacf17f7a, 0xffef675f, 0xd69e3925,
    0x81f8d945, 0x7fe0ffed, 0x475b91a5, 0xe06d6ae2,
    0x9e78df7b, 0xdfba91f9, 0xb1afc0cf, 0x35a2a66a,
    0xd73fffff, 0xdd3a2dec, 0x36ae9de1, 0x7fa517ec,
    0x810e9a72, 0x19c4d939, 0x38db0ddc, 0x780af7c0,
    0x867dee5a, 0xba02b876, 0xbb3beb56, 0x5cc67f9a,
    0xd24fd0dd, 0x2744f71c, 0xc300cac5, 0xf8dbbaef,
    0xab12c7c0, 0xdbedecca, 0x9c5fee7f, 0x57c5e472,
    0xcf76bdbc, 0x0ac3988d, 0x7f78bb65, 0xbdc8edfe,
    0x3fbd0b18, 0xff4fc78f, 0x99bb40fd, 0xe13b3e1f,
    0x78694f71, 0x708f2607, 0x871cbf1f, 0xbdaf871b,
    0xfe9aab6f, 0x18dc1966, 0x34c5e7fb, 0xf5e0e074,
    0x33b039d5, 0x233335d8, 0xc3d0b980, 0xb80b65e0,
    0x3822dc23, 0x003ade43, 0x01d8ac36, 0xdf5b05c8,
    0x8398b04d, 0x64f897ca, 0x7326c561, 0x78693f52,
    0x6a3c9944, 0x2ab7b794, 0x46d9da00, 0x657f14fc,
    0x4081f3a7, 0xe24f6f25, 0x70178f38, 0x93bfd777,
    0xfadb0c20, 0x2680fb3e, 0x4d6732c6, 0x4da23e83,
    0xd1918b7f, 0xc0a9c69a, 0xa179e658, 0xbf06b5d5,
    0x447cdb01, 0x8f444fd9, 0xefdf22e8, 0xbd8bd81d,
    0xf58b2bdf, 0xfcc281cf, 0x0afa47c1, 0xfd49d1da,
    0x31a31d70, 0xa81c7e67, 0xe7c8a72f, 0xab47e77f,
    0x8474712b, 0x1d227828, 0x1fc5fb57, 0x2e3d71ff,
    0x1462a01b, 0xfccb751f, 0x3fffbe5d, 0x9311d216,
    0x3f8cb07c, 0xf0b2d2ce, 0xb0a47e0e, 0xf0678201,
    0x7d0864a5, 0x35667c52, 0xdeef637b, 0x5efc1ac0,
    0x80ffff85, 0x0274ca62, 0xfffbe743, 0x11e0bbff,
    0xd570faf5, 0xa94be585, 0x3a48fb01, 0xc0395410,
    0x3d719e39, 0x15db1d12, 0x0003fc7f, 0xa6b16687,
    0xa3ca61aa, 0x2535153a, 0x51c37db5, 0xc232f1e9,
    0x5e3659c6, 0x2f64ca2a, 0x163d0780, 0xfd2a286d,
    0xc540fdf7, 0x25463f4b, 0x1265a928, 0x2ab51d96,
    0xb5728bf2, 0xd8eee02c, 0xd491feab, 0x68f8ee63,
    0x2c8f1500, 0x6a34f537, 0x6c36c895, 0x5c26c2f1,
    0xfe87e52e, 0xe787f91b, 0xb29f838a, 0xfc6fabb8,
    0x126cfdf5, 0x0175743c, 0x06707ac0, 0x853476fc,
    0xef97c00e, 0xf040a1a8, 0x0000c78b, 0x9fed36c4,
    0xc8873faf, 0xc142e469, 0xcca3c8e7, 0xebaf33f3,
    0x2393b97a, 0x8a1f0200, 0x5e338d95, 0x186cbc1f,
    0xf87d5e1a, 0xa8e13f6b, 0xd7ba4939, 0x9256cd3f,
    0xf7e82b1b, 0x31c0e7c2, 0xab29c0bb, 0x7dfd2dd7,
    0x55bc0597, 0xbb76a3a4, 0xe9aebd0f, 0x67de1caa,
    0x81fad799, 0x78ec09df, 0x0080b663, 0x775e930d,
    0x609fa261, 0x6085fc7e, 0x4e85f507, 0xfe63dc41,
    0xaf0cb8b3, 0xbbfe6196, 0xa05f50b9, 0x8773e430,
    0xd9cae9c5, 0xf89410d0, 0x60c7bd0f, 0x00ef20e0,
    0x42c0c60f, 0x5c0b6386, 0x0bf94687, 0x47d9caf3,
    0x08e4a0b0, 0xe2bd962d, 0xbe396c0d, 0x192d9d1e,
    0x762b0f6d, 0x54fac4bf, 0xd0217f05, 0x707815e1,
    0x8d019a22, 0x1bb568fc, 0xff8fff6b, 0x0b4131ab,
    0xc634f068, 0xfb10ab38, 0x7e594aed, 0x076809c1,
    0x60463d03, 0x9ecbdd35, 0x831f29a3, 0x19c9ab6f,
    0xccfef7ba, 0x70f39baa, 0x820468cf, 0xfdb7cba4,
    0x102ede81, 0x5b08f7b8, 0x07e52b6d, 0x01117874,
    0x224f60d3, 0x131afe40, 0xcff0170a, 0x990b8a4f,
    0x84301f81, 0x73f0f96e, 0xc0e47ac0, 0x327aa40b,
    0x58e0c4f9, 0x40f06018, 0x4118a1dd, 0x6e20b440,
    0x245599e2, 0x9f5a302b, 0xff1dffbf, 0xbe2bc7bb,
    0xd4e45524, 0x828a07c5, 0xad4acee9, 0x99464a3c,
    0xdbd7c8f2, 0xa278f036, 0xbe5d8e0f, 0x216281d7,
    0xa7e34032, 0xfe7ff547, 0x2ecfea3f, 0x0cfa8e9c,
    0x351707b5, 0xeb6e8bd8, 0xfa65f513, 0xfa450a54,
    0x9cbac6f0, 0xd42d43f0, 0xcb789f9f, 0xafa0065a,
    0x88f28df2, 0xc987941f, 0xb04fad8f, 0x56b14071,
    0xe045a7aa, 0x3b21fcd7, 0x965ba86b, 0x69b0d76c,
    0xd470cba5, 0x8cf09d70, 0x57b940f1, 0xcbd47c99,
    0x199e0eca, 0x8c33706b, 0x0567c5b7, 0xae6f25c4,
    0xf9e47feb, 0xd63307dd, 0xb83a9f40, 0x1399d904,
    0xa44dab8f, 0xddfd1d0e, 0x1156dcbf, 0xfbc5ce3d,
    0xc0b7cdf4, 0xff9c274c, 0x3fff5bad, 0x1b47900d,
    0x6b6580b9, 0x0b360507, 0x8a163f9a, 0x64f4de35,
    0xf33c5f31, 0x6dfbb9fc, 0xe06a1a87, 0x45a38eff,
    0xf0e81958, 0x43ab8b18, 0x0ff3eee1, 0xee0e449a,
    0x3c4fdf0a, 0xd11ea5fe, 0x9a7ee9ac, 0xfa3a357d,
    0x277a5a1a, 0x77c33953, 0x39670426, 0xebf40750,
    0x6aebf480, 0x7ebec755, 0x21b0ba22, 0xbedd8cee,
    0xa331dfc7, 0x20c85bf5, 0x800f6178, 0xdfbf8300,
    0x24e0452e, 0x1ff84d1d, 0xfc8e85fd, 0x82403363,
    0x257b44d1, 0x808be3b9, 0x4e2053bc, 0xaf03ef8a,
    0xf3cf13a0, 0x462a2382, 0x15fbff83, 0xbbff562b,
    0xba9ee3ee, 0x63da06e0, 0x35751d32, 0xa0d865b9,
    0xfca61624, 0x93644586, 0x91391564, 0xd740e40b,
    0xb63413fc, 0x58fecab5, 0x614b4355, 0x425f5fd8,
    0xff21d271, 0x3ee7cf1c, 0x0565b01d, 0xb8f708ee,
    0x00ea1f8f, 0x811ac6bb, 0xe8dd030e, 0x82477eb2,
    0xe3057f87, 0x73198c0c, 0x8b8c25b8, 0xaebe5b07,
    0x62c37b70, 0x310038e6, 0xf387abfe, 0x2f56d0f0,
    0x62973951, 0xf5d7e120, 0x9ef0ad03, 0x0300ae73,
    0x1200d8a1, 0xb9de1fa5, 0xec6e1b49, 0xaf6a474d,
    0x8f6e73a9, 0x2f3c1ea4, 0xf17b338d, 0x7c84fdbe,
    0x304b3dd4, 0xb18c353d, 0xf480c296, 0x021dd212,
    0xcba1311e, 0xe6704130, 0x2a746867, 0xceb1837b,
    0xf7a6083a, 0x31b7ea0b, 0x16e85c87, 0x2a3b6f8b,
    0xd32ceafc, 0x6c65b50c, 0x0fbbbc9f, 0xb8989bf9,
    0x5d8ff430, 0x8d2fe056, 0x1db5dc41, 0xe889e100,
    0x0b213b54, 0xa5df3135, 0x0a1ea40e, 0xc2d2e58c,
    0xfd0f97a3, 0x0ff398a5, 0x2ad4a9c3, 0x21a09658,
    0x589b651e, 0x00bb23bc, 0xedaa0001, 0x66d4d424,
    0xfe3c2d7d, 0xe9984611, 0x1695af96, 0xd42e03c9,
    0x9d53f081, 0x3fe78443, 0x66ebb469, 0x118f10b1,
    0x90d5ffc7, 0x6ca5c66e, 0x4863b900, 0xbe78d881,
    0x6cc0ead5, 0x04ce834b, 0xf54a17ca, 0xa7f3cceb,
    0x3e57ef99, 0xfadb00bb, 0x90c9a3f4, 0x7fa12804,
    0x26ba3ee8, 0xd927fd8d, 0x01c0fffb, 0x22291ead,
    0xf3df78fd, 0x4b9b4740, 0xc03f69a8, 0x6e99757a,
    0x19d687a1, 0xb4e3f46b, 0x173f7e59, 0xc69f67fe,
    0x6f200840, 0xe7b983d0, 0x2ff4d5ee, 0xff834946,
    0xbabb60c7, 0x77feded7, 0x8084a0d3, 0xe1c3fd97,
    0x0c80ba0c, 0xca03a602, 0x05e403e0, 0x79047705,
    0x787bf9f0, 0xb75155d5, 0x0123773e, 0x83fed0bf,
    0x6d38c0ab, 0x2f42e2c4, 0x1bd8ecec, 0x860198aa,
    0x009f070d, 0x8172f029, 0xedbb259d, 0x2e34fda6,
    0xc99aaa5e, 0xd5ddb1f2, 0xb552aa7a, 0xef9a83d4,
    0x4f68c803, 0x55abf304, 0x3a5cacdc, 0x623a76dc,
    0xe81a81be, 0xfd16f994, 0x38225b1c, 0x2a23f299,
    0x17ff0056, 0x18440add, 0xd63ab951, 0xc84961b5,
    0x2c9e145a, 0xe63c49f0, 0xff5f470b, 0xb79ddaf5,
    0x43489f15, 0x2dcb65cb, 0x3fc16188, 0xbf60c2ed,
    0x403db2b4, 0x55e162c1, 0x03d1f128, 0x53438d80,
    0x9377cbf9, 0x6b25eced, 0xa9e782e3, 0xfaf31770,
    0x17f34571, 0x71757eb9, 0xb0684b05, 0x4c05217c,
    0xc0610f4f, 0x3d241f61, 0x2a1d22b4, 0x6e2dcd9a,
    0x5cdaa1c2, 0x944fd6c7, 0x8dffaf33, 0xaba5f39d,
    0xbbec1176, 0x5b2d30fd, 0xc548d759, 0x8de1706e,
    0x2f944eff, 0x2d7a9d21, 0x5ccfb55e, 0x98bae6eb,
    0xce30644f, 0xbba7ecea, 0x1795612b, 0x79f8a0bb,
    0x47590d1e, 0xdddb1fbd, 0x2285fa64, 0xa887d19c,
    0xa366d6db, 0xa7f5115d, 0xf3eb77f1, 0x87e975fd,
    0x1b37e3be, 0xf1b28fd7, 0x0cca3c84, 0x3b585b77,
    0xffed0f84, 0xcffafb4d, 0x55eef939, 0x1913d09c,
    0x3cbb015d, 0x3c711405, 0xb7d1eb83, 0x351b2977,
    0x3c29ffc6, 0xcfe17f08, 0x9933e8c9, 0xe1e977af,
    0x7cd0abde, 0x217c089d, 0x3d984aff, 0x8e0d84b1,
    0x14b65cbf, 0x5fc78204, 0xf887301b, 0x29fc5a95,
    0xca461180, 0xc95e57cc, 0xf3a8ae43, 0x7fa419ee,
    0xd23f4dfb, 0x9613aa9a, 0x2a4832bb, 0x45c673cf,
    0xf98c0964, 0xf12b2536, 0xae5f23b6, 0xade10f3a,
    0xbf83df4d, 0xeb624887, 0x3ca49513, 0xa42c323e,
    0xcc947d79, 0xa6995591, 0x3acba047, 0x5e18a52a,
    0x7ffff7af, 0xf51af0f8, 0xcd4cf1ad, 0x57aa1e87,
    0xe5a4e2b6, 0x6293f3a8, 0x8e55277b, 0x00867b3a,
    0x7091fff8, 0xe6a1b028, 0x6242f52e, 0x743134aa,
    0xa2dea374, 0xfe6a50b2, 0x4fcc01d1, 0x5b1747c2,
    0x123a6b54, 0x0a0ef5ff, 0x657521b2, 0x1573ba20,
    0xf95f20e2, 0xdffe0d52, 0xa13efd9d, 0x850c7355,
    0x97a7a984, 0xaab1ef1c, 0x7cffc97b, 0xf07d161a,
    0xfea255e9, 0x7063ae85, 0x638b1bd6, 0xe42de373,
    0xe768dfed, 0xcc3f36fc, 0x2f49c9a1, 0x0c3f835a,
    0xf0c2d154, 0x09fe486b, 0xa671fc31, 0x2cc6e185,
    0x58ebcf87, 0xddfd014f, 0xd34f5b9c, 0x38b03e4d,
    0xe30f4aa9, 0x3d8d2b95, 0xc653e7a9, 0x8d1bb0be,
    0x39b48e3b, 0x75db048a, 0xfd271719, 0xc6aee30b,
    0x764f3b5f, 0x49af5dc6, 0x5b0ad27e, 0xdf5c4f48,
    0xa37a5960, 0x6b0265fb, 0xeb1f3e82, 0x0080a29e,
    0xc9ddb580, 0x03e0f58e, 0x015b824b, 0xd7ef3c1a,
    0xcac0ba6a, 0x5fd637c0, 0x5e788dcc, 0xb1f357f7,
    0x7c8c00c0, 0x00358029, 0x01b00001, 0x04c04362,
    0x0034300c, 0x720f8009, 0x0018a0c3, 0xa7ed30b7,
    0xcf4656e1, 0xc0001d4a, 0x4087b907, 0x7fee0130,
    0x30850e18, 0x581f04f7, 0xf7ff4fe2, 0xa7eafffc,
    0x1a1abe05, 0x05c28450, 0xb55b9d5d, 0x5f68e3be,
    0x0efd89fb, 0x1d1ede9f, 0xc279a19a, 0xad84a7a3,
    0x286e25db, 0xa4355b05, 0x87d8929d, 0xe55a7c2c,
    0xdc6df083, 0x88660cfd, 0x7fc07f80, 0xe8d62196,
    0x826e2e98, 0x3f2bfb39, 0xb8a9f3ef, 0x2260845d,
    0x3423e62e, 0x66e4fe31, 0x7f906912, 0x8bd7457d,
    0xb7cdf7d3, 0xfcdaf6a6, 0x9ee86b3d, 0x1f5ffff7,
    0x780b18cc, 0x65a00148, 0x3bbf9458, 0x0aefa58a,
    0xae667dab, 0x67c55f4f, 0x8179beae, 0xe6540dd2,
    0x1ccf7242, 0xea6ecb03, 0xc28c9c31, 0xbb9c572e,
    0xc01f2e2f, 0x9a25437b, 0x558cb675, 0xc63b4ed3,
    0xe06e27df, 0x4e2e9860, 0x17acea7d, 0x06b3ba66,
    0xebd649d3, 0xe4e1e154, 0xd8136b0d, 0x03271324,
    0x9312523a, 0x156fb44c, 0xc373a266, 0x160e75fd,
    0x78901c88, 0x4886060d, 0x5eb9cc39, 0xa9638e64,
    0xcff5459c, 0x19e27288, 0xe1ff3086, 0x7e3c8237,
    0x05360904, 0xf85bfdc5, 0xfc8e5a87, 0x00308555,
    0x7769e6db, 0x20ecf60f, 0x1b86a7ab, 0xed75ab8d,
    0xeb7f9dfa, 0x08e2a968, 0x4bb34bc7, 0xf8d9196d,
    0x31d38fed, 0x46eae5cc, 0x66e1307f, 0xfd06d5c1,
    0x850cb4bd, 0x241b2ca1, 0x8f165563, 0xf54153f6,
    0x70859632, 0x253fd04a, 0x1d92ff52, 0x7cfff1ff,
    0xdbc68964, 0x0f529b8f, 0xeb87f565, 0x8d0e4fc1,
    0x8a6794c8, 0xa009a7bf, 0x1d7fcab8, 0x75366b1c,
    0x262d12a6, 0xd6bb8ea6, 0xc0d04d04, 0xcae9372f,
    0x4bfd620f, 0x784fcd03, 0x5fc381e7, 0x5407a7cb,
    0xf53ef71a, 0x3ea5444f, 0x63ebab0a, 0x9a0a235c,
    0x87d06a9d, 0x80083f84, 0x7f78b799, 0x6dbcd25d,
    0x2931eca4, 0xb8207992, 0x9b977ef8, 0xceb13371,
    0x5eea7b95, 0x98a645d1, 0x3c95803a, 0x4214b96b,
    0x3579f578, 0x359face1, 0xafa271ff, 0x3711867d,
    0xdaa6fe67, 0x47c9327a, 0x4dd2f2c7, 0x064ecee6,
    0xf3ef46c9, 0xfb68220e, 0x2fac397e, 0x1ee4833b,
    0x0fb095d4, 0x99783a34, 0xbaea518d, 0x7e3ca108,
    0xac7f78ed, 0xb01d3047, 0xf604f3f0, 0x4a94c55a,
    0xfd79eeee, 0xc67ca16e, 0xe6f5135f, 0x16e9b79f,
    0x1adcdf96, 0xfaac3832, 0x0002d8e6, 0xebe827dc,
    0x1c9a05fd, 0x7b9a31b0, 0x21b0ab43, 0x655a804d,
    0x4009c0de, 0x00a78220, 0x2b0b1e82, 0xdca9f261,
    0x2bd0600d, 0xff108c07, 0x255f71fa, 0xb4c6ebaa,
    0xb6369990, 0xbe3f6734, 0xe18825e2, 0x68d703fa,
    0xf375a581, 0x59958df1, 0x0bf5d392, 0xad198111,
    0x721ec4a1, 0x3eebdeab, 0x05c70fd8, 0x065c4714,
    0x02a0700f, 0x1e921f28, 0xf8ff9482, 0x00cdff8e,
    0x081faaaa, 0x4b975aed, 0x2b6614b8, 0xdd153273,
    0x317c31fd, 0x53db04ef, 0x619dfd21, 0x726bbca3,
    0x644edce2, 0x80427a3a, 0x82678020, 0x30ed6e49,
    0xf19f7347, 0x777d8c71, 0x8dc3798e, 0x3ddaa2ce,
    0x5a8e0701, 0xf3fd63ce, 0xc5ebcc81, 0xc58fecd1,
    0x2e0e8159, 0xe51825ed, 0xeb90026f, 0xb60a23c4,
    0x2c4bf31d, 0xa01a8ecf, 0x5190fdcc, 0x252a333a,
    0xc0f788b6, 0x89389863, 0x923d4460, 0xfce7d02e,
    0xe2fb27c1, 0xe26b6c4e, 0x80e70ea1, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

static unsigned int avc_clip1[] = {
    0xbbd2e141, 0xe764a8bc, 0x0c0880d5, 0x38bb0a6e,
    0x0a6b07f7, 0x77ac6065, 0x040c94ff, 0xce15704a,
    0xc989f061, 0xfaf82117, 0xcbad5945, 0x6d9daaa6,
    0xdd878959, 0x1a5f2e3b, 0x1e5ec361, 0x8b007c0f,
    0xbf471eed, 0x90b139ae, 0xc600f07a, 0x85ee5242,
    0xed995482, 0xff6f5b9d, 0x010bc02d, 0x40256504,
    0xd810fd3f, 0x30bef589, 0xadf21133, 0x0f66f6fa,
    0x51c215c2, 0x872e3481, 0x000bf079, 0xa6e50c33,
    0xb81cdddc, 0x95404536, 0xd7e8c5f7, 0x84186434,
    0xa9cf3bac, 0xdc387cd1, 0x53823ab3, 0xa005736c,
    0x290c7fac, 0x5864641d, 0xebcc3506, 0xedbb5d60,
    0xc75c5efc, 0xa5b71787, 0xee7b5ebd, 0xf734ef39,
    0xa67e78da, 0x82fa42de, 0x3810868d, 0x2c7a38c8,
    0xc65f96ff, 0x72673187, 0x07d7a30a, 0x16ff9818,
    0x507ce850, 0x1b24d0be, 0xc3e3509e, 0x09f625ec,
    0xfee00bc5, 0xfee0208d, 0x256524c4, 0x830b1383,
    0x7c3f40c7, 0x5ca61d8d, 0x7134c076, 0x957b0e0c,
    0xc6c758cf, 0x610b8f5f, 0x47814301, 0xf8c6c05b,
    0x5a6c2fa6, 0x4f78c2ca, 0xeec6d9c0, 0x23b7843b,
    0x72f1eff5, 0x80760900, 0x3f1df421, 0xf12fc10f,
    0x3a38121e, 0xeeb89aed, 0x1f1913eb, 0xcbb31871,
    0xd4d5fd78, 0xce3f5aa3, 0x5334bea8, 0x7094c330,
    0xff019653, 0x61e02683, 0x3f855c97, 0x1e7e1e7e,
    0xc7189434, 0xf0f221da, 0xd408b533, 0x84edd41e,
    0x3b840ec4, 0x216f3ff7, 0xc43ea988, 0x732310c5,
    0x774d0ed2, 0xc7da7677, 0xbb99751c, 0xdef758ea,
    0xa08030c5, 0x194fb88c, 0x5d2cdf01, 0x31883f9e,
    0xd88040da, 0xca5193f2, 0xe14b4d4a, 0x121a8a03,
    0xa6fe32ce, 0xc76f64e6, 0xf72b760b, 0x32b1ca76,
    0xde807e6c, 0x0bc27b7f, 0xd14d0204, 0xe4d7f39d,
    0xb2d40890, 0x2c7b0080, 0x01640a31, 0x7828377f,
    0xd0b57923, 0x7a8e81ee, 0x1ea59e1d, 0xd32905e5,
    0x51103cf9, 0x0822a87d, 0x9fc886b9, 0x27aaa526,
    0xc4c25a0c, 0x39eff6d1, 0x42be2114, 0xb1e0be80,
    0x71551a96, 0x65ec526e, 0x5254683a, 0x2f53a761,
    0x817c79c5, 0x22605079, 0x7b32b62b, 0xf94d4576,
    0xf83c836d, 0xf35ca5ae, 0x48385aa1, 0x03ec899f,
    0xbf3cb34a, 0x28292c12, 0x2b322a70, 0x3a8920e1,
    0x0d04313b, 0x92b96796, 0x2264d217, 0xa175f3dd,
    0x477af378, 0xf3d7cc31, 0x742867ff, 0xe04c2a32,
    0xbfbdf54c, 0x78c04b0c, 0x8887a4f1, 0x77b63003,
    0x7d45a0df, 0x41ec95db, 0xa6508183, 0x58343aaa,
    0xca8380c1, 0x8bda0563, 0x853a1b72, 0x0525d346,
    0x9790cbe7, 0x30881bb6, 0xaa6fc729, 0xd36cfbd4,
    0x8d76b7fa, 0x29a871d5, 0xdae527bd, 0x3030e020,
    0x8bcf8ab7, 0x61cb2f97, 0x13846041, 0x4ecde00c,
    0xdef0ff00, 0xb71fa71b, 0xec2f31be, 0xe9520c24,
    0x80459473, 0x30300658, 0xa3805ad8, 0xb47eafd2,
    0x04375a5f, 0x06092cf1, 0x6894ac58, 0xfdfec525,
    0x0d0889b2, 0x1b804608, 0x1f1f142c, 0xf87ba8c9,
    0x26800347, 0xf26f3d19, 0xe558e220, 0x145dfa14,
    0xba317016, 0x3bb06f65, 0x07cf7309, 0xa429c442,
    0xcce3604d, 0x727f694c, 0x28371b89, 0xd86d80a6,
    0x2c77943e, 0x60cc8df9, 0x84e9e09c, 0xf31b7067,
    0xd0158393, 0x29bc8cd8, 0xd88bc6a1, 0xa33f98a8,
    0xf2340773, 0xab695f5f, 0x28c4f872, 0xcd486133,
    0x167f7cdf, 0x531faf43, 0xa0847374, 0xd5004438,
    0xbf6e3c3c, 0x2e720484, 0xb75eee52, 0x10fef2a7,
    0x63e38812, 0xba96765b, 0x8a244e41, 0xa6120d37,
    0x15154a10, 0x117869f5, 0x199f5d1c, 0xd6a89007,
    0x08182d25, 0x48151314, 0xbef97a60, 0xa9f8c8e8,
    0xf5b1d037, 0x913062cd, 0x8b1d5f46, 0x81eee004,
    0x12f85444, 0xafb969ab, 0xde74a38f, 0x1287118d,
    0x8c654586, 0xd0c0a92c, 0x42c7e3b6, 0x6c59f1de,
    0xc7c22556, 0x1aca992f, 0x0583a104, 0x424053a5,
    0x53232a5d, 0x2e9907f8, 0x9031a20b, 0x146267d7,
    0x8af06791, 0x4abffacd, 0x688f40e6, 0x80e8d8f4,
    0x976b02b3, 0x33de017b, 0x479c8943, 0xd8a744cb,
    0x412c7311, 0x2b1dee02, 0x649429ba, 0x191d3111,
    0x041743de, 0x0c67c2a4, 0x4140e20f, 0x350b9685,
    0xc7c2eea9, 0x16847488, 0xb3027037, 0x38c81979,
    0x166164a7, 0x7198e5a1, 0x0414241b, 0xb92b29bf,
    0xe50dbeb4, 0xb711c032, 0xbf398b73, 0x0abed4a6,
    0x9a048545, 0x94cdafe3, 0x54659149, 0x20ec4cdf,
    0x6d0a8b14, 0x271b812f, 0xdbee2ab6, 0x811a8a74,
    0xc5449582, 0x855cdd1e, 0x12b06e17, 0x345a572b,
    0xbad98795, 0xcdc7038c, 0x1b8d0e61, 0x000000a5,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* hardcoded here without a bitstream parser helper
 */
static VAPictureParameterBufferH264 pic_param[2] = {
    {
CurrPic:
        {
            0, 0, 8, 0, 0
        },
ReferenceFrames:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        picture_width_in_mbs_minus1: 10,
        picture_height_in_mbs_minus1: 8,
        bit_depth_luma_minus8: 0,
        bit_depth_chroma_minus8: 0,
        num_ref_frames: 7,
        {
            {
                chroma_format_idc: 1,
                residual_colour_transform_flag: 0,
                gaps_in_frame_num_value_allowed_flag: 0,
                frame_mbs_only_flag: 1,
                mb_adaptive_frame_field_flag: 0,
                direct_8x8_inference_flag: 1,
                MinLumaBiPredSize8x8: 0,
                log2_max_frame_num_minus4: 1,
                pic_order_cnt_type: 1,
                log2_max_pic_order_cnt_lsb_minus4: 0,
                delta_pic_order_always_zero_flag: 1,
            }
        },
        num_slice_groups_minus1: 0,
        slice_group_map_type: 0,
        slice_group_change_rate_minus1: 0,
        pic_init_qp_minus26: 0,
        pic_init_qs_minus26: 0,
        chroma_qp_index_offset: 0,
        second_chroma_qp_index_offset: 0,
        {
            {
                entropy_coding_mode_flag: 0,
                weighted_pred_flag: 0,
                weighted_bipred_idc: 0,
                transform_8x8_mode_flag: 0,
                field_pic_flag: 0,
                constrained_intra_pred_flag: 0,
                pic_order_present_flag: 0,
                deblocking_filter_control_present_flag: 0,
                redundant_pic_cnt_present_flag: 0,
                reference_pic_flag: 1,
            }
        },
        frame_num: 0
    },
    {
CurrPic:
        {
            1, 1, 8, 1, 1
        },
ReferenceFrames:
        {
            {0, 0, 8, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        picture_width_in_mbs_minus1: 10,
        picture_height_in_mbs_minus1: 8,
        bit_depth_luma_minus8: 0,
        bit_depth_chroma_minus8: 0,
        num_ref_frames: 7,
        {
            {
                chroma_format_idc: 1,
                residual_colour_transform_flag: 0,
                gaps_in_frame_num_value_allowed_flag: 0,
                frame_mbs_only_flag: 1,
                mb_adaptive_frame_field_flag: 0,
                direct_8x8_inference_flag: 1,
                MinLumaBiPredSize8x8: 0,
                log2_max_frame_num_minus4: 1,
                pic_order_cnt_type: 1,
                log2_max_pic_order_cnt_lsb_minus4: 0,
                delta_pic_order_always_zero_flag: 1,
            }
        },
        num_slice_groups_minus1: 0,
        slice_group_map_type: 0,
        slice_group_change_rate_minus1: 0,
        pic_init_qp_minus26: 0,
        pic_init_qs_minus26: 0,
        chroma_qp_index_offset: 0,
        second_chroma_qp_index_offset: 0,
        {
            {
                entropy_coding_mode_flag: 0,
                weighted_pred_flag: 0,
                weighted_bipred_idc: 0,
                transform_8x8_mode_flag: 0,
                field_pic_flag: 0,
                constrained_intra_pred_flag: 0,
                pic_order_present_flag: 0,
                deblocking_filter_control_present_flag: 0,
                redundant_pic_cnt_present_flag: 0,
                reference_pic_flag: 1,
            }
        },
        frame_num: 1
    }
};

static VAIQMatrixBufferH264 iq_matrix[2] = {
    {
ScalingList4x4:
        {
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}
        },
ScalingList8x8:
        {{0}}
    },
    {
ScalingList4x4:
        {
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}
        },
ScalingList8x8:
        {{0}}
    }
};

static VASliceParameterBufferH264 slice_param_surface0[4] = {
    {
        slice_data_size: 1101,
        slice_data_offset: 0,
        slice_data_flag: 0,
        slice_data_bit_offset: 28,
        first_mb_in_slice: 0,
        slice_type: 2,
        direct_spatial_mv_pred_flag: 0,
        num_ref_idx_l0_active_minus1: 0,
        num_ref_idx_l1_active_minus1: 0,
        cabac_init_idc: 0,
        slice_qp_delta: 6,
        disable_deblocking_filter_idc: 0,
        slice_alpha_c0_offset_div2: 0,
        slice_beta_offset_div2: 0,
RefPicList0:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
RefPicList1:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        luma_log2_weight_denom: 0,
        chroma_log2_weight_denom: 0,
        luma_weight_l0_flag: 0,
luma_weight_l0:
        {0},
luma_offset_l0:
        {0},
        chroma_weight_l0_flag: 0,
chroma_weight_l0:
        {{0, 0}},
chroma_offset_l0:
        {{0, 0}},
        luma_weight_l1_flag: 0,
luma_weight_l1:
        {0},
luma_offset_l1:
        {0},
        chroma_weight_l1_flag: 0,
chroma_weight_l1:
        {{0, 0}},
chroma_offset_l1:
        {{0, 0}}
    },
    {
        slice_data_size: 1133,
        slice_data_offset: 1101,
        slice_data_flag: 0,
        slice_data_bit_offset: 32,
        first_mb_in_slice: 22,
        slice_type: 2,
        direct_spatial_mv_pred_flag: 0,
        num_ref_idx_l0_active_minus1: 0,
        num_ref_idx_l1_active_minus1: 0,
        cabac_init_idc: 0,
slice_qp_delta:
        -1,
            disable_deblocking_filter_idc: 0,
            slice_alpha_c0_offset_div2: 0,
            slice_beta_offset_div2: 0,
    RefPicList0:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
RefPicList1:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        luma_log2_weight_denom: 0,
        chroma_log2_weight_denom: 0,
        luma_weight_l0_flag: 0,
luma_weight_l0:
        {0},
luma_offset_l0:
        {0},
        chroma_weight_l0_flag: 0,
chroma_weight_l0:
        {{0, 0}},
chroma_offset_l0:
        {{0, 0}},
        luma_weight_l1_flag: 0,
luma_weight_l1:
        {0},
luma_offset_l1:
        {0},
        chroma_weight_l1_flag: 0,
chroma_weight_l1:
        {{0, 0}},
chroma_offset_l1:
        {{0, 0}}
    },
    {
        slice_data_size: 1115,
        slice_data_offset: 2234,
        slice_data_flag: 0,
        slice_data_bit_offset: 34,
        first_mb_in_slice: 46,
        slice_type: 2,
        direct_spatial_mv_pred_flag: 0,
        num_ref_idx_l0_active_minus1: 0,
        num_ref_idx_l1_active_minus1: 0,
        cabac_init_idc: 0,
slice_qp_delta:
        -1,
            disable_deblocking_filter_idc: 0,
            slice_alpha_c0_offset_div2: 0,
            slice_beta_offset_div2: 0,
    RefPicList0:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
RefPicList1:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        luma_log2_weight_denom: 0,
        chroma_log2_weight_denom: 0,
        luma_weight_l0_flag: 0,
luma_weight_l0:
        {0},
luma_offset_l0:
        {0},
        chroma_weight_l0_flag: 0,
chroma_weight_l0:
        {{0, 0}},
chroma_offset_l0:
        {{0, 0}},
        luma_weight_l1_flag: 0,
luma_weight_l1:
        {0},
luma_offset_l1:
        {0},
        chroma_weight_l1_flag: 0,
chroma_weight_l1:
        {{0, 0}},
chroma_offset_l1:
        {{0, 0}}
    },
    {
        slice_data_size: 951,
        slice_data_offset: 3349,
        slice_data_flag: 0,
        slice_data_bit_offset: 36,
        first_mb_in_slice: 76,
        slice_type: 2,
        direct_spatial_mv_pred_flag: 0,
        num_ref_idx_l0_active_minus1: 0,
        num_ref_idx_l1_active_minus1: 0,
        cabac_init_idc: 0,
slice_qp_delta:
        -1,
            disable_deblocking_filter_idc: 0,
            slice_alpha_c0_offset_div2: 0,
            slice_beta_offset_div2: 0,
    RefPicList0:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
RefPicList1:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        luma_log2_weight_denom: 0,
        chroma_log2_weight_denom: 0,
        luma_weight_l0_flag: 0,
luma_weight_l0:
        {0},
luma_offset_l0:
        {0},
        chroma_weight_l0_flag: 0,
chroma_weight_l0:
        {{0, 0}},
chroma_offset_l0:
        {{0, 0}},
        luma_weight_l1_flag: 0,
luma_weight_l1:
        {0},
luma_offset_l1:
        {0},
        chroma_weight_l1_flag: 0,
chroma_weight_l1:
        {{0, 0}},
chroma_offset_l1:
        {{0, 0}}
    }
};

static VASliceParameterBufferH264 slice_param_surface1[2] = {
    {
        slice_data_size: 1091,
        slice_data_offset: 0,
        slice_data_flag: 0,
        slice_data_bit_offset: 32,
        first_mb_in_slice: 0,
        slice_type: 0,
        direct_spatial_mv_pred_flag: 0,
        num_ref_idx_l0_active_minus1: 0,
        num_ref_idx_l1_active_minus1: 0,
        cabac_init_idc: 0,
slice_qp_delta:
        -1,
            disable_deblocking_filter_idc: 0,
            slice_alpha_c0_offset_div2: 0,
            slice_beta_offset_div2: 0,
    RefPicList0:
        {
            {0, 0, 8, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
RefPicList1:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        luma_log2_weight_denom: 0,
        chroma_log2_weight_denom: 0,
        luma_weight_l0_flag: 0,
luma_weight_l0:
        {0},
luma_offset_l0:
        {0},
        chroma_weight_l0_flag: 0,
chroma_weight_l0:
        {{0, 0}},
chroma_offset_l0:
        {{0, 0}},
        luma_weight_l1_flag: 0,
luma_weight_l1:
        {0},
luma_offset_l1:
        {0},
        chroma_weight_l1_flag: 0,
chroma_weight_l1:
        {{0, 0}},
chroma_offset_l1:
        {{0, 0}}
    },
    {
        slice_data_size: 138,
        slice_data_offset: 1091,
        slice_data_flag: 0,
        slice_data_bit_offset: 44,
        first_mb_in_slice: 92,
        slice_type: 0,
        direct_spatial_mv_pred_flag: 0,
        num_ref_idx_l0_active_minus1: 0,
        num_ref_idx_l1_active_minus1: 0,
        cabac_init_idc: 0,
slice_qp_delta:
        -1,
            disable_deblocking_filter_idc: 0,
            slice_alpha_c0_offset_div2: 0,
            slice_beta_offset_div2: 0,
    RefPicList0:
        {
            {0, 0, 8, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
RefPicList1:
        {
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0},
            {VA_INVALID_SURFACE, 0, 1, 0, 0}
        },
        luma_log2_weight_denom: 0,
        chroma_log2_weight_denom: 0,
        luma_weight_l0_flag: 0,
luma_weight_l0:
        {0},
luma_offset_l0:
        {0},
        chroma_weight_l0_flag: 0,
chroma_weight_l0:
        {{0, 0}},
chroma_offset_l0:
        {{0, 0}},
        luma_weight_l1_flag: 0,
luma_weight_l1:
        {0},
luma_offset_l1:
        {0},
        chroma_weight_l1_flag: 0,
chroma_weight_l1:
        {{0, 0}},
chroma_offset_l1:
        {{0, 0}}
    }
};


#define CLIP_WIDTH  176
#define CLIP_HEIGHT 144

#define AVC_SURFACE_NUM 2

#define IF_EQUAL(a, b)         (a == b)
#define IF_EQUAL_M(a, b, c, d) (a == b && a == c && a == d && b == c && b == d && c == d)

void dumpMvs(VADecStreamOutData *streamout, int mbIndex)
{
    if (IF_EQUAL_M(streamout->QW8[0].MvFwd_x, streamout->QW8[1].MvFwd_x, streamout->QW8[2].MvFwd_x, streamout->QW8[2].MvFwd_x)
        && IF_EQUAL_M(streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_y, streamout->QW8[2].MvFwd_y, streamout->QW8[2].MvFwd_y)) {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*          %3s            *\n", streamout->DW0.MbSkipFlag != 0 ? "Skip" : "    ");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*%3d,%3d->0                *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y);
        printf("*                          *\n");
        printf("****************************\n\n");
    } else if ((IF_EQUAL(streamout->QW8[1].MvFwd_x, streamout->QW8[3].MvFwd_x)
                && IF_EQUAL(streamout->QW8[1].MvFwd_y, streamout->QW8[3].MvFwd_y))
               && (IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[2].MvFwd_x)
                   && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[2].MvFwd_y))
               && !(IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[1].MvFwd_x)
                    && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_y))) {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*%3d,%3d->0  * %3d,%3d->0  *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_x, streamout->QW8[1].MvFwd_y);
        printf("*            *             *\n");
        printf("****************************\n\n");
    } else if ((IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[1].MvFwd_x)
                && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_y))
               && (IF_EQUAL(streamout->QW8[2].MvFwd_x, streamout->QW8[3].MvFwd_x)
                   && IF_EQUAL(streamout->QW8[2].MvFwd_y, streamout->QW8[3].MvFwd_y))
               && !(IF_EQUAL(streamout->QW8[0].MvFwd_x, streamout->QW8[2].MvFwd_x)
                    && IF_EQUAL(streamout->QW8[0].MvFwd_y, streamout->QW8[2].MvFwd_y))) {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*%3d,%3d->0                *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y);
        printf("*                          *\n");
        printf("****************************\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*                          *\n");
        printf("*%3d,%3d->0                *\n", streamout->QW8[2].MvFwd_x, streamout->QW8[2].MvFwd_y);
        printf("*                          *\n");
        printf("****************************\n\n");
    } else {
        printf("*************MB:%2d*********\n", mbIndex);
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*%3d,%3d->0  * %3d,%3d->0  *\n", streamout->QW8[0].MvFwd_x, streamout->QW8[0].MvFwd_y, streamout->QW8[1].MvFwd_x, streamout->QW8[1].MvFwd_y);
        printf("*            *             *\n");
        printf("***************************\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*            *             *\n");
        printf("*%3d,%3d->0  * %3d,%3d->0  *\n", streamout->QW8[2].MvFwd_x, streamout->QW8[2].MvFwd_y, streamout->QW8[3].MvFwd_x, streamout->QW8[3].MvFwd_y);
        printf("*            *             *\n");
        printf("****************************\n\n");
    }
}

int main(int argc, char **argv)
{
    VAEntrypoint entrypoints[5];
    int num_entrypoints, vld_entrypoint;
    VAConfigAttrib attrib;
    VAConfigID config_id;
    VASurfaceID surface_ids[AVC_SURFACE_NUM];
    VAContextID context_id;
    VABufferID pic_param_buf, iqmatrix_buf, slice_param_buf, slice_data_buf, streamout_buf;
    VABufferID tmp_buff_ids[5];
    int major_ver, minor_ver;
    VADisplay   va_dpy;
    VAStatus va_status;
    int is_dump_streamout = 0;
    int surface_index;
    VASurfaceStatus surface_status;
    unsigned char *pbuf;
    unsigned int mb_counts = ((CLIP_WIDTH + 15) / 16) * ((CLIP_HEIGHT + 15) / 16);
    unsigned int streamout_buffsize = mb_counts * sizeof(VADecStreamOutData);

    va_init_display_args(&argc, argv);

    if (argc > 1)
        is_dump_streamout = 1;

    va_dpy = va_open_display();
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    assert(va_status == VA_STATUS_SUCCESS);

    va_status = vaQueryConfigEntrypoints(va_dpy, VAProfileH264Main, entrypoints,
                                         &num_entrypoints);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    for (vld_entrypoint = 0; vld_entrypoint < num_entrypoints; vld_entrypoint++) {
        if (entrypoints[vld_entrypoint] == VAEntrypointVLD)
            break;
    }
    if (vld_entrypoint == num_entrypoints) {
        /* not find VLD entry point */
        assert(0);
    }

    /* Assuming finding VLD, find out the format for the render target */
    attrib.type = VAConfigAttribRTFormat;
    vaGetConfigAttributes(va_dpy, VAProfileH264Main, VAEntrypointVLD,
                          &attrib, 1);
    if ((attrib.value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        assert(0);
    }

    va_status = vaCreateConfig(va_dpy, VAProfileH264Main, VAEntrypointVLD,
                               &attrib, 1, &config_id);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    va_status = vaCreateSurfaces(
                    va_dpy,
                    VA_RT_FORMAT_YUV420, CLIP_WIDTH, CLIP_HEIGHT,
                    &surface_ids[0], 2,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    /* Create a context for this decode pipe */
    va_status = vaCreateContext(va_dpy, config_id,
                                CLIP_WIDTH,
                                ((CLIP_HEIGHT + 15) / 16) * 16,
                                VA_PROGRESSIVE,
                                &surface_ids[0],
                                2,
                                &context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");

    for (surface_index = 0 ; surface_index < AVC_SURFACE_NUM; surface_index++) {
        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VAPictureParameterBufferType,
                                   sizeof(VAPictureParameterBufferH264),
                                   1, &pic_param[surface_index],
                                   &pic_param_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VAIQMatrixBufferType,
                                   sizeof(VAIQMatrixBufferH264),
                                   1, &iq_matrix[surface_index],
                                   &iqmatrix_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        if (surface_index == 0) {
            va_status = vaCreateBuffer(va_dpy, context_id,
                                       VASliceParameterBufferType,
                                       sizeof(VASliceParameterBufferH264),
                                       4,
                                       &slice_param_surface0[0], &slice_param_buf);
        } else {
            va_status = vaCreateBuffer(va_dpy, context_id,
                                       VASliceParameterBufferType,
                                       sizeof(VASliceParameterBufferH264),
                                       2,
                                       &slice_param_surface1[0], &slice_param_buf);
        }
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VASliceDataBufferType,
                                   surface_index == 0 ? sizeof(avc_clip) : sizeof(avc_clip1),
                                   1,
                                   surface_index == 0 ? avc_clip : avc_clip1,
                                   &slice_data_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        /* Create StreamOut va buffer */
        va_status = vaCreateBuffer(va_dpy, context_id,
                                   VADecodeStreamoutBufferType,
                                   streamout_buffsize,
                                   1, NULL, &streamout_buf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");

        tmp_buff_ids[0] = pic_param_buf;
        tmp_buff_ids[1] = iqmatrix_buf;
        tmp_buff_ids[2] = slice_param_buf;
        tmp_buff_ids[3] = slice_data_buf;
        tmp_buff_ids[4] = streamout_buf;

        va_status = vaBeginPicture(va_dpy, context_id, surface_ids[surface_index]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");

        va_status = vaRenderPicture(va_dpy, context_id, tmp_buff_ids, 5);
        CHECK_VASTATUS(va_status, "vaRenderPicture");

        va_status = vaEndPicture(va_dpy, context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");

        va_status = vaSyncSurface(va_dpy, surface_ids[surface_index]);
        CHECK_VASTATUS(va_status, "vaSyncSurface");

        va_status = vaQuerySurfaceStatus(va_dpy, surface_ids[surface_index], &surface_status);//to check surface_status if needed
        CHECK_VASTATUS(va_status, "vaQuerySurfaceStatus");

        /*map streamout buffer to dump*/
        VADecStreamOutData *dec_streamout_buf = (VADecStreamOutData *)malloc(streamout_buffsize);
        if (NULL == dec_streamout_buf) {
            printf("Failed to malloc for dec streamout buf.\n");
            assert(0);
        }
        va_status = vaMapBuffer(va_dpy, streamout_buf, (void **)(&pbuf));
        CHECK_VASTATUS(va_status, "vaMapBuffer");
        memcpy(dec_streamout_buf, pbuf, streamout_buffsize);// to check streamout data for usage
        va_status = vaUnmapBuffer(va_dpy, streamout_buf);
        CHECK_VASTATUS(va_status, "vaUnmapBuffer");

        if (is_dump_streamout && surface_index != 0) {
            //dump streamout buffer to local file
            VADecStreamOutData *temp_dec_streamout_buf = dec_streamout_buf;
            unsigned int i;
            for (i = 0; i < mb_counts && temp_dec_streamout_buf != NULL; i++) {
                dumpMvs(temp_dec_streamout_buf++, i);
            }
        }

        if (dec_streamout_buf) {
            free(dec_streamout_buf);
        }
    }

    printf("press any key to exit\n");
    getchar();

    vaDestroySurfaces(va_dpy, surface_ids, 2);
    vaDestroyConfig(va_dpy, config_id);
    vaDestroyContext(va_dpy, context_id);

    vaTerminate(va_dpy);
    va_close_display(va_dpy);
    return 0;
}
