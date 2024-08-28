project('Laser Brain Stimulator',
        ['c', 'cpp'],
        default_options : ['c_std=gnu11',
                           'cpp_std=c++17'],
        license: 'GPL-3.0+',
        version: '0.2',
)

labrstim_version = meson.project_version()
labrstim_copyright = 'Kevin Allen <allen@uni-heidelberg.de> | Matthias Klumpp <matthias@tenstral.net>'

compiler = meson.get_compiler('c')

message('This program will only run on Raspberry Pi 3 hardware with the Galdur board.')

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

config_h = configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration: conf
)

#
# Settings
#
add_project_arguments(['-D_GNU_SOURCE'], language: 'c')
add_project_arguments(['-Werror=implicit-function-declaration', '-Wno-unused-parameter'], language: 'c')

if (get_option('maintainer'))
    add_project_arguments(['-Werror', '-Wall', '-Wextra'], language: 'c')
endif

device_tune_args = []
if target_machine.cpu_family() == 'aarch64'
    message('Optimizing device binaries for RPi4')
    # tuning flags for the RPi4, and build device code for ARM64
    device_tune_args = [
        '-march=armv8-a+crc',
        '-mtune=cortex-a72',
    ]
else
    message('No optimizing device binaries for RPi4')
endif

#
# Subdirs
#
if get_option('device')
    subdir('galdur')
endif
if get_option('guiclient')
    subdir('client')
endif
if get_option('device')
    subdir('src')
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
