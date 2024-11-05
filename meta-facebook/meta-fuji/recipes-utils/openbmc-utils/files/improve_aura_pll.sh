#!/bin/bash

read_aura_reg () {
    a=$(/usr/sbin/i2cget -y -f 10 0x68 "$1")
    echo "$a"
}

turn_aura_page () {
    /usr/sbin/i2cset -y -f 10 0x68 0xff "$1"
}

write_aura_reg() {
    /usr/sbin/i2cset -y -f 10 0x68 "$1" "$2"
}

dump_aura_page () {
   turn_aura_page "$1"
   for ((num=1; num <= "$2"; num++))
   do
      hex=$(printf '0x%x' "$a")
      read_val=$(read_aura_reg "$hex")
      echo "$1" : "$hex" : "$read_val"
   done
}
dump_aura_all_page() {
    echo Page 0x0
    dump_aura_page 0x0 235
    echo Page 0x1
    dump_aura_page 0x1 47
    echo Page 0x2
    dump_aura_page 0x2 79
    echo Page 0x3
    dump_aura_page 0x3 111
    echo Page 0x4
    dump_aura_page 0x4 111
    echo Page 0x6
    dump_aura_page 0x6 239
    echo Page 0x7
    dump_aura_page 0x7 239
    echo Page 0x0a
    dump_aura_page 0x0a 238
    echo Page 0x1a
    dump_aura_page 0x1a 99
    echo Page 0x0b
    dump_aura_page 0x0b 238
    echo Page 0x1b
    dump_aura_page 0x1b 99
    echo Page 0x0c
    dump_aura_page 0x0c 238
    echo Page 0x1c
    dump_aura_page 0x1c 99
    echo Page 0x0d
    dump_aura_page 0x0d 238
    echo Page 0x1d
    dump_aura_page 0x1d 99
}

#DCO Config
config_aura_dco () {
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0x17 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0x42 0x00
    /usr/sbin/i2cset -y -f 10 0x68 0x57 0x42
    /usr/sbin/i2cset -y -f 10 0x68 0x0f 0x00
    /usr/sbin/i2cset -y -f 10 0x68 0x0f 0x01
    /usr/sbin/i2cset -y -f 10 0x68 0x0f 0x00
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0x0f 0x10
    /usr/sbin/i2cset -y -f 10 0x68 0x0f 0x40
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0xbd 0xc3
    /usr/sbin/i2cset -y -f 10 0x68 0xd1 0xb0
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x1a
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cget -y -f 10 0x68 0x57
    /usr/sbin/i2cset -y -f 10 0x68 0x57 0x42
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0x51 0x00
    /usr/sbin/i2cset -y -f 10 0x68 0x52 0x48
    /usr/sbin/i2cset -y -f 10 0x68 0x53 0x55
    /usr/sbin/i2cset -y -f 10 0x68 0x54 0x55
    /usr/sbin/i2cset -y -f 10 0x68 0x55 0x35
    /usr/sbin/i2cset -y -f 10 0x68 0x56 0x00
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0b
    /usr/sbin/i2cget -y -f 10 0x68 0x57
    /usr/sbin/i2cset -y -f 10 0x68 0x57 0x01
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0c
    /usr/sbin/i2cget -y -f 10 0x68 0x57
    /usr/sbin/i2cset -y -f 10 0x68 0x57 0x01
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0d
    /usr/sbin/i2cget -y -f 10 0x68 0x57
    /usr/sbin/i2cset -y -f 10 0x68 0x57 0x01
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x00
}

# Increase Freq
increase_aura_freq() {
    /usr/sbin/i2cset -y -f 10 0x68 0x64 0xae
    /usr/sbin/i2cset -y -f 10 0x68 0x64 0xee
    /usr/sbin/i2cset -y -f 10 0x68 0x64 0xae
}

# Decrease Freq
decrease_aura_freq () {
    /usr/sbin/i2cset -y -f 10 0x68 0x64 0xae
    /usr/sbin/i2cset -y -f 10 0x68 0x64 0xbe
    /usr/sbin/i2cset -y -f 10 0x68 0x64 0xae
}

check_permanent_fix () {
    echo "0"
}

check_volatile_fix() {
    # Enable Debug Mode
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0xbd 0xc3
    /usr/sbin/i2cset -y -f 10 0x68 0xb3 0x57

    # Read Divider
    reg_read=$(read_aura_reg 0xd0)
    reg_read=$(read_aura_reg 0xb5)
    driver="${reg_read}"
    reg_read=$(read_aura_reg 0xb6)
    driver="${driver} ${reg_read}"
    reg_read=$(read_aura_reg 0xb7)
    driver="${driver} ${reg_read}"
    reg_read=$(read_aura_reg 0xb8)
    driver="${driver} ${reg_read}"
    reg_read=$(read_aura_reg 0xb9)
    driver="${driver} ${reg_read}"
    reg_read=$(read_aura_reg 0xba)
    driver="${driver} ${reg_read}"
    reg_read=$(read_aura_reg 0xbb)
    driver="${driver} ${reg_read}"
    reg_read=$(read_aura_reg 0xbc)
    driver="${driver} ${reg_read}"

    # Read Denominator
    /usr/sbin/i2cset -y -f 10 0x68 0xb3 0x56
    reg_read=$(read_aura_reg 0xd0)
    reg_read=$(read_aura_reg 0xb5)
    denominator="${reg_read}"
    reg_read=$(read_aura_reg 0xb6)
    denominator="${denominator} ${reg_read}"
    reg_read=$(read_aura_reg 0xb7)
    denominator="${denominator} ${reg_read}"
    reg_read=$(read_aura_reg 0xb8)
    denominator="${denominator} ${reg_read}"
    reg_read=$(read_aura_reg 0xb9)
    denominator="${denominator} ${reg_read}"
    reg_read=$(read_aura_reg 0xba)
    denominator="${denominator} ${reg_read}"
    reg_read=$(read_aura_reg 0xbb)
    denominator="${denominator} ${reg_read}"

    # Disable Debug Mode
    /usr/sbin/i2cset -y -f 10 0x68 0xbd 0x00
    # Return values:
    #    0: Needs fix,
    #    1: Already fixed,
    #    Anything Else: Unknown state
    if [ "$driver" == "0x00 0x00 0x55 0x55 0x55 0x15 0x34 0x00" ]; then
        echo "0"
    elif [ "$driver" == "0x00 0x20 0xaa 0xaa 0x2a 0x16 0x34 0x00" ]; then
        echo "1"
    else
        echo "$driver"
    fi
}

check_if_aura() {
    a=$(i2cdetect -y 10|grep -c 68)
    if [ "$a" == "1" ]; then
       echo "1"
    else
       echo "0"
    fi
}

improve_aura_pll() {
    aura_found=$(check_if_aura)
    if [ "$aura_found" == "1" ]; then
       echo "Aura Crystal found. Moving on"
    else
       echo "Aura chip not found. Exiting..."
       exit 0
    fi
    perma_fix=$(check_permanent_fix)
    if [ "$perma_fix" == "1" ]; then
       echo "Permanent Aura Fix Applied. No need to apply volatile fix."
       exit
    else
       echo "No Permanent Aura Fix Found"
    fi
    retry="3"
    while [ "$retry" != "0" ]; do
        retry=$(("$retry" - 1))
        echo "Retry left : $retry"
        vola_fix=$(check_volatile_fix)
        if [ "$vola_fix" == "1" ]; then
           echo "Volatile Aura Fix Applied. No need to apply volatile fix."
           echo "AURA_FIX_SUCCESS"
           exit
        elif [ "$vola_fix" == "0" ]; then
           echo "No Volatile Aura Fix Found"
        else
           echo "Aura is in bad state. Trying soft reset of Aura."
           /usr/sbin/i2cset -y -f 10 0x68 0xff 0x00
           /usr/sbin/i2cset -y -f 10 0x68 0xfe 0x01
           /usr/sbin/i2cset -y -f 10 0x68 0xfe 0x00
           echo "Sleeping for 10 seconds for PLL on Aura to lock"
           sleep 10
        fi
        echo "Applying Auro Volatile Fix..."
        echo "Configuring Aura DCO"
        config_aura_dco
        echo "Increasing Aura all PLL freq : Execution 1 or 4"
        increase_aura_freq
        echo "Increasing Aura all PLL freq : Execution 2 or 4"
        increase_aura_freq
        echo "Increasing Aura all PLL freq : Execution 3 or 4"
        increase_aura_freq
        echo "Increasing Aura all PLL freq : Execution 4 or 4"
        increase_aura_freq
        vola_fix=$(check_volatile_fix)
        if [ "$vola_fix" == "1" ]; then
             echo "AURA_FIX_SUCCESS"
             exit
        else
             echo "Unexpected driver value after programming : $vola_fix"
        fi
    done
    echo "AURA_FIX_FAILURE"
    exit 1
}

# Main Entry. Run the script to improve Aura Pll
if [ "$1" == "check" ]; then
    if [ "$(check_if_aura)" != "1" ]; then
        echo "UNSUPPORTED_HW_AURA_CRYSTAL_NOT_FOUND"
        exit 1
    fi
    status=$(check_volatile_fix)
    if [ "$status" == "1" ]; then
         echo "FIX_APPLIED"
         exit 0
    elif [ "$status" == "0" ]; then
         echo "FIX_NOT_APPLIED"
         exit 1
    else
         echo "UNKNOWN_STATE"
         exit 1
    fi
else
    improve_aura_pll
fi
