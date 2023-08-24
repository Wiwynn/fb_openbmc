cc = meson.get_compiler('cpp')

yaap_shared_a = static_library(
    'yaap_shared_a',
    'classes/csJtag.cpp',
    'classes/lpcPostCode.cpp',
    'classes/relay.cpp',
    'classes/gpuDebug.cpp',
    'classes/device.cpp',
    'classes/hdt.cpp',
    'classes/lpc.cpp',
    'classes/private.cpp',
    'classes/system.cpp',
    'classes/jtag.cpp',
    'classes/i2c.cpp',
    'classes/lpcRomEmulation.cpp',
    'classes/gpuScan.cpp',
    'classes/cpuDebug.cpp',
    'classes/mux.cpp',
    'classes/base.cpp',
    'classes/wombatUvdMux.cpp',
    'hal/hal_instances.cpp',
    'infrastructure/connectionHandler.cpp',
    'infrastructure/socketHandler.cpp',
    'infrastructure/server.cpp',
    'yaarp/src/error_status_stream.cpp',
    'yaarp/src/input_stream.cpp',
    'yaarp/src/output_stream.cpp',
    include_directories: [
        '.',
        'yaarp/include',
    ],
)

yaap_shared_dep = declare_dependency(
    link_with: [ yaap_shared_a ],
    include_directories: [
        '.',
        'yaarp/include',
    ],
)
