
cc = meson.get_compiler('cpp')

# Add additional library dependencies.
pal_deps = []
# Machine-specific wrappers
machine_wrapper_files = []

if get_option('yaapbic') == true
    machine_wrapper_files += [
        'yaap_bic/yaapBicWrap.cpp',
    ]
    foreach lib : get_option('yaaplib')
        pal_deps += cc.find_library(lib)
    endforeach
endif

yaap_shared_a = static_library(
    'yaap_shared_a',
    'classes/base.cpp',
    'classes/cpuDebug.cpp',
    'classes/csJtag.cpp',
    'classes/device.cpp',
    'classes/gpuDebug.cpp',
    'classes/gpuScan.cpp',
    'classes/hdt.cpp',
    'classes/i2c.cpp',
    'classes/jtag.cpp',
    'classes/lpc.cpp',
    'classes/lpcPostCode.cpp',
    'classes/lpcRomEmulation.cpp',
    'classes/mux.cpp',
    'classes/relay.cpp',
    'classes/system.cpp',
    'classes/wombatUvdMux.cpp',
    'classes/private.cpp',
    'hal/hal_instances.cpp',
    'infrastructure/connectionHandler.cpp',
    'infrastructure/server.cpp',
    'infrastructure/socketHandler.cpp',
    'yaarp/src/error_status_stream.cpp',
    'yaarp/src/input_stream.cpp',
    'yaarp/src/output_stream.cpp',
    machine_wrapper_files,
    include_directories: [
        '.',
        'yaarp/include',
    ],
    dependencies : pal_deps,
)

yaap_shared_dep = declare_dependency(
    link_with: [ yaap_shared_a ],
    include_directories: [
        '.',
        'yaarp/include',
    ],
)
