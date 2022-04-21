# Copyright 2017-present Facebook. All Rights Reserved.

FILESEXTRAPATHS:append := "${THISDIR}/files:"

LOCAL_URI += " \
    file://PlatInfo.cpp \
    file://ipmb_peci_interface.c \
    file://ipmb_peci_interface.h \
    "

DEPENDS += "cjson safec gtest libpeci cli11 libipmb libipmi libpal"
RDEPENDS:${PN} += "cjson safec libpeci libipmb libipmi libpal"
EXTRA_OECMAKE = "-DIPMB_PECI_INTF=ON"

