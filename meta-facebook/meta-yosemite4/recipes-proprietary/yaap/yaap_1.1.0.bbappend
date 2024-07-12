# Copyright 2022-present Facebook. All Rights Reserved.

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
DEFAULT_PREFERENCE = "1"

DEPENDS += "libbic"

EXTRA_OEMESON += " -Dbic-jtag=enabled -Denabled-pldm=enabled"

LOCAL_URI += " \
    file://0001-Support-YAAPD-for-YV4.patch \
    file://Source/Linux/bmc/plat/meson.build \
    file://Source/Linux/bmc/plat/yaap_platform.cpp \
"
