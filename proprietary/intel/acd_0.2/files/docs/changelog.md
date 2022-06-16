# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).


## [2.0] - 03/28/2022

### Bug Fix

- Fixed bug related to fail cases/reporting of CPUID, CoreMasks, CHACounts.
- Fixed Memory leak for BAFI Summary / Triage Sections 

### New

- Added OptanePMem auto discovery feature
- Added OptanePMem Smart-Health-Info feature
- Added Global Maximum Time
- Added cha_mask to METADATA section
- Added _version for crashlog section
- Added timeout information when timeout occurs.
- acd_verify: Add Uncore Chop to Summary
- acd_verify: Add comments for Uncore Gen4 DMI Regs on expected 0x90/0x93 

### Changed

- Complete changes for “PECI Engine” 
	- Update input files syntax
	- Order sections in the right sequential order
	- Remove old syntax from input files.
	- Updates to Logger function. 
	- Remove Unused Code due to transition to PECI Engine.
	- Add looping/parsing of input file “Sections”
	- Change “Sections” to be a structure of arrays.
	- Additions to PECIHeaders key in the input file.
	- Add enable / disable for Validation functions.
- Register changes/additions for crashdump_input_spr.json
- Register changes/additions for crashdump_input_sprhbm.json
- Remove crash_data level from all input files.
- Changed crashdump_ver to BMC_EGS_2.0
- Update Unit tests.
- Updates for build instructions.


## [0.7] - 12/02/2021

### Bug Fix

- Fixed bug in Core Crashdump Version String Decode which was causing a
mismatch on version comparison check.
- Fixed bug when choosing unformatted printing though a build define and while
using BAFI, the output was incorrectly still saved as formatted.
- Added correct enumerated bus translations for MMIO using buses 8,9,10,11

### New

- Added dumping of core Crashlog data.
- Added WrEndPointConfigPciLocal command and Unocre registers SAD2TAD Section.
- Added Crashlog exclude list to give an option of excluding AgentIDs
- acd_verify enhancements for file checking.
- Added SPRHBM support, including specific MCA banks and separate input file.
- Added PLL section to the PM_Info record.
- Added big_core MaxCollectionCores input file parameter as a performance
option.
- Added "UseRegEnable" to the input file for bigcore, this feature allows for
selecting which specific bigcore registers to save to the output file.
- NVD additions, identifyDIMM feature, errorlog.
- Added New Uncore registers, ICX, SPR, SPRHBM, and Telemetry.

### Changed

- Changed "PECI Engine" Logging function.
- Conversion of multiple records to "PECI Engine" style input file syntax;
PM_Info, address_map(ICX), crashlog, big_core, metadata, TOR.
- Unit test changed for new functionality.
- Changed crashdump_ver to BMC_EGS_0.7

## [0.6] - 09/20/2021

### New

- Add "useSections" key to input file to allow user to run uncore test flow
  using "Sections/Uncore*" or use old "uncore" section from the input file without
  recompiling the code.
- Add support for PCH crashlog extraction flow.
- Add optional BAFI summary output.
- Use optane-memory repo for NVD feature.

## [0.5] - 06/30/2021

### Bug Fix
- Input file reading errors were not reported and crashdump output file did not
have any data suggesting the read error happened.

### New

- Performance enhancements for Uncore section were added. This controls via
`_max_time_sec` flag for PCI (30 seconds), MMIO (30 seconds) and RDIAMSR (30
seconds) records. If ACD takes more than the time mentioned by `_max_time_sec`
for a particular record, it will abort further collection of those record and
move ahead to other records. If such case is hit, `_<section_name>_aborted` key
will be inserted in the output file.
- Performance enhancement for big core section were added. This controls via
`_max_collection_sec` flag (default 15 seconds). If ACD takes more time than
mentioned by `_max_collection_sec`, it will abort further collection of big core
crash data and move ahead to other records. If such a case is hit,
`_big_core_aborted` key will be inserted in output file.
- Enable optional NVD feature to collect CSRs.
- Enable optional triage feature.
- Extended and added new Unit tests for various sections of crashdump.
- SPR: Added acd_verify for processing the output file into a report.

### Changed
- Changes to uncore MCA hierarchy  in  the input file.
- In uncore MCA add skip bank when there is an error on that bank, will print `N/A`.

## [0.4] - 03/31/2021

### Bug Fix

- Fix BIOS_ID Dbus path issue.

### New

- Update Version label to 0.4
- Add register counts to output file.
- Add additional registers to MetaData.CPU section(s). ierrloggingreg, firstierrtsc, firstmcerrtsc and mcerrloggingreg.
- Implement PECI Driver Selection option in Input file.
- Many Unit Test enhancements.
- SPR: Implement first revision of PM_Info section.
- SPR: Support in Big_core input file for SPR B0 specific change.


### Changed

- Modify big_core wait algorithm to wait for the first core to be available for big_core data for the IERR case only.
- Change Ordering on how Records are collected, new method is to collect by each record type accross all CPU’s then move to the next Record Type.
- Remove "vcode_ver" from MetaData section due to being N/A.
- Updates to input file for Uncore Register section.
- ICX: Modify PM_Info Register names in input files. Non-Functional change.
- SPR: Remove Address_Map record from SPR. Only, Registers are now in Uncore section.
- SPR: Changes to MCA registers in Input file.

## [0.3] - 12/15/2020

### Bug Fix

### New

- Support for crashdump records MetaData, MCA, TOR, Uncore, BigCore. (for SPR, the address_map record will now be included in the Uncore register Record.) PM_Info section will be supported in the Beta release.

### Changed

