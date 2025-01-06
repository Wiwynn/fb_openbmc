SUMMARY = "Facebook OpenBMC Network packages"

LICENSE = "GPL-2.0-only"
PR = "r1"

inherit packagegroup

RDEPENDS:${PN} += " \
  dhcp-client \
  iproute2 \
  ntp \
  ntp-utils \
  ntpdate \
  sntp \
  "
