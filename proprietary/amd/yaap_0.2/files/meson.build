project(
    'YAAP',
    'cpp',
    version: '1.1.0',
    default_options: [
        'buildtype=debugoptimized',
    ],
)

add_project_arguments(
    '-DDEBUGGING_ENABLED',
    '-DDEBUG_LEVEL=DEBUG_FATAL',
    '-DUNALIGNED_ACCESSES_SUPPORTED=false',
    '-DYAAP_LITTLE_ENDIAN',
    language: 'cpp',
)

gpiodcxx_dep = [
    dependency('libgpio-ctrl'),
]

if (get_option('bic-jtag').enabled())
  add_project_arguments('-DBIC_JTAG', language: 'cpp')
endif

subdir('Source')
