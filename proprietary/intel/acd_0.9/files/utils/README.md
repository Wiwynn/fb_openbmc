# Crashdump Split

## How to Build
### Install Dependencies

The [CMakeLists.txt](./CMakeLists.txt) located in current directory is used to build the project. You can use the option `-DARM_BUILD=ON` to enable ARM build (for running BMC), by default it is x86 build. The following dependencies are required to install to `BASE_PATH`: `~/src` in Linux to build the project:

```bash
mkdir ~/src && cd ~/src

#build and install for ARM deps
sudo apt install gcc-arm-linux-gnueabihgit clone
git clone https://github.com/DaveGamble/cJSON.git cjson_src_arm && cd cjson_src_arm && git checkout v1.7.15
cmake . -DENABLE_CJSON_UTILS=On -DENABLE_CJSON_TEST=Off -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc -DBUILD_SHARED_LIBS=Off -DCMAKE_INSTALL_PREFIX=~/src/cjson_arm
make -j && make install && cd ~/src

git clone https://github.com/rurban/safeclib.git safeclib_src_arm && cd safeclib_src_arm && git checkout 5da8464c092b2f234442824d3dbb49343e58bb16
cmake . -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc -DBUILD_SHARED_LIBS=Off -DCMAKE_INSTALL_PREFIX=~/src/safeclib_arm
make -j && make isntall && cd ~/src

#build and install for x86 deps
git clone https://github.com/DaveGamble/cJSON.git cjson_src_x86 && cd cjson_src_x86 && git checkout v1.7.15
cmake . -DENABLE_CJSON_UTILS=On -DENABLE_CJSON_TEST=Off -DCMAKE_INSTALL_PREFIX=~/src/cjson_x86
make -j && make install && cd ~/src

git clone https://github.com/rurban/safeclib.git safeclib_src_x86 && cd safeclib_src_x86 && git checkout 5da8464c092b2f234442824d3dbb49343e58bb16
cmake . -DCMAKE_SYSTEM_NAME=Linux -DBUILD_SHARED_LIBS=Off -DCMAKE_INSTALL_PREFIX=~/src/safeclib_x86
make -j && make install && cd ~/src

```

### Build Options 
The following options are available for building the project across platforms:
```bash 
mkdir build && cd build
# For ARM 
cmake .. -DARM_BUILD=ON && build
# For x86 
cmake .. && build
```

## Usage
```bash
❯ ./crashdump-split
Filename is required for split mode.
Name: crashdump-split

Description: Split (or Merge) crashdump files

Usage Options:
  crashdump-split [--help] [--merge] [--numfiles numfiles] [--numcpus expectnumcpus] [filename]

  --help                    Shows this information, also printed when no parameters are used.
  --merge basename          Run in merge mode using basename. Merge files with basename*.json
  --numfiles numfiles       Split mode only: split the file into numfiles with expectnumcpus/numfiles CPUs per file (Default: 2 files).
  --numcpus expectnumcpus   Split mode only: Expect that there are expectnumcpus in the input file. (Default: 2 cpus)
  --remove                  Remove input files after processing.
  filename                  Split mode: split this json filename. Merge mode: Ignored

Examples:
  Split mode:   crashdump-split --numfiles 4 --numcpus 8 crashdump_8cpus_041724-1120.json
  Merge mode:   crashdump-split --merge crashdump_8cpus_041724-1120
```

## Process Flow
### Split Mode
```bash
crashdump-split --numfiles 4 --numcpus 8 crashdump_8cpus_041724-1120.json
```
1. Parameter check: 
    ```C
    numfiles > 0 && numcpus > 0 && < max_cpu_count 
    //number of files and cpus must be greater than 0 and numcpus must less than max_cpu_count(4096)
    numcpus > numfiles 
    //number of cpus must be greater than or equal to number of files
    numcpus % numfiles == 0 
    //number of cpus must be divisible by number of files
    ```
2. Trim basename from parameter: `crashdump_8cpus_041724-1120.json` => `crashdump_8cpus_041724-1120`.
4. Split the "journal" section to the 1st splitted file **ONLY**.
5. In the "crash_data" section:
    1. Take the METADATA as a common METADATA for all the split files's METADATA section (no split).
    2. Split the "PROCESSORS" in "crash_data" into numfiles files with numcpus/numfiles CPUs per file.
    3. Save the above sections to file pattern: <basename>_<X>of<Y>.json
6. The rest sections except `crash_data` and `journal` will be ignored.

### Merge Mode
```bash
crashdump-split --merge crashdump_8cpus_041724-1120
```
1. Take the `crashdump_8cpus_041724-1120` as the basename and search the pattern: `crashdump_8cpus_041724-1120_<X>of<Y>.json` in the same folder.
2. If there are files found, start sanity check:
    1. All the <X> in the files must be unique and continuous from 1 to Y.
    2. All the <Y> must be the same and equal to the total number of files.
    3. The total files to be merged must less than: `#define MAX_FILES_TO_MERGE 1024
` 
3. Check and Merge:
    1. The `METADATA` and `journal` (if there is) in the 1st input file: <basename>_1ofY.json will be used as the common `METADATA` section for the output (merged) file.
    2. The `cpu*` sections will be merged into the merge file. Before merging, the ppin is verified to match the ppin in the common `METADATA`, and `cpu*` as the key in the `PROCESSOR` section will be checked to ensure uniqueness.
4. The merged file will be saved to: <basename>_merged.json.