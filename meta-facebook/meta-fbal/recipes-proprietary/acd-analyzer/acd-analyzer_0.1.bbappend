# Copyright 2021-present Facebook. All Rights Reserved.
FILESEXTRAPATHS_append := "${THISDIR}/files:"

SRC_URI += "file://4s_device_map.json \
            file://4s_memory_map.json \
           "

EXTRA_MAP_FILES = "4s_device_map.json 4s_memory_map.json"

do_install_append() {
  install -d ${D}/var/bafi
  for f in ${EXTRA_MAP_FILES}; do
    install -m 644 ${WORKDIR}/$f ${D}/var/bafi/$f
  done
}
