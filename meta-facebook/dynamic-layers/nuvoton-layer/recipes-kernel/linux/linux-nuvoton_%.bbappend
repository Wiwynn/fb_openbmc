FILESEXTRAPATHS:prepend := "${THISDIR}/files:${THISDIR}/../../../../recipes-kernel/linux/files:"

SRC_URI:append:mf-fb-secondary-emmc = " \
    file://emmc-btrfs.scc \
    file://emmc-btrfs.cfg \
    "

KERNEL_FEATURES:append:mf-fb-secondary-emmc = " \
    emmc-btrfs.scc \
    "
