# Copyright (c) 2007 Intel Corporation. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sub license, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice (including the
# next paragraph) shall be included in all copies or substantial portions
# of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
# IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
# ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# libva-utils package version number
# package version number follows libva package version number to
# comply with the same version used for released
#
# - major version is libva_major_version
# - minor version is libva_minor_version
# - micro version is libva_micro_version
# - pre version is libva_pre_version, usually development version
m4_define([libva_utils_major_version], [2])
m4_define([libva_utils_minor_version], [22])
m4_define([libva_utils_micro_version], [0])
m4_define([libva_utils_pre_version],   [0])

m4_define([libva_utils_version],
          [libva_utils_major_version.libva_utils_minor_version.libva_utils_micro_version])
m4_if(libva_utils_pre_version, [0], [], [
m4_append([libva_utils_version], libva_utils_pre_version, [.pre])
])

# libva minimum version requirement, at this released version
# libva-utils was created
m4_define([libva_api_min_version], [1.1.0])

# Wayland minimum version number
m4_define([wayland_api_version], [1.0.0])

AC_PREREQ(2.57)
AC_INIT([libva_utils],
        [libva_utils_version],
        [https://github.com/intel/libva-utils/issues/new],
        [libva-utils],
        [https://github.com/intel/libva-utils])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([dist-bzip2])

AC_CONFIG_HEADERS([config.h])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

TODAY="`LC_ALL=C date +'%a, %d %b %Y %X %z'`"
AC_SUBST(TODAY)

LIBVA_UTILS_MAJOR_VERSION=libva_utils_major_version
LIBVA_UTILS_MINOR_VERSION=libva_utils_minor_version
LIBVA_UTILS_MICRO_VERSION=libva_utils_micro_version
LIBVA_UTILS_VERSION=libva_utils_version
AC_SUBST(LIBVA_UTILS_MAJOR_VERSION)
AC_SUBST(LIBVA_UTILS_MINOR_VERSION)
AC_SUBST(LIBVA_UTILS_MICRO_VERSION)
AC_SUBST(LIBVA_UTILS_VERSION)

AC_ARG_ENABLE(drm,
    [AC_HELP_STRING([--enable-drm],
                    [build with VA/DRM API support @<:@default=yes@:>@])],
    [], [enable_drm="yes"])

AC_ARG_ENABLE(x11,
    [AC_HELP_STRING([--enable-x11],
                    [build with VA/X11 API support @<:@default=auto@:>@])],
    [enable_x11="$enableval"],
    [enable_x11="auto"])

AC_ARG_ENABLE([wayland],
    [AC_HELP_STRING([--enable-wayland],
                    [build with VA/Wayland API support @<:@default=auto@:>@])],
    [enable_wayland="$enableval"],
    [enable_wayland="auto"])

AC_ARG_ENABLE([tests],
    [AC_HELP_STRING([--enable-tests],
                    [build unit tests @<:@default=no@:>@])],
    [], [enable_tests="no"])


LT_INIT
AC_DISABLE_STATIC
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG

AC_HEADER_STDC
AC_SYS_LARGEFILE

# Check for __attribute__((visibility()))
AC_CACHE_CHECK([whether __attribute__((visibility())) is supported],
    ac_cv_have_gnuc_visibility_attribute,
    [cat > conftest.c <<EOF
int foo __attribute__ ((visibility ("hidden"))) = 1;
int bar __attribute__ ((visibility ("protected"))) = 1;
EOF
    ac_cv_have_gnuc_visibility_attribute="no"
    if ${CC-cc} -Werror -S conftest.c -o conftest.s >/dev/null 2>&1; then
        if grep '\.hidden.*foo' conftest.s >/dev/null; then
            if grep '\.protected.*bar' conftest.s >/dev/null; then
                ac_cv_have_gnuc_visibility_attribute="yes"
            fi
        fi
    fi
    rm -f conftest.[cs]
])
if test "$ac_cv_have_gnuc_visibility_attribute" = "yes"; then
    AC_DEFINE([HAVE_GNUC_VISIBILITY_ATTRIBUTE], [1],
              [Defined to 1 if GCC visibility attribute is supported])
fi

# Check for -fstack-protector
ssp_cc=yes
if test "X$CC-cc" != "X"; then
    AC_MSG_CHECKING([whether ${CC-cc} accepts -fstack-protector])
    ssp_old_cflags="$CFLAGS"
    CFLAGS="$CFLAGS -fstack-protector"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[alloca(100);]])], [], [ssp_cc=no])
    AC_MSG_RESULT([$ssp_cc])
    if test "X$ssp_cc" = "Xno"; then
        CFLAGS="$ssp_old_cflags"
    else
        AC_DEFINE([ENABLE_SSP_CC], 1, [Define if SSP C support is enabled.])
    fi
fi
AM_CONDITIONAL(USE_SSP, test "$ssp_cc" = "yes")

# Check for libva (for dynamic linking)
LIBVA_API_MIN_VERSION=libva_api_min_version
PKG_CHECK_MODULES([LIBVA], [libva >= $LIBVA_API_MIN_VERSION])
LIBVA_VERSION=`$PKG_CONFIG --variable=libva_version libva`
AC_SUBST(LIBVA_VERSION)
LIBVA_API_VERSION=`$PKG_CONFIG --modversion libva`
VA_MAJOR_VERSION=`echo "$LIBVA_API_VERSION" | cut -d'.' -f1`
VA_MINOR_VERSION=`echo "$LIBVA_API_VERSION" | cut -d'.' -f2`
VA_MICRO_VERSION=`echo "$LIBVA_API_VERSION" | cut -d'.' -f3`

# Check for DRM (mandatory)
PKG_CHECK_MODULES([LIBVA_DRM], [libdrm libva-drm = $LIBVA_API_VERSION])

USE_DRM="no"
if test "$enable_drm" = "yes"; then
    USE_DRM="yes"
    AC_DEFINE([HAVE_VA_DRM], [1], [Defined to 1 if VA/DRM API is supported])
fi
AM_CONDITIONAL(USE_DRM, test "$USE_DRM" = "yes")

# Check for X11
USE_X11="no"
if test "x$enable_x11" != "xno"; then
    PKG_CHECK_MODULES([X11],    [x11 libva-x11 = $LIBVA_API_VERSION],
        [USE_X11="yes"], [:])

    if test "x$USE_X11" = "xno" -a "x$enable_x11" = "xyes"; then
       AC_MSG_ERROR([X11 explicitly enabled, but $X11_PKG_ERRORS])
    fi

    if test "$USE_X11" = "yes"; then
       AC_DEFINE([HAVE_VA_X11], [1], [Defined to 1 if VA/X11 API is supported])
    fi
fi
AM_CONDITIONAL(USE_X11, test "$USE_X11" = "yes")

dnl Check for Wayland
WAYLAND_API_VERSION=wayland_api_version
AC_SUBST(WAYLAND_API_VERSION)

USE_WAYLAND="no"
if test "x$enable_wayland" != "xno"; then
    PKG_CHECK_MODULES([WAYLAND], [wayland-client >= wayland_api_version libva-wayland = $LIBVA_API_VERSION],
        [USE_WAYLAND="yes"], [:])

    if test "x$USE_WAYLAND" = "xno" -a "x$enable_wayland" = "xyes"; then
        AC_MSG_ERROR([wayland explicitly enabled, but $WAYLAND_PKG_ERRORS])
    fi

    if test "$USE_WAYLAND" = "yes"; then

        WAYLAND_PREFIX=`$PKG_CONFIG --variable=prefix wayland-client`
        AC_PATH_PROG([WAYLAND_SCANNER], [wayland-scanner],,
                     [${WAYLAND_PREFIX}/bin$PATH_SEPARATOR$PATH])

        AC_DEFINE([HAVE_VA_WAYLAND], [1],
                  [Defined to 1 if VA/Wayland API is supported])
    fi
fi

AM_CONDITIONAL(USE_WAYLAND, test "$USE_WAYLAND" = "yes")

pkgconfigdir=${libdir}/pkgconfig
AC_SUBST(pkgconfigdir)

dnl Check for builds without backend
if test "$USE_DRM:$USE_X11:$USE_WAYLAND" = "no:no:no"; then
    AC_MSG_ERROR([Please select at least one backend (DRM, X11, Wayland)])
fi

AM_CONDITIONAL(ENABLE_TESTS, test "$enable_tests" = "yes")

AC_OUTPUT([
    Makefile
    common/Makefile
    test/Makefile
    vainfo/Makefile
    encode/Makefile
    decode/Makefile
    putsurface/Makefile
    videoprocess/Makefile
    vendor/intel/Makefile
    vendor/intel/sfcsample/Makefile
])


# Print a small summary
BACKENDS=""
AS_IF([test $USE_DRM = yes], [BACKENDS="$BACKENDS drm"])
AS_IF([test $USE_X11 = yes], [BACKENDS="$BACKENDS x11"])
AS_IF([test $USE_WAYLAND = yes], [BACKENDS="$BACKENDS wayland"])

echo
echo "libva-utils - ${LIBVA_UTILS_VERSION}"
echo
echo Libva VA-API version ............. : $LIBVA_API_VERSION
echo Installation prefix .............. : $prefix
echo Extra window systems ............. : $BACKENDS
echo Enable Unit-tests .................... : $enable_tests
echo
