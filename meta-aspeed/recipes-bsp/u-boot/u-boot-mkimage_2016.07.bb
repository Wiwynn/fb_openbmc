SUMMARY = "U-Boot bootloader image creation tool"
LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=a2c678cfd4a4d97135585cad908541c6"
SECTION = "bootloader"

DEPENDS = "openssl"

SRCREV = "44d5a2f4ff45bbaafff0c3bccc8a9f7509a5dffb"
PV = "v2016.07"

SRC_URI = "file://u-boot-v2016.07 \
           file://fw_env.config \
          "

S = "${WORKDIR}/u-boot-${PV}"

EXTRA_OEMAKE = 'CROSS_COMPILE="${TARGET_PREFIX}" CC="${CC} ${CFLAGS} ${LDFLAGS}" STRIP=true V=1'

do_compile () {
  oe_runmake sandbox_defconfig
  oe_runmake cross_tools NO_SDL=1
}

do_install () {
  install -d ${D}${bindir}
  install -m 0755 tools/mkimage ${D}${bindir}/uboot-mkimage
  ln -sf uboot-mkimage ${D}${bindir}/mkimage
}

BBCLASSEXTEND = "native nativesdk"

