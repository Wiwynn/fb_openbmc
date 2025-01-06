SUMMARY = "Facebook OpenBMC RESTful packages"

LICENSE = "GPL-2.0-only"
PR = "r1"

inherit packagegroup

RDEPENDS:${PN} += " \
  aiohttp \
  async-timeout \
  chardet \
  multidict \
  packagegroup-openbmc-python3 \
  rest-api \
  yarl \
  "
