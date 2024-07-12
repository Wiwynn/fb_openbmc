SUMMARY = "TURIN CPU Crashdump"
DESCRIPTION = "CPU utilities for dumping CPU Crashdump"
SECTION = "base"
DEFAULT_PREFERENCE = "-1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"

DEPENDS = "boost libobmc-i2c apml nlohmann-json phosphor-logging sdbusplus"

inherit meson pkgconfig systemd

LOCAL_URI = " \
    file://meson.build \
    file://meson_options.txt \
    file://LICENSE \
    file://src/Config.cpp \
    file://src/dbus_ras.cpp \
    file://src/fatal_error.cpp \
    file://src/main.cpp \
    file://src/runtime_errors.cpp \
    file://src/write_cper_data.cpp \
    file://inc/Config.hpp \
    file://inc/cper.hpp \
    file://inc/cper_runtime.hpp \
    file://inc/ras.hpp \
    file://service_files/com.amd.crashdump.service \
    "

do_install:append() {
    install -d ${D}${localstatedir}/lib/amd-ras
    install -m 0755 ${S}/config/config_file ${D}${localstatedir}/lib/amd-ras/config_file
}
