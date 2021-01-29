
SUMMARY = "CPU Crashdump"
DESCRIPTION = "CPU utilities for dumping CPU Crashdump and registers over PECI"
SECTION = "base"
PR = "r1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE;md5=43c09494f6b77f344027eea0a1c22830"

DEPENDS = "cjson safec gtest libpeci cli11"

inherit cmake

EXTRA_OECMAKE = "-DCRASHDUMP_BUILD_UT=OFF"

SRC_URI[sha256sum] = "a810ec1b695eea04aa4126937cc9dcbeaca69213419756a34d5648d546eca36f"

SRC_URI = "file://cmake-format.json \
           file://CMakeLists.txt \
           file://crashdump.cpp \
           file://crashdump.hpp \
           file://crashdump_input_cpx.json \
           file://crashdump_input_icx.json \
           file://CrashdumpSections \
           file://LICENSE \
           file://PlatInfo.cpp \
           file://tests \
           file://utils.cpp \
           file://utils.hpp \
          "
SRCREV = "wht-0.65"

S = "${WORKDIR}"

RDEPENDS_${PN} += "cjson safec libpeci"

FILES_${PN} += "${prefix}/share/crashdump"
