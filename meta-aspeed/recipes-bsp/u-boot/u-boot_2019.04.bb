require u-boot-aspeed.inc
require recipes-bsp/u-boot/u-boot.inc

DEPENDS += "bc-native dtc-native"

PV = "v2019.04"
SRC_URI = "file://u-boot-v2019.04"

# the do_compile will only update .scmversion when u-boot source changed
# the UBOOT_LOCALVERSION change won't update .scmversion because 
# the .scmversion file already exists. 
# Thus UBOOT_LOCALVERSION += "${OPENBMC_VERSION}" won't help
#
# prepend action which updates .scmversion with OPENBMC_VERSION 
# to do_compile to make sure when OPENBMC_VERSION update get into u-boot
do_compile_prepend() {
  echo " ${OPENBMC_VERSION}" > ${S}/.scmversion
  echo " ${OPENBMC_VERSION}" > ${B}/.scmversion
}

