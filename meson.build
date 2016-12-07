project('Laser Brain Stimulator', 'c')

labrstim_version = '0.1'
labrstim_copyright = 'Kevin Allen <allen@uni-heidelberg.de> | Matthias Klumpp <matthias@tenstral.net>'

compiler = meson.get_compiler('c')

#
# Sources
#
labrstim_src = [
    'src/defaults.h',
    'src/main.c',
    'src/brintf.h',
    'src/brintf.c',
    'src/fftw-functions.h',
    'src/fftw-functions.c',
    'src/data-file-si.h',
    'src/data-file-si.c',
    'src/timespec-utils.h',
    'src/timespec-utils.c',
    'src/tasks.h',
    'src/tasks.c',
    'src/max1133daq.h',
    'src/max1133daq.c',
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

if not compiler.has_header('linux/spi/spidev.h')
    error('Unable to find "spidev.h" header. Are you running Linux with SPI enabled and headers installed?')
endif

#
# Configuration
#

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'LaBrStim')
conf.set_quoted('PACKAGE_VERSION', labrstim_version)
conf.set_quoted('PACKAGE_COPYRIGHT', labrstim_copyright)

configure_file(input: 'config.h.in',
               output: 'config.h',
               configuration: conf
)

#
# Targets
#
executable('labrstim',
           [labrstim_src],
           dependencies: [fftw3_dep, thread_dep, glib_dep]
)
