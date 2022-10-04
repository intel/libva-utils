gtest_src = [
  'gtest/src/gtest-all.cc',
  'gtest/src/gtest_main.cc',
]
gtest_inc = include_directories('gtest', 'gtest/include')

test_flags = [
  '-DGTEST_USE_OWN_TR1_TUPLE=0',
  '-DGTEST_LANG_CXX11=1',
  '-DGTEST_HAS_TR1_TUPLE=1',
  '-std=c++11',
]

if not use_win32
  test_flags += [
    '-DGTEST_HAS_PTHREAD=1',
    '-DPTHREADS',
  ]
endif

gtest = static_library('gtest', gtest_src,
                       cpp_args: test_flags,
                       include_directories: gtest_inc)
gtest_dep = declare_dependency(link_with: gtest,
                               include_directories: gtest_inc)

test_src = [
  'test_main.cpp',
  'test_internal.cpp',
  'test_va_api_fixture.cpp',
  'test_va_api_get_create_config.cpp',
  'test_va_api_config_attribs.cpp',
  'test_va_api_createsurfaces.cpp',
  'test_va_api_createcontext.cpp',
  'test_va_api_createbuffer.cpp',
  'test_va_api_display_attribs.cpp',
  'test_va_api_get_max_values.cpp',
  'test_va_api_init_terminate.cpp',
  'test_va_api_query_config.cpp',
  'test_va_api_query_vendor.cpp',
]

tests_deps = [gtest_dep, dependency('threads')]

if use_win32
  tests_deps += [ win32_deps ]
else
  tests_deps += [ drm_deps ]
endif

tests = executable('test_va_api', test_src,
                   cpp_args: test_flags,
                   dependencies: tests_deps,
                   install: true)

test('test_va', tests)
