FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:${THISDIR}/../../../../recipes-bsp/uboot/files:"

SRCREV = "687e060e1e3c5d38554732d91301d4bb64a621d0"

SRC_URI +="file://yosemite4-common.cfg \
           file://yosemite4.cfg \
           file://0001-uboot-disable-mmc-gmac3-and-add-r2-pins.patch \
           file://0002-uboot-change-console-type-to-ttyS4.patch \
           "
