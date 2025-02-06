project('libva-utils', 'c', 'cpp',
        version : '2.22.0.1',
        default_options : [
          'warning_level=2',
          'c_std=gnu99',
          'default_library=static',
        ],
        license : 'MIT',
        meson_version : '>= 0.42.0')

c = meson.get_compiler('c')

threads = dependency('threads')

libva_dep = dependency('libva', version: '>= 1.1.0')

libva_utils_flags = [ '-Wno-unused-parameter',
                      '-Wno-sign-compare' ]

backends = ''

c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')

#Add Mebo support
use_mebo = false
if get_option('mebo') != 'disabled'
  require_mebo = get_option('mebo') == 'enabled'
  local_mebo_path = join_paths(meson.current_source_dir(),
                               'encode', 'libmebo', 'libs')
  mebo_dep = cpp_compiler.find_library('mebo',
                                      dirs :[local_mebo_path,
                                            '/usr/local/lib/x86_64-linux-gnu'],
                                      required:false)
  if not mebo_dep.found() and require_mebo
     error('''LibMebo library not installed in local path or system:
            local path=/encode/libmebo/libs''')
  endif

  if mebo_dep.found()
        use_mebo = true
        message('mebo library found')
        libva_utils_flags += ['-DHAVE_MEBO=1']
    endif
  if not mebo_dep.found()
    error('LibMebo not installed')
  endif
endif

use_brc = false
if get_option('brc') != 'disabled'
  require_brc = get_option('brc') == 'enabled'
  local_brc_path = join_paths(meson.current_source_dir(),
                               'encode', 'libmebo', 'libs')
  brc_dep = cpp_compiler.find_library('brc',
                                      dirs :[local_brc_path,
                                            '/usr/local/lib/x86_64-linux-gnu'],
                                      required:false)
  if not brc_dep.found() and require_brc
     error('''Libbrc library not installed in local path or system:
            local path=/encode/libmebo/libs''')
  endif

  if brc_dep.found()
        use_brc = true
        message('brc library found')
        libva_utils_flags += ['-DHAVE_BRC=1']
    endif
  if not brc_dep.found()
    error('LIBBRC not installed')
  endif
endif

use_brc_controller = false
if get_option('brc_controller') != 'disabled'
  require_brc_controller = get_option('brc_controller') == 'enabled'
  local_brc_controller_path = join_paths(meson.current_source_dir(),
                               'encode', 'libmebo', 'libs')
  brc_controller_dep = cpp_compiler.find_library('brc_controller',
                                      dirs :[local_brc_controller_path,
                                            '/usr/local/lib/x86_64-linux-gnu'],
                                      required:false)
  if not brc_controller_dep.found() and require_brc_controller
     error('Libbrc_controller library not installed in local path or system:
            local path=/encode/libmebo/libs')
  endif

  if brc_controller_dep.found()
        use_brc_controller = true
        message('Libbrc_controller library found')
        libva_utils_flags += ['-DHAVE_BRC=1']
    endif
  if not brc_controller_dep.found()
    error('Libbrc_controller not installed')
  endif
endif

#Add AOM_AV1_RC support
use_aom_av1_rc = false
if get_option('aom_av1_rc') != 'disabled'
  require_aom_av1_rc = get_option('aom_av1_rc') == 'enabled'
  local_aom_av1_rc_path = join_paths(meson.current_source_dir(),
                                    'encode', 'libs', 'aom_libs')
  aom_av1_rc_dep = cpp_compiler.find_library('aom_av1_rc',
                                      dirs :[local_aom_av1_rc_path,
                                      '/usr/local/lib/x86_64-linux-gnu'],
                                      required:false)
  if not aom_av1_rc_dep.found() and require_aom_av1_rc
     error('''libaom_av1_rc.so not installed in local path or system: 
                                 local path= encode/libmebo/libs/aom_libs''')
  endif

  if aom_av1_rc_dep.found()
        use_aom_av1_rc = true
        message('aom_av1_rc library found ')
        libva_utils_flags += ['-DHAVE_AOM_AV1_RC=1']
    endif

  if not aom_av1_rc_dep.found()
    error('Lib AOM AV1 RC not installed')
  endif
endif

# DRM
use_drm = false
drm_deps=[]
if get_option('drm') != 'false'
  require_drm = get_option('drm') == 'true'
  drm_deps = [
    dependency('libva-drm', version: libva_dep.version(), required: require_drm),
    dependency('libdrm', required: require_drm),
  ]
  use_drm = true
  foreach d : drm_deps
    if not d.found()
      use_drm = false
    endif
  endforeach
  if use_drm
    backends += ' drm'
    libva_utils_flags += [ '-DHAVE_VA_DRM=1' ]
  endif
endif

# X11
use_x11 = false
if get_option('x11') != 'false'
  require_x11 = get_option('x11') == 'true'
  x11_deps = [
    dependency('x11', required: require_x11),
    dependency('libva-x11', version: libva_dep.version(), required: require_x11),
  ]
  use_x11 = true
  foreach d : x11_deps
    if not d.found()
      use_x11 = false
    endif
  endforeach
  if use_x11
    backends += ' x11'
    libva_utils_flags += [ '-DHAVE_VA_X11=1' ]
  endif
endif

# WAYLAND
use_wayland = false
if get_option('wayland') != 'false'
  require_wayland = get_option('wayland') == 'true'
  wayland_deps = [
    dependency('wayland-client', version: '>= 1.0.0', required: require_wayland),
    dependency('libva-wayland', version: libva_dep.version(), required: require_wayland),
  ]
  use_wayland = true
  foreach d : wayland_deps
    if not d.found()
      use_wayland = false
    endif
  endforeach
  if use_wayland
    backends += ' wayland'
    libva_utils_flags += [ '-DHAVE_VA_WAYLAND=1' ]
  endif
endif

# WIN32
use_win32 = false
if get_option('win32') != 'false'
  require_win32 = get_option('win32') == 'true'
  use_win32 = dependency('libva-win32', version: libva_dep.version(), required: require_win32).found()
  if use_win32
    libva_utils_flags = [ '-DHAVE_VA_WIN32=1', '-DNOMINMAX']
    dep_dxheaders = dependency('directx-headers', required : false)
    if not dep_dxheaders.found()
      dep_dxheaders = dependency('DirectX-Headers',
        version : '>= v1.0.2',
        fallback : ['DirectX-Headers', 'dep_dxheaders'],
        required : use_win32
      )
    endif
  endif
endif

add_project_arguments(libva_utils_flags,
                      language: ['c', 'cpp'])

if use_win32
  subdir('getopt')
  win32_deps = [ dependency('libva-win32', required: require_win32), idep_getopt, dep_dxheaders]
endif

subdir('common') # Uses win32_deps
subdir('vainfo')

if not use_win32
  subdir('decode')
  subdir('encode')
  subdir('putsurface')
  subdir('videoprocess')
  subdir('vendor/intel')
  subdir('vendor/intel/sfcsample')
endif

if get_option('tests')
  subdir('test')
endif

if meson.version().version_compare('>= 0.53')
  summary({
    'Libva VA-API version' : libva_dep.version(),
    'Installation prefix' : get_option('prefix'),
    'Extra window systems' : backends,
    'Enable Unit-tests': get_option('tests')
  }, bool_yn: true)
endif
