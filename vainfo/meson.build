executable('vainfo', [ 'vainfo.c' ],
           c_args: [ '-DLIBVA_VERSION_S="' + meson.project_version() + '"' ],
           dependencies: [ libva_display_dep, dependency('threads'), ],
           install: true)
