
SUMMARY = "CPU Crashdump Analyzer"
DESCRIPTION = "CPU utilities for BMC Assisted FRU Isolation"
SECTION = "base"
PR = "r1"
LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://src/main.cpp;beginline=4;endline=25;md5=e649cb307ba42704786a36f8274d418c"

inherit cmake

SRC_URI = "file://CMakeLists.txt \
           file://default_memory_map.json \
           file://include \
           file://src \
           file://test \
           file://tools \
          "

S = "${WORKDIR}"

DEPENDS += "nlohmann-json"
