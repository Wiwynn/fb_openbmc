SUMMARY = "Phosphor OpenBMC JTAG Library"
DESCRIPTION = "Phosphor OpenBMC JTAG Common Library"
PR = "r1"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

DEPENDS += "systemd glib-2.0"
RDEPENDS_${PN} += "libsystemd glib-2.0"
TARGET_CC_ARCH += "${LDFLAGS}"

S = "${WORKDIR}"

SRC_URI = "file://libobmcjtag.cpp \
           file://libobmcjtag.hpp \
           file://libobmccpld.cpp \
           file://libobmccpld.hpp \
		   file://jtag.h \
		   file://jtag-tool.cpp \
           file://Makefile \
           file://COPYING.MIT \
          "

do_install() {
    install -d ${D}${bindir}
    install -m 0755 jtag-tool ${D}${bindir}/jtag-tool
}

DEPENDS += "cli11"

FILES:${PN} = "${bindir}"