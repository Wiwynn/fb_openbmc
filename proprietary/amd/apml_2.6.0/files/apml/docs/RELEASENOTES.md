# Advanced Platform Management Link (APML) Library

Thank you for using AMD APML Library (formerly known as EPYC™ System Management Interface (E-SMI) Out-of-band Library).

# Changes Notes

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
