# Copyright 2022-present Facebook. All Rights Reserved.

require recipes-core/images/fboss-lite-image.inc
require fbdarwin-image-layout.inc

IMAGE_INSTALL += " \
  prefdl-eeprom \
  serfmon-cache \
  show-tech \
  "

#
# IPMI is Not supported in fbdarwin.
#
IMAGE_INSTALL:remove = " \
  ipmi-lite \
  kcsd \
  "
