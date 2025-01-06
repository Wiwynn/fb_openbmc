
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://yosemite.conf \
           "

do_install:append() {
    install -d ${D}${sysconfdir}/sensors.d
    install -m 644 ${UNPACKDIR}/yosemite.conf ${D}${sysconfdir}/sensors.d/yosemite.conf
}
