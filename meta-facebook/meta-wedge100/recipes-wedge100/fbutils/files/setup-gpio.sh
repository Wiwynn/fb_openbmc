#!/bin/bash
#
# Copyright 2014-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#

### BEGIN INIT INFO
# Provides:          gpio-setup
# Required-Start:
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description:  Set up GPIO pins as appropriate
### END INIT INFO

# This file contains definitions for the GPIO pins that were not otherwise
# defined in other files.  We should probably move some more of the
# definitions to this file at some point.

# The commented-out sections are generally already defined elsewhere,
# and defining them twice generates errors.

# The exception to this is the definition of the GPIO H0, H1, and H2
# pins, which seem to adversely affect the rebooting of the system.
# When defined, the system doesn't reboot cleanly.  We're still
# investigating this.

. /usr/local/fbpackages/utils/ast-functions

# BMC_PWR_BTN_IN_N, uServer power button in, on GPIO D0
# BMC_PWR_BTN_OUT_N, uServer power button out, on GPIO D1
devmem_clear_bit $(scu_addr 90) 1
devmem_clear_bit $(scu_addr 8c) 8
devmem_clear_bit $(scu_addr 70) 21
gpio_export D0 BMC_PWR_BTN_IN_N
gpio_export D1 BMC_PWR_BTN_OUT_N

# BMC_ISO_BUF_EN: GPIOC2 to low, SCU90[0] and SCU90[24] must be 0
devmem_clear_bit $(scu_addr 90) 0
devmem_clear_bit $(scu_addr 90) 24
gpio_export C2 BMC_ISO_BUF_EN

# Set BMC_PWR_BTN_OUT_N to high before enabling BMC_ISO_BUF_EN
gpio_set D1 1

# Enable isolation buffer
gpio_set C2 1
