FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

PACKAGECONFIG[satellitesensor] = "-Dsatellite=enabled, -Dsatellite=disabled"

SYSTEMD_SERVICE:${PN} += "${@bb.utils.contains('PACKAGECONFIG', 'satellitesensor', \
                                               'xyz.openbmc_project.satellitesensor.service', \
                                               '', d)}"

PACKAGECONFIG += " \
    satellitesensor \
"

SRC_URI += " \
    file://0101-dbus-sensors-Add-SatelliteSensor-support.patch \
    file://0102-SatelliteSensor-add-PowerState-support.patch \
    "

SYSTEMD_OVERRIDE:${PN}:append = "\
    wait-host0-state-ready.conf:xyz.openbmc_project.satellitesensor.service.d/wait-host0-state-ready.conf \
"
