#!/bin/sh
#
# Copyright 2020-present Facebook. All Rights Reserved.
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
# Provides:          setup-fan
# Required-Start:    board-id
# Required-Stop:
# Default-Start:     S
# Default-Stop:
# Short-Description: Set fan speed
### END INIT INFO

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

# shellcheck disable=SC1091
. /usr/local/bin/openbmc-utils.sh

if ! wedge_is_bmc_personality; then
    echo "uServer is not in BMC personality. Skipping all fan control"
    return
fi

echo "Setup fan speed..."
/usr/local/bin/set_fan_speed.sh 30
# ELBERTTODO TUNE FSCD
runsv /etc/sv/fscd > /dev/null 2>&1 &
echo "done."
