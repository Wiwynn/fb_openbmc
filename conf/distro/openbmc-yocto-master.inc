# recipes-connectivity/dhcp doesn't exist anymore on master but older
# branches need the bbappend for compatibility.
BBMASK:append = " dhcp.*\.bbappend"
# recipes-utils/flashrom/flashrom_1.2 is needed for both dunfell and kirkstone.
BBMASK:append = " flashrom/flashrom_1.2\.bbappend"
# The linux-aspeed_5.% only exists for non-LF systems; this probably wasn't
# a great way to handle these patches anyhow... They should likely be moved
# to the recipe.
BBMASK:append:openbmc-fb-lf = " acd_0.2/linux-aspeed_5\.%\.bbappend"
# QEMU, which is used for ptest, doesn't have the meta-aspeed either.
BBMASK:append:qemuall = " linux/linux-aspeed_%.bbappend"
BBMASK:append:qemuall = " acd_0.2/linux-aspeed_5\.%\.bbappend"
