# Copyright 2023-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

LOCAL_URI += " \
    file://0001-proprietary-intel-BHS-crashdump-v0.9-to-support-with.patch \
    file://0002-proprietary-intel-BHS-crashdump-v0.9-to-support-dump.patch \
    file://0003-proprietary-intel-BHS-crashdump-v0.9-to-support-add-.patch \
    file://0004-proprietary-intel-BHS-crashdump-v0.9-to-support-add-.patch \
    file://0005-greatlakes-olympic-2.0-Add-OP2-Platinfo-for-support-.patch \
    "

DEPENDS += "libipmb libipmi"
RDEPENDS:${PN} += "libipmb libipmi"
EXTRA_OECMAKE = "-DNO_SYSTEMD=ON -DIPMB_PECI_INTF=ON"
