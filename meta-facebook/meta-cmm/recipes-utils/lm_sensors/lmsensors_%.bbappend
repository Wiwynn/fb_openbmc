
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://cmm.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/cmm.conf ${D}${sysconfdir}/sensors.d/cmm.conf
}
