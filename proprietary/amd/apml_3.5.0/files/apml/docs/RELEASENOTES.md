# Advanced Platform Management Link (APML) Library

Thank you for using AMD APML Library (formerly known as EPYC™ System Management Interface (E-SMI) Out-of-band Library).

# Changes Notes

## Highlights of Minor Release v3.5.0
* Add support for family 1Ah and model 10h - 1Fh
* Add new API to add support for
   - DIMM spd register data (0x70)
   - DIMM spd serial number
* Formatting in tool
 
## Highlights of Minor Release v3.3.0
* Add support for the new APML features on family 19h amd model 90h - 9Fh
   - New SBRMI mailbox messages including
        - GetCurrentXGMIPState (0x85)
        - MaxOperatingTemperature (0xA2)
        - GetSlowdownTemperature (0xA3)
        - HBMDeviceInformation (0xB7)
        - GetPCIeStats (0XBA)
   - Update in API definition 0xA4 to ret controller, driver status
* Add support for the new APML features on family 1Ah amd model 00h - 0Fh
   - Support for RTC timer (0x21)
   - Upadte in API definition of mailbox message 0x61(BMC_RAS_RUNTIME_ERR_VALIDITY_CHECK)
* Add Platform supported property to Mailbox APIs
* Change visibility of mailbox messages from NDA to Public,
  this will update the header file of the APIs
  - 0x16 (Read CCLK Frequency Limit)
  - 0x17 (Read Socket C0 Residency)
  - 0x1F (Read PPIN fuse)
  - 0x5B (BMC_RAS_DBG_LOG_VALIDITY_CHECK)
  - 0x5C (BMC_RAS_DBG_LOG_DUMP)
* Tool changes:
  - Update help section to return messages supported as per platform
  - New tool options to read/write RMI & TSI registers
  - Fix the TSI regsisters summary, in case the RMI module is not installed
  - Formatting in tool
* Updated EULA doc

## Highlights of Major Release v3.0.0
* Add support for the new APML features on family 1Ah amd model 00h - 0Fh
   - New SBRMI mailbox messages including, [61h to 67h]
	- BMC Runtime error validity support (61h - 65h)
	- Post Package Repair Support (66h, 67h)
	  - NOTE: Definition may change as E2E validation pending
   - Library will now look for the apml dev nodes with sbrmi# and sbrmi-<client-addr>
     These changes will help multiple sockets based on client address.
   - Update alertmask and alert status to support all the threads
   - Formatting fix in tool
   - update tool

## Highlights of Minor Release v2.8.2
* Update header file path in esmi_oob/ header files
* Cosmetic changes in tool print for SBTSI summary

## Highlights of Major Release v2.8.0
* Add support for the new APML features on family 19h amd model 90h - 9Fh
   - New SBRMI mailbox messages including, [80h and later]
	- GPU Telemetry
	- HBM Telemetry
	- BIST result on basis of DIE-IDs
	- Number of sockets in system/node
   - New SBTSI Registers
	- HBM cofiguration
	- HBM Temperature High and Low Threshold
   - APML tool, new improved TSI summary
   - Using revision instead of cpuid for messages 8h and 9h

## Highlights of Minor Release v2.6.0
* Add support for the following features on family 19h and model 10 - 1Fh
   - BMC RAS DELAY RESET ON SYNCFLOOD OVERRIDE [0x6Ah]
   - Read Post Code [0x20h]
   - Clear SBRMI RAS status register [0x4c]

## Highlights of Minor Release v2.4
* Add support for the following features on family 19h and model 10h - 1Fh
    - Software workaround for the Erratum:1444
    - Support for warm reset after syncflood in mailbox
    - Validate thread as input by user for CPUID and MCA MSR reg read

## Highlights of Minor Release v2.2
* Add support for the following features on family 19h and model 10h - 1Fh
    - Read microcode revision
* Following NDA only features
    - Read CCLK Frequency Limit
    - Read socket C0 residency
    - Read PPIN fuse
    - BMC RAS DF Error validity check
    - BMC RAS DF Error Dump

## Highlights of Minor Release v2.1

* Update library/tool based on APML spec from PPR for AMD Family 19h Model 11h B1
* Introduced a module in apml_tool to provide cpuid information
* Bug fix (display temperature)

## Highlights of Major Release v2.0
* Rename ESMI_OOB library to APML Library
* Rework the library to use APML modules (apml_sbrmi and apml_sbtsi)
    - This helps us acheive better locking across the protocols
    - APML modules takes care of the probing the APML devices and provding interfaces
* Add features supported on Family 19h Model 10h-1Fh
    - Support APML over I2C and APML over I3C
    - Handle thread count >127 per socket
    - CPUID and MSR read over I3C

## Highlights of Minor Release v1.1

* Single command to create Doxygen based PDF document
* Improved the esmi tool

## Highlights of major release v1.0
APIs to monitor and control the following features:
* Power
    * Get current power consumed
    * Get and set cap/limit
    * Get max power cap/limit
* Performance
    * Get/Set boostlimit
    * Get DDR bandwidth
    * Set DRAM throttle
* Temeprature
    * Get CPU temperature
    * Set High/Low temperature threshold
    * Set Temp offset
    * Set alert threshold sample & alert config
    * Set readorder

# Supported Processors
* Family 17h model 31h
* Family 19h model 0h~0Fh & 10h~1Fh

# Supported BMCs
Compile this library for any BMC running Linux.
Use the [APML Library/tool Support](https://github.com/amd/apml_library/issues) to provide your feedback.

# Supported Operating Systems
APML Library is tested on OpenBMC for Aspeed AST2600 and RPI-3b based BMCs with the following "System requirements"

# System Requirements
## Hardware requirements
BMC with I2C/I3C controller as master, I2C/I3C master adapter channel (SCL and SDA lines) connected to the "Supported Processors"

## Software requirements

To build the APML library, the following components are required.

***Note:***
*The listed software versions are being used in development,earlier versions are not
guaranteed to work.*

### Compilation requirements
* CMake (v3.5.0)
* APML library upto v1.1 depends on libi2c-dev
* Doxygen (1.8.13)
* LaTeX (pdfTeX 3.14159265-2.6-1.40.18)

# Dependencies
APML library upto v1.1 depends on libi2c-dev.
The later versions of library depends on the [apml_modules](https://github.com/amd/apml_modules)

# Support
To provide your feedback, bug reports, support and feature requests,
refer to [APML Library Support](https://github.com/amd/apml_library/issues).
