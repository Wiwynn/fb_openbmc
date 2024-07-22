# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [bhs-0.9] - 05/15/2024

### New
- Enable GZip compression to output files by default.
- TOR CHA output structure has been collapsed into a string of delimited values.
- Add available-memory check for Uncore PCI/MMIO Sections, if amount of available memory does not meet threshold, skip section. (to revert: edit crashdump_input_<proc>.json files, remove all keys  "MemRequiredMB"  )
- Add support to delete Input file "Section" from dynamic memory after processing.
- Add CHA Phy2log Translation for GNR, SRF, GNRD to retrieve physical to logical CHA IDs.
- Add Crashlog's MaxTimeSec in input files.
- Add new BigCore Core Crashdump  version number  0x4035002 to input files for GNR/GNRD for proper decode of BigCore data.
- Add post-processing utility: crashdump-split.

### Changed
- Move GNR/SRF telemetry uncore output under SOC.
- Reduce GNR BigCore size by setting "UseRegEnable" : True. With this change only the MCA Regs are captured in the BigCore section.  (to revert: edit the crashdump_input_gnr.json file to set  "UseRegEnable" to False to dump all registers as appropriate)   
- Disable BAFI for 2+ CPU platforms.
- Update uncore regs to SS v1.2
- Remove SPR, SPRHBM and EMR input files.
- Change SRF maxCollectionCores to 64. (practical max size)
- Update 0x4xx MCCHAN MCA Registers to match EDS. 
- acd_verify: various changes.

### Bug Fix
- Fix SRF skip core issue. In cases where the AtomCore data is Being collected but a read failure is encountered, prior code was skipping all the remaining cores in the module. The change allows each core in the module a chance to be read.
- Fix BigCore timeout management error. Prior BigCore collection code was not checking often enough for Max Timeout limits, leading to longer than desired delays during certain failure scenarios. Code changed to check for max time reached at more appropriate point in the flow.

## [bhs-0.8] - 03/06/2024

### New
- Enable crashdump to run as standalone executable in non-openbmc environment
with three external libraries dependency (libpeci, cjson and safec) and
add reference code "engine/main.c".
- Add Phy2log translation feature for GNR, SRF, GNRD to retrieve physical to
logical Core IDs of the compute dies.

### Changed
- Reduce the use of "Wno" compiler flags in CMakeLists.txt.
- Skip MMIO reg read when enum bus read failed/disabled.
- Remove crashlog S3M IBL agent. (It has been merged to main S3M agent)
- Increase max console lines to 500.
- Change input file key from MaxTimeInSec to MaxCpuTimeInSec.
- Update non-safe-c functions to safe-c functions.

### Bug Fix
- Fix enum bus for GNRD. 
- Fix isPeciAvailable error handling.
- Fix format specifier to avoid dbus crashes.

## [bhs-0.7] - 12/5/2023

### New
 - Limit the execution of Crashdump Discovery to valid c-die.
 - Update GNR, GNRD and SRF uncore regs to v1.1.
 - Update MaxTimeInSec to 600s.(reduced from 1200s).
 - Update SRF AtomCore decode for proper checksum byte mapping.
 - Update crashdump output filename to be lowercase.
 - Update compiler flag warnings for .c files.
 - Update MCA merged banks for proper decode.
 - Add GNRD support for BigCore and NAC Crashlog.

### Changed
 - Remove SPR/SPRHBM/EMR support.
 - Convert Ping to RdPkgConfig to support PECI over MCTP.
 - Change %llu format specifier to PRIx64 macro for ARM compilers.
 - Refactor _version key to generate at JSON path creation.
 - acd_verify: Place all _time fields into the output table.
 - acd_verify: Count all the "N/A" values in the output file.
 - acd_verify: Remove duplicates with schema in acd_verify's warning message.
 - acd_verify: Update N/A count accuracy in PECI fail table.

### Bug Fix
 - Add MaxTimeSec feature to TOR section.
 - Fix updates on pointer usage warnings indicated by Coverity Scan.
 - Fix skipOnFail management for LoopOnModule to ensure each module gets at least one read. 
 - Correct the IO die id for b2cxl instances 2-9.
 - acd_verify: Fix several new discrepancies found in GNR/SRF templates.
 - acd_verify: Fine-tune the template design approach: now both GNR and SRF share the same templates.

## [bhs-0.6] - 8/22/2023

### New
 - Add SOC level in GNR/SRF input files, including registers from TOR, MCA, and Uncore.
 - Add merged MC Banks in GNR/SRF input files.
 - Add MSRs for core SMI error source in the "RdIAMSR" section.
 - acd_verify: Add EMR as valid product.
 - acd_verify : Add UPI banks enabled check.
 - acd_verify: Add support of newest output file structure.

### Changed
 - Change NAC Crashlog control into input file.
 - Move MCA MSE, MCHN, and B2CMI registers from computeX into soc.
 - Correct TOR access to instances for multi-die SKUs.
 - Change BigCore/atomcore pulling for optimization.
 - Enable Crashlog/UncoreMCA only option via trigger modifier.
 - Update libpeci version required.

### Bug Fix
 - Fix Memory leak within BAFI Summary / Triage Sections.
 - Fix GNR/SRF number UPIs Banks being logged.

## [bhs-0.5] - 6/27/2023

### New

- Add S3M crashlog agents to GNR/SRF input files.
- Add SRF atom core support.
- Add journal feature to crashdump output file and improve logging messages readability.
- Add cjson_version keyword to RdGlobalVars.
- Add lower and upper bounds checks to "Repeat" key.
- Add EMR support.
- Add early_ucode_patch_ver and platform_id read at startup.
- Enable PMT crashlog support.
- Enable BAFI GNR support.

### Change

- Update GNR and SRF uncore regs to v0.9.
- Change pmt::transport::Status to pmt::Status.
- Update MCA_Core section output to module/core hierarchy.
- _version update and WaitOnMcerr updates to input file.
- Update GNR/SRF input file MCA status reg order.
- RdPkgConfig changes to _dom version.
- Update Metadata MCA Registers.
- Update SRF MCA_CORE section to module/core hierarchy.
- Add sdbus signals: CrashdumpLogDeleted, CrashdumpDeleteAll, and CrashdumpComplete.
- Remove ICX remanent code.
- Clang-format-15 clean up.
- Remove crashlog rearm code.

### Bug Fix

- Fix incorrect coreMask1 offset.
- Fix isCpuCrashlogEnabled function hard coded client address issue.
- Fix _cpuid_source, _core_mask_source, and _cha_count_source events incorrect issue.
- BHS schema check bug fix.
- Fix memory leak issue.

## [bhs-0.45] - 4/17/2023

### Bug Fix

- GNR/SRF: For incorrect MCA LLC Label, every register used "llc1"
instead of the proper number sequence. "llc0", "llc1", "llc2".
- GNR/SRF: Fix for reporting the proper DBUS signal on crashdump file creation.

### Changed
 
- Print out crashdump version on application start.
- Complete the code to collect all crashlog agents on both io/compute dies.
- Implement “DieMaskInfo” structure for easier handling of DieMask data.
- Finish code unique handling of SRF flow though the code.
- Upon crashdump startup wait for CPU’s to be powered up before proceeding.
- Set default model to GNR to be used in cases where read of CPUID fails.
- Only override to MAX DIE MASK on a real EVENT, (not on Startup)

## [bhs-0.4] - 3/17/2023

### Changed

- Alpha release
- Hierarchical Structure: 100% completed
- MetaData: 90% completed
- MCA: 100% completed
- big_core: 100% completed
- crashlog: 50% completed
- TOR: 100% completed
- Uncore Regs: 30% completed
- Fixed static code analysis tools reported issues

## [bhs-0.3] - 12/14/2022

### New

- Initial version, Pre-Alpha
- Hierarchical Structure: 90% completed 
- MetaData: 80% completed 
- MCA: 25% completed
- big_core: Untested work in progress
- crashlog: 30% completed 
- TOR: 75% completed
- Uncore Regs: 30% completed

### Changed

- Fixed static code analysis tools reported issues

### Notes

SPR: Supports the same functionality found in Eaglestream Crashdump for the SPR processor.

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

