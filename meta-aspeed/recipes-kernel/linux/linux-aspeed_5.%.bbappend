SRC_URI += "\
    ${@bb.utils.contains('MACHINE_FEATURES', 'mtd-ubifs', \
                         'file://ubifs.scc file://ubifs.cfg', '', d)} \
    "

KERNEL_FEATURES:append = " \
    ${@bb.utils.contains('MACHINE_FEATURES', 'mtd-ubifs', \
                         'ubifs.scc', '', d)} \
    "
