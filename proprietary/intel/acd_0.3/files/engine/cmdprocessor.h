/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2021 Intel Corporation.
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly stated in the
 * License.
 *
 ******************************************************************************/

#ifndef CRASHDUMP_CMDPROCESSOR_H
#define CRASHDUMP_CMDPROCESSOR_H

#include <search.h>

#include "crashdump.h"
#include "logger_internal.h"
#include "validator.h"

#define CD_NUM_OF_PECI_CMDS 31
#define CMD_ERROR_VALUE "PECI_RETURN_ERROR"

#define PECI_ADDR_VAR "peci_addr"
#define CPUID_VAR "cpuid"
#define CPUID_SOURCE_VAR "cpuid_source"
#define COREMASK_SOURCE_VAR "coremask_source"
#define CHACOUNT_SOURCE_VAR "chacount_source"
#define COREMASK_VAR "coremask"
#define CHAMASK_VAR "chamask"
#define CHACOUNT_VAR "chacount"
#define CORECOUNT_VAR "corecount"
#define CRASHCORECOUNT_VAR "crashcorecount"
#define CRASHCOREMASK_VAR "crashcoremask"
#define BMC_FW_VER_VAR "bmc_fw_ver"
#define BIOS_ID_VAR "bios_id"
#define ME_FW_VER_VAR "me_fw_ver"
#define _TIMESTAMP_VAR "timestamp"
#define TRIGGER_TYPE_VAR "trigger_type"
#define PLATFORM_NAME_VAR "platform_name"
#define CRASHDUMP_VER_VAR "crashdump_ver"
#define INPUT_FILE_VER_VAR "_input_file_ver"
#define _INPUT_FILE_VAR "_input_file"
#define _RESET_DETECTED_VAR "_reset_detected"
#define DIEMASK_VAR "diemask"

typedef struct
{
    CPUModel cpuModel;
    uint8_t stepping;
} PECICPUID;

typedef struct
{
    cJSON* params;
    cJSON* outputPath;
    cJSON* repeats;
} PECICmdInput;

typedef union
{
    uint64_t u64;
    uint32_t u32[2];
} RegVal;

typedef struct
{
    EPECIStatus ret;
    uint8_t cc;
    RegVal val;
    char* stringVal;  // contains the string that will be printed
    bool printString; // if set it logger will print stringVal.
    uint8_t size;
    PECICPUID cpuID;
} PECICmdOutput;

typedef struct
{
    PECICmdInput in;
    PECICmdOutput out;
    cJSON* paramsTracker;
    cJSON* internalVarsTracker;
    char* internalVarName;
    cJSON* root;
    LoggerStruct* logger;
    RunTimeInfo* runTimeInfo;
    ValidatorParams* validatorParams;
    PlatformInfo* platformInfo;
} CmdInOut;

typedef struct
{
    uint32_t cpubusno_valid;
    uint32_t cpubusno;
    int bitToCheck;
    uint8_t shift;
} PostEnumBus;

typedef struct
{
    uint64_t chaMask0;
    uint64_t chaMask1;
} ChaCount;

acdStatus BuildCmdsTable(ENTRY* entry);
acdStatus Execute(ENTRY* entry, CmdInOut* cmdInOut, CPUInfo* cpuInfo);

#endif // CRASHDUMP_CMDPROCESSOR_H
