# Copyright 2023-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

DEPENDS += "cli11 apml libmisc-utils"
DEPENDS:remove = "libgpiod"
LDFLAGS =+ "-lmisc-utils"
EXTRA_OECMAKE = "-DNO_SYSTEMD=ON"
