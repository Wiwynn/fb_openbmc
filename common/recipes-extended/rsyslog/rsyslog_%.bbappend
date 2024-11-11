FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
PACKAGECONFIG:remove = "gnutls"
DEPENDS:append = " curl libgcrypt"
PTEST_ENABLED = ""

# This patch to initrd doesn't apply to the latest rsyslog in Yocto Kirkstone.
# Eventually we'll need to have a distro switch in here, but for now, we can
# just turn it off for systemd platforms.
SRC_URI += " \
    ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', '', \
        'file://0001-initd-use-flock-on-start-stop-daemon.patch;patchdir=${WORKDIR}', \
        d)} \
"

SRC_URI:append = " \
    ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'file://pidfile.conf', '', d)} \
"

do_install:append() {
    # Force rsyslog to create a PID file so that logrotate works correctly.
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
        mkdir -p ${D}/etc/systemd/system/syslog.service.d/
        install -m 644 ${WORKDIR}/pidfile.conf ${D}/etc/systemd/system/syslog.service.d/pidfile.conf
    fi
}
