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
    'src/timespec-utils.c'
]

bri_dummy_src =  ['src/bri/dummy-intf.h', 'src/bri/dummy-intf.c']
bri_mcpadv_src = ['src/bri/mcpadv-intf.h', 'src/bri/mcpadv-intf.c']
bri_comedi_src = ['src/bri/comedi-intf.h', 'src/bri/comedi-intf.c']

#
# Dependencies
#
fftw3_dep  = dependency('fftw3')
thread_dep = dependency('threads')
comedi_dep = dependency('comedilib', required: false)

#
# Configuration
#

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'LaBrStim')
conf.set_quoted('PACKAGE_VERSION', labrstim_version)
conf.set_quoted('PACKAGE_COPYRIGHT', labrstim_copyright)

bri_intf_type = get_option('daq_stim_intf')
bri_src = bri_dummy_src
if (bri_intf_type == 'comedi')
    # Use COMEDI interface.
    if not comedi_dep.found()
        error('Could not find the headers for COMEDI. Please install the COMEDI development files to continue.')
    endif

    bri_src += bri_comedi_src
    conf.set('USE_COMEDI', true)
elif (bri_intf_type == 'mcpadv')
    # Use MCP ADV via SPI and GPIO for stimulation
    if not compiler.has_header('linux/spi/spidev.h')
        error('Unable to find "spidev.h" header. Are you running Linux with SPI enabled and headers installed?')
    endif

    bri_src += bri_mcpadv_src
    conf.set('USE_MCPADV', true)
endif


configure_file(input: 'config.h.in',
               output: 'config.h',
               configuration: conf
)

#
# Targets
#
executable('labrstim',
           [labrstim_src, bri_src],
           dependencies: [fftw3_dep, thread_dep, comedi_dep]
)
