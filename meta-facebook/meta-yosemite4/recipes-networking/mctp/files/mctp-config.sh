#!/bin/sh

localEid=8

# Set mctpi3c link up and assign local address.
# The Sentinel Dome BICs are after the I3C hub on I3C bus0 and bus1.
busnum=0
while [ $busnum -le 1 ]
do
    mctp link set mctpi3c${busnum} up
    mctp addr add ${localEid} dev mctpi3c${busnum}
    busnum=$((busnum+1))
done

# Set mctpi2c link up and assign local address.
# The NICs are on the i2c bus24 to bus27.
busnum=24
while [ $busnum -le 27 ]
do
    mctp link set mctpi2c${busnum} up
    mctp addr add ${localEid} dev mctpi2c${busnum}
    busnum=$((busnum+1))
done
