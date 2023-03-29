# Copyright 2022-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

LOCAL_URI += " \
    file://PlatInfo.cpp \
    "

DEPENDS += "libipmi libpldm-oem"
LDFLAGS =+ "-lpldm_oem"
EXTRA_OECMAKE = "-DNO_SYSTEMD=ON -DBIC_PECI_INTF=ON"
