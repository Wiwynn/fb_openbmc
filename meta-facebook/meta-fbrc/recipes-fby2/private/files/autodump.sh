#!/bin/bash

SLOT_NAME=$1

case $SLOT_NAME in
    slot1)
      SLOT_NUM=1
      ;;
    slot2)
      SLOT_NUM=2
      ;;
    slot3)
      SLOT_NUM=3
      ;;
    slot4)
      SLOT_NUM=4
      ;;
    *)
      N=${0##*/}
      N=${N#[SK]??}
      echo "Usage: $N {slot1|slot2|slot3|slot4}"
      exit 1
      ;;
esac

# File format autodump<slot_id>.pid (See pal_is_crashdump_ongoing()
# function definition)
PID_FILE="/var/run/autodump$SLOT_NUM.pid"

# check if auto crashdump is already running
if [ -f $PID_FILE ]; then
  echo "Another auto crashdump for $SLOT_NAME is runnung"
  exit 1
else 
  touch $PID_FILE
fi

DUMP_SCRIPT="/usr/local/bin/dump.sh"
CRASHDUMP_FILE="/mnt/data/crashdump_$SLOT_NAME"

#HEADER LINE for the dump
$DUMP_SCRIPT "time" > $CRASHDUMP_FILE
#COREID dump
$DUMP_SCRIPT $SLOT_NAME "coreid" >> $CRASHDUMP_FILE
#MSR dump
$DUMP_SCRIPT $SLOT_NAME "msr" >> $CRASHDUMP_FILE

rm $PID_FILE
exit 0

