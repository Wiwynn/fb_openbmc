# Copyright 2017-present Facebook. All Rights Reserved.

FILESEXTRAPATHS_append := "${THISDIR}/files:"
SRC_URI += "file://PlatInfo.cpp \
            file://CMakeLists.txt \
            file://crashdump.cpp \
            file://crashdump.hpp \
            file://ipmb_peci_interface.c \
            file://ipmb_peci_interface.h \
           "

DEPENDS = "cjson safec gtest libpeci cli11 libipmb libipmi libpal"
RDEPENDS_${PN} += "cjson safec libpeci libipmb libipmi libpal"
