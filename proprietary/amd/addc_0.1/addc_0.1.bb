SUMMARY = "GENOA CPU Crashdump"
DESCRIPTION = "CPU utilities for dumping CPU Crashdump"
SECTION = "base"
DEFAULT_PREFERENCE = "-1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"

DEPENDS = "boost libobmc-i2c apml nlohmann-json"

inherit cmake pkgconfig

LOCAL_URI = " \
    file://CMakeLists.txt \
    file://LICENSE \
    file://src/bic_apml_interface.cpp \
    file://src/bic_apml_interface.hpp \
    file://src/main.cpp \
    file://src/plat.cpp \
    file://inc/cper.hpp \
    file://service_files/com.amd.crashdump.service \
    file://config/config_file \
    "

do_install:append() {
    install -d ${D}${localstatedir}/lib/amd-ras
    install -m 0755 ${S}/config/config_file ${D}${localstatedir}/lib/amd-ras/config_file
}