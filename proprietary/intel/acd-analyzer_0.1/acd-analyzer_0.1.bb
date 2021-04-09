
SUMMARY = "CPU Crashdump Analyzer"
DESCRIPTION = "CPU utilities for BMC Assisted FRU Isolation"
SECTION = "base"
PR = "r1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://src/main.cpp;beginline=3;endline=15;md5=0e2780fb91ea7316227e74094c57dabd"

inherit cmake

SRC_URI = "file://CMakeLists.txt \
           file://default_device_map.json \
           file://default_memory_map.json \
           file://default_silkscreen_map.json \
           file://include \
           file://src \
           file://test \
           file://tests \
           file://tools \
          "

S = "${WORKDIR}"

DEPENDS += "nlohmann-json"
