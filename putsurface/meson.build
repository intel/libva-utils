deps = [
  libva_display_dep,
  threads,
]

if use_x11
  executable('putsurface', [ 'putsurface_x11.c' ],
             dependencies: deps,
             install: true)
endif
if use_wayland
  executable('putsurface_wayland', [ 'putsurface_wayland.c' ],
             dependencies: deps,
             install: true)
endif
