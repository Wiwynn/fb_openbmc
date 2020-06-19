require recipes-bsp/u-boot/u-boot.inc
require u-boot-common.inc

DEPENDS += "bc-native dtc-native"

PV = "v2019.04"
SRC_URI = "file://u-boot-v2019.04"

OVERRIDES += ":bld-uboot"
