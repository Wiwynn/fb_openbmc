# this is a wild bbappending for all packages begin with u-boot
# thus it will apply to all version of
# u-boot, u-boot-fw-utils, u-boot-tools(or legacy u-boot-mkimage)
# the main purpose is to use local u-boot code base instead of
# git://github.com/facebook/openbmc-uboot.git
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

def uboot_base_pv(d):
    return d.getVar('PV', True).split('+')[0]
uboot_base_pv[vardeps] = "PV"

PV := "${@uboot_base_pv(d)}"

SRC_URI:remove:uboot-aspeed-fb = "git://github.com/facebook/openbmc-uboot.git;branch=${SRCBRANCH};protocol=https"
# Notice have to use append and don't remove the leading space
SRC_URI:append:uboot-aspeed-fb = " file://u-boot-${PV}"
S:uboot-aspeed-fb = "${WORKDIR}/u-boot-${PV}"
