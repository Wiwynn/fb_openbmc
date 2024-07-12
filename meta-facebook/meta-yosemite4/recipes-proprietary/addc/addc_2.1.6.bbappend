# Copyright 2022-present Facebook. All Rights Reserved.
DEFAULT_PREFERENCE = "1"
FILESEXTRAPATHS:append := "${THISDIR}/files:"

EXTRA_OEMESON:append = " -Dbic-apml-intf=true -Denabled-pldm=true"

DEPENDS += " libbic cli11 apml"
DEPENDS:remove = "libgpiodcxx"

inherit systemd

RDEPENDS:${PN}:append = " bash"

SRC_URI += " \
    file://0001-Support-addc-for-YV4.patch \
    file://ras-polling.sh \
    file://ras-polling@.service \
"

FILES:${PN}:append = " \
    ${libexecdir}/* \
    ${systemd_system_unitdir}/* \
"

SYSTEMD_SERVICE:${PN} += "${@' '.join(['ras-polling@{}.service'.format(i) for i in (d.getVar('OBMC_HOST_INSTANCES', True) or '').split()])}"

do_install:append() {
    install -d ${D}${libexecdir}/amd-ras
    install -m 0755 ${WORKDIR}/ras-polling.sh ${D}${libexecdir}/amd-ras/
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/ras-polling@.service ${D}${systemd_system_unitdir}/ras-polling@.service
}
