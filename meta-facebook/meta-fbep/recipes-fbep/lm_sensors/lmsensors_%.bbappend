
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://fbep.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/fbep.conf ${D}${sysconfdir}/sensors.d/fbep.conf
}
