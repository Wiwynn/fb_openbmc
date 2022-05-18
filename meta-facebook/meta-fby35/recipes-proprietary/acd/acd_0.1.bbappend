# Copyright 2017-present Facebook. All Rights Reserved.

FILESEXTRAPATHS:append := "${THISDIR}/files:"

LOCAL_URI += " \
    file://PlatInfo.cpp \
    file://ipmb_peci_interface.c \
    file://ipmb_peci_interface.h \
    "

DEPENDS += "libipmb libipmi"
RDEPENDS:${PN} += "libipmb libipmi"
EXTRA_OECMAKE = "-DIPMB_PECI_INTF=ON"
