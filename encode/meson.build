threads = dependency('threads')
m = c.find_library('m')


executable('avcenc', [ 'avcenc.c' ],
           dependencies: [ libva_display_dep, threads ],
           install: true)
executable('h264enc', [ 'h264encode.c' ],
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
