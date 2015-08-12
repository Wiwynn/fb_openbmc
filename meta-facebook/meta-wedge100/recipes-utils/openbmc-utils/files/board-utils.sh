# Copyright 2015-present Facebook. All Rights Reserved.

SYSCPLD_SYSFS_DIR="/sys/class/i2c-adapter/i2c-12/12-0031"
PWR_MAIN_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_main_n"
PWR_USRV_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_usrv_en"
PWR_USRV_BTN_SYSFS="${SYSCPLD_SYSFS_DIR}/pwr_usrv_btn_en"
SLOTID_SYSFS="${SYSCPLD_SYSFS_DIR}/slotid"

wedge_iso_buf_enable() {
    # TODO, no isolation buffer
    return 0
}

wedge_iso_buf_disable() {
    # TODO, no isolation buffer
    return 0
}

wedge_has_com_e() {
    if i2cget -f -y 4 0x20 > /dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

wedge_is_us_on() {
    local val n retries prog
    if [ $# -gt 0 ]; then
        retries="$1"
    else
        retries=1
    fi
    if [ $# -gt 1 ]; then
        prog="$2"
    else
        prog=""
    fi
    if [ $# -gt 2 ]; then
        default=$3              # value 0 means defaul is 'ON'
    else
        default=1
    fi
    if wedge_has_com_e; then
        val=$(cat $PWR_USRV_SYSFS 2> /dev/null | head -n 1)
        if [ -z "$val" ]; then
            return $default
        elif [ "$val" == "0x1" ]; then
            return 0            # powered on
        else
            return 1
        fi
    fi
    # For Panther+, need to read GPIO
    n=1
    while true; do
        val=$(cat /sys/class/i2c-adapter/i2c-4/4-0040/gpio_inputs 2>/dev/null)
        if [ -n "$val" ]; then
            break
        fi
        n=$((n+1))
        if [ $n -gt $retries ]; then
            echo -n " failed to read GPIO. "
            return $default
            break
        fi
        echo -n "$prog"
        sleep 1
    done
    if [ "$((val & (0x1 << 14)))" != "0" ]; then
        # powered on already
        return 0
    else
        return 1
    fi
}

wedge_board_type() {
    echo 'WEDGE100'
}

wedge_slot_id() {
    printf "%d\n" $(cat $SLOTID_SYSFS)
}

wedge_board_rev() {
    local val0 val1 val2
    val0=$(gpio_get BOARD_REV_ID0)
    val1=$(gpio_get BOARD_REV_ID1)
    val2=$(gpio_get BOARD_REV_ID2)
    echo $((val0 | (val1 << 1) | (val2 << 2)))
}

# Should we enable OOB interface or not
wedge_should_enable_oob() {
    # wedge100 uses BMC MAC since beginning
    return -1
}

wedge_power_on_board() {
    local val
    # power on main power, uServer power, and enable power button
    val=$(cat $PWR_MAIN_SYSFS | head -n 1)
    if [ "$val" != "0x1" ]; then
        echo 1 > $PWR_MAIN_SYSFS
        sleep 2
    fi
    val=$(cat $PWR_USRV_BTN_SYSFS | head -n 1)
    if [ "$val" != "0x1" ]; then
        echo 1 > $PWR_USRV_BTN_SYSFS
        sleep 1
    fi
    # For COMe, we don't power on COMe now.
    # For Panther+, we do need to give it power as we need to
    # communicate w/ the FPGA to read GPIOs
    if ! wedge_has_com_e; then
        val=$(cat $PWR_USRV_SYSFS | head -n 1)
        if [ "$val" != "0x1" ]; then
            echo 1 > $PWR_USRV_SYSFS
            sleep 2
        fi
    fi
}
