
#
# Sources
#
labrstim_src = [
    'defaults.h',
    'main.c',
    'fftw-functions.h',
    'fftw-functions.c',
    'data-file-si.h',
    'data-file-si.c',
    'utils.h',
    'utils.c',
    'tasks.h',
    'tasks.c',
    'stimpulse.h',
    'stimpulse.c'
]

#
# Targets
#
executable('labrstim',
    [labrstim_src, config_h],
    dependencies: [fftw3_dep,
                   thread_dep,
                   glib_dep,
                   math_lib,
                   galdur_dep],
    c_args: device_tune_args,
    include_directories: include_directories('..'),
    install: true
)

executable('labrstim-ttyctl',
    ['ttyctl.c', config_h],
    dependencies: [glib_dep,
                   gio_unix_dep],
    c_args: device_tune_args,
    include_directories: include_directories('..'),
    install: true
)

subdir('spikedetect')
