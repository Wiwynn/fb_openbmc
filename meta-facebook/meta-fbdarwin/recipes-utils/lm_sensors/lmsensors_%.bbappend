
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://fbdarwin.conf"

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/fbdarwin.conf ${D}${sysconfdir}/sensors.d/fbdarwin.conf
}
