# Meson build file for the spike detection module

kfr_dep = dependency('KFR', method: 'cmake', modules: ['kfr', 'kfr_dsp','kfr_io'])

executable('labrstim-spikedetect',
    ['main.cpp',
     '../data-file-si.h',
     '../data-file-si.c',
     '../stimpulse.h',
     '../stimpulse.c',
     '../utils.h',
     '../utils.c'],
     dependencies: [glib_dep,
                    kfr_dep,
                    galdur_dep,
                    math_lib],
     c_args: device_tune_args,
     cpp_args: device_tune_args,
     install: true,
)
