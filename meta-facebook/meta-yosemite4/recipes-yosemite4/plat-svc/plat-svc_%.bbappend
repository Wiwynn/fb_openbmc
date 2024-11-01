FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://yosemite4-i2cv2-init.service \
    file://yosemite4-i2cv2-init \
    "

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN}:append = " \
    yosemite4-i2cv2-init.service \
    "

do_install:append:() {
    install -d ${D}${libexecdir}
    install -m 0755 ${WORKDIR}/yosemite4-i2cv2-init ${D}${libexecdir}
}
