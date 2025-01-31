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

mebo_cpp = cpp_compiler.find_library('mebo')
aom_av1_rc_cpp = cpp_compiler.find_library('aom_av1_rc')

mebo_c = c_compiler.find_library('mebo')
aom_av1_rc_c = c_compiler.find_library('aom_av1_rc')

dl = c.find_library('dl')
dl_dep_cpp = cpp_compiler.find_library('dl', required:true)

if libva_dep.version().version_compare('>= 1.14.0')
    executable('av1encode', [ 'av1encode.c', 'libmebo/libmebo_interface.cc'  ],
            dependencies: [ libva_display_dep, threads, m, dl, dl_dep_cpp,
                            mebo_cpp, mebo_c, aom_av1_rc_cpp, aom_av1_rc_c],
            install: true)
endif