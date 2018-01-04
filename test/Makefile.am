AUTOMAKE_OPTIONS = subdir-objects

AM_CXXFLAGS =							\
	-I$(top_srcdir)/test/gtest/include			\
	$(NULL)

AM_CPPFLAGS =							\
	-DGTEST_HAS_PTHREAD=1					\
	-DGTEST_USE_OWN_TR1_TUPLE=0				\
	-DGTEST_LANG_CXX11=1					\
	-DGTEST_HAS_TR1_TUPLE=1					\
	-DPTHREADS						\
	-DVA_DRIVERS_PATH="\"$(LIBVA_DRIVERS_PATH)\""		\
	-std=c++11						\
	$(NULL)

AM_LDFLAGS =							\
	-pthread						\
	$(NULL)

# libgtest
noinst_LTLIBRARIES = libgtest.la

libgtest_la_SOURCES =						\
	gtest/src/gtest-all.cc					\
	gtest/src/gtest_main.cc					\
	$(NULL)

libgtest_la_CXXFLAGS =						\
	-I$(top_srcdir)/test/gtest				\
	$(AM_CXXFLAGS)						\
	$(NULL)

EXTRA_DIST =							\
	gtest/docs						\
	gtest/include						\
	gtest/src/*.cc						\
	gtest/src/*.h						\
	gtest/README.md						\
	gtest/LICENSE						\
	$(NULL)

# test_va_api
bin_PROGRAMS = test_va_api
noinst_HEADERS =						\
	test.h							\
	test_data.h						\
	test_defs.h						\
	test_streamable.h					\
	test_utils.h						\
	test_va_api_fixture.h					\
	$(NULL)

test_va_api_SOURCES =						\
	test_main.cpp						\
	test_internal.cpp					\
	test_va_api_fixture.cpp					\
	test_va_api_get_create_config.cpp			\
	test_va_api_config_attribs.cpp				\
	test_va_api_createsurfaces.cpp				\
	test_va_api_createcontext.cpp				\
	test_va_api_createbuffer.cpp				\
	test_va_api_display_attribs.cpp				\
	test_va_api_get_max_values.cpp				\
	test_va_api_init_terminate.cpp				\
	test_va_api_query_config.cpp				\
	test_va_api_query_vendor.cpp				\
	$(NULL)

test_va_api_LDFLAGS =						\
	$(LIBVA_LDFLAGS)					\
	$(LIBVA_DRM_LDFLAGS)					\
	$(AM_LDFLAGS)						\
	$(NULL)

test_va_api_LDADD =						\
	libgtest.la						\
	$(LIBVA_DEPS_LIBS)					\
	$(LIBVA_DRM_LIBS)					\
	$(NULL)

test_va_api_CPPFLAGS =						\
	$(LIBVA_CFLAGS)						\
	$(LIBVA_DRM_CFLAGS)					\
	$(AM_CPPFLAGS)						\
	$(NULL)

test_va_api_CXXFLAGS =						\
	-Wall -Werror						\
	$(AM_CXXFLAGS)						\
	$(NULL)

check-local: test_va_api
	$(builddir)/test_va_api
