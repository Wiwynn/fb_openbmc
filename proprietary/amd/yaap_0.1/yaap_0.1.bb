SUMMARY = "AMD YAAP daemon"
HOMEPAGE = "https://github.com/AMDESE/YAAP"
LICENSE = "CLOSED"

DEPENDS = "libgpiod"

inherit meson pkgconfig

# Generate this list with:
#   find -name "*.[ch]" -o -name "*.[ch]pp" -o -name "meson*" | \
#         sed "s#./files/#        file://#" | sed 's#$# \\#' | sort
LOCAL_URI = " \
        file://meson.build \
        file://meson_options.txt \
        file://Source/Linux/bmc/hw/aspeed.h \
        file://Source/Linux/bmc/hw/csjtag_hw.cpp \
        file://Source/Linux/bmc/hw/csjtag_hw.h \
        file://Source/Linux/bmc/hw/gpuDebug_hw.cpp \
        file://Source/Linux/bmc/hw/gpuDebug_hw.h \
        file://Source/Linux/bmc/hw/hdt_hw.cpp \
        file://Source/Linux/bmc/hw/hdt_hw.h \
        file://Source/Linux/bmc/hw/header_hw.cpp \
        file://Source/Linux/bmc/hw/header_hw.h \
        file://Source/Linux/bmc/hw/i2c_hw.cpp \
        file://Source/Linux/bmc/hw/i2c_hw.h \
        file://Source/Linux/bmc/hw/jtag_hw.cpp \
        file://Source/Linux/bmc/hw/jtag_hw.h \
        file://Source/Linux/bmc/hw/lpcPostCode_hw.cpp \
        file://Source/Linux/bmc/hw/lpcPostCode_hw.h \
        file://Source/Linux/bmc/hw/lpcRomEmu_hw.cpp \
        file://Source/Linux/bmc/hw/lpcRomEmu_hw.h \
        file://Source/Linux/bmc/hw/mux_hw.cpp \
        file://Source/Linux/bmc/hw/mux_hw.h \
        file://Source/Linux/bmc/hw/relay_hw.cpp \
        file://Source/Linux/bmc/hw/relay_hw.h \
        file://Source/Linux/bmc/hw/system_hw.cpp \
        file://Source/Linux/bmc/hw/system_hw.h \
        file://Source/Linux/bmc/hw/triggers_hw.cpp \
        file://Source/Linux/bmc/hw/triggers_hw.h \
        file://Source/Linux/bmc/main.cpp \
        file://Source/Linux/bmc/meson.build \
        file://Source/meson.build \
        file://Source/Shared/classes/base.cpp \
        file://Source/Shared/classes/base.h \
        file://Source/Shared/classes/cpuDebug.cpp \
        file://Source/Shared/classes/cpuDebug.h \
        file://Source/Shared/classes/csJtag.cpp \
        file://Source/Shared/classes/csJtag.h \
        file://Source/Shared/classes/device.cpp \
        file://Source/Shared/classes/device.h \
        file://Source/Shared/classes/gpuDebug.cpp \
        file://Source/Shared/classes/gpuDebug.h \
        file://Source/Shared/classes/gpuScan.cpp \
        file://Source/Shared/classes/gpuScan.h \
        file://Source/Shared/classes/hdt.cpp \
        file://Source/Shared/classes/hdt.h \
        file://Source/Shared/classes/i2c.cpp \
        file://Source/Shared/classes/i2c.h \
        file://Source/Shared/classes/jtag.cpp \
        file://Source/Shared/classes/jtag.h \
        file://Source/Shared/classes/lpc.cpp \
        file://Source/Shared/classes/lpc.h \
        file://Source/Shared/classes/lpcPostCode.cpp \
        file://Source/Shared/classes/lpcPostCode.h \
        file://Source/Shared/classes/lpcRomEmulation.cpp \
        file://Source/Shared/classes/lpcRomEmulation.h \
        file://Source/Shared/classes/mux.cpp \
        file://Source/Shared/classes/mux.h \
        file://Source/Shared/classes/private.cpp \
        file://Source/Shared/classes/private.h \
        file://Source/Shared/classes/relay.cpp \
        file://Source/Shared/classes/relay.h \
        file://Source/Shared/classes/system.cpp \
        file://Source/Shared/classes/system.h \
        file://Source/Shared/classes/wombatUvdMux.cpp \
        file://Source/Shared/classes/wombatUvdMux.h \
        file://Source/Shared/doc.h \
        file://Source/Shared/doc_hal.h \
        file://Source/Shared/hal/errs.h \
        file://Source/Shared/hal/gpuDebug.h \
        file://Source/Shared/hal/hal_instances.cpp \
        file://Source/Shared/hal/hdt.h \
        file://Source/Shared/hal/header.h \
        file://Source/Shared/hal/i2c.h \
        file://Source/Shared/hal/jtag.h \
        file://Source/Shared/hal/lpcPostCode.h \
        file://Source/Shared/hal/lpcRomEmulation.h \
        file://Source/Shared/hal/mux.h \
        file://Source/Shared/hal/relay.h \
        file://Source/Shared/hal/system.h \
        file://Source/Shared/hal/trigger.h \
        file://Source/Shared/infrastructure/connectionHandler.cpp \
        file://Source/Shared/infrastructure/connectionHandler.h \
        file://Source/Shared/infrastructure/debug.h \
        file://Source/Shared/infrastructure/profile.h \
        file://Source/Shared/infrastructure/server.cpp \
        file://Source/Shared/infrastructure/server.h \
        file://Source/Shared/infrastructure/socketHandler.cpp \
        file://Source/Shared/infrastructure/socketHandler.h \
        file://Source/Shared/local/jtag.h \
        file://Source/Shared/meson.build \
        file://Source/Shared/yaarp/include/yaarp/error_status_stream.h \
        file://Source/Shared/yaarp/include/yaarp/input_stream.h \
        file://Source/Shared/yaarp/include/yaarp/output_stream.h \
        file://Source/Shared/yaarp/include/yaarp/streams.h \
        file://Source/Shared/yaarp/src/error_status_stream.cpp \
        file://Source/Shared/yaarp/src/input_stream.cpp \
        file://Source/Shared/yaarp/src/output_stream.cpp \
        file://Source/Shared/yaarp/yaarp_test/input_stream_test.cpp \
        file://Source/Shared/yaarp/yaarp_test/output_stream_test.cpp \
        file://Source/Shared/yaarp/yaarp_test/random_io_test.cpp \
        file://Source/Shared/yaap_bic/yaapBicWrap.h \
        file://Source/Shared/yaap_bic/yaapBicWrap.cpp \
"
