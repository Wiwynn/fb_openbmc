FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " \
    file://0100-Revert-Explicitly-set-verify_none.patch \
    file://0101-mutual-tls-meta-Support-svc-and-host-entity-types.patch \
"
EXTRA_OEMESON:append = " -Dredfish-updateservice-use-dbus=enabled"
