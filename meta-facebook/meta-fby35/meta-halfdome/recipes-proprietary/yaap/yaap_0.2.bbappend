# Copyright 2022-present Facebook. All Rights Reserved.

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
DEFAULT_PREFERENCE = "1"
DEPENDS += "libbic libfby35-common"

EXTRA_OEMESON += " -Dbic-jtag=enabled"

LOCAL_URI += " \
    file://Source/Linux/bmc/plat/meson.build \
    file://Source/Linux/bmc/plat/yaap_platform.cpp \
"
