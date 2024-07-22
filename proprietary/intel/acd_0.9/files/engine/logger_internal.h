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

#ifndef LOGGER_INTERNAL_H
#define LOGGER_INTERNAL_H

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdio.h>

#define LOGGER_JSON_PATH_STRING_LEN 128
#define MAX_NUM_PATH_LEVELS 12
#define STRINGIFY_BUFFER_LEN 64000
#define STRINGIFY_KEY_LEN 64
#define STRINGIFY_FORMAT "%s%s"
#define STRINGIFY_DELIMITER ";"
#define STRINGIFY_FORMAT_KEYVALUE "%s:%s"
#define STRINGIFY_FORMAT_KEYNAME "%s_all"
#define STRINGIFY_KEY_ENCODING "_encoding"
#define STRINGIFY_KEY_DELIMITER "_delimiter"
#define STRINGIFY_KEY_ELEMENTS "_elements"

typedef struct
{
    char* pathStringToken;
    char pathString[LOGGER_JSON_PATH_STRING_LEN];    // Tokenized and gets
                                                     // destructed
    char pathStringRaw[LOGGER_JSON_PATH_STRING_LEN]; // Stays clean
    char* pathLevelToken[MAX_NUM_PATH_LEVELS];
    int numberOfTokens;
} PathParsing;

typedef struct
{
    char* registerName;
    char* sectionName;
    bool extraLevel;
    bool logRegister;
    uint8_t size;
    bool sizeFromOutput;
    int rootAtLevel;
    cJSON* jsonOutput;
    bool zeroPaddedPrint;
} NameProcessing;

typedef struct
{
    uint8_t cpu;
    uint8_t domainID;
    uint8_t compute;
    uint8_t io;
    uint8_t module;
    uint8_t core;
    uint8_t logicalCore;
    uint8_t thread;
    uint8_t cha;
    uint8_t chaOffset;
    uint8_t repeats;
    bool skipFlag;
    bool skipCrashCores;
    bool skipOnFailFromInputFile;
    int version;
    char* currentSectionName;
    bool isDomainCompute;
} ContextLogger;

typedef enum
{
    KEY_VALUE = 0,
    KEY_DELIMITED_VALUES,
} LogMode;

typedef struct
{
    LogMode mode;
    bool outputCompleted;
    char stringifyKey[STRINGIFY_KEY_LEN];
    char stringifyBuffer[STRINGIFY_BUFFER_LEN];
    uint32_t bufferSize;
    uint32_t elementCount;
} LogInfo;

typedef struct
{
    PathParsing pathParsing;
    NameProcessing nameProcessing;
    ContextLogger contextLogger;
    LogInfo logInfo;
} LoggerStruct;

#endif // LOGGER_INTERNAL_H
