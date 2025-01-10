FILESEXTRAPATHS:prepend := "${THISDIR}/linux-aspeed:"
SRC_URI += " \
    file://1001-i2c-aspeed-Acknowledge-Tx-ack-late-when-in-SLAVE_REA.patch \
    file://1002-ipmi-ssif_bmc-Fix-new-request-loss-when-bmc-ready-fo.patch \
    file://1003-ipmi-ssif_bmc-add-GPIO-based-alert-mechanism.patch \
    file://1004-ARM-dts-aspeed-catalina-update-catalina-dts-file.patch \
    file://1005-ARM-dts-aspeed-catalina-add-extra-ncsi-properties.patch \
    file://1006-ARM-dts-aspeed-catalina-add-ipmb-dev-node.patch \
    file://1007-ARM-dts-aspeed-catalina-add-max31790-nodes.patch \
    file://1008-ARM-dts-aspeed-catalina-add-raa228004-nodes.patch \
    file://1009-ARM-dts-aspeed-catalina-enable-ssif-alert-pin.patch \
    file://1010-ARM-dts-aspeed-catalina-enable-uart-dma-mode.patch \
    file://1011-ARM-dts-aspeed-catalina-update-pdb-board-cpld-ioexp-.patch \
    file://1012-ARM-dts-aspeed-catalina-add-hdd-board-cpld-ioexp.patch \
    file://catalina-uart.cfg \
"
