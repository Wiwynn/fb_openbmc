# this is a wild bbappending for all packages begin with u-boot
# thus it will apply to all version of
# u-boot, u-boot-fw-utils, u-boot-tools(or legacy u-boot-mkimage)
# the main purpose is to use local u-boot code base instead of
# git://github.com/facebook/openbmc-uboot.git
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:remove = "git://github.com/facebook/openbmc-uboot.git;branch=${SRCBRANCH};protocol=https"
SRC_URI += "file://u-boot-${PV}"
S = "${WORKDIR}/u-boot-${PV}"
