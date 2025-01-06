
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://fby35.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/fby35.conf ${D}${sysconfdir}/sensors.d/fby35.conf
}
