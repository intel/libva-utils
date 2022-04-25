# Copyright (c) 2022 Intel Corporation. All Rights Reserved.
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

bin_PROGRAMS = vavpp vppscaling_csc vppdenoise vppsharpness vppchromasitting vppblending vppscaling_n_out_usrptr vacopy vpp3dlut vpphdr_tm

AM_CPPFLAGS = \
	-Wall					\
	$(LIBVA_CFLAGS)				\
	-I$(top_srcdir)/common			\
	$(NULL)

if USE_SSP
AM_CPPFLAGS += -fstack-protector
endif

TEST_LIBS = \
	$(LIBVA_LIBS)				\
	$(top_builddir)/common/libva-display.la	\
	$(NULL)

vavpp_SOURCES = vavpp.cpp
vavpp_LDADD   = $(TEST_LIBS)

vppscaling_csc_SOURCES = vppscaling_csc.cpp
vppscaling_csc_LDADD = $(TEST_LIBS)

vppdenoise_SOURCES = vppdenoise.cpp
vppdenoise_LDADD   = $(TEST_LIBS)

vppsharpness_SOURCES = vppsharpness.cpp
vppsharpness_LDADD   = $(TEST_LIBS)

vppchromasitting_SOURCES = vppchromasitting.cpp
vppchromasitting_LDADD   = $(TEST_LIBS)

vppblending_SOURCES = vppblending.cpp
vppblending_LDADD   = $(TEST_LIBS)

vppscaling_n_out_usrptr_SOURCES = vppscaling_n_out_usrptr.cpp
vppscaling_n_out_usrptr_LDADD   = $(TEST_LIBS)

vacopy_SOURCES = vacopy.cpp
vacopy_LDADD = $(TEST_LIBS)

vpp3dlut_SOURCES = vpp3dlut.cpp
vpp3dlut_LDADD   = $(TEST_LIBS)

vpphdr_tm_SOURCES = vpphdr_tm.cpp
vpphdr_tm_LDADD   = $(TEST_LIBS)

valgrind:(bin_PROGRAMS)
	for a in(bin_PROGRAMS); do \
		valgrind --leak-check=full --show-reachable=yes .libs/$$a; \
	done
