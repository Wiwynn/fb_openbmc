FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += " \
    file://0001-configurations-nvidia_hmc-add-satellite-sensors.patch \
    file://0002-configurations-nvidia_io_board-recalculate-bus-numbe.patch \
    file://0003-configurations-Revise-CX7-NIC-card-temperature-senso.patch \
"
