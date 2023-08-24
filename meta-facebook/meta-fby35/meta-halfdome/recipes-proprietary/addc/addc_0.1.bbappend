# Copyright 2022-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

EXTRA_OECMAKE = "-DNO_SYSTEMD=ON -DBIC_APML_INTF=ON"

DEPENDS += " libbic cli11 apml"
DEPENDS:remove = "libgpiod"
