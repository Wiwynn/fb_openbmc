FILESEXTRAPATHS:prepend := "${THISDIR}/linux-nuvoton:${THISDIR}/../../../../recipes-kernel/linux/files:"

KSRC = "git://github.com/Wiwynn/linux-nuvoton.git;protocol=https;branch=${KBRANCH}"
KBRANCH = "NPCM-6.1-OpenBMC"
LINUX_VERSION = "6.1.12"
SRCREV = "bde7d51c162671bde79e7d5755ebc66d358fd18a"

SRC_URI += "file://yosemite4-common.cfg \
            file://yosemite4.cfg \
            file://0001-arm64-dts-nuvoton-Add-pin127-and-pin159-to-npcm845-p.patch \
            file://0002-ARM64-dts-nuvoton-Add-initial-yosemitev4-device-tree.patch \
           "
