FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://0500-Support-get-static-eid-config-from-entity-manager.patch \
    file://0501-Add-method-for-setting-up-MCTP-EID-by-config-path.patch \
    file://0502-mctpd-fix-mctpd-crash-issue.patch \
"
