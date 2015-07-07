#!/bin/sh

. /usr/local/fbpackages/utils/ast-functions

/usr/local/bin/watchdog_ctrl.sh off

# Disable the dual boot watch dog
devmem_clear_bit 0x1e78502c 0
