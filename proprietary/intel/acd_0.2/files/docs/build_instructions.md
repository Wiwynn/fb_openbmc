# Building CrashDump with Intel-BMC/openbmc

## Building OpenBMC image with the crashdump source code

1. Clone Intel OpenBMC code from [GitHub](https://github.com/Intel-BMC/openbmc).
    ```shell
    git clone https://github.com/Intel-BMC/openbmc
    cd openbmc
    ```

2. Clone the `meta-egs` and `meta-restricted` repo inside meta-openbmc-mods.
    ```shell
    cd meta-openbmc-mods
    git clone git@github.com:Intel-BMC/meta-egs.git
    git clone git@github.com:Intel-BMC/meta-restricted.git
    ```

3. Follow this step if you have got `crashdump` source in a tar.gz or zip file.
Else go to step 4.
    ```shell
    cd meta-openbmc-mods/meta-common/recipes-core/crashdump
    mkdir -p files/crashdump
    tar -xvf crashdump.tar.gz -C ./files/crashdump
    ```

4. When the `crashdump` module compiles successfully, OpenBMC needs to install
`crashdump` to the image file. Add the `IMAGE_INSTALL` command to the `local.
conf.sample` file in the `conf` folder.

    ```shell
    cd meta-openbmc-mods/meta-egs
    vi ./conf/local.conf.sample
    ```

    ```vi
    IMAGE_INSTALL += "crashdump"
    ```

5. After the bitbake file is created, use `devtool` as follows to allow OpenBMC
to use the `crashdump` from the source code.
    ```shell
    cd <root of openbmc source code>
    export TEMPLATECONF=meta-openbmc-mods/meta-<platform>/conf
    source oe-init-build-env
    devtool modify crashdump
    devtool build crashdump
    ```

6. Build `openbmc` image.

    ```shell
    bitbake intel-platforms
    ```

## New Input File Format (NIFF)

Follow below steps to enable NIFF test flow:

1. Copy input file to /tmp/crashdump/input directory

```example
cp /usr/share/crashdump/input/crashdump_input_spr.json /tmp/crashdump/input
```

2. Set input file "UseSections" value to "true"

## Enabling optional features

### NVD

1. Ask Intel representative for optane-memory repo access and follow repo
   instruction to setup .bb file.

   [Github](https://github.com/Intel-BMC/optane-memory)

2. Change `NVD_SECTION` flag to ON in CMakeList.txt

   ```
   option (NVD_SECTION "Add NVD section to the crashdump contents." ON)
   ```

   Add fis to DEPENDS variable if it does not already exist.
   ```
   DEPENDS = "boost cjson sdbusplus safec gtest libpeci fis"
   ```

3. For non-Yocto build only

   ```shell
   git clone git@github.com:Intel-BMC/optane-memory.git
   cd optane-memory\fis
   mkdir build && cd build
   cmake ..
   sudo make install
   ```

### BAFI (BMC assisted FRU Isolation)

1. Ask Intel representative for `Intel System Crashdump BMC assisted FRU Isolation`
   access and follow repo instruction to setup .bb file.

   [Intel Developer Zone](https://www.intel.com/content/www/us/en/secure/design/confidential/software-kits/kit-details.html?kitId=724248&s=Newest)

2. Select one of below BAFI options:

   a) Change `TRIAGE_SECTION` flag to ON in CMakeList.txt to enable BAFI triage output (no NDA required):

   ```
   option (TRIAGE_SECTION "Add triage section to the crashdump contents." ON)
   ```

   b) Change `BAFI_NDA_OUTPUT` flag to ON in CMakeList.txt to enable BAFI full output:

   Notes: NDA (Non Disclosure Agreement) is required to see output. Keep this flag OFF when users of crashdump do not have an Intel NDA.

   ```
   option (BAFI_NDA_OUTPUT "Add summary section to the crashdump contents." ON)
   ```

3. For Non-Yocto build only

   ```shell
   # unzip BAFI source code inside crashdump source directory and named bafi
   cd bafi
   mv include bafi

   # Add the following line to CMakeList.txt
   include_directories (${BAFI_SOURCE_DIR})
   ```

## Building and running Crashdump Unit tests

1. Inside the crashdump source code folder, run the following steps

    ```shell
    mkdir build
    cd build
    cmake .. -DCRASHDUMP_BUILD_UT=ON -DCODE_COVERAGE=ON
    make
    ```

2. To run the unit tests execute the binary file in
    ```shell
    cd build
    ./crashdump_ut
    ```

3. To generate the code coverage report, run the following `ccov` target. The
code coverage report can be launched by opening in `build\ccov\index.html`.
    ```
    cd build
    cmake .. -DCRASHDUMP_BUILD_UT=ON -DCODE_COVERAGE=ON
    make ccov
    ```

### Unit Test Ubuntu 18.04 Support

Make the following changes if Ubuntu 18.04 is used for building unit test:

CMakeLists.txt:

Change sdbusplus.git tag value
   ```
   GIT_TAG 6adfe948ee55ffde8457047042d0d55aa3d8b4a7 // original
   GIT_TAG 0f19c87276e46b56edbae70a71749353d401ed39 // change
   ```

crashdump.cpp:

Remove **override** from below function
   ```
   int get_errno() const noexcept override // original
   int get_errno() const noexcept // change
   ```

## Troubleshooting
The following are possible issues and the corresponding solutions.

### PECI lib failure
- EagleStream/Whitley supports PECI 4.0, please make sure the PECI driver
supports 4.0
- Telemetry Watcher Config commands may not yet be released and can be found in
the repository containing the peci patches. The repository location is
[meta-restricted](https://github.com/Intel-BMC/meta-restricted/tree/master/recipes-core/libpeci/libpeci).
This may require contacting Intel for access to the repository.

### safec fail
- This is the lib which provides bounds checking to make existing standard C
library safer.
- Use devtool to compile safec, make sure source in bb file.
- Confirm source version of safec
- Remove the perl dependency

### SECURITY FLAGS
- If your compiler supports it we recommend to add to the cmake file.
    ```cmake
    set(CMAKE_CXX_FLAGS "-fstack-protector-strong")
    set(CMAKE_C_FLAGS "-fstack-protector-strong")
    ```