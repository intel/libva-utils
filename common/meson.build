libva_display_src = [ 'va_display.c' ]
libva_display_deps = [ libva_dep ]

if use_x11
  libva_display_src += [ 'va_display_x11.c' ]
  libva_display_deps += x11_deps
endif

if use_drm
  libva_display_src += [ 'va_display_drm.c' ]
  libva_display_deps += drm_deps
endif

if use_wayland
  libva_display_src += [ 'va_display_wayland.c' ]
  libva_display_deps += wayland_deps
endif

if use_win32
  libva_display_src += [ 'va_display_win32.cpp' ]
  libva_display_deps += win32_deps
endif

libva_display_lib = static_library('common', libva_display_src,
                                   dependencies: libva_display_deps,
                                   include_directories: include_directories('.'))
libva_display_dep = declare_dependency(link_with: libva_display_lib,
                                       dependencies: libva_display_deps,
                                       include_directories: include_directories('.'))
