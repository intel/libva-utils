project('libva-utils', 'c', 'cpp',
        version : '2.6.0.1',
        default_options : [
          'warning_level=2',
          'c_std=gnu99',
          'default_library=static',
        ],
        license : 'MIT',
        meson_version : '>= 0.42.0')

c = meson.get_compiler('c')

libva_dep = dependency('libva', version: '>= 1.1.0')

libva_utils_flags = [ '-Wno-unused-parameter',
                      '-Wno-sign-compare' ]

# DRM
use_drm = false
if get_option('drm') != 'false'
  require_drm = get_option('drm') == 'true'
  drm_deps = [
    dependency('libdrm', version: '>= 2.4', required: require_drm),
    dependency('libva-drm', required: require_drm),
  ]
  use_drm = true
  foreach d : drm_deps
    if not d.found()
      use_drm = false
    endif
  endforeach
  if use_drm
    libva_utils_flags += [ '-DHAVE_VA_DRM=1' ]
  endif
endif

# X11
use_x11 = false
if get_option('x11') != 'false'
  require_x11 = get_option('x11') == 'true'
  x11_deps = [
    dependency('x11', required: require_x11),
    dependency('xext', required: require_x11),
    dependency('xfixes', required: require_x11),
    dependency('libva-x11', required: require_x11),
  ]
  use_x11 = true
  foreach d : x11_deps
    if not d.found()
      use_x11 = false
    endif
  endforeach
  if use_x11
    libva_utils_flags += [ '-DHAVE_VA_X11=1' ]
  endif
endif

# WAYLAND
use_wayland = false
if get_option('wayland') != 'false'
  require_wayland = get_option('wayland') == 'true'
  wayland_deps = [
    dependency('wayland-client', version: '>= 1.0.0', required: require_wayland),
    dependency('libva-wayland', required: require_wayland),
  ]
  use_wayland = true
  foreach d : wayland_deps
    if not d.found()
      use_wayland = false
    endif
  endforeach
  if use_wayland
    libva_utils_flags += [ '-DHAVE_VA_WAYLAND=1' ]
  endif
endif

add_project_arguments(libva_utils_flags,
                      language: ['c', 'cpp'])

subdir('common')
subdir('decode')
subdir('encode')
subdir('putsurface')
subdir('vainfo')
subdir('videoprocess')

if get_option('tests')
  subdir('test')
endif
