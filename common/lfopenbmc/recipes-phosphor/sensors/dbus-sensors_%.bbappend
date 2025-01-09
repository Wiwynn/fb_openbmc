FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " \
    file://0001-PSUSensor-add-ina233-support.patch \
    file://0002-PSUSensor-add-adm1281-support.patch \
    file://0003-DeviceMgmt-fix-device-not-found.patch \
    file://0004-PSUSensor-Fix-error-for-decimal-part-of-scalefactor.patch \
"

SRC_URI:append:fb-compute-multihost = " \
     file://0005-Utils-support-powerState-for-multi-node-system.patch \
"
