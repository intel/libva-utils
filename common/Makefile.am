# Copyright (c) 2012 Intel Corporation. All Rights Reserved.
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

noinst_LTLIBRARIES = libva-display.la

libva_display_cflags = \
	$(LIBVA_CFLAGS)	\
	$(NULL)

libva_display_libs = \
	$(LIBVA_LDFLAGS) \
	$(NULL)

source_c		= va_display.c
source_h		= va_display.h loadsurface.h loadsurface_yuv.h

if USE_X11
source_c		+= va_display_x11.c
libva_display_cflags	+= $(X11_CFLAGS) $(LIBVA_X11_CFLAGS)
libva_display_libs	+= $(X11_LIBS) $(LIBVA_X11_LIBS)
endif

if USE_DRM
source_c		+= va_display_drm.c
libva_display_cflags	+= $(LIBVA_DRM_CFLAGS)
libva_display_libs	+= $(LIBVA_DRM_LIBS)
endif

if USE_WAYLAND
source_c		+= va_display_wayland.c
libva_display_cflags	+= $(WAYLAND_CFLAGS) $(LIBVA_WAYLAND_CFLAGS)
libva_display_libs	+= $(WAYLAND_LIBS) $(LIBVA_WAYLAND_LIBS)
endif

libva_display_la_SOURCES= $(source_c)
noinst_HEADERS		= $(source_h)
libva_display_la_CFLAGS	= $(libva_display_cflags)
libva_display_la_LIBADD	= $(libva_display_libs)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = \
	loadsurface.h		\
	loadsurface_yuv.h	\
	$(NULL)
