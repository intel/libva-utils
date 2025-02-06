m = c.find_library('m')


executable('avcenc', [ 'avcenc.c' ],
           dependencies: [ libva_display_dep, threads ],
           install: true)
executable('h264encode', [ 'h264encode.c' ],
           dependencies: [ libva_display_dep, threads, m ],
           install: true)
executable('hevcencode', [ 'hevcencode.c' ],
           dependencies: [ libva_display_dep, threads, m ],
           install: true)
executable('mpeg2vaenc', [ 'mpeg2vaenc.c' ],
           dependencies: [ libva_display_dep, threads ],
           install: true)
executable('jpegenc', [ 'jpegenc.c' ],
           dependencies: [ libva_display_dep, threads ],
           install: true)
executable('svctenc', [ 'svctenc.c' ],
           dependencies: [libva_display_dep, threads, m ])
executable('vp9enc', [ 'vp9enc.c' ],
           dependencies: [libva_display_dep, threads, m ],
           install: true)
executable('vp8enc', [ 'vp8enc.c' ],
           dependencies: [libva_display_dep, threads, m ],
           install: true)

c_compiler = meson.get_compiler('c')
cpp_compiler = meson.get_compiler('cpp')

dl = c.find_library('dl')
dl_dep_cpp = cpp_compiler.find_library('dl', required:true)


if libva_dep.version().version_compare('>= 1.14.0')

    av1_deps = [libva_display_dep, threads, m, dl, dl_dep_cpp]
    if use_mebo and mebo_dep.found()
        av1_deps += mebo_dep
    endif

    if use_aom_av1_rc and aom_av1_rc_dep.found()
        av1_deps += aom_av1_rc_dep
    endif

    if use_brc and brc_dep.found()
        av1_deps += brc_dep
    endif

    if use_brc_controller and brc_controller_dep.found()
        av1_deps += brc_controller_dep
    endif

    executable('av1encode', [ 'av1encode.c', 'libmebo/libmebo_interface.cc'  ],
            dependencies: av1_deps,
            install: true)
endif