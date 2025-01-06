SUMMARY = "Facebook OpenBMC base packages"

LICENSE = "GPL-2.0-only"
PR = "r1"

inherit packagegroup

RDEPENDS:${PN} += " \
  i2c-tools \
  kernel-modules \
  lmsensors-sensors \
  logrotate \
  os-release \
  passwd-util \
  rsyslog \
  tzdata \
  u-boot \
  u-boot-fw-utils \
  "
