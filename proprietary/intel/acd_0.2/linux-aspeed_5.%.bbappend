# Copyright 2022-present Facebook. All Rights Reserved.
FILESEXTRAPATHS:prepend := "${THISDIR}/linux-aspeed:"

def release_patches(d):
    if d.getVar('EAGLESTREAM_CRASHDUMP', True):
        return "file://0002-Add-PECI-Telemetry-command.patch \
                file://0004-Add-ConfigWatcher-workaround.patch \
               "
    return ""
SRC_URI += '${@release_patches(d)}'
