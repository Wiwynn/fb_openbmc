inherit aspeed_uboot_image

# /dev
include ast-dev.inc

# Base this image on core-image-minimal
include recipes-core/images/core-image-minimal.bb

# Changing the image compression from gz to lzma achieves 30% saving (~3M).
# However, the current u-boot does not have lzma enabled. Stick to gz
# until we generate a new u-boot image.
IMAGE_FSTYPES += "cpio.lzma.u-boot"
UBOOT_IMAGE_ENTRYPOINT = "0x40800000"

PYTHON_PKGS = " \
  python-core \
  python-io \
  python-json \
  python-shell \
  python-subprocess \
  python-argparse \
  python-ctypes \
  python-datetime \
  python-email \
  python-threading \
  python-mime \
  python-pickle \
  python-misc \
  python-netserver \
  "

NTP_PKGS = " \
  ntp \
  ntp-utils \
  sntp \
  ntpdate \
  "

# Include modules in rootfs
IMAGE_INSTALL += " \
  kernel-modules \
  u-boot-fw-utils \
  fbutils \
  fan-ctrl \
  rackmon \
  watchdog-ctrl \
  i2c-tools \
  sensor-setup \
  usb-console \
  oob-nic \
  lmsensors-sensors \
  wedge-eeprom \
  sms-kcsd \
  rest-api \
  bottle \
  ipmid \
  po-eeprom \
  bitbang \
  ${PYTHON_PKGS} \
  ${NTP_PKGS} \
  iproute2 \
  dhcp-client \
  "

IMAGE_FEATURES += " \
  ssh-server-openssh \
  tools-debug \
  "

DISTRO_FEATURES += " \
  ext2 \
  ipv6 \
  nfs \
  usbgadget \
  usbhost \
  "

# 24M
IMAGE_ROOTFS_SIZE = "24576"
# and don't put overhead behind my back
IMAGE_OVERHEAD_FACTOR = "1"

IMAGE_PREPROCESS_COMMAND += " generate_data_mount_dir ; "
IMAGE_POSTPROCESS_COMMAND += " flash_image_generate ; "

FLASH_IMAGE_NAME ?= "flash-${MACHINE}-${DATETIME}"
FLASH_IMAGE_LINK ?= "flash-${MACHINE}"
# 16M
FLASH_SIZE ?= "16384"
FLASH_UBOOT_OFFSET ?= "0"
# 512k
FLASH_KERNEL_OFFSET ?= "512"
# 3M
FLASH_ROOTFS_OFFSET ?= "3072"

flash_image_generate() {
  kernelfile="${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGETYPE}"
  ubootfile="${DEPLOY_DIR_IMAGE}/u-boot.${UBOOT_SUFFIX}"
  # rootfs has to match the type defined in IMAGE_FSTYPES"
  rootfs="${DEPLOY_DIR_IMAGE}/${IMAGE_LINK_NAME}.cpio.lzma.u-boot"
  if [ ! -f $kernelfile ]; then
    echo "Kernel file ${kernelfile} does not exist"
    return 1
  fi
  if [ ! -f $ubootfile ]; then
    echo "U-boot file ${ubootfile} does not exist"
    return 1
  fi
  if [ ! -f $rootfs ]; then
    echo "Rootfs file ${rootfs} does not exist"
    return 1
  fi
  dst="${DEPLOY_DIR_IMAGE}/${FLASH_IMAGE_NAME}"
  rm -rf $dst
  dd if=/dev/zero of=${dst} bs=1k count=${FLASH_SIZE}
  dd if=${ubootfile} of=${dst} bs=1k seek=${FLASH_UBOOT_OFFSET}
  dd if=${kernelfile} of=${dst} bs=1k seek=${FLASH_KERNEL_OFFSET}
  dd if=${rootfs} of=${dst} bs=1k seek=${FLASH_ROOTFS_OFFSET}
  dstlink="${DEPLOY_DIR_IMAGE}/${FLASH_IMAGE_LINK}"
  rm -rf $dstlink
  ln -sf ${FLASH_IMAGE_NAME} $dstlink
}

generate_data_mount_dir() {
  mkdir -p "${IMAGE_ROOTFS}/mnt/data"
}
