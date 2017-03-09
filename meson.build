project('Laser Brain Stimulator', 'c')

labrstim_version = '0.1'
labrstim_copyright = 'Kevin Allen <allen@uni-heidelberg.de> | Matthias Klumpp <matthias@tenstral.net>'

compiler = meson.get_compiler('c')

message('This program will only run on Raspberry Pi 3 hardware with the Galdur board.')

#
# Sources
#
labrstim_src = [
    'src/defaults.h',
    'src/main.c',
    'src/fftw-functions.h',
    'src/fftw-functions.c',
    'src/data-file-si.h',
    'src/data-file-si.c',
    'src/utils.h',
    'src/utils.c',
    'src/tasks.h',
    'src/tasks.c',
    'src/gpio.h',
    'src/gpio.c',
    'src/stimpulse.h',
    'src/stimpulse.c'
]

#
# Dependencies
#
fftw3_dep  = dependency('fftw3')
thread_dep = dependency('threads')
glib_dep = dependency('glib-2.0')
math_lib = compiler.find_library('m')

#
# Configuration
#

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'LaBrStim')
conf.set_quoted('PACKAGE_VERSION', labrstim_version)
conf.set_quoted('PACKAGE_COPYRIGHT', labrstim_copyright)

conf.set('SIMULATE_DATA', get_option('simulate_data'))
if get_option('debug_print')
    conf.set('DEBUG_PRINT', 1)
else
    conf.set('DEBUG_PRINT', 0)
endif


configure_file(input: 'config.h.in',
               output: 'config.h',
               configuration: conf
)

#
# Settings
#
add_global_arguments(['-Werror=implicit-function-declaration', '-Wno-unused-parameter'], language: 'c')

if (get_option('maintainer'))
    add_global_arguments(['-Werror', '-Wall', '-Wextra'], language: 'c')
endif

if (get_option('optimize'))
    # we optimize as much as possible if this option is set
    add_global_arguments(['-O3', '-march=native'], language: 'c')
endif

#
# Subdirs
#
subdir('galdur')

#
# Targets
#
executable('labrstim',
           [labrstim_src],
           dependencies: [fftw3_dep,
                          thread_dep,
                          glib_dep,
                          math_lib,
                          galdur_dep]
)
