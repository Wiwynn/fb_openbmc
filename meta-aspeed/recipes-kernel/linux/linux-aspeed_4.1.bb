
SRC_URI = "file://linux-aspeed-4.1 \
          "

LINUX_VERSION ?= "4.1.51"
LINUX_VERSION_EXTENSION ?= "-aspeed"
LIC_FILES_CHKSUM = "file://COPYING;md5=d7810fab7487fb0aad327b76f1be7cd7"

PR = "r1"
PV = "${LINUX_VERSION}"

include linux-aspeed.inc

S = "${WORKDIR}/linux-aspeed-4.1"
