# Copyright 2022-present Facebook. All Rights Reserved.
FILESEXTRAPATHS:prepend := "${THISDIR}/libpeci:"

def release_patches(d):
    if d.getVar('EAGLESTREAM_CRASHDUMP', True):
        return "file://0002-Add-PECI-Telemetry-command.patch"
    return ""
SRC_URI += '${@release_patches(d)}'
