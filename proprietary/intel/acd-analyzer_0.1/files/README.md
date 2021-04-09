# BMC Assisted FRU Isolation

## Overview

The entire solution allows to parse crashdump.json into human readable form.

The product consists of series of C++ header files that can be attached to any
application or built as a standalone command line application with following
major functions:

- Table of Requests (TOR) resgiters
- Machine Check Architecture (MCA) registers
- PCIe Advanced Error Reporting (AER) errors
- PCI address mapping

Results are presented in a JSON format.

## Building

This product can be built on any linux based system, as well as part of OpenBMC
Yocto component.

### Building on linux system

#### Prerequisite
Your tool chain shall consists of.
* C++ compiler that supports C++17 features (i.e. G++8)
* Cmake
* Conan dependency package

#### Building

In top level repository issue the following commands:

```
mkdir build
cd build

conan install ..

cmake ..

make
```

This will result in creation of bafi executable in build/bin
directory, errors otherwise.

## Usage

```
Usage:
  bafi [-d][-t][-r][-v][-h][-m memory_map_file][-p device_map_file][-s silkscreen_map_file] crashdump_file

Options:
  -d                    generate BAFI output from all crashdump files in current folder
  -t                    print triage information
  --triage              print triage 
  -r                    print output in one line
  -v                    version information
  --version             version information
  -h                    print help
  --help                print help
  -m              	import memory map from json file
  --memory-map    	import memory map from json file
  -p                 import device map from json file
  --pcie_names       import device map from json file
  -s                 import silkscreen map from json file
  --silkscreen_names import silkscreen map from json file
  memory_map_file 	JSON file containing memory map information
  device_map_file    JSON file containing device map information
  crashdump_file  	JSON file containing Autonomous Crashdump

Instructions:
  Step 1: In Summary TSC section, locate which TSC register has "Occurred first
          between all TSCs" tag.
  Step 2: From the first MCERR or first IERR information in step 1, locate the
          IP and MCE bank number that asserted FirstIERR and FirstMCERR (i.e.
          FirstMCERR=CHA1 bank1	M2MEM 0 bank 12, UPI 0 bank 5).
  Step 3: Correlate above MCE bank number with error information from MCE
          section or find device Bus/Device/Function number in "First.MCERR"
          section if available.
```

## FAQ

### There is no PCIe proper information, so BDF for Type2, and Type3 is returned "Please refer to system address map"
Current implementation have only dummy PCIe device lookup, which exists in
include pcilookup.hpp file. It contains only single entry for BDF in memory
map. Customers, to have full view on FRU Isolation and determine which
component encountered failure, shall use on of the below methods to fill memory
map:

#### Input data statically into BAFI code.
In file include/pcilookup.hpp there is a code like this one:
```
memorymap = {
    {0x00000001, 0x00000002, 0x1, 0x0, 0x0},
    /*
    Examples of other memory map entries
    {0xCB300000, 0xCB3FFFFF, 0x65, 0, 0},
    {0xC6000000, 0xCB2FFFFF, 0x65, 0, 0},
    {0xE1000000, 0xE10FFFFF, 0xb3, 0, 0},
    {0xC000, 0xCFFF, 0xb3, 0, 0}
    */
};
```
Feel free to add another lines, with given format:
```
{lower_memory_addres, upper_limit_memory_address, Bus, Device, Function},
```

#### Create or convert memory map and import to BAFI
Create (or convert according to next question) memory map in below JSON format:
```
{
   "memoryMap": [
      {
         "addressBase": "0x00000000",
         "addressLimit": "0xFFFF0000",
         "bus": "0x98",
         "device": "0x98",
         "function": "0x98"
      },
      {
         "addressBase": "0xFFFF0001",
         "addressLimit": "0xFFFFFFFF",
         "bus": "0x99",
         "device": "0x99",
         "function": "0x99"
      }
   ]
}
```

Next import this map to BAFI. Memory map will be read in below order:
   1. memory map provied as argument (-m, --memory-map)
   2. /var/bafi/default_memory_map.json (LINUX) or
   C:\temp\bafi\default_memory_map.json (WINDOWS)
   3. default_memory_map.json located in bafi binary directory
   4. static default map from source code (pcilookup.hpp)

### I have cscript memory map created using pci.resource_check() command or iomem.txt from Linux /proc/iomem. Can I use it in BAFI?
Yes. You can use tools/cscript_map_converter.py like this:
```
python3 csript_map_converter.py <cscript_memory_map_file_format> or <iomem.txt> [-o output_path]
```
This command will create default_memory_map.json that can be later used in BAFI
using -m (--memory-map) option or by copying this map to proper location (check
previous question).

#### Create or convert PCIe device map and import to BAFI
Create (or convert according to next question) device map in below JSON format:
```
{
   "devicesMap": [
      {
         "description": "Example device A",
         "bus": "0x0",
         "device": "0x0",
         "function": "0x0"
      },
      {
         "description": "Example device B",
         "bus": "0x1",
         "device": "0x0",
         "function": "0x0"
      }
   ]
}
```

Next import this map to BAFI. Device map will be read in below order:
   1. PCIe device map provied as argument (-p, --pcie_names)
   2. /var/bafi/default_device_map.json (LINUX) or
   C:\temp\bafi\default_device_map.json (WINDOWS)
   3. default_device_map.json located in bafi binary directory

### I have cscript device map created using pci.devices() command or lspci command output from Linux. Can I use it in BAFI?
Yes. You can use tools/devices_log_converter.py like this:
```
python3 devices_log_converter.py <cscript_device_map_file_format> or <lspci.txt> [-o output_path]
```
This command will create default_device_map.json that can be later used in BAFI
using -p (--pcie_names) option.

#### Create or convert silkscreen map and import to BAFI
Create silkscreen map in below JSON format:
```
{
  "MemoryLocationPcb": [
    {
      "Socket": 0,
      "IMC": 0,
      "Channel": 0,
      "Slot": 0,
      "SlotName": "J8J1"
    },
    {
      "Socket": 0,
      "IMC": 0,
      "Channel": 0,
      "Slot": 1,
      "SlotName": "J8J2"
    }
  ]
}
```

Next import this map to BAFI. Silkscreen map will be read in below order:
   1. Silkscreen map provied as argument (-s, --silkscreen_names)
   2. /var/bafi/default_silkscreen_map.json (LINUX) or
   C:\temp\bafi\default_silkscreen_map.json (WINDOWS)
   3. default_silkscreen_map.json located in bafi binary directory

### I have multiple crashdump files and I want to create triage report for all of them in Excel format. How can I do it?
You can use tools/bafi_excel_summary.py like this:
```
python3 bafi_excel_summary.py <crashdump_folder>
```
This command by default will create output.csv file with triage report from all
crashdumps located in <crashdump_folder> and it's subfolders. For more
information on usage and optional arguments run script with '-h' option.
Please note that minimum supported Python version is 3.6.

Please note that minimum supported Python version is 3.6.

### I would like to test if I properly integrated BAFI in my solution. How can I do that?
You can test your solution using BAFI compliance tests. Please read
tests\bafi_testing.md for more details.

### I compiled Windows binary but when I start it I receive and error about missing DLL files. How can I fix it?
You need to download latest Microsoft Visual C++ Redistributable package. Please
check following link for more information:
https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads

### Can I share BAFI traige output with users that did not sign NDA with Intel?
Yes. Unlike full output, BAFI triage (-t, --triage) output contains only publicly
available information that can be shared with end user. Full BAFI output requires
NDA agreement between end customer and Intel.
