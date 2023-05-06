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
if libva_dep.version().version_compare('>= 1.14.0')
    executable('av1encode', [ 'av1encode.c' ],
            dependencies: [ libva_display_dep, threads, m ],
            install: true)
endif