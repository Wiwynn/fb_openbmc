
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://fbcc.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/fbcc.conf ${D}${sysconfdir}/sensors.d/fbcc.conf
}
