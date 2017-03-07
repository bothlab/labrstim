
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
    'gld-adc.h',
    'gld-adc.c'
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

#
# Targets
#
galdur_lib = static_library('galdur',
           [galdur_src],
           dependencies: [thread_dep,
                          glib_dep,
                          math_lib]
)
galdur_dep = declare_dependency(link_with: galdur_lib,
                                include_directories: include_directories('.'),
                                dependencies: [glib_dep, thread_dep]
)

executable('galdur-adc',
           ['tools/galdur-adc.c'],
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
