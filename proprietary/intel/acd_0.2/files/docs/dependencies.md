# Dependencies

## BOOST
- Install [Boost](https://www.boost.org/)
    - Tested with: version 1.73

## SDBUSPLUS
- Install [sdbusplus](https://github.com/openbmc/sdbusplus)
    -  Tested with: 0f19c87276e46b56edbae70a71749353d401ed39

## SAFECLIB
- Install [safeclib](https://github.com/rurban/safeclib)
    - Tested with: 5da8464c092b2f234442824d3dbb49343e58bb16

## PECI
- <peci.h> from [libpeci](https://github.com/openbmc/libpeci)
    - Tested with: bc641112abc99b4a972665aa984023a6713a21ac
- <peci-ioctl.h> from [PECI kernel driver](https://github.com/openbmc/linux/blob/dev-5.4/include/uapi/linux/peci-ioctl.h)

## CJSON
- Install [CJSON](https://github.com/DaveGamble/cJSON)
    - Tested with verison v1.7.12

## NVD (optional)
- [nvd](https://github.com/Intel-BMC/optane-memory)

## BAFI (optional)
- [bafi](https://github.com/Intel-BMC/bafi)

## GTEST (optional - only when building unit tests)
- Install [gtest](https://github.com/google/googletest)
    - Tested with: 2134e3fd857d952e03ce76064fad5ac6e9036104