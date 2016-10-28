project('Laser Brain Stimulator', 'c')

labrstim_version = '0.1'
labrstim_copyright = 'Kevin Allen <allen@uni-heidelberg.de> | Matthias Klumpp <matthias@tenstral.net>'

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
    'src/timespec-utils.h',
    'src/timespec-utils.c',
    'src/comedi-intf.h',
    'src/comedi-intf.c'
]

#
# Dependencies
#
comedi_dep = dependency('comedilib')
fftw3_dep  = dependency('fftw3')
thread_dep = dependency('threads')

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
           labrstim_src,
           dependencies: [comedi_dep, fftw3_dep, thread_dep]
)
