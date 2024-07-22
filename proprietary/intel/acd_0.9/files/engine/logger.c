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

/*
Logger API
KeyValue output: (default)
LogConfig(loggerStruct,KEY_VALUE)
LogRegister()
 |---->LogProcessValue()
        |---->LogKeyValue()
                |---->AddToCJSON if mode = KEY_VALUE
                |---->AddToStringifyBuffer if mode = KEY_DELIMITED_VALUES

DelimitedValues output:
LogConfig(loggerStruct,KEY_DELIMITED_VALUES)
LogDelimitedValue(loggerStruct, key, string)
 |---->AddToStringifyBuffer
void LogDumpDelimitedValues(root, loggerStruct,expectedElements)
 |----> LogKeyValue()

*/
#include "logger.h"

acdStatus getPath(cJSON* section, LoggerStruct* loggerStruct)
{
    loggerStruct->nameProcessing.rootAtLevel =
        INVALID_VALUE; // Default set to invalid
    if (section != NULL)
    {
        cJSON* RootAtLevelJson =
            cJSON_GetObjectItemCaseSensitive(section, "RootAtLevel");
        if (RootAtLevelJson != NULL)
        {
            loggerStruct->nameProcessing.rootAtLevel =
                RootAtLevelJson->valueint;
        }
        cJSON* OutputPath =
            cJSON_GetObjectItemCaseSensitive(section, "OutputPath");
        if (OutputPath == NULL)
        {
            return ACD_FAILURE;
        }
        // We need to save the OutputPath into a fix char array, because
        // strtok_s modifies the source, and we need to keep the input file
        // object intact, for the second cpu.
        strcpy_s(loggerStruct->pathParsing.pathString,
                 LOGGER_JSON_PATH_STRING_LEN, OutputPath->valuestring);
        strcpy_s(loggerStruct->pathParsing.pathStringRaw,
                 LOGGER_JSON_PATH_STRING_LEN, OutputPath->valuestring);
        loggerStruct->pathParsing.pathStringToken =
            loggerStruct->pathParsing.pathString;
        return ACD_SUCCESS;
    }
    return ACD_FAILURE;
}

acdStatus ParsePath(LoggerStruct* loggerStruct)
{
    int j = 0;
    loggerStruct->pathParsing.numberOfTokens = 0;
    for (int i = 0; i < MAX_NUM_PATH_LEVELS; i++)
    {
        loggerStruct->pathParsing.pathLevelToken[i] = "\n";
    }
    size_t len = strnlen_s(loggerStruct->pathParsing.pathStringToken,
                           LOGGER_JSON_STRING_LEN);
    char* p2str;
    char* token =
        strtok_s(loggerStruct->pathParsing.pathStringToken, &len, "/", &p2str);
    while (token)
    {
        loggerStruct->pathParsing.pathLevelToken[j++] = token;
        token = strtok_s(NULL, &len, "/", &p2str);
    }
    loggerStruct->pathParsing.numberOfTokens = j;
    if (loggerStruct->pathParsing.numberOfTokens == 0)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Number of Tokens on path are zero.\n");
        return ACD_FAILURE;
    }
    return ACD_SUCCESS;
}

acdStatus ParseNameSection(CmdInOut* cmdInOut, LoggerStruct* loggerStruct)
{
    cJSON* it = NULL;
    int position = 0;
    loggerStruct->nameProcessing.extraLevel = false;
    loggerStruct->nameProcessing.sizeFromOutput = false;
    loggerStruct->nameProcessing.zeroPaddedPrint = false;
    cJSON* nameJSON =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->in.outputPath, "Name");
    if (nameJSON == NULL)
    {
        loggerStruct->nameProcessing.logRegister = false;
        // In case there is no Name section just exit and do not log the
        // register
        return ACD_SUCCESS;
    }
    cJSON* sizeJSON =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->in.outputPath, "Size");
    if (sizeJSON != NULL)
    {
        loggerStruct->nameProcessing.size = sizeJSON->valueint;
        loggerStruct->nameProcessing.sizeFromOutput = true;
    }
    cJSON* zeroPaddedJSON =
        cJSON_GetObjectItemCaseSensitive(cmdInOut->in.outputPath, "ZeroPadded");
    if (zeroPaddedJSON != NULL)
    {
        loggerStruct->nameProcessing.zeroPaddedPrint =
            cJSON_IsTrue(zeroPaddedJSON);
    }

    cJSON_ArrayForEach(it, nameJSON)
    {
        switch (position)
        {
            case NAME_POSITION:
                loggerStruct->nameProcessing.registerName = it->valuestring;
                break;
            case SECTION_POSITION:
                loggerStruct->nameProcessing.extraLevel = true;
                loggerStruct->nameProcessing.sectionName = it->valuestring;
                break;
            default:
                return ACD_FAILURE;
        }
        position++;
    }
    return ACD_SUCCESS;
}

acdStatus GenerateJsonPath(CmdInOut* cmdInOut, cJSON* root,
                           LoggerStruct* loggerStruct, bool useRootPath)
{
    char jsonItemString[LOGGER_JSON_STRING_LEN];
    cJSON* cjsonLevels[MAX_NUM_PATH_LEVELS];
    cJSON* lastLevelJSON = NULL;
    uint8_t itemsToProcess = 0;
    for (uint8_t i = 0; i < MAX_NUM_PATH_LEVELS; i++)
    {
        cjsonLevels[i] = NULL;
    }
    cjsonLevels[0] = root;

    if (!loggerStruct->nameProcessing.sizeFromOutput)
    {
        loggerStruct->nameProcessing.size = cmdInOut->out.size;
    }

    itemsToProcess = loggerStruct->pathParsing.numberOfTokens;
    if (useRootPath)
    {
        if (loggerStruct->nameProcessing.rootAtLevel <=
            loggerStruct->pathParsing.numberOfTokens)
        {
            itemsToProcess = loggerStruct->nameProcessing.rootAtLevel;
        }
    }
    for (int i = 0; i < itemsToProcess; i++)
    {
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, "%s",
                      loggerStruct->pathParsing.pathLevelToken[i]);
        int match = 1;
        strcmp_s(LOGGER_CPU, strnlen_s(LOGGER_CPU, sizeof(LOGGER_CPU)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_CPU,
                          loggerStruct->contextLogger.cpu);
        }
        match = 1;
        strcmp_s(LOGGER_COMPUTE,
                 strnlen_s(LOGGER_COMPUTE, sizeof(LOGGER_COMPUTE)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                          LOGGER_COMPUTE, loggerStruct->contextLogger.compute);
        }
        match = 1;
        strcmp_s(LOGGER_IO, strnlen_s(LOGGER_IO, sizeof(LOGGER_IO)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_IO,
                          loggerStruct->contextLogger.io);
        }
        match = 1;
        strcmp_s(LOGGER_MODULE, strnlen_s(LOGGER_MODULE, sizeof(LOGGER_MODULE)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_MODULE,
                          loggerStruct->contextLogger.module);
        }
        match = 1;
        strcmp_s(LOGGER_CORE, strnlen_s(LOGGER_CORE, sizeof(LOGGER_CORE)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_CORE,
                          loggerStruct->contextLogger.core);
        }
        match = 1;
        strcmp_s(LOGGER_THREAD, strnlen_s(LOGGER_THREAD, sizeof(LOGGER_THREAD)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_THREAD,
                          loggerStruct->contextLogger.thread);
        }
        match = 1;
        strcmp_s(LOGGER_CHA, strnlen_s(LOGGER_CHA, sizeof(LOGGER_CHA)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_CHA,
                          loggerStruct->contextLogger.cha);
        }
        match = 1;
        strcmp_s(LOGGER_CBO, strnlen_s(LOGGER_CBO, sizeof(LOGGER_CBO)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_CBO,
                          loggerStruct->contextLogger.cha +
                              loggerStruct->contextLogger.chaOffset);
        }
        match = 1;
        strcmp_s(LOGGER_CBO_UC, strnlen_s(LOGGER_CBO_UC, sizeof(LOGGER_CBO_UC)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_CBO_UC,
                          loggerStruct->contextLogger.cha +
                              loggerStruct->contextLogger.chaOffset);
        }
        match = 1;
        strcmp_s(LOGGER_LLC, strnlen_s(LOGGER_LLC, sizeof(LOGGER_LLC)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_LLC,
                          loggerStruct->contextLogger.cha +
                              loggerStruct->contextLogger.chaOffset);
        }
        match = 1;
        strcmp_s(LOGGER_LLC_UC, strnlen_s(LOGGER_LLC_UC, sizeof(LOGGER_LLC_UC)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_LLC_UC,
                          loggerStruct->contextLogger.cha +
                              loggerStruct->contextLogger.chaOffset);
        }
        match = 1;
        strcmp_s(LOGGER_DOMAIN, strnlen_s(LOGGER_DOMAIN, sizeof(LOGGER_DOMAIN)),
                 loggerStruct->pathParsing.pathLevelToken[i], &match);
        if (match == 0)
        {
            // Check if io or compute
            if (loggerStruct->contextLogger.isDomainCompute)
            {
                cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                              LOGGER_COMPUTE,
                              loggerStruct->contextLogger.compute);
            }
            else
            {
                cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_IO,
                              loggerStruct->contextLogger.io);
            }
        }
        if ((cjsonLevels[i + 1] = cJSON_GetObjectItemCaseSensitive(
                 cjsonLevels[i], jsonItemString)) == NULL)
        {
            cJSON_AddItemToObject(cjsonLevels[i], jsonItemString,
                                  cjsonLevels[i + 1] = cJSON_CreateObject());
            // Add version if we are creating a new object at rootpath or No
            // rootpath given and this is full path
            if ((loggerStruct->nameProcessing.rootAtLevel == (i + 1)) ||
                ((loggerStruct->nameProcessing.rootAtLevel == INVALID_VALUE) &&
                 (loggerStruct->pathParsing.numberOfTokens == (i + 1))))
            {
                logVersion(cmdInOut, cjsonLevels[i + 1], loggerStruct);
            }
        }
    }
    loggerStruct->nameProcessing.jsonOutput = cjsonLevels[itemsToProcess];
    if (useRootPath)
    {
        loggerStruct->nameProcessing.extraLevel = false;
    }
    if (loggerStruct->nameProcessing.extraLevel)
    {
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, "%s",
                      loggerStruct->nameProcessing.sectionName);
        if ((lastLevelJSON = cJSON_GetObjectItemCaseSensitive(
                 cjsonLevels[itemsToProcess], jsonItemString)) == NULL)
        {
            cJSON_AddItemToObject(cjsonLevels[itemsToProcess], jsonItemString,
                                  lastLevelJSON = cJSON_CreateObject());
        }
        loggerStruct->nameProcessing.jsonOutput = lastLevelJSON;
    }
    if (loggerStruct->nameProcessing.jsonOutput == NULL)
    {
        return ACD_FAILURE;
    }
    return ACD_SUCCESS;
}

void GenerateRegisterName(char* registerName, LoggerStruct* loggerStruct)
{
    char* p2str;
    errno_t resultCha = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_CHA, strnlen_s(LOGGER_CHA, sizeof(LOGGER_CHA)), &p2str);
    errno_t resultCbo = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_CBO, strnlen_s(LOGGER_CBO, sizeof(LOGGER_CBO)), &p2str);
    errno_t resultLlc = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_LLC, strnlen_s(LOGGER_LLC, sizeof(LOGGER_LLC)), &p2str);
    errno_t resultCpu = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_CPU, strnlen_s(LOGGER_CPU, sizeof(LOGGER_CPU)), &p2str);
    errno_t resultCore = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_CORE, strnlen_s(LOGGER_CORE, sizeof(LOGGER_CORE)), &p2str);
    errno_t resultThread = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_THREAD, strnlen_s(LOGGER_THREAD, sizeof(LOGGER_THREAD)), &p2str);
    errno_t resultSpecial = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_REPEAT, strnlen_s(LOGGER_REPEAT, sizeof(LOGGER_REPEAT)), &p2str);
    errno_t resultCompute =
        strstr_s(loggerStruct->nameProcessing.registerName,
                 LOGGER_JSON_STRING_LEN, LOGGER_COMPUTE,
                 strnlen_s(LOGGER_COMPUTE, sizeof(LOGGER_COMPUTE)), &p2str);
    errno_t resultIO = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_IO, strnlen_s(LOGGER_IO, sizeof(LOGGER_IO)), &p2str);

    errno_t resultDomain = strstr_s(
        loggerStruct->nameProcessing.registerName, LOGGER_JSON_STRING_LEN,
        LOGGER_DOMAIN, strnlen_s(LOGGER_DOMAIN, sizeof(LOGGER_DOMAIN)), &p2str);
    if (resultCha == EOK || resultCbo == EOK || resultLlc == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.cha +
                          loggerStruct->contextLogger.chaOffset);
    }
    else if (resultCompute == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.compute);
    }
    else if (resultIO == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.io);
    }
    else if (resultCore == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.core);
    }
    else if (resultCpu == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.cpu);
    }
    else if (resultThread == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.thread);
    }
    else if (resultSpecial == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.repeats);
    }
    else if (resultDomain == EOK)
    {
        cd_snprintf_s(registerName, LOGGER_JSON_STRING_LEN,
                      loggerStruct->nameProcessing.registerName,
                      loggerStruct->contextLogger.domainID);
    }
    else
    {
        strcpy_s(registerName, LOGGER_JSON_STRING_LEN,
                 loggerStruct->nameProcessing.registerName);
    }
}

void LoggerInit(LoggerStruct* loggerStruct)
{
    // Set Default values
    loggerStruct->logInfo.mode = KEY_VALUE,
    loggerStruct->logInfo.outputCompleted = true,
    loggerStruct->logInfo.bufferSize = 0,
    loggerStruct->logInfo.elementCount = 0,
    memset_s(loggerStruct->logInfo.stringifyBuffer, STRINGIFY_BUFFER_LEN, 0,
             STRINGIFY_BUFFER_LEN);
    memset_s(loggerStruct->logInfo.stringifyKey, STRINGIFY_KEY_LEN, 0,
             STRINGIFY_KEY_LEN);
}

void LogConfig(LoggerStruct* loggerStruct, LogMode mode)
{
    LogInfo* logInfo = &loggerStruct->logInfo;

    // Clear members
    LoggerInit(loggerStruct);

    // Set new mode
    logInfo->mode = mode;
    logInfo->outputCompleted = false;
}

void LogKeyValue(LoggerStruct* loggerStruct, cJSON* parent, char* registerName,
                 char* value)
{
    char jsonItemString[LOGGER_JSON_STRING_LEN];
    char jsonKeyString[LOGGER_JSON_STRING_LEN];
    LogInfo* logInfo = &loggerStruct->logInfo;

    if (logInfo->mode == KEY_VALUE)
    {
        cJSON_AddStringToObject(parent, registerName, value);
    }
    else if (logInfo->mode == KEY_DELIMITED_VALUES)
    {
        // Encode JSON key/value with semicolons; keyvalue becomes value
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                      STRINGIFY_FORMAT_KEYVALUE, registerName, value);

        // Key is parent + _all
        cd_snprintf_s(jsonKeyString, LOGGER_JSON_STRING_LEN,
                      STRINGIFY_FORMAT_KEYNAME, parent->string);
        LogDelimitedValue(loggerStruct, jsonKeyString, jsonItemString);
    }
}
void LogProcessValue(char* registerName, CmdInOut* cmdInOut,
                     LoggerStruct* loggerStruct, cJSON* parent)
{
    char jsonItemString[LOGGER_JSON_STRING_LEN];
    char jsonNameString[LOGGER_JSON_STRING_LEN];

    if (cmdInOut->out.printString)
    {
        LogKeyValue(loggerStruct, parent, registerName,
                    cmdInOut->out.stringVal);
        cmdInOut->out.printString = false;
        return;
    }
    switch (loggerStruct->nameProcessing.size)
    {
        case sizeof(uint8_t):
            cmdInOut->out.val.u64 &= 0xFF;
            break;
        case sizeof(uint16_t):
            cmdInOut->out.val.u64 &= 0xFFFF;
            break;
        case sizeof(uint32_t):
            cmdInOut->out.val.u64 &= 0xFFFFFFFF;
            break;
        default:
            break;
    }
    if (loggerStruct->contextLogger.skipFlag)
    {
        if (cmdInOut->runTimeInfo->maxGlobalRunTimeReached &&
            cmdInOut->runTimeInfo->globalMaxPrint)
        {
            cd_snprintf_s(jsonNameString, LOGGER_JSON_STRING_LEN,
                          LOGGER_GLOBAL_TIMEOUT,
                          loggerStruct->contextLogger.cpu,
                          loggerStruct->contextLogger.currentSectionName);
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                          LOGGER_INFO_TIMEOUT,
                          cmdInOut->runTimeInfo->maxGlobalTime);
            // Don't stringify
            cJSON_AddStringToObject(parent, jsonNameString, jsonItemString);
            cmdInOut->runTimeInfo->globalMaxPrint = false;
        }
        if (cmdInOut->runTimeInfo->maxSectionRunTimeReached &&
            cmdInOut->runTimeInfo->sectionMaxPrint)
        {
            cd_snprintf_s(jsonNameString, LOGGER_JSON_STRING_LEN,
                          LOGGER_SECTION_TIMEOUT,
                          loggerStruct->contextLogger.cpu,
                          loggerStruct->contextLogger.currentSectionName);
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                          LOGGER_INFO_TIMEOUT,
                          cmdInOut->runTimeInfo->maxSectionTime);
            // Don't stringify
            cJSON_AddStringToObject(parent, jsonNameString, jsonItemString);
            cmdInOut->runTimeInfo->sectionMaxPrint = false;
        }
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, LOGGER_NA);
        LogKeyValue(loggerStruct, parent, registerName, jsonItemString);
    }
    else if (cmdInOut->out.ret != PECI_CC_SUCCESS)
    {
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                      LOGGER_FIXED_DATA_CC_RC, cmdInOut->out.cc,
                      cmdInOut->out.ret);
        LogKeyValue(loggerStruct, parent, registerName, jsonItemString);
    }
    else if (PECI_CC_UA(cmdInOut->out.cc))
    {
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                      LOGGER_DATA_64_bits_CC_RC, cmdInOut->out.val.u64,
                      cmdInOut->out.cc, cmdInOut->out.ret);
        LogKeyValue(loggerStruct, parent, registerName, jsonItemString);
    }
    else
    {
        if (loggerStruct->nameProcessing.zeroPaddedPrint)
        {
            char* formatStr;
            switch (loggerStruct->nameProcessing.size)
            {
                case 1:
                    formatStr = "0x%02x";
                    break;
                case 2:
                    formatStr = "0x%04x";
                    break;
                case 4:
                    formatStr = "0x%08x";
                    break;
                case 8:
                    formatStr = "0x%016llx";
                    break;
                default:
                    formatStr = "0x%016llx";
                    break;
            }
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, formatStr,
                          cmdInOut->out.val.u64);
        }
        else
        {
            cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN,
                          LOGGER_DATA_64bits, cmdInOut->out.val.u64);
        }
        LogKeyValue(loggerStruct, parent, registerName, jsonItemString);
    }
}

void LogRegister(CmdInOut* cmdInOut, cJSON* root, LoggerStruct* loggerStruct)
{
    char registerName[LOGGER_JSON_STRING_LEN];

    if (GenerateJsonPath(cmdInOut, root, loggerStruct, false) == ACD_SUCCESS)
    {
        GenerateRegisterName(&(registerName[0]), loggerStruct);
        LogProcessValue(&(registerName[0]), cmdInOut, loggerStruct,
                        loggerStruct->nameProcessing.jsonOutput);
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log Value, path is Null.\n");
    }
}

void LogDelimitedValue(LoggerStruct* loggerStruct, char* key, char* singleStr)
{
    LogInfo* logInfo = &loggerStruct->logInfo;

    // Only supported under KEY_DELIMITED_VALUES mode
    if (logInfo->mode != KEY_DELIMITED_VALUES)
    {
        // This shouldn't happen so print out to notify
        CRASHDUMP_PRINT(ERR, stderr, "Incorrect call to LogString()\n");
        return;
    }

    // Check if we need to initialize key
    if (logInfo->stringifyKey[0] == 0 && key != NULL)
    {
        // Keep a copy
        strcpy_s(logInfo->stringifyKey, sizeof(logInfo->stringifyKey), key);
    }

    // Add to raw string to buffer with delimiter
    if (logInfo->bufferSize < STRINGIFY_BUFFER_LEN)
    {
        // Note: vsnprintf_s doesn't seem to like our large 64k buffer(returns
        // -403). Only add 2048 maximum at a time.
        size_t writeLen =
            MIN((STRINGIFY_BUFFER_LEN - logInfo->bufferSize), 2048);
        if (logInfo->bufferSize == 0)
        {
            // Special case for first value; no delimiter
            logInfo->bufferSize +=
                cd_snprintf_s(logInfo->stringifyBuffer + logInfo->bufferSize,
                              writeLen, STRINGIFY_FORMAT, "", singleStr);
        }
        else
        {
            logInfo->bufferSize += cd_snprintf_s(
                logInfo->stringifyBuffer + logInfo->bufferSize, writeLen,
                STRINGIFY_FORMAT, STRINGIFY_DELIMITER, singleStr);
        }

        logInfo->elementCount++;
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Stringify buffer is full. Dumping.\n");
    }
}

void LogDumpDelimitedValues(cJSON* outputNode, LoggerStruct* loggerStruct,
                            uint32_t expectedElements)
{
    cJSON* obj = NULL;
    LogInfo* logInfo = &loggerStruct->logInfo;

    if (logInfo->mode == KEY_DELIMITED_VALUES)
    {
        // Set register name to stringify key
        if (logInfo->stringifyKey[0] != 0)
        {
            loggerStruct->nameProcessing.registerName = logInfo->stringifyKey;
        }
        else
        {
            loggerStruct->nameProcessing.registerName = "Unnamed";
        }

        // Change mode to JSON to store in CJSON
        logInfo->mode = KEY_VALUE;

        // Write full string buffer
        LogKeyValue(loggerStruct, outputNode,
                    loggerStruct->nameProcessing.registerName,
                    logInfo->stringifyBuffer);

        // Add the _encoding
        obj = cJSON_GetObjectItem(outputNode, STRINGIFY_KEY_ENCODING);
        if (obj == NULL)
        {
            LogKeyValue(loggerStruct, outputNode, STRINGIFY_KEY_ENCODING,
                        "string");
        }

        // Add the delimiter
        obj = cJSON_GetObjectItem(outputNode, STRINGIFY_KEY_DELIMITER);
        if (obj == NULL)
        {
            LogKeyValue(loggerStruct, outputNode, STRINGIFY_KEY_DELIMITER,
                        STRINGIFY_DELIMITER);
        }

        // Add elements
        obj = cJSON_GetObjectItem(outputNode, STRINGIFY_KEY_ELEMENTS);
        if (obj == NULL)
        {
            char jsonValueString[LOGGER_JSON_STRING_LEN];
            cd_snprintf_s(jsonValueString, LOGGER_JSON_STRING_LEN, "0x%x",
                          expectedElements);
            LogKeyValue(loggerStruct, outputNode, STRINGIFY_KEY_ELEMENTS,
                        jsonValueString);
        }

        // Clean complete
        logInfo->outputCompleted = true;
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Not in stringify mode. Can't dump\n");
    }
}

void GenerateVersion(cJSON* Section, int* Version)
{
    int productType = 0;
    int recordType = 0;
    int revisionNum = 0;
    productType = cJSONToInt(
        cJSON_GetObjectItemCaseSensitive(Section, "ProductType"), 16);
    recordType =
        cJSONToInt(cJSON_GetObjectItemCaseSensitive(Section, "RecordType"), 16);
    revisionNum =
        cJSONToInt(cJSON_GetObjectItemCaseSensitive(Section, "Revision"), 16);
    if (productType == 0 && recordType == 0 && revisionNum == 0)
    {
        *Version = 0;
    }
    else
    {
        *Version = recordType << RECORD_TYPE_OFFSET |
                   productType << PRODUCT_TYPE_OFFSET |
                   revisionNum << REVISION_OFFSET;
    }
}

void logVersion(CmdInOut* cmdInOut, cJSON* level, LoggerStruct* loggerStruct)
{
    if (loggerStruct->contextLogger.version != 0)
    {
        char jsonItemString[LOGGER_JSON_STRING_LEN];
        cd_snprintf_s(jsonItemString, LOGGER_JSON_STRING_LEN, "0x%08x",
                      loggerStruct->contextLogger.version);
        if (level != NULL)
        {
            cJSON_AddStringToObject(level, LOGGER_VERSION_STRING,
                                    jsonItemString);
        }
    }
}

void logRecordDisabled(CmdInOut* cmdInOut, cJSON* root,
                       LoggerStruct* loggerStruct)
{
    loggerStruct->nameProcessing.extraLevel = false;
    if (GenerateJsonPath(cmdInOut, root, loggerStruct, true) == ACD_SUCCESS)
    {
        cJSON* previousVersion = cJSON_GetObjectItemCaseSensitive(
            loggerStruct->nameProcessing.jsonOutput, RECORD_ENABLE);
        if (previousVersion == NULL)
        {
            cJSON_AddBoolToObject(loggerStruct->nameProcessing.jsonOutput,
                                  RECORD_ENABLE, false);
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Could not log Record Disable, path is Null.\n");
    }
}

void logSectionRunTime(cJSON* parent, struct timespec* start, char* key)
{
    char timeString[64];
    struct timespec finish = {};
    uint64_t timeVal = 0;
    if (parent == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Could not log %s, parent is Null.\n",
                        key);
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &finish);
    uint64_t runTimeInNs = tsToNanosecond(&finish) - tsToNanosecond(start);

    timeVal = runTimeInNs;

    cd_snprintf_s(timeString, sizeof(timeString), "%.2fs",
                  (double)timeVal / 1e9);

    if (cJSON_GetObjectItemCaseSensitive(parent, key) == NULL)
    {
        cJSON_AddStringToObject(parent, key, timeString);
    }

    clock_gettime(CLOCK_MONOTONIC, start);
}

void logSectionFailCount(cJSON* parent, int failCount, char* sectionName)
{
    char failString[64];
    if (parent == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Could not log failcount, parent is Null.\n");
        return;
    }
    /*  Create keyname as "_failcount_<sectionName>"
        With a length of 64 this leaves us with the max of 53 char sectionName.
        The longest is currently 27 char (Metadata_Cpu_Compute_Early)
    */
    cd_snprintf_s(failString, sizeof(failString) - 1, "_failcount_%s",
                  sectionName);

    cJSON_AddNumberToObject(parent, failString, failCount);
}

void logStringAtPath(cJSON* root, cJSON* section, char* name, char* value)
{
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct = {};

    cmdInOut.root = root;
    cmdInOut.logger = &loggerStruct;
    cmdInOut.out.ret = PECI_CC_INVALID_REQ;
    cmdInOut.out.size = 0;
    cmdInOut.out.stringVal = "";

    getPath(section->child, &loggerStruct);
    ParsePath(&loggerStruct);

    // Create the Path
    if (GenerateJsonPath(&cmdInOut, root, &loggerStruct, false) == ACD_SUCCESS)
    {
        cJSON_AddStringToObject(loggerStruct.nameProcessing.jsonOutput, name,
                                value);
    }
}
