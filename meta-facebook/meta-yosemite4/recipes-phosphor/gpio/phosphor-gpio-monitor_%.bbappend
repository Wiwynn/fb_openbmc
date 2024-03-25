FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

inherit obmc-phosphor-systemd systemd

SRC_URI += "file://yosemite4-phosphor-multi-gpio-monitor.json \
            file://yosemite4-n-phosphor-multi-gpio-monitor.json \
            file://set-button-sled.service \
            file://probe-slot-device@.service \
            file://probe-slot-device \
            file://rescan-fru-device@.service \
            file://rescan-fru-device \
            file://slot-hot-plug@.service \
            "

RDEPENDS:${PN}:append = " bash"

FILES:${PN} += "${systemd_system_unitdir}/*"

SYSTEMD_SERVICE:${PN} += " \
    set-button-sled.service \
    probe-slot-device@.service \
    rescan-fru-device@.service \
    slot-hot-plug@.service \
    "

SYSTEMD_AUTO_ENABLE = "enable"

do_install:append:() {
    install -m 0644 ${WORKDIR}/set-button-sled.service ${D}${systemd_system_unitdir}/set-button-sled.service
    install -m 0644 ${WORKDIR}/probe-slot-device@.service ${D}${systemd_system_unitdir}/probe-slot-device@.service
    install -m 0644 ${WORKDIR}/rescan-fru-device@.service ${D}${systemd_system_unitdir}/rescan-fru-device@.service
    install -m 0644 ${WORKDIR}/slot-hot-plug@.service ${D}${systemd_system_unitdir}/slot-hot-plug@.service
    install -d ${D}${libexecdir}/${PN}
    install -m 0755 ${WORKDIR}/probe-slot-device ${D}${libexecdir}/${PN}/
    install -m 0755 ${WORKDIR}/rescan-fru-device ${D}${libexecdir}/${PN}/
}

do_install:append:yosemite4() {
    install -d ${D}${datadir}/phosphor-gpio-monitor
    install -m 0644 ${WORKDIR}/yosemite4-phosphor-multi-gpio-monitor.json \
                    ${D}${datadir}/phosphor-gpio-monitor/phosphor-multi-gpio-monitor.json
}

do_install:append:yosemite4-n() {
    install -d ${D}${datadir}/phosphor-gpio-monitor
    install -m 0644 ${WORKDIR}/yosemite4-n-phosphor-multi-gpio-monitor.json \
                    ${D}${datadir}/phosphor-gpio-monitor/phosphor-multi-gpio-monitor.json
}