
SRC_URI = "file://linux-aspeed-4.7 \
	   file://linux-aspeed-4.7/defconfig-4.7-ast2500 \
          "

LINUX_VERSION ?= "4.7"
LINUX_VERSION_EXTENSION ?= "-aspeed"

PR = "r1"
PV = "${LINUX_VERSION}"

include linux-aspeed.inc

S = "${WORKDIR}/linux-aspeed-4.7"

# The defconfig only constrains itself to ast2500
# Other similar boards can use the same config with a different device tree
kernel_do_configure() {
        touch ${B}/.scmversion ${S}/.scmversion

        cp "${WORKDIR}/linux-aspeed-4.7/defconfig-4.7-ast2500" "${B}/.config"
        eval ${KERNEL_CONFIG_COMMAND}
}

# This isn't the ideal way to use devicetree, but it makes it compatible
# with older uboot. If using a uboot that has the 3 argument bootm,
# then don't use this function
kernel_do_compile() {
        unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS MACHINE

	# Compile dts -> dtb
	oe_runmake ARCH=arm dtbs

	# Compile kernel, ask it for a uImage, which will give us what we really want: the uboot config.
	oe_runmake ${KERNEL_IMAGETYPE_FOR_MAKE} ${KERNEL_ALT_IMAGETYPE} CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_EXTRA_ARGS} $use_alternate_initrd

	# Then delete the uImage - we'll make it later
	rm arch/arm/boot/uImage

	# Output a zImage (a kernel binary without the uboot header)
        oe_runmake zImage CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_EXTRA_ARGS} $use_alternate_initrd

	# Put the dtb file directly on the end of the zImage.
	# CMM specific devicetree for now
	cat arch/arm/boot/zImage arch/arm/boot/dts/aspeed-bmc-facebook-cmm.dtb > arch/arm/boot/zImage-with-dts
	mv arch/arm/boot/zImage-with-dts arch/arm/boot/zImage

	# Now create a uImage from the zImage using the config we created earlier
	$(cat arch/arm/boot/.uImage.cmd | cut -d'=' -f2)
}

EXPORT_FUNCTIONS kernel_do_configure kernel_do_compile
