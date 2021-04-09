# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

## [1.46] - 2021-04-01
### Added

### Changed
- **[SPR]** first error bank numeration

### Removed

### Fixed
- **[ICX]** memory error decoding in triage information

## [1.45] - 2021-03-17
### Added
- **[ICX]** correctable and uncorrectable memory error decoding
- **[ICX]** silkscreen_map (if not provided in the argument) by default taken in priority from:
   - /var/bafi/default_silkscreen_map.json (LINUX) or
   C:\temp\bafi\default_silkscreen_map.json (WINDOWS)
   - default_silkscreen_map.json located in bafi binary directory
- **[General]** memory errors, package thermal status in triage information
- **[General]** silkscreen map file parameter in bafi_excel_summary.py

### Changed
- **[General]** triage outputs every socket information

### Removed

### Fixed

## [1.40] - 2021-02-01 (Initial Eaglestream release)
### Added
- **[SPR]** Eaglestream crashdump rev 0.3 support
- **[ICX-D]** ICX-D crashdump support

### Changed

### Removed

### Fixed

## [1.15] - 2021-01-26
### Added
- **[General]** devices_log_converter.py which enables to parse pci_devices or
lspci format
- **[General]** device map (if not provied in the argument) by default taken in priority from:
   - /var/bafi/default_device_map.json (LINUX) or
   C:\temp\bafi\default_device_map.json (WINDOWS)
   - default_device_map.json located in bafi binary directory
- **[General]** CHA misc decoding

### Changed

### Removed

### Fixed
- **[SKX]** FirstIERR and FirstMCERR not parsing correctly

## [1.14] - 2020-12-31
### Added

### Changed
- **[General]** changes MCA_ERR_SRC_LOG register from uncore to metadata

### Removed

### Fixed
- **[General]** missing MCACOD decoded info in summary for DCU and PCU banks
- **[CLX]** missing MCA section details
- **[CLX]** missing FirstIERR and FirstMCERR in Errors_Per_Socket

## [1.13] - 2020-12-21
### Added
- **[General]** specifying output path for cscript_map_converter.py using -o flag
- **[SKX]** Parsing CHA registers for SKX, CLX and CPX

### Changed

### Removed

### Fixed

## [1.1] - 2020-10-13
### Added
- **[General]** printing PCIe AER information for every socket in error triage
- **[General]** bafi_excel_summary.py script to create triage report from multiple files
in csv format
- **[General]** printing type of support for each CPU using -v flag (release or engineering)

### Changed

### Removed

### Fixed
- -t and -d flags combination

## [1.05] - 2020-09-28
### Added

### Changed

### Removed

### Fixed
- TORdump address map to crashdump system address

## [1.0] - 2020-09-18
### Added
- -d flag which enables to analyze multiple crashdumps contained in a current
folder
- -t flag which prints out error triage information
- multiple flags usage

### Changed
- binary name (crashdump_analyzer.exe -> bafi.exe)

### Removed

### Fixed

## [0.96] - 2020-08-13
### Added
- Linux /proc/iomem text format support in cscript_map_converter.py
- -r flag which enables to print output in one line

### Changed
- Json keys don't contain dots anymore, changed to underscores

### Removed

### Fixed
- Fixed bug to support Purley ACD rev0.6 crashdump.json

## [0.95] - 2020-08-07
### Added
- TSC comparison between all sockets
- Version information

### Changed

### Removed

### Fixed

## [0.9] - 2020-07-31
### Added
- Purley crashdump CPU support
- I/O Errors section in summary if first MCERR is UBOX error

### Changed

### Removed

### Fixed

## [0.6] - 2020-06-19
### Added
- MCA CHA banks address decoding
- bank numbers in FirstMCERR summary for ICX
- PROCHOT_LOG and PROCHOT_STATUS in summary
- file based memory map provided as argument
- memory map (if not provied in argument) by default taken in priority:
   - /var/bafi/default_memory_map.json (LINUX) or
   C:\temp\bafi\default_memory_map.json (WINDOWS)
   - default_memory_map.json located in crashdump_analyzer binary directory
   - static default from source code

### Changed
- README text details

### Removed
- multiple default BDF entries in table

### Fixed
- Windows compilation issues

## [0.55] - 2020-04-10
### Added

### Changed
- README text details

### Removed

### Fixed
- code styling
- "Three Strike Timout Error" to "TOR Timeout Error" in report ErrorType
- copyright headers update

## [0.54] - 2020-03-26
### Added
- decoding ICX bank6 MISC register decoding (PCIe bus, device, funtion, segment)
- TSC comparison between sockets for cross platorm first IERR and MCERR
distinction

### Changed

### Removed

### Fixed

## [0.53] - 2020-03-23
### Added

### Changed
- MCA decoding rules according to EDS 0.8

### Removed

### Fixed
- MCA decoding with missing fields inside MCx node
- TOR decoding with index instead of subindex in TOR entry

## [0.52] - 2020-03-11
### Added

### Changed
- “Please refer to PCIe/IO MMIO map” to “Please refer to system address map”
- BDF from "0x0:0x1C:0x0" to "Bus: 0x0, Device: 0x1c, Function: 0x0"

### Removed

### Fixed

## [0.51] - 2020-03-04
### Added
- _total_time field to JSON output for script work time measurement
- decoding crashdump JSON file downloaded directly from Redfish interface
- created change tracking file CHANGELOG.md

### Changed

### Removed

### Fixed
- decoding crashdumps with missing "TOR" section

## [0.50] - 2020-01-07 (Initial release)
### Added
- decoding TOR registers
- decoding MCE registers
- decoding correctable and uncorrectable PCIe AER registers
- decoding TSC registers
- summary section with most important infromation

### Changed

### Removed

### Fixed

