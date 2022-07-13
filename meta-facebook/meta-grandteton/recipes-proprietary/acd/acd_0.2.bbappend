# Copyright 2022-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

LOCAL_URI += " \
    file://PlatInfo.cpp \
    "

EXTRA_OECMAKE = "-DNO_SYSTEMD=ON"
