# overwrite the PV and SRC_URI to use in-tree source code
# instead of git://github.com/facebook/openbmc-uboot.git
# as FB shipit service will automatically expose the code
# changes in tree there
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

def uboot_base_pv(d):
    return d.getVar('PV', True).split('+')[0]
uboot_base_pv[vardeps] = "PV"

PV := "${@uboot_base_pv(d)}"

SRC_URI:remove = "git://github.com/facebook/openbmc-uboot.git;branch=${SRCBRANCH};protocol=https"
# Notice have to use append and don't remove the leading space
SRC_URI:append = " file://u-boot-${PV}"
S = "${WORKDIR}/u-boot-${PV}"
