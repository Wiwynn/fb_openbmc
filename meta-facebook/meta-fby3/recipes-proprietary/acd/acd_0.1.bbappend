# Copyright 2017-present Facebook. All Rights Reserved.

FILESEXTRAPATHS_append := "${THISDIR}/files:"
SRC_URI += "file://PlatInfo.cpp \
            file://ipmb_peci_interface.c \
            file://ipmb_peci_interface.h \
           "

EXTRA_OECMAKE = "-DIPMB_PECI_INTF=ON"

DEPENDS += "libipmb libipmi libpal"
RDEPENDS_${PN} += "libipmb libipmi libpal"
