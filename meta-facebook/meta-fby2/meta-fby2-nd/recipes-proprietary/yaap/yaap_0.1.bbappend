# Copyright 2022-present Facebook. All Rights Reserved.

FILESEXTRAPATHS:append := "${THISDIR}/files:"
DEFAULT_PREFERENCE = "1"
DEPENDS += "libbic libfby2-common"
RDEPENDS:${PN} += "libbic libfby2-common"

EXTRA_OEMESON += " -Dyaapbic=true -Dyaaplib=bic,fby2_common"
