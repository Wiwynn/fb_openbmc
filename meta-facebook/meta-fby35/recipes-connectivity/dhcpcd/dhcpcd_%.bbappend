FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += " \
    file://wait_mac_addr_ready \
    "

RDEPENDS:${PN}:append = " bash"

do_install:append:mf-ncsi() {
    install -m 0755 ${UNPACKDIR}/wait_mac_addr_ready \
        ${D}${libexecdir}/dhclient/pre.d/
}
