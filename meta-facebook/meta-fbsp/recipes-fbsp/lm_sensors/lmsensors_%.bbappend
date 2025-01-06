
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://fbsp.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/fbsp.conf ${D}${sysconfdir}/sensors.d/fbsp.conf
}
