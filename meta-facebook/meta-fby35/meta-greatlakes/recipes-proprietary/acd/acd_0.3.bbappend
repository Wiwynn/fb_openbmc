# Copyright 2023-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

LOCAL_URI += " \
    file://PlatInfo.cpp \
    "

DEPENDS += "libipmb libipmi"
RDEPENDS:${PN} += "libipmb libipmi"
EXTRA_OECMAKE = "-DNO_SYSTEMD=ON -DIPMB_PECI_INTF=ON"
