
galdur_copyright = 'Matthias Klumpp <matthias@tenstral.net>'

compiler = meson.get_compiler('c')

#
# Sources
#
galdur_src = [
    'bcm2835.h',
    'bcm2835.c'
]

#
# Targets
#
galdur_lib = static_library('galdur',
           [galdur_src],
           dependencies: [thread_dep,
                          glib_dep,
                          math_lib]
)
