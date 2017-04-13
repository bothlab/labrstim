
galdur_copyright = 'Matthias Klumpp <matthias@tenstral.net>'

compiler = meson.get_compiler('c')

#
# Sources
#
galdur_src = [
    'bcm2835.h',
    'bcm2835.c',
    'gld-init.h',
    'gld-init.c',
    'gld-utils.h',
    'gld-utils.c',
    'gld-gpio.h',
    'gld-gpio.c',
    'gld-adc.h',
    'gld-adc.c',
    'gld-dac.h',
    'gld-dac.c'
]

#
# Configuration
#

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'Galdur')
conf.set_quoted('PACKAGE_VERSION', labrstim_version)
conf.set_quoted('PACKAGE_COPYRIGHT', galdur_copyright)

conf.set('SIMULATE_DATA', get_option('simulate_data'))
if get_option('debug_print')
    conf.set('DEBUG_PRINT', 1)
else
    conf.set('DEBUG_PRINT', 0)
endif

configure_file(input: '../config.h.in',
               output: 'config.h',
               configuration: conf
)

add_global_arguments(['-D_GNU_SOURCE'], language: 'c')

#
# Targets
#
galdur_lib = static_library('galdur',
           [galdur_src],
           dependencies: [thread_dep,
                          glib_dep,
                          math_lib],
           c_args: ['-Wall',  '-O3', '-nostartfiles', '-ffreestanding']
)
galdur_dep = declare_dependency(link_with: galdur_lib,
                                include_directories: include_directories('.'),
                                dependencies: [glib_dep, thread_dep]
)

executable('galdur-adc',
           ['tools/galdur-adc.c'],
           dependencies: [galdur_dep]
)
executable('galdur-blink',
           ['tools/galdur-blink.c'],
           dependencies: [galdur_dep]
)
executable('galdur-dac',
           ['tools/galdur-dac.c'],
           dependencies: [galdur_dep]
)
executable('galdur-io',
           ['tools/galdur-io.c'],
           dependencies: [galdur_dep]
)

#
# Tests
#
executable('test-maxdaq',
           [galdur_src,
            'tests/test-maxdaq.c'],
           dependencies: [thread_dep,
                          glib_dep,
                          math_lib]
)
