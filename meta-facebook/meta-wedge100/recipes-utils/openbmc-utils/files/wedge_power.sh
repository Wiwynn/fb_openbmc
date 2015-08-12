#!/bin/bash
#
# Copyright 2015-present Facebook. All Rights Reserved.
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

. /usr/local/bin/openbmc-utils.sh

PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin

prog="$0"

PWR_BTN_GPIO="BMC_PWR_BTN_OUT_N"
PWR_SYSTEM_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_cyc_all_n"
PWR_USRV_RST_SYSFS="${SYSCPLD_SYSFS_DIR}/usrv_rst_n"

usage() {
    echo "Usage: $prog <command> [command options]"
    echo
    echo "Commands:"
    echo "  status: Get the current microserver power status"
    echo
    echo "  on: Power on microserver if not powered on already"
    echo "    options:"
    echo "      -f: Re-do power on sequence no matter if microserver has "
    echo "          been powered on or not."
    echo
    echo "  off: Power off microserver ungracefully"
    echo
    echo "  reset: Power reset microserver ungracefully"
    echo "    options:"
    echo "      -s: Power reset whole wedge system ungracefully"
    echo
}

do_status() {
    echo -n "Microserver power is "
    if wedge_is_us_on; then
        echo "on"
    else
        echo "off"
    fi
    return 0
}

do_on_com_e() {
    echo 1 > $PWR_USRV_SYSFS
}

do_on_panther_plus() {
    local pulse_us n retries
    # generate the power on pulse
    pulse_us=500000             # 500ms
    retries=3
    n=1
    while true; do
        gpio_set $PWR_BTN_GPIO 1
        usleep $pulse_us
        # generate the power on pulse
        gpio_set $PWR_BTN_GPIO 0
        usleep $pulse_us
        gpio_set $PWR_BTN_GPIO 1
        sleep 3
        if wedge_is_us_on 1 '' 1; then
            break
        fi
        n=$((n+1))
        if [ $n -gt $retries ]; then
            return 1
        fi
        echo -n "..."
    done
    return 0
}

do_on() {
    local force opt ret
    force=0
    while getopts "f" opt; do
        case $opt in
            f)
                force=1
                ;;
            *)
                usage
                exit -1
                ;;

        esac
    done
    echo -n "Power on microserver ..."
    if [ $force -eq 0 ]; then
        # need to check if uS is on or not
        if wedge_is_us_on 10 "."; then
            echo " Already on. Skip!"
            return 1
        fi
    fi

    # power on sequence
    if wedge_has_com_e; then
        do_on_com_e
        ret=$?
    else
        do_on_panther_plus
        ret=$?
    fi

    if [ $ret -eq 0 ]; then
        echo " Done"
    else
        echo " Failed"
    fi
    return $ret
}

do_off_panther_plus() {
    # first make sure, button is high
    gpio_set $PWR_BTN_GPIO 1
    # then, hold it down for 5s
    gpio_set $PWR_BTN_GPIO 0
    sleep 5
    gpio_set $PWR_BTN_GPIO 1
    return 0
}

do_off_com_e() {
    echo 0 > $PWR_USRV_SYSFS
    return 0
}

do_off() {
    local ret
    echo -n "Power off microserver ..."
    if wedge_has_com_e; then
        do_off_com_e
        ret=$?
    else
        do_off_panther_plus
        ret=$?
    fi

    if [ $ret -eq 0 ]; then
        echo " Done"
    else
        echo " Failed"
    fi
    return $ret
}

do_reset() {
    local system opt
    system=0
    while getopts "s" opt; do
        case $opt in
            s)
                system=1
                ;;
            *)
                usage
                exit -1
                ;;
        esac
    done
    if [ $system -eq 1 ]; then
        echo -n "Power reset whole system ..."
        echo 0 > $PWR_SYSTEM_SYSFS
    else
        if ! wedge_is_us_on; then
            echo "Power resetting microserver that is powered off has no effect."
            echo "Use '$prog on' to power the microserver on"
            return -1
        fi
        echo -n "Power reset microserver ..."
        echo 0 > $PWR_USRV_RST_SYSFS
        sleep 1
        echo 1 > $PWR_USRV_RST_SYSFS
    fi
    echo " Done"
    return 0
}

if [ $# -lt 1 ]; then
    usage
    exit -1
fi

command="$1"
shift

case "$command" in
    status)
        do_status $@
        ;;
    on)
        do_on $@
        ;;
    off)
        do_off $@
        ;;
    reset)
        do_reset $@
        ;;
    *)
        usage
        exit -1
        ;;
esac

exit $?
