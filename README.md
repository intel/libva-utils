[![Stories in Ready](https://badge.waffle.io/intel/libva-utils.png?label=ready&title=Ready)](http://waffle.io/intel/libva-utils)
[![Build Status](https://travis-ci.org/intel/libva-utils.svg?branch=master)](https://travis-ci.org/intel/libva-utils)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/11613/badge.svg)](https://scan.coverity.com/projects/intel-libva-utils)

# Libva-utils Project

libva-utils is a collection of utilities and examples to exercise VA-API in accordance with the libva project. --enable-tests (default = no) provides a suite of unit-tests based on Google Test Framework. A driver implementation is necessary to properly operate.

VA-API is an open-source library and API specification, which provides access to graphics hardware acceleration capabilities
for video processing. It consists of a main library and driver-specific acceleration backends for each supported hardware vendor.

If you would like to contribute to libva, check our [Contributing guide](https://github.com/intel/libva-utils/blob/master/CONTRIBUTING.md).

We also recommend taking a look at the ['janitorial' bugs](https://github.com/intel/libva-utils/issues?q=is%3Aopen+is%3Aissue+label%3AJanitorial) in our list of open issues as these bugs can be solved without an extensive knowledge of libva-utils.

We would love to help you start contributing!

The libva-utils development team can be reached via github issues.

# Build and Install Libva-utils

### Install Libva
You could refer to https://github.com/intel/libva to install Libva

### Build Libva-utils
Take latest libva-utils version:
```
git clone https://github.com/intel/libva-utils.git
cd libva-utils
```

Build libva-utils by autogen. You could add ```--enable-tests``` to run unit test
```
./autogen.sh or ./autogen.sh --enable-tests
make
sudo make install
```

or build using Meson
```
mkdir build
cd build
meson .. or meson .. -Dtests=true
ninja
sudo ninja install
```


### Validate your environment
You could run ```vainfo``` to check your media stack environment is correct or not as below.
```
sys@KBL:~/github/libva-utils$ vainfo
Trying display: drm
libva info: VA-API version 1.14.0
libva info: Trying to open /usr/lib/x86_64-linux-gnu/dri/iHD_drv_video.so
libva info: Found init function __vaDriverInit_1_14
libva info: va_openDriver() returns 0
vainfo: VA-API version: 1.18 (libva 2.18.0.pre1)
vainfo: Driver version: Intel iHD driver for Intel(R) Gen Graphics - 22.3.1 ()
vainfo: Supported profile and entrypoints
      VAProfileMPEG2Simple            : VAEntrypointVLD
      VAProfileMPEG2Main              : VAEntrypointVLD
      VAProfileH264Main               : VAEntrypointVLD
      VAProfileH264Main               : VAEntrypointEncSliceLP
      VAProfileH264High               : VAEntrypointVLD
      VAProfileH264High               : VAEntrypointEncSliceLP
      VAProfileJPEGBaseline           : VAEntrypointVLD
      VAProfileJPEGBaseline           : VAEntrypointEncPicture
      VAProfileH264ConstrainedBaseline: VAEntrypointVLD
      VAProfileH264ConstrainedBaseline: VAEntrypointEncSliceLP
      VAProfileVP8Version0_3          : VAEntrypointVLD
      VAProfileHEVCMain               : VAEntrypointVLD
      VAProfileHEVCMain10             : VAEntrypointVLD
      VAProfileVP9Profile0            : VAEntrypointVLD
      VAProfileVP9Profile2            : VAEntrypointVLD
      ...
```
