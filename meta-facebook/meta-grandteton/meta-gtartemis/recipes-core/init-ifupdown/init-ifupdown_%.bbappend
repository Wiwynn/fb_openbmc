FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

NETWORK_INTERFACES = "auto/lo manual/eth0"

RUN_DHC6_FILE = "run-dhc6_prefix64_ncsi.sh"

do_install:append() {
    sed -i '/up ip link set eth0 up/a \  up sleep 5' ${D}${sysconfdir}/network/interfaces
}