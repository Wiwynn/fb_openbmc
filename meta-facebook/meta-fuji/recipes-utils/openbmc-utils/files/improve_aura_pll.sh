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

config_aura_cl_parameter() {
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x00
    /usr/sbin/i2cset -y -f 10 0x68 0x2c 0xa2
    /usr/sbin/i2cset -y -f 10 0x68 0x2d 0x29
    /usr/sbin/i2cset -y -f 10 0x68 0x0f 0x02
}

check_volatile_fix() {
    turn_aura_page 0x0
    reg_read=$(read_aura_reg 0x2d)

    # Return values:
    #    0: Needs fix,
    #    1: Already fixed,
    #    Anything Else: Unknown state
    if [ "$reg_read" == "0x3f" ]; then
        echo "0"
    elif [ "$reg_read" == "0x29" ]; then
        echo "1"
    else
        echo "2"
    fi
}

check_legacy_volatile_fix() {
    # Enable Debug Mode
    /usr/sbin/i2cset -y -f 10 0x68 0xff 0x0a
    /usr/sbin/i2cset -y -f 10 0x68 0xbd 0xc3
    /usr/sbin/i2cset -y -f 10 0x68 0xb3 0x57

    # Read Driver
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

    # Disable Debug Mode
    /usr/sbin/i2cset -y -f 10 0x68 0xbd 0x00

    # Return "1" if previous fix is detected, otherwise "0"
    if [ "$driver" == "0x00 0x20 0xaa 0xaa 0x2a 0x16 0x34 0x00" ]; then
        echo "1"
    else
        echo "0"
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
       echo "Aura chip not found. Exiting."
       echo "AURA_NOT_FOUND"
       exit 0
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
           legacy_fix=$(check_legacy_volatile_fix)
           if [ "$legacy_fix" == "1" ]; then
              echo "Legacy fix applied. Skip applying the new fix."
              echo "AURA_FIX_SUCCESS"
              exit
           else
              echo "No Volatile Aura Fix Found"
           fi
        else
           echo "Aura is in bad state. Trying soft reset of Aura."
           /usr/sbin/i2cset -y -f 10 0x68 0xff 0x00
           /usr/sbin/i2cset -y -f 10 0x68 0xfe 0x01
           /usr/sbin/i2cset -y -f 10 0x68 0xfe 0x00
           sleep 1
        fi
        echo "Applying Auro Volatile Fix..."
        config_aura_cl_parameter
        vola_fix=$(check_volatile_fix)
        if [ "$vola_fix" == "1" ]; then
             echo "AURA_FIX_SUCCESS"
             exit 0
        else
             echo "Unexpected driver value after programming."
        fi
    done
    echo "AURA_FIX_FAILURE"
    exit 1
}

# Main Entry. Run the script to improve Aura Pll
if [ "$1" == "check" ]; then
    if [ "$(check_if_aura)" != "1" ]; then
        echo "AURA_NOT_FOUND"
        exit 0
    fi
    status=$(check_volatile_fix)
    if [ "$status" == "1" ]; then
         echo "FIX_APPLIED"
         exit 0
    else
         legacy_fix=$(check_legacy_volatile_fix)
         if [ "$legacy_fix" == "1" ]; then
              echo "Legacy fix found. Mark it as fix appied." 
              echo "FIX_APPLIED"
              exit
         fi
         echo "FIX_NOT_APPLIED"
         exit 1
    fi
else
    improve_aura_pll
fi
