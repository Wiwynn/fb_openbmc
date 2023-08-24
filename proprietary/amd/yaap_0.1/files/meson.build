project(
    'YAAP',
    'cpp',
    version: '1.1.0',
    default_options: [
        'buildtype=debugoptimized',
        'warning_level=3',
        'werror=true',
    ],
)

add_project_arguments(
    '-DDEBUGGING_ENABLED',
    '-DDEBUG_LEVEL=DEBUG_INFO',
    '-DUNALIGNED_ACCESSES_SUPPORTED=false',
    '-DYAAP_LITTLE_ENDIAN',
    language: 'cpp',
)

if get_option('yaapbic') == true
add_project_arguments(
    '-DLIBYAAP_BIC',
    language: 'cpp',
)
endif

gpiodcxx_dep = dependency(
    'libgpiodcxx',
    fallback: ['libgpiod', 'gpiodcxx_dep'],
    default_options: ['bindings=cxx'],
)

subdir('Source')
