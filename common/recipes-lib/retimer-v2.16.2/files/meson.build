project('libretimer', 'c',
    version: '0.1',
    license: 'GPL2',
    default_options: ['werror=false', 'warning_level=3'],
    meson_version: '>=0.40')

headers = [
    'aries_a0_reg_defines.h',
    'aries_api.h',
    'aries_api_types.h',
    'aries_error.h',
    'aries_globals.h',
    'aries_i2c.h',
    'aries_link.h',
    'aries_margin.h',
    'aries_misc.h',
    'aries_mpw_reg_defines.h',
    'astera_log.h',
    'platform.h',
]

sources = [
    'aries_api.c',
    'aries_i2c.c',
    'aries_link.c',
    'aries_margin.c',
    'aries_misc.c',
    'astera_log.c',
    'platform.c',
]

# Import the plat-specific subdirectory for platform
# headers/sources/dependencies.
subdir('plat')


install_headers(
    headers,
    subdir: 'openbmc')

srcs = files(
  sources,
)

rt_deps = [
    dependency('libobmc-i2c'),
]

# Retimer library.
rt_lib = shared_library('retimer', srcs,
    dependencies: rt_deps,
    version: meson.project_version(),
    install: true)

# pkgconfig for Retimer library.
pkg = import('pkgconfig')
pkg.generate(libraries: [rt_lib],
    name: meson.project_name(),
    version: meson.project_version(),
    description: 'Aries retimer Library v2.16.2')
