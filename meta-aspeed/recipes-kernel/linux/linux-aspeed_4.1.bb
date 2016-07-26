
SRC_URI = "file://linux-aspeed-4.1 \
          "

LINUX_VERSION ?= "4.1.15"
LINUX_VERSION_EXTENSION ?= "-aspeed"

PR = "r1"
PV = "${LINUX_VERSION}"

include linux-aspeed.inc

S = "${WORKDIR}/linux-aspeed-4.1"
