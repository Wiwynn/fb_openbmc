FILESEXTRAPATHS:prepend := "${THISDIR}/linux-nuvoton:${THISDIR}/../../../../recipes-kernel/linux/files:"

KSRC = "git://github.com/Wiwynn/linux-nuvoton.git;protocol=https;branch=${KBRANCH}"
KBRANCH = "NPCM-6.1-OpenBMC"
LINUX_VERSION = "6.1.12"
SRCREV = "bde7d51c162671bde79e7d5755ebc66d358fd18a"

SRC_URI += "file://yosemite4-common.cfg \
            file://yosemite4.cfg \
            file://0001-ARM64-dts-nuvoton-Add-initial-yosemitev4-device-tree.patch \
            file://0002-arm64-dts-Revised-gpio-i2c-and-uart-setting-for-yose.patch \
            file://0003-arm64-dts-yosemite4-n-add-regulator-for-ADC.patch \
            file://0004-arm64-dts-yosemite4-n-add-flash-1-in-DTS.patch \
            file://0005-yosemite4-n-dts-add-slew-rate-for-spi-and-i3c.patch \
            file://0006-yosemite4-n-dts-add-tpm-setting.patch \
            file://0007-hwmon-max31790-support-to-config-PWM-as-TACH.patch \
            file://0008-hwmon-max31790-add-fanN_enable-for-all-fans.patch \
           "
