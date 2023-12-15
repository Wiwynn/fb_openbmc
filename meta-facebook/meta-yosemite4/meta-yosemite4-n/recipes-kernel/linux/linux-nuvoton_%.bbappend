FILESEXTRAPATHS:prepend := "${THISDIR}/linux-nuvoton:${THISDIR}/../../../../recipes-kernel/linux/files:"

KSRC = "git://github.com/Wiwynn/linux-nuvoton.git;protocol=https;branch=${KBRANCH}"
KBRANCH = "NPCM-6.1-OpenBMC"
LINUX_VERSION = "6.1.12"
SRCREV = "844feba31d1f127c8370753b51c42be2765fb6b0"

SRC_URI += "file://yosemite4-common.cfg \
            file://yosemite4.cfg \
            file://0001-arm64-dts-nuvoton-Add-pin127-and-pin159-to-npcm845-p.patch \
            file://0002-ARM64-dts-nuvoton-Add-initial-yosemitev4-device-tree.patch \
           "
