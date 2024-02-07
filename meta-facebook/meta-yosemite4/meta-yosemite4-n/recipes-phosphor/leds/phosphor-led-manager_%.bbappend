FILESEXTRAPATHS:prepend:= "${THISDIR}/${PN}:"
SRC_URI:append = " file://led.yaml \
                           "

FILES:${PN}:append = " ${datadir}/phosphor-led-manager/led.yaml"

do_install:append() {
    DEST=${D}${datadir}/phosphor-led-manager
    install -d ${DEST}
    install -m 0644 -D ${WORKDIR}/led.yaml ${DEST}/led.yaml
}