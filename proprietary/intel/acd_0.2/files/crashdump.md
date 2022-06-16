# Autonomous CrashDump

## Overview

This source code implements crashdump for the Intel Xeon Server platforms. The
crashdump application collects debug data from the processors over the PECI
bus when executed. Some specific system level data may be collected as defined
in the METADATA record type. The intention of the usage model is that the
crashDump application will be called upon the BMC detecting an error event.

## Implementation

### Source Description

This source consists of the main file, crashdump.cpp, utility files, and a set
of files under the directory engine that each are used for parsing the input
file and generate the crashdump output. The record types as follows:

- METADATA
- MCA
- Uncore
- TOR
- PM_Info
- big_core
- crashlog
- NVD


### System Requirements

### Initialization/Start-Up

It is expected that the crashdump application is ready/running and avaliable to
be called after BMC has been booted.

### System Reset Avoidence

System resets and/or power cycling the CPU's in reaction to an error event must
be disabled or delayed until after crashdump collection has completed.

### Data Extraction Dependencies

PECI Access to record data is restricted according to the following table.

| Record      | Gate    |
| ---         | ---     |
| METADATA    | None    |
| Address_map | None    |
| Big_Core    | 3strike |
| MCA         | None    |
| PM_Info     | IERR    |
| TOR         | IERR    |
| Uncore      | None    |
|             |         |

### File Output Format

The output is stored in the JSON output format.

Example: (*shows a subset of the data collected in the METADATA record*)

```json
{
    "crash_data": {
        "METADATA": {
            "crashdump_ver": "BMC_1924",
        "cpu0": {
            "cores_per_cpu": "0xc",
            "cpuid": "0x606a0",
            "peci_id": "0x30",
        },
        "cpu1": {
            "cores_per_cpu": "0xc",
            "cpuid": "0x606a0",
            "peci_id": "0x31",
        }
    }
}
```

## Executing CrashDump

There are two ways to execute CrashDump.

- On-Demand
  - /redfish/v1/Systems/system/LogServices/Crashdump/
    Actions/Oem/Crashdump.OnDemand
- Direct call upon detection of an Error Event, Possible Error events
  - IERR
  - ERR2
  - SMI Timeout

## Extracting Output

### On-Demand CrashDump

For Intel OpenBMC, a redfish command can be used for a requesting and extracting
an immediate snapshot of the CrashDump output. This code both requests the data
to be acquired and for the file to be extracted. This request is handled through
the following command.

- /redfish/v1/Systems/system/LogServices/Crashdump/Actions/Oem/
  Crashdump.OnDemand

## Extracting the Output file(s)

For Intel OpenBMC, The method to extract the autonomously triggered crashdump is
to use the following redfish command:

- /redfish/v1/Systems/system/LogServices/Crashdump/Entries

The result of this command is an array of the available Crashdump entries with
their links. When you request each link, you will get the Crashdump file.

## Schema file

TBD

## Interpreting Output

The decoder/analyzer/summarizer tool is provided in the CScripts.
See CScripts python Script:

- \cscripts\cpu name\toolext\cd_summarizer.py

## Security/Privacy

The big_core record type contains architectural register information from the
core and the information in these register could be private.

Examples of big_core data collected.

- Last Branch Records
- Last Execption Records
- General Registers: RAX,RBX,RCX..., R8, R9, R10...
- Instruction Pointer: LIP
- Selectors: CS,DS,ES,FS,GS,SS
- MTRR Registers
- Control registers: CR0,CR2,CR3,CR4
- Other Misc. control registers

