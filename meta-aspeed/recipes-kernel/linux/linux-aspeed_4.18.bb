SRC_URI = "file://linux-aspeed-4.18 \
          "

LINUX_VERSION ?= "4.18.16"
LINUX_VERSION_EXTENSION ?= "-aspeed"
LIC_FILES_CHKSUM = "file://COPYING;md5=bbea815ee2795b2f4230826c0c6b8814"

PR = "r1"
PV = "${LINUX_VERSION}"

include linux-aspeed.inc

S = "${WORKDIR}/linux-aspeed-4.18"
