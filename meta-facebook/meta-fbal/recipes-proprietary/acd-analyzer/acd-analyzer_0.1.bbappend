# Copyright 2021-present Facebook. All Rights Reserved.
FILESEXTRAPATHS:append := "${THISDIR}/files:"

SRC_URI += "file://4s_device_map.json \
            file://4s_memory_map.json \
            file://ex_device_map.json \
            file://ex_memory_map.json \
           "

EXTRA_MAP_FILES = "4s_device_map.json 4s_memory_map.json ex_device_map.json ex_memory_map.json"

do_install:append() {
  install -d ${D}/var/bafi
  for f in ${EXTRA_MAP_FILES}; do
    install -m 644 ${WORKDIR}/$f ${D}/var/bafi/$f
  done
}
