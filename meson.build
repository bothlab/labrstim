project('Laser Brain Stimulator', 'c')

labrstim_version = '0.1'
labrstim_copyright = 'Kevin Allen <allen@uni-heidelberg.de> | Matthias Klumpp <matthias@tenstral.net>'

compiler = meson.get_compiler('c')

add_global_arguments('-std=gnu11', language: 'c')

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
    'src/stimpulse.h',
    'src/stimpulse.c'
]

#
# Dependencies
#
fftw3_dep  = dependency('fftw3f') # single-precision fftw
thread_dep = dependency('threads')
glib_dep = dependency('glib-2.0')
systemd_dep = dependency('systemd')
gio_unix_dep = dependency('gio-unix-2.0')
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
    conf.set('DEBUG', 1)
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

#
# Subdirs
#
if (get_option('device'))
    subdir('galdur')
endif
if (get_option('guiclient'))
    subdir('client')
endif

#
# Targets for device software
#

if (get_option('device'))
    executable('labrstim',
                [labrstim_src],
                dependencies: [fftw3_dep,
                                thread_dep,
                                glib_dep,
                                math_lib,
                                galdur_dep],
                install: true
    )

    executable('labrstim-ttyctl',
                ['src/ttyctl.c'],
                dependencies: [glib_dep,
                                gio_unix_dep],
                install: true
    )
endif


#
# Data for device
#
if (get_option('device'))
    sd_data = configuration_data()
    sd_data.set('LSTTYCTL_INSTALL_BIN', get_option('prefix') + '/bin/labrstim-ttyctl')

    configure_file(input: 'src/labrstim-ttyctl.service.in',
                   output: 'labrstim-ttyctl.service',
                   configuration: sd_data)
    install_data(meson.current_build_dir() + '/labrstim-ttyctl.service',
                   install_dir: systemd_dep.get_pkgconfig_variable('systemdsystemunitdir')
    )
endif
