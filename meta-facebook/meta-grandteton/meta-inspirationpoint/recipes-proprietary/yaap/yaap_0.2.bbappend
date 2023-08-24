# Copyright 2023-present Facebook. All Rights Reserved.

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
DEFAULT_PREFERENCE = "1"

LOCAL_URI += " \
    file://Source/Linux/bmc/plat/meson.build \
    file://Source/Linux/bmc/plat/yaap_platform.cpp \
"
