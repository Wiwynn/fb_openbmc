SUMMARY = "BirchStream CPU Crashdump"
DESCRIPTION = "CPU utilities for dumping CPU Crashdump and registers over PECI"
SECTION = "base"
PR = "r1"
DEFAULT_PREFERENCE = "-1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE;md5=eed054adf11169b8a51e82ece595b6c6"

DEPENDS = "boost safec libpeci cjson gtest cli11"

inherit cmake pkgconfig

SRC_URI[sha256sum] = "a810ec1b695eea04aa4126937cc9dcbeaca69213419756a34d5648d546eca36f"

S="${WORKDIR}/sources"
UNPACKDIR="${S}"

LOCAL_URI = " \
    file://engine \
    file://tests \
    file://utils \
    file://cmake-format.json \
    file://CMakeLists.txt \
    file://crashdump_input_gnr.json \
    file://crashdump_input_gnrd.json \
    file://crashdump_input_srf.json \
    file://crashdump.cpp \
    file://crashdump.hpp \
    file://LICENSE \
    file://pmt_crashlog.cpp \
    file://pmt_crashlog.hpp \
    file://telemetry_input_gnr.json \
    file://telemetry_input_srf.json \
    file://utils_common_dbusplus.cpp \
    file://utils_common_dbusplus.hpp \
    file://utils_triage.cpp \
    file://utils_triage.hpp \
    "

RDEPENDS:${PN} += "safec libpeci cjson"

FILES:${PN} += "${prefix}/share/crashdump"
