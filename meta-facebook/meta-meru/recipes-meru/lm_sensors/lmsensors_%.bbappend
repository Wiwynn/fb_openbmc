
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://meru.conf"

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/meru.conf ${D}${sysconfdir}/sensors.d/meru.conf
}
