Crashdump

## Installation Steps:
Intel Builds CrashDump against this OpenBMC version.
https://github.com/Intel-BMC/openbmc/tree/intel/meta-openbmc-mods

Dependencies:
- <peci.h> from libpeci (https://github.com/openbmc/libpeci)
    Tested with: a2ceec2aa139277cebb62e1eda449ef60fa4c962
- <peci-ioctl.h> from the PECI kernel driver (https://github.com/openbmc/linux/blob/dev-5.4/include/uapi/linux/peci-ioctl.h)
- Install sdbusplus (https://github.com/openbmc/sdbusplus)
    Tested with: 1b7a58871cba4b97025f493f72cbb26ca6b21254
- Install boost (https://www.boost.org/) Tested with: version 1.73
- Install CJSON (https://github.com/DaveGamble/cJSON) Tested with verison v1.7.12
- Install safeclib (https://github.com/rurban/safeclib)
    Tested with: 60786283fd61cd621a5d1df00e083a1c1e3cf52a
- (optional)Install gtest (https://github.com/google/googletest)
    Tested with: 2134e3fd857d952e03ce76064fad5ac6e9036104

## BUILD
run cmake . to generate make files
(Optional to run unit testing) run cmake --DCRASHDUMP_BUILD_UT=ON
run make on same directory

## SECURITY FLAGS
if your compiler supports we recommend to add to the cmake file
set(CMAKE_CXX_FLAGS "-fstack-protector-strong")
set(CMAKE_C_FLAGS "-fstack-protector-strong")

## AutoStart
copy the file com.intel.crashdump into the /lib/systemd/system directory.
Once it is copied you can manually start the process typing systemctl start
    com.intel.crashdump

