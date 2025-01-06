# Copyright 2021-present Facebook. All Rights Reserved.

SUMMARY = "GPIO Status Monitoring Daemon"
DESCRIPTION = "Daemon for monitoring GPIO"
SECTION = "base"
PR = "r1"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://gpiod.c;beginline=4;endline=16;md5=da35978751a9d71b73679307c4d296ec"

inherit meson

SRC_URI = "file://meson.build \
           file://gpiod.c \
           file://setup-gpiod.sh \
           file://run-gpiod.sh \
          "

S = "${WORKDIR}/sources"
UNPACKDIR = "${S}"

DEPENDS += " libpal update-rc.d-native libgpio-ctrl libobmc-sensors"
RDEPENDS:${PN} += " libpal libgpio-ctrl libobmc-sensors"

do_install:append() {
  install -d ${D}${sysconfdir}/init.d
  install -d ${D}${sysconfdir}/rcS.d
  install -d ${D}${sysconfdir}/sv
  install -d ${D}${sysconfdir}/sv/gpiod
  install -m 755 ${UNPACKDIR}/setup-gpiod.sh ${D}${sysconfdir}/init.d/setup-gpiod.sh
  install -m 755 ${UNPACKDIR}/run-gpiod.sh ${D}${sysconfdir}/sv/gpiod/run
  update-rc.d -r ${D} setup-gpiod.sh start 89 5 .
}

