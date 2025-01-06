
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://fbgc.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/fbgc.conf ${D}${sysconfdir}/sensors.d/fbgc.conf
}
