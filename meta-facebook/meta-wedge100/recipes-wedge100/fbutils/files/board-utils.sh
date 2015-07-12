# Copyright 2015-present Facebook. All Rights Reserved.
wedge_iso_buf_enable() {
    # TODO, no isolation buffer
    return 0
}

wedge_iso_buf_disable() {
    # TODO, no isolation buffer
    return 0
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
    return 0
}

wedge_board_rev() {
    # TODO
    return 0
}

# Should we enable OOB interface or not
wedge_should_enable_oob() {
    # wedge100 uses BMC MAC since beginning
    return -1
}
