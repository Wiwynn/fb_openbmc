require meta-yosemite4/recipes-phosphor/images/fb-yosemite4-phosphor-image.inc

IMAGE_FEATURES:append = " \
    obmc-leds \
"

OBMC_IMAGE_EXTRA_INSTALL:append = " \
    bic-update \
    bios-fw-update \
    cpld-fw-handler \
    cxl-fw-update \
    i3c-tools \
    loadsvf \
"
