# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

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
