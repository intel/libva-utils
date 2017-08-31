/*
 * Copyright (C) 2016 Intel Corporation. All Rights Reserved.
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

#ifndef test_streamable_h
#define test_streamable_h

#include <iostream>
#include <va/va.h>

inline std::ostream&
operator<<(std::ostream& os, const VAProfile& profile)
{
    os << static_cast<int>(profile) << ":";

    switch(profile) {
    case VAProfileNone:
        return os << "VAProfileNone";
    case VAProfileMPEG2Simple:
        return os << "VAProfileMPEG2Simple";
    case VAProfileMPEG2Main:
        return os << "VAProfileMPEG2Main";
    case VAProfileMPEG4Simple:
        return os << "VAProfileMPEG4Simple";
    case VAProfileMPEG4AdvancedSimple:
        return os << "VAProfileMPEG4AdvancedSimple";
    case VAProfileMPEG4Main:
        return os << "VAProfileMPEG4Main";
    case VAProfileVC1Simple:
        return os << "VAProfileVC1Simple";
    case VAProfileVC1Main:
        return os << "VAProfileVC1Main";
    case VAProfileVC1Advanced:
        return os << "VAProfileVC1Advanced";
    case VAProfileH263Baseline:
        return os << "VAProfileH263Baseline";
    case VAProfileJPEGBaseline:
        return os << "VAProfileJPEGBaseline";
    case VAProfileVP8Version0_3:
        return os << "VAProfileVP8Version0_3";
    case VAProfileHEVCMain:
        return os << "VAProfileHEVCMain";
    case VAProfileHEVCMain10:
        return os << "VAProfileHEVCMain10";
    case VAProfileVP9Profile0:
        return os << "VAProfileVP9Profile0";
    case VAProfileVP9Profile1:
        return os << "VAProfileVP9Profile1";
    case VAProfileVP9Profile2:
        return os << "VAProfileVP9Profile2";
    case VAProfileVP9Profile3:
        return os << "VAProfileVP9Profile3";
    case VAProfileH264ConstrainedBaseline:
        return os << "VAProfileH264ConstrainedBaseline";
    case VAProfileH264High:
        return os << "VAProfileH264High";
    case VAProfileH264Main:
        return os << "VAProfileH264Main";
    case VAProfileH264MultiviewHigh:
        return os << "VAProfileH264MultiviewHigh";
    case VAProfileH264StereoHigh:
        return os << "VAProfileH264StereoHigh";
    default:
        return os << "Unknown VAProfile";
    }
}

inline std::ostream&
operator<<(std::ostream& os, const VAEntrypoint& entrypoint)
{
    os << static_cast<int>(entrypoint) << ":";

    switch(entrypoint) {
    case VAEntrypointVLD:
        return os << "VAEntrypointVLD";
    case VAEntrypointIZZ:
        return os << "VAEntrypointIZZ";
    case VAEntrypointIDCT:
        return os << "VAEntrypointIDCT";
    case VAEntrypointMoComp:
        return os << "VAEntrypointMoComp";
    case VAEntrypointDeblocking:
        return os << "VAEntrypointDeblocking";
    case VAEntrypointVideoProc:
        return os << "VAEntrypointVideoProc";
    case VAEntrypointEncSlice:
        return os << "VAEntrypointEncSlice";
    case VAEntrypointEncSliceLP:
        return os << "VAEntrypointEncSliceLP";
    case VAEntrypointEncPicture:
        return os << "VAEntrypointEncPicture";
    case VAEntrypointFEI:
        return os << "VAEntrypointFEI";
    default:
        return os << "Unknown VAEntrypoint";
    }
}

#endif
