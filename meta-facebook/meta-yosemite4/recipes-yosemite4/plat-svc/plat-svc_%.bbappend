FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://0001-meta-facebook-yosemite4-optimize-i2cv2-frequency.patch \
    "

do_patch() {
    patch -p1 < "${WORKDIR}/0001-meta-facebook-yosemite4-optimize-i2cv2-frequency.patch"
}
