SUMMARY = "EagleStream CPU Crashdump"
DESCRIPTION = "CPU utilities for dumping CPU Crashdump and registers over PECI"
SECTION = "base"
PR = "r1"
DEFAULT_PREFERENCE = "-1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE;md5=43c09494f6b77f344027eea0a1c22830"

DEPENDS = "boost safec libpeci cjson gtest cli11"

inherit cmake pkgconfig

SRC_URI[sha256sum] = "a810ec1b695eea04aa4126937cc9dcbeaca69213419756a34d5648d546eca36f"

LOCAL_URI = " \
    file://cmake-format.json \
    file://CMakeLists.txt \
    file://crashdump.cpp \
    file://crashdump.hpp \
    file://crashdump_input_icx.json \
    file://crashdump_input_sprhbm.json \
    file://crashdump_input_spr.json \
    file://engine \
    file://LICENSE \
    file://PlatInfo.cpp \
    file://telemetry_input_icx.json \
    file://telemetry_input_sprhbm.json \
    file://telemetry_input_spr.json \
    file://tests \
    file://utils_dbusplus.cpp \
    file://utils_dbusplus.hpp \
    file://utils_triage.cpp \
    file://utils_triage.hpp \
    "

RDEPENDS:${PN} += "safec libpeci cjson"

FILES:${PN} += "${prefix}/share/crashdump"
