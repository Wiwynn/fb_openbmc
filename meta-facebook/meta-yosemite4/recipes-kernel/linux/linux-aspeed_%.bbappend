FILESEXTRAPATHS:prepend := "${THISDIR}/linux-aspeed:"
SRC_URI += " \
    file://0500-add-meta-yv4-bmc-dts-setting.patch \
    file://0501-hwmon-ina233-Add-ina233-driver.patch \
    file://0502-hwmon-max31790-support-to-config-PWM-as-TACH.patch \
    file://0503-Add-adm1281-driver.patch \
    file://0504-hwmon-max31790-add-fanN_enable-for-all-fans.patch \
    file://0505-ARM-dts-aspeed-yosemite4-support-mux-to-cpld.patch \
    file://0506-ARM-dts-aspeed-yosemite4-Revise-gpio-name.patch \
    file://0507-remove-pincontrol-on-GPIO-U5.patch \
    file://0508-NCSI-Add-propety-no-channel-monitor-and-start-redo-p.patch \
    file://0509-Meta-yv4-dts-add-mac-config-property.patch \
    file://0510-yosemite4-Add-EEPROMs-for-NICs-in-DTS.patch \
    file://0511-Add-ina233-and-ina238-devicetree-config.patch \
    file://0512-i3c-master-export-i3c_masterdev_type.patch \
    file://0513-i3c-master-export-i3c_bus_type-symbol.patch \
    file://0514-i3c-master-add-i3c_for_each_dev-helper.patch \
    file://0515-i3c-add-i3cdev-module-to-expose-i3c-dev-in-dev.patch \
    file://0516-add-i3cdev-documentation.patch \
    file://0517-kernel-6.6-drivers-i3c-add-i3c-related-drivers-from-.patch \
    file://0518-drivers-i3c-add-i3c-hub-driver.patch \
    file://0519-arm-dts-aspeed-add-i3c-config-in-yv4-dts.patch \
    file://0520-ARM-dts-aspeed-g6-Add-I3C-controller-nodes.patch \
    file://0521-i3c-export-send-CCC-command-API.patch \
    file://0522-mctp-i3c-MCTP-I3C-driver.patch \
    file://0523-mctp-i3c-Add-config-option-for-PEC-calculation.patch \
"

