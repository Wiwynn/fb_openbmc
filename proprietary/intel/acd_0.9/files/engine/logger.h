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

#ifndef LOGGER_H
#define LOGGER_H
#include "cmdprocessor.h"
#include "crashdump.h"
#include "logger_internal.h"
#include "utils.h"

void LogDumpDelimitedValues(cJSON* outputNode, LoggerStruct* loggerStruct,
                            uint32_t expectedElements);
void LogRegister(CmdInOut* cmdInOut, cJSON* root, LoggerStruct* loggerStruct);
void LogConfig(LoggerStruct* loggerStruct, LogMode mode);
void LogDelimitedValue(LoggerStruct* loggerStruct, char* key, char* singleStr);
acdStatus getPath(cJSON* OutputJSON, LoggerStruct* loggerStruct);
acdStatus ParsePath(LoggerStruct* loggerStruct);
acdStatus ParseNameSection(CmdInOut* cmdInOut, LoggerStruct* loggerStruct);
void logVersion(CmdInOut* cmdInOut, cJSON* root, LoggerStruct* loggerStruct);
void GenerateVersion(cJSON* Section, int* Version);
void logSectionRunTime(cJSON* parent, struct timespec* start, char* key);
void logRecordDisabled(CmdInOut* cmdInOut, cJSON* root,
                       LoggerStruct* loggerStruct);
acdStatus GenerateJsonPath(CmdInOut* cmdInOut, cJSON* root,
                           LoggerStruct* loggerStruct, bool useRootPath);
void LogProcessValue(char* registerName, CmdInOut* cmdInOut,
                     LoggerStruct* loggerStruct, cJSON* parent);
void logSectionFailCount(cJSON* parent, int failCount, char* sectionName);
void logStringAtPath(cJSON* root, cJSON* section, char* name, char* value);
#define LOGGER_JSON_STRING_LEN 64
#define LOGGER_DATA_64bits "0x%" PRIx64 ""
#define LOGGER_DATA_64_bits_CC_RC "0x%" PRIx64 ",CC:0x%x,RC:0x%x"
#define LOGGER_FIXED_DATA_CC_RC "0x0,CC:0x%x,RC:0x%x"
#define LOGGER_INFO_TIMEOUT "%ds"
#define LOGGER_GLOBAL_TIMEOUT "_cpu%d_%s_global_timeout:"
#define LOGGER_SECTION_TIMEOUT "_cpu%d_%s_section_timeout:"
#define LOGGER_VERSION_STRING "_version"
#define LOGGER_NA "N/A"
#define RECORD_ENABLE "_record_enable"
#define NAME_POSITION 0
#define SECTION_POSITION 1
#define INVALID_VALUE 255

#define LOGGER_CPU "cpu%d"
#define LOGGER_MODULE "module%d"
#define LOGGER_CORE "core%d"
#define LOGGER_THREAD "thread%d"
#define LOGGER_CBO "cbo%d"
#define LOGGER_CBO_UC "CBO%d"
#define LOGGER_LLC "llc%d"
#define LOGGER_LLC_UC "LLC%d"
#define LOGGER_CHA "cha%d"
#define LOGGER_REPEAT "%d"
#define LOGGER_COMPUTE "compute%d"
#define LOGGER_IO "io%d"
#define LOGGER_DOMAIN "%domain"

#endif // LOGGER_H
