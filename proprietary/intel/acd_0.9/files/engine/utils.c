/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2019 Intel Corporation.
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

#include "utils.h"

#include <safe_mem_lib.h>
#include <safe_str_lib.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
struct timespec crashdumpStart;

int cd_snprintf_s(char* str, size_t len, const char* format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(str, len, format, args);
    va_end(args);
    if (ret < CD_SNPRINTF_S_INVALID)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "cd_snprintf_s: Error writing formatted data to string %d\n", ret);
    }
    return ret;
}

cJSON* readInputFile(const char* filename)
{
    char* buffer = NULL;
    cJSON* jsonBuf = NULL;
    long int length = 0;
    FILE* fp = fopen(filename, "r");

    if (fp == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Error reading input file - Can't open file: %s\n",
                        filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    if (length == -1L)
    {
        fclose(fp);
        CRASHDUMP_PRINT(ERR, stderr,
                        "Error reading input file with filename: %s\n",
                        filename);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);
    buffer = (char*)calloc(length + 1, sizeof(char));
    if (buffer)
    {
        size_t result = 0;
        result = fread(buffer, 1, length, fp);
        if ((int)result != length)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Error reading input file - fread read %zu bytes, "
                            "but length is %ld",
                            result, length);
            fclose(fp);
            FREE(buffer);
            return NULL;
        }
        buffer[result] = '\0';
    }

    fclose(fp);
    // Convert and return cJSON object from buffer
    jsonBuf = cJSON_Parse(buffer);
    if (jsonBuf == NULL)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Error reading input file - can't parse cjson file %s\n", filename);
        FREE(buffer);
        return NULL;
    }
    FREE(buffer);
    return jsonBuf;
}

cJSON* getCommandFromSection(cJSON* root, const char* section,
                             const char* peciCommand)
{
    cJSON* commands = cJSON_GetObjectItem(
        getNewCrashDataSection(root, (char*)section), "Commands");
    cJSON* command = NULL;
    cJSON_ArrayForEach(command, commands)
    {
        cJSON* child = cJSON_GetObjectItemCaseSensitive(command, peciCommand);
        if (child != NULL)
        {
            return child;
        }
    }
    return NULL;
}

bool readInputFileFlag(cJSON* pJsonChild, bool defaultValue, char* keyName)
{
    bool valueFromInputFile = false;
    bool keyfound = false;
    if (pJsonChild == NULL)
    {
        return defaultValue;
    }
    cJSON* section = cJSON_GetObjectItemCaseSensitive(pJsonChild, keyName);
    if (section != NULL)
    {
        keyfound = true;
        valueFromInputFile = cJSON_IsTrue(section);
    }
    if (!keyfound)
    {
        return defaultValue;
    }
    else
    {
        return valueFromInputFile;
    }
}

cJSON* getNewCrashDataSection(cJSON* root, char* section)
{
    cJSON* sections = cJSON_GetObjectItemCaseSensitive(root, "Sections");

    if (sections != NULL)
    {
        cJSON* subsection = NULL;
        cJSON_ArrayForEach(subsection, sections)
        {
            cJSON* child =
                cJSON_GetObjectItemCaseSensitive(subsection, section);
            if (child != NULL)
            {
                return child;
            }
        }
    }
    return NULL;
}

cJSON* getNewCrashDataSectionObjectOneLevel(cJSON* root, char* section,
                                            const char* firstLevel)
{
    cJSON* child = getNewCrashDataSection(root, section);

    if (child != NULL)
    {
        return cJSON_GetObjectItemCaseSensitive(child, firstLevel);
    }

    return child;
}

bool isCoreRegVersionMatch(cJSON* root, uint32_t version, CoreType coreType)
{
    cJSON* child;

    if (coreType == ACD_BIG_CORE)
    {
        child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);
    }
    else
    {
        child = getNewCrashDataSection(root, ATOM_CORE_SECTION_NAME);
    }

    if (child != NULL)
    {
        char jsonItemName[NAME_STR_LEN] = {0};
        cd_snprintf_s(jsonItemName, NAME_STR_LEN, "0x%x", version);

        cJSON* decodeArray = cJSON_GetObjectItemCaseSensitive(child, "Decode");

        if (decodeArray != NULL)
        {
            cJSON* mapItem = NULL;
            cJSON_ArrayForEach(mapItem, decodeArray)
            {
                cJSON* versionArray =
                    cJSON_GetObjectItemCaseSensitive(mapItem, "version");
                if (NULL != versionArray)
                {
                    cJSON* versionItem = NULL;
                    cJSON_ArrayForEach(versionItem, versionArray)
                    {
                        int mismatch = 1;
                        strcmp_s(jsonItemName,
                                 strnlen_s(jsonItemName, NAME_STR_LEN),
                                 versionItem->valuestring, &mismatch);
                        if (0 == mismatch)
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool isValueInJsonArray(cJSON* jsonArray, uint32_t value)
{
    if (NULL == jsonArray || 0 == value)
    {
        return false;
    }

    char* endptr = NULL;
    cJSON* jsonItem = NULL;
    cJSON_ArrayForEach(jsonItem, jsonArray)
    {
        uint32_t jsonValue = strtoul(jsonItem->valuestring, &endptr, 16);
        if (endptr == jsonItem->valuestring)
        {
            return false;
        }

        if (jsonValue == value)
        {
            return true;
        }
    }

    return false;
}

cJSON* getValidBigCoreMap(cJSON* decodeArray, char* version,
                          uint32_t crashdumpVersionSize)
{
    if (NULL == decodeArray)
    {
        return NULL;
    }

    cJSON* mapItem = NULL;
    cJSON_ArrayForEach(mapItem, decodeArray)
    {
        cJSON* versionArray =
            cJSON_GetObjectItemCaseSensitive(mapItem, "version");
        if (NULL != versionArray)
        {
            cJSON* versionItem = NULL;
            cJSON_ArrayForEach(versionItem, versionArray)
            {
                int mismatch = 1;
                strcmp_s(versionItem->valuestring,
                         strnlen_s(version, NAME_STR_LEN), version, &mismatch);
                if (0 == mismatch)
                {
                    cJSON* jsonSize = cJSON_GetObjectItemCaseSensitive(
                        mapItem, "valid_header_size");
                    if (jsonSize == NULL ||
                        isValueInJsonArray(jsonSize, crashdumpVersionSize))
                    {
                        return mapItem;
                    }
                }
            }
        }
    }

    return NULL;
}

cJSON* getCrashDataSectionCoreRegList(cJSON* root, char* version,
                                      uint32_t crashdumpVersionSize,
                                      CoreType coreType)
{
    cJSON* child;
    if (coreType == ACD_BIG_CORE)
    {
        child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);
    }
    else
    {
        child = getNewCrashDataSection(root, ATOM_CORE_SECTION_NAME);
    }

    if (child != NULL)
    {
        cJSON* decodeObject = cJSON_GetObjectItemCaseSensitive(child, "Decode");

        if (decodeObject != NULL)
        {
            cJSON* mapObject =
                getValidBigCoreMap(decodeObject, version, crashdumpVersionSize);
            if (mapObject != NULL)
            {
                return cJSON_GetObjectItemCaseSensitive(mapObject, "reg_list");
            }
            return mapObject;
        }
        return decodeObject;
    }
    return child;
}

uint32_t getCrashDataSectionCoreSize(cJSON* root, char* version,
                                     uint32_t crashdumpVersionSize,
                                     CoreType coreType)
{
    uint32_t size = 0;
    cJSON* child;

    if (coreType == ACD_BIG_CORE)
    {
        child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);
    }
    else
    {
        child = getNewCrashDataSection(root, ATOM_CORE_SECTION_NAME);
    }

    if (child != NULL)
    {
        cJSON* decodeObject = cJSON_GetObjectItemCaseSensitive(child, "Decode");

        if (decodeObject != NULL)
        {
            cJSON* mapObject =
                getValidBigCoreMap(decodeObject, version, crashdumpVersionSize);

            if (mapObject != NULL)
            {
                cJSON* jsonSize =
                    cJSON_GetObjectItemCaseSensitive(mapObject, "_size");

                if ((jsonSize != NULL) && cJSON_IsString(jsonSize))
                {
                    size = strtoul(jsonSize->valuestring, NULL, 16);
                }
            }
        }
    }

    return size;
}

cJSON* getCrashDataSectionCoreSizeArray(cJSON* root, char* version,
                                        uint32_t crashdumpVersionSize,
                                        CoreType coreType)
{
    cJSON* jsonSizeArray = NULL;
    cJSON* child;

    if (coreType == ACD_BIG_CORE)
    {
        child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);
    }
    else
    {
        child = getNewCrashDataSection(root, ATOM_CORE_SECTION_NAME);
    }

    if (child != NULL)
    {
        cJSON* decodeObject = cJSON_GetObjectItemCaseSensitive(child, "Decode");

        if (decodeObject != NULL)
        {
            cJSON* mapObject =
                getValidBigCoreMap(decodeObject, version, crashdumpVersionSize);

            if (mapObject != NULL)
            {
                jsonSizeArray = cJSON_GetObjectItemCaseSensitive(
                    mapObject, "valid_header_size");

                if ((jsonSizeArray != NULL) && cJSON_IsArray(jsonSizeArray))
                {
                    return jsonSizeArray;
                }
            }
        }
    }
    return NULL;
}

void storeCrashDataSectionCoreSize(cJSON* root, char* version,
                                   uint32_t totalSize,
                                   uint32_t crashdumpVersionSize,
                                   CoreType coreType)
{
    cJSON* child;

    if (coreType == ACD_BIG_CORE)
    {
        child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);
    }
    else
    {
        child = getNewCrashDataSection(root, ATOM_CORE_SECTION_NAME);
    }

    if (child != NULL)
    {
        char jsonItemName[NAME_STR_LEN] = {0};
        cd_snprintf_s(jsonItemName, NAME_STR_LEN, "0x%x", totalSize);
        cJSON_AddStringToObject(
            getValidBigCoreMap(
                cJSON_GetObjectItemCaseSensitive(child, "Decode"), version,
                crashdumpVersionSize),
            "_size", jsonItemName);
    }
}

errno_t createPathWithFmtSpecifier(char* default_file, char* path,
                                   char* inputFileNameFmt)
{
    errno_t rc = strcpy_s(default_file, MAX_PATH_LEN, path);
    if (rc != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to copy string: %s\n",
                        strerror(rc));
        return rc;
    }
    rc = strcat_s(default_file, MAX_PATH_LEN, inputFileNameFmt);
    if (rc != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to concatenate string: %s\n",
                        strerror(rc));
        return rc;
    }
    return rc;
}

cJSON* selectAndReadInputFile(Model cpuModel, char** filename, bool isTelemetry,
                              char* defaultPath, char* overridePath)
{
    char cpuStr[CPU_STR_LEN] = {0};
    char nameStr[NAME_STR_LEN] = {0};

    switch (cpuModel)
    {
        case cd_emr:
            strcpy_s(cpuStr, sizeof("emr"), "emr");
            break;
        case cd_spr:
            strcpy_s(cpuStr, sizeof("spr"), "spr");
            break;
        case cd_sprhbm:
            strcpy_s(cpuStr, sizeof("sprhbm"), "sprhbm");
            break;
        case cd_srf:
            strcpy_s(cpuStr, sizeof("srf"), "srf");
            break;
        case cd_gnr:
            strcpy_s(cpuStr, sizeof("gnr"), "gnr");
            break;
        case cd_gnrd:
            strcpy_s(cpuStr, sizeof("gnrd"), "gnrd");
            break;
        default:
            CRASHDUMP_PRINT(ERR, stderr,
                            "Error selecting input file (CPUID 0x%x).\n",
                            cpuModel);
            return NULL;
    }

    char override_file[MAX_PATH_LEN];
    errno_t rc;

    if (isTelemetry)
    {
        rc = createPathWithFmtSpecifier(override_file, overridePath,
                                        DEFAULT_TELEMETRY_FILENAME_FMT);
    }
    else
    {
        rc = createPathWithFmtSpecifier(override_file, overridePath,
                                        DEFAULT_INPUT_FILENAME_FMT);
    }

    if (rc != EOK)
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Failed to create override path string: %s\n",
                        strerror(rc));
        return NULL;
    }

    cd_snprintf_s(nameStr, NAME_STR_LEN, override_file, cpuStr);

    if (access(nameStr, F_OK) != -1)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Using override file - %s\n", nameStr);
    }
    else
    {
        char default_file[MAX_PATH_LEN];

        if (isTelemetry)
        {
            rc = createPathWithFmtSpecifier(default_file, defaultPath,
                                            DEFAULT_TELEMETRY_FILENAME_FMT);
        }
        else
        {
            rc = createPathWithFmtSpecifier(default_file, defaultPath,
                                            DEFAULT_INPUT_FILENAME_FMT);
        }
        if (rc != EOK)
        {
            CRASHDUMP_PRINT(ERR, stderr,
                            "Failed to create default path string: %s\n",
                            strerror(rc));
            return NULL;
        }
        cd_snprintf_s(nameStr, NAME_STR_LEN, default_file, cpuStr);
    }
    *filename = (char*)malloc(sizeof(nameStr));
    if (*filename == NULL)
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Error selecting input file (CPUID 0x%x) - Cannot allocate file.\n",
            cpuModel);
        return NULL;
    }
    strcpy_s(*filename, sizeof(nameStr), nameStr);

    return readInputFile(nameStr);
}

uint64_t tsToNanosecond(struct timespec* ts)
{
    return ((ts->tv_sec * (uint64_t)1e9) + ts->tv_nsec);
}

inline struct timespec
    calculateTimeRemaining(uint32_t maxWaitTimeFromInputFileInSec)
{
    struct timespec current = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &current);

    uint64_t runTimeInNs =
        tsToNanosecond(&current) - tsToNanosecond(&crashdumpStart);

    uint64_t maxWaitTimeFromInputFileInNs =
        maxWaitTimeFromInputFileInSec * (uint64_t)1e9;

    struct timespec timeRemaining = {0, 0};
    if (runTimeInNs < maxWaitTimeFromInputFileInNs)
    {
        timeRemaining.tv_sec =
            (maxWaitTimeFromInputFileInNs - runTimeInNs) / 1e9;
        timeRemaining.tv_nsec =
            (maxWaitTimeFromInputFileInNs - runTimeInNs) % (uint64_t)1e9;

        return timeRemaining;
    }

    return timeRemaining;
}

inline uint32_t getDelayFromInputFile(CPUInfo* cpuInfo, char* sectionName)
{
    cJSON* inputDelayField = getNewCrashDataSectionObjectOneLevel(
        cpuInfo->inputFile.bufferPtr, sectionName, "MaxWaitSec");

    if (inputDelayField != NULL)
    {
        return (uint32_t)inputDelayField->valueint;
    }

    return 0;
}

uint32_t getCollectionTimeFromInputFile(CPUInfo* cpuInfo)
{
    cJSON* inputField;

    if (cpuInfo->coreType == ACD_BIG_CORE)
    {
        inputField = getNewCrashDataSectionObjectOneLevel(
            cpuInfo->inputFile.bufferPtr, BIG_CORE_SECTION_NAME,
            "MaxCollectionSec");
    }
    else
    {
        inputField = getNewCrashDataSectionObjectOneLevel(
            cpuInfo->inputFile.bufferPtr, ATOM_CORE_SECTION_NAME,
            "MaxCollectionSec");
    }

    if (inputField != NULL)
    {
        return (uint32_t)inputField->valueint;
    }

    return 0;
}

inline bool getSkipFromNewInputFile(CPUInfo* cpuInfo, char* sectionName)
{
    cJSON* torSection =
        getNewCrashDataSection(cpuInfo->inputFile.bufferPtr, sectionName);
    cJSON* skipIfFailRead =
        cJSON_GetObjectItemCaseSensitive(torSection, "SkipOnFail");

    if (skipIfFailRead != NULL)
    {
        return cJSON_IsTrue(skipIfFailRead);
    }

    return false;
}

uint8_t getBus(Model model, uint8_t u8index)
{
    return sPciReg[u8index].u8Bus;
}

int getPciRegister(CPUInfo* cpuInfo, SRegRawData* sRegData, uint8_t u8index,
                   uint8_t domainID)
{
    int peci_fd = -1;
    int ret = 0;
    uint16_t u16Offset = 0;
    uint8_t u8Size = 0;
    uint8_t u8Bus = 0;

    if (u8index < sizeof(sPciReg) / sizeof(sPciReg[0]))
    {
        u8Size = sPciReg[u8index].u8Size;
        u8Bus = sPciReg[u8index].u8Bus;
    }
    if (0 == u8Size)
    {
        return ACD_FAILURE;
    }
    ret = peci_Lock(&peci_fd, PECI_WAIT_FOREVER);
    if (ret != PECI_CC_SUCCESS)
    {
        sRegData->ret = ret;
        return ACD_FAILURE;
    }
    switch (sPciReg[u8index].u8Size)
    {
        case UT_REG_DWORD:
            ret = peci_RdEndPointConfigPciLocal_seq_dom(
                cpuInfo->clientAddr, domainID, sPciReg[u8index].u8Seg, u8Bus,
                sPciReg[u8index].u8Dev, sPciReg[u8index].u8Func,
                sPciReg[u8index].u16Reg, sPciReg[u8index].u8Size,
                (uint8_t*)&sRegData->uValue.u64, peci_fd, &sRegData->cc);
            sRegData->ret = ret;
            if (ret != PECI_CC_SUCCESS)
            {
                peci_Unlock(peci_fd);
                return ACD_FAILURE;
            }
            sRegData->uValue.u64 &= 0xFFFFFFFF;
            break;
        case UT_REG_QWORD:
            for (uint8_t u8Dword = 0; u8Dword < 2; u8Dword++)
            {
                u16Offset = ((sPciReg[u8index].u16Reg) >> (u8Dword * 8)) & 0xFF;
                u8Size = sPciReg[u8index].u8Size / 2;
                ret = peci_RdEndPointConfigPciLocal_seq_dom(
                    cpuInfo->clientAddr, domainID, sPciReg[u8index].u8Seg,
                    u8Bus, sPciReg[u8index].u8Dev, sPciReg[u8index].u8Func,
                    u16Offset, u8Size, (uint8_t*)&sRegData->uValue.u32[u8Dword],
                    peci_fd, &sRegData->cc);
                sRegData->ret = ret;
                if (ret != PECI_CC_SUCCESS)
                {
                    peci_Unlock(peci_fd);
                    return ACD_FAILURE;
                }
            }
            break;
        default:
            ret = ACD_FAILURE;
    }
    peci_Unlock(peci_fd);
    return ACD_SUCCESS;
}

inputField getFlagValueFromInputFile(CPUInfo* cpuInfo, char* sectionName,
                                     char* flagName)
{
    cJSON* flagField = getNewCrashDataSectionObjectOneLevel(
        cpuInfo->inputFile.bufferPtr, sectionName, flagName);

    if (flagField != NULL)
    {
        return (cJSON_IsTrue(flagField) ? FLAG_ENABLE : FLAG_DISABLE);
    }

    return FLAG_NOT_PRESENT;
}

static struct timespec
    calculateTimeRemainingFromStart(uint32_t maxTimeInSec,
                                    struct timespec sectionStartTime)
{
    struct timespec current = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &current);

    uint64_t runTimeInNs =
        tsToNanosecond(&current) - tsToNanosecond(&sectionStartTime);

    uint64_t maxTimeInNs = maxTimeInSec * (uint64_t)1e9;

    struct timespec timeRemaining = {0, 0};
    if (runTimeInNs < maxTimeInNs)
    {
        timeRemaining.tv_sec = (maxTimeInNs - runTimeInNs) / 1e9;
        timeRemaining.tv_nsec = (maxTimeInNs - runTimeInNs) % (uint64_t)1e9;
    }

    return timeRemaining;
}

executionStatus checkMaxTimeElapsed(uint32_t maxTime,
                                    struct timespec sectionStartTime)
{
    struct timespec timeRemaining =
        calculateTimeRemainingFromStart(maxTime, sectionStartTime);

    if (0 == tsToNanosecond(&timeRemaining))
    {
        return EXECUTION_ABORTED;
    }

    return EXECUTION_TILL_ABORT;
}

double getTimeRemainingFromStart(uint32_t maxTime,
                                 struct timespec sectionStartTime)
{
    struct timespec timeRemaining =
        calculateTimeRemainingFromStart(maxTime, sectionStartTime);
    uint64_t Time =
        ((timeRemaining.tv_sec * (uint64_t)1e9) + timeRemaining.tv_nsec);
    double totalTime = (double)Time / 1e9;
    return totalTime;
}

int clampInt(int val, int lBound, int uBound)
{
    int t = val < lBound ? lBound : val;
    return t > uBound ? uBound : t;
}

void getRangeValues(cJSON* section, const char* rangeName, int* low, int* high)
{
    if (section == NULL || rangeName == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Error: Null arguments\n");
        return;
    }

    cJSON* range = cJSON_GetObjectItemCaseSensitive(section, rangeName);
    if (range == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Error: %s cJSON object is NULL\n",
                        rangeName);
        return;
    }

    if (cJSON_IsArray(range) && cJSON_GetArraySize(range) == 2)
    {
        *low = cJSON_GetArrayItem(range, 0)->valueint;
        *high = cJSON_GetArrayItem(range, 1)->valueint;
    }
    else
    {
        CRASHDUMP_PRINT(
            ERR, stderr,
            "Error: %s cJSON object requires an array with two parameters\n",
            rangeName);
    }
}

cJSON* getNVDSection(cJSON* root, const char* const section, bool* const enable)
{
    *enable = false;
    cJSON* child = cJSON_GetObjectItemCaseSensitive(
        getNewCrashDataSection(root, "NVD"), section);

    if (child != NULL)
    {
        cJSON* recordEnable = cJSON_GetObjectItem(child, RECORD_ENABLE);
        if (recordEnable == NULL)
        {
            *enable = true;
        }
        else
        {
            *enable = cJSON_IsTrue(recordEnable);
        }
    }

    return child;
}

cJSON* getNVDSectionRegList(cJSON* root, const char* const section,
                            bool* const enable)
{
    cJSON* child = getNVDSection(root, section, enable);

    if (child != NULL)
    {
        return cJSON_GetObjectItemCaseSensitive(child, "reg_list");
    }

    return child;
}

cJSON* getPMEMSectionErrLogList(cJSON* root, const char* const section,
                                bool* const enable)
{
    cJSON* child = getNVDSection(root, section, enable);

    if (child != NULL)
    {
        return cJSON_GetObjectItemCaseSensitive(child, "log_list");
    }

    return child;
}

bool isSprHbm(const CPUInfo* cpuInfo)
{
    EPECIStatus status;
    uint8_t cc = 0;
    uint32_t val = 0;

    // Notes: See doc 611488 SPR EDS Volume 1, Table 91 for PLATFORM ID details
    status = peci_RdPkgConfig(cpuInfo->clientAddr, 0, 1, sizeof(uint32_t),
                              (uint8_t*)&val, &cc);
    if (status != PECI_CC_SUCCESS || (PECI_CC_UA(cc)))
    {
        return false;
    }
    else
    {
        return (val == SPR_HBM_PLATFORM_ID) ? true : false;
    }
}

cJSON* getCrashlogAgentsFromInputFile(cJSON* root)
{
    cJSON* crashlogSection = getNewCrashDataSection(root, "crashlog");

    if (crashlogSection != NULL)
    {
        return cJSON_GetObjectItemCaseSensitive(crashlogSection, "Agent");
    }

    return NULL;
}

uint8_t getMaxCollectionCoresFromInputFile(CPUInfo* cpuInfo)
{
    cJSON* inputField;

    if (cpuInfo->coreType == ACD_BIG_CORE)
    {
        inputField = getNewCrashDataSectionObjectOneLevel(
            cpuInfo->inputFile.bufferPtr, BIG_CORE_SECTION_NAME,
            "MaxCollectionCores");
    }
    else
    {
        inputField = getNewCrashDataSectionObjectOneLevel(
            cpuInfo->inputFile.bufferPtr, ATOM_CORE_SECTION_NAME,
            "MaxCollectionCores");
    }

    if (inputField != NULL)
    {
        return (uint8_t)inputField->valueint;
    }

    return 0;
}

/******************************************************************************
 *
 *   fillInputFile
 *
 *   This function fills in the crashdump input filename.
 *
 ******************************************************************************/
void fillInputFile(CPUInfo* cpuInfo, PlatformInfo* platformInfo)
{
    if (cpuInfo->inputFile.filenamePtr == NULL)
    {
        platformInfo->input_file = MD_NA;
    }
    else
    {
        platformInfo->input_file = cpuInfo->inputFile.filenamePtr;
    }
}

/******************************************************************************
 *
 *   logResetDetected
 *
 *   This function logs the section which was being captured when reset occurred
 *
 ******************************************************************************/
int logResetDetected(cJSON* metadata, PlatformState* platformState)
{
    char resetSection[SI_JSON_STRING_LEN];
    if (platformState->resetDetected)
    {
        cd_snprintf_s(resetSection, SI_JSON_STRING_LEN, RESET_DETECTED_NAME,
                      platformState->resetCpu, platformState->resetSectionName);
        CRASHDUMP_PRINT(INFO, stderr, "Reset occurred while in section %s\n",
                        resetSection);
    }
    else
    {
        cd_snprintf_s(resetSection, SI_JSON_STRING_LEN, "%s",
                      RESET_DETECTED_DEFAULT);
    }

    cJSON_AddStringToObject(metadata, "_reset_detected", resetSection);

    return 0;
}

/******************************************************************************
 *
 *   fillCrashdumpVersion
 *
 *   This function fills in the crashdump_ver JSON info
 *
 ******************************************************************************/
int fillCrashdumpVersion(char* cSectionName, cJSON* pJsonChild)
{
    cJSON_AddStringToObject(pJsonChild, cSectionName, CRASHDUMP_VER);
    return ACD_SUCCESS;
}

uint8_t getNumberOfSections(CPUInfo* cpuInfo)
{
    cJSON* sections = cJSON_GetObjectItemCaseSensitive(
        cpuInfo->inputFile.bufferPtr, "Sections");
    if (sections != NULL)
    {
        return cJSON_GetArraySize(sections);
    }
    CRASHDUMP_PRINT(ERR, stderr, "Cannot find Number of Sections!\n");
    return 0;
}

int cJSONToInt(cJSON* cjsonObj, int base)
{
    if (cjsonObj != NULL)
    {
        if (cJSON_IsString(cjsonObj))
        {
            return (int)strtoull(cjsonObj->valuestring, NULL, base);
        }
    }
    return 0;
}

uint8_t getNumberOfCpus(CPUInfo* cpus)
{
    uint8_t count = 0;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        if (cpus[i].clientAddr != 0x0)
        {
            count++;
        }
    }
    return count;
}

bool isCrashlogFlow(const char* triggerType)
{
    TriggerString split;
    int err;
    err = splitTriggerString(triggerType, &split);
    if (err == 0)
    {
        if (split.modifier[0] == '\0')
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr, "Trigger string %s has error!\n",
                        triggerType);
        return false;
    }
}

bool isPostResetFlow(const char* triggerType)
{
    return (getTriggerType(triggerType) == TRIGGER_POST_RESET);
}

STriggerType getTriggerType(const char* triggerType)
{
    int status = 0;
    char* p_substr = 0;
    if (triggerType == NULL)
    {
        return TRIGGER_UNKNOWN;
    }

    // Search for Post Reset
    status = strstr_s(
        (char*)triggerType, strnlen_s(triggerType, MAX_TRIGGER_STR_LEN),
        POST_RESET_TRIGGER, sizeof(POST_RESET_TRIGGER), &p_substr);
    if (status == EOK)
    {
        return TRIGGER_POST_RESET;
    }

    // Search for IERR
    status = strstr_s((char*)triggerType,
                      strnlen_s(triggerType, MAX_TRIGGER_STR_LEN), IERR_TRIGGER,
                      sizeof(IERR_TRIGGER), &p_substr);
    if (status == EOK)
    {
        return TRIGGER_IERR;
    }

    // Search for ERR2
    status = strstr_s((char*)triggerType,
                      strnlen_s(triggerType, MAX_TRIGGER_STR_LEN), ERR2_TRIGGER,
                      sizeof(ERR2_TRIGGER), &p_substr);
    if (status == EOK)
    {
        return TRIGGER_ERR2;
    }

    return TRIGGER_NODECODE;
}

bool isErrorFlow(const char* triggerType)
{
    return ((getTriggerType(triggerType) == TRIGGER_IERR) ||
            (getTriggerType(triggerType) == TRIGGER_ERR2));
}

bool getFlagFromSection(cJSON* inputFileBuf, int sectionIdx, char* flagName)
{
    /* Notes: if flag is not present, return true */

    cJSON* sections =
        cJSON_GetObjectItemCaseSensitive(inputFileBuf, "Sections");
    cJSON* obj = cJSON_GetArrayItem(sections, sectionIdx);
    if (obj == NULL)
    {
        return true;
    }
    cJSON* flag = cJSON_GetObjectItemCaseSensitive(obj->child, flagName);
    if (flag == NULL || cJSON_IsTrue(flag))
    {
        return true;
    }
    else
    {
        return false;
    }
}

cJSON* getJsonFlagFromSection(cJSON* inputFileBuf, int sectionIdx,
                              char* flagName)
{
    cJSON* sections =
        cJSON_GetObjectItemCaseSensitive(inputFileBuf, "Sections");
    cJSON* obj = cJSON_GetArrayItem(sections, sectionIdx);
    if (obj == NULL)
    {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive(obj->child, flagName);
}

/*  Notes: crashdump will signal "CrashdumpFail" only when are get triggered
    by "ERR2/IERR" and we have reported failures otherwise success is reported
*/
bool isCrashDumpFail(int sectionFailCount, const char* triggerType)
{
    // Don't fail if trigger is "PostReset"
    if (isPostResetFlow(triggerType))
    {
        return false;
    }
    // Fail for error triggers if we have any reported failures
    else if (isErrorFlow(triggerType) && sectionFailCount > 0)
    {
        return true;
    }
    // Don't fail; no reported failures
    else
    {
        return false;
    }
}

acdStatus fillDimmMasks(CPUInfo* cpuInfo)
{
    cJSON* dimmMasks = cJSON_GetObjectItemCaseSensitive(
        getNewCrashDataSection(cpuInfo[0].inputFile.bufferPtr, "NVD"),
        "dimm_masks");

    for (size_t i = 0; i < MAX_CPUS; i++)
    {
        CPUInfo* cpu = &cpuInfo[i];
        cpu->dimmMask = 0x0;
    }

    if (dimmMasks != NULL)
    {
        int count = getNumberOfCpus(cpuInfo);
        if (count == cJSON_GetArraySize(dimmMasks))
        {
            for (int i = 0; i < cJSON_GetArraySize(dimmMasks); i++)
            {
                cJSON* subitem = cJSON_GetArrayItem(dimmMasks, i);
                cpuInfo[i].dimmMask = strtoull(subitem->valuestring, NULL, 16);
            }
        }
    }

    return ACD_SUCCESS;
}

bool isCPUGNRD(CPUInfo* cpuInfo)
{
    bool hasGNRD = false;
    for (size_t i = 0; i < MAX_CPUS; i++)
    {
        hasGNRD |= (cpuInfo[i].model == cd_gnrd) ? true : false;
    }
    return hasGNRD;
}

int splitTriggerString(const char* input, TriggerString* result)
{
    if (!input || !result)
    {
        return EINVAL;
    }

    char buffer[MAX_TRIGGER_STR_LEN];

    if (strnlen_s(input, MAX_TRIGGER_STR_LEN) >= MAX_TRIGGER_STR_LEN)
    {
        return ERANGE;
    }

    strcpy_s(buffer, sizeof(buffer), input);

    char* nextToken = NULL;
    size_t len = strnlen_s(buffer, MAX_TRIGGER_STR_LEN);
    char* token = strtok_s(buffer, &len, ".", &nextToken);

    if (token)
    {
        strcpy_s(result->trigger, sizeof(result->trigger), token);
    }
    else
    {
        strcpy_s(result->trigger, sizeof(result->trigger), input);
        result->modifier[0] = '\0';
        return 0;
    }

    token = strtok_s(NULL, &len, ".", &nextToken);
    if (token)
    {
        strcpy_s(result->modifier, sizeof(result->modifier), token);
    }
    else
    {
        result->modifier[0] = '\0';
    }

    return 0;
}

void concatStrings(char* buffer, size_t bufferSize, const char* str1,
                   const char* str2)
{
    size_t len1 = strnlen_s(str1, bufferSize);
    size_t len2 = strnlen_s(str2, bufferSize);

    if (len1 + len2 < bufferSize)
    {
        strcpy_s(buffer, bufferSize, str1);
        strcat_s(buffer, bufferSize - len1, str2);
    }
    else
    {
        CRASHDUMP_PRINT(ERR, stderr,
                        "Buffer size is too small for concatenation.\n");
    }
}

bool isSectionNotBigOrAtomCore(cJSON* recordType)
{
    if (recordType != NULL)
    {
        int mismatchbigcore = 1;
        int mismatchatomcore = 1;
        char recordTypeStr[NAME_STR_LEN] = {0};

        cd_snprintf_s(recordTypeStr, NAME_STR_LEN, "0x0%x",
                      RECORD_TYPE_CORECRASHLOG);
        strcmp_s(recordTypeStr, strnlen_s(recordTypeStr, sizeof(recordTypeStr)),
                 recordType->valuestring, &mismatchbigcore);

        cd_snprintf_s(recordTypeStr, NAME_STR_LEN, "0x0%x",
                      RECORD_TYPE_ATOM_CORECRASHLOG);
        strcmp_s(recordTypeStr, strnlen_s(recordTypeStr, sizeof(recordTypeStr)),
                 recordType->valuestring, &mismatchatomcore);

        if (mismatchbigcore != 0 && mismatchatomcore != 0)
        {
            return true;
        }
    }
    return false;
}

cJSON* getFirstBigOrAtomCoreFromSocket(CPUInfo* cpuInfo, cJSON* parent)
{
    cJSON* bigOrAtomCore = NULL;
    if (!cpuInfo->dieMaskInfo.dieMaskSupported)
    {
        return parent;
    }

    // Only for GNR and SRF scenario where we have computes
    // The code bellow will use the dieMask to seach for the first compute
    //  that is active and present in the output.
    int logicComputeNum = 0;
    cJSON* compute = NULL;
    // Go through all possible dies according to maxDies
    for (uint16_t die = 0; die < cpuInfo->dieMaskInfo.compute.maxDies; die++)
    {
        // If die not in mask
        if (!CHECK_BIT(cpuInfo->dieMaskInfo.compute.effectiveMask, die))
        {
            // Go to next die
            continue;
        }
        // If die not discovered, increment logicComputeNum
        if (!CHECK_BIT(cpuInfo->crashdumpDiscoveryMask, logicComputeNum))
        {
            logicComputeNum++; // logicComputeNum stores logic num of compute
            continue;
        }

        // Form compute[logicComputeNum] string
        char computeStr[NAME_STR_LEN];
        cd_snprintf_s(computeStr, sizeof(computeStr), "compute%d",
                      logicComputeNum);
        // If compute[logicComputeNum] exists
        if ((compute = cJSON_GetObjectItemCaseSensitive(parent, computeStr)) ==
            NULL)
        {
            // We've found the first compute, let's exit the loop
            break;
        }
    }

    // If compute is NULL don't search for core within NULL
    if (compute == NULL)
    {
        return NULL;
    }

    // Here we use the compute found in the previous lines to get
    //  the big or atom core
    if (cpuInfo->coreType == ACD_ATOM_CORE)
    {
        bigOrAtomCore = cJSON_GetObjectItemCaseSensitive(compute, "atom_core");
    }
    else
    {
        bigOrAtomCore = cJSON_GetObjectItemCaseSensitive(compute, "big_core");
    }

    return bigOrAtomCore;
}

acdStatus CompressFile(compressOptions options)
{
    pid_t pid;

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return ACD_FAILURE;
    }

    if (pid == 0)
    { /* Child process */

        // Replace the process image with gzip
        if (options.keepOriginal)
        {
            char* args[] = {"gzip", "-k", "-f", options.inputFilePath, NULL};
            execvp(args[0], args);
        }
        else
        {
            char* args[] = {"gzip", "-f", options.inputFilePath, NULL};
            execvp(args[0], args);
        }

        // execvp will only return if an error occurred.
        CRASHDUMP_PRINT(ERR, stderr,
                        "An error occurred while compressing the file\n");
        return ACD_FAILURE;
    }
    else
    { /* Parent process */

        // Wait for the child process to finish
        wait(NULL);
    }
    return ACD_SUCCESS;
}

uint8_t getMaxNumSocketsForBafi(cJSON* inputFile)
{
    cJSON* numOfSockets =
        cJSON_GetObjectItemCaseSensitive(inputFile, "MaxSocketsForBafi");
    if (numOfSockets != NULL)
    {
        return (uint8_t)numOfSockets->valueint;
    }
    else
    {
        return MAX_SOCKETS_FOR_BAFI;
    }
}

bool isBAFIEnabled(cJSON* inputFile, bool isTelemetry)
{
    bool summaryEnable = true;
    if (isTelemetry)
    {
        summaryEnable = false;
    }

    bool isEnabled =
        readInputFileFlag(inputFile, summaryEnable, SUMMARY_ENABLE_IN_FLAG);
    return isEnabled;
}

bool isTriageEnabled(cJSON* inputFile, bool isTelemetry)
{
    bool triageEnable = true;
    if (isTelemetry)
    {
        triageEnable = false;
    }
    bool isEnabled =
        readInputFileFlag(inputFile, triageEnable, TRIAGE_ENABLE_IN_FLAG);
    return isEnabled;
}

#define MAX_LINE_LEN 256
SystemUsageInfo getCurrentAvailableMemory()
{

    SystemUsageInfo info = {0, false};
    FILE* file = fopen("/proc/meminfo", "r");
    if (file == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to open /proc/meminfo\n");
        return info;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf_s(line, "MemAvailable: %" PRIu64 " kB",
                     &info.memoryAvailable) == 1)
        {
            info.isSuccess = true;
            break;
        }
    }

    if (ferror(file))
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to read /proc/meminfo\n");
        info.isSuccess = false;
    }
    else if (!info.isSuccess)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to parse MemAvailable\n");
    }

    fclose(file);
    return info;
}

AppUsageInfo getCurrentAppMemoryUsage()
{
    AppUsageInfo info = {0, 0, false};
    char filename[MAX_LINE_LEN];
    sprintf_s(filename, MAX_LINE_LEN, "/proc/%d/status", getpid());

    FILE* file = fopen(filename, "r");
    if (file == NULL)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to open %s\n", filename);
        return info;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf_s(line, "VmPeak: %" PRIu64 " kB", &info.memoryPeak) == 1)
        {
            info.isSuccess = true;
        }
        else if (sscanf_s(line, "VmSize: %" PRIu64 " kB", &info.memoryUsage) ==
                 1)
        {
            info.isSuccess = true;
        }
    }

    if (ferror(file))
    {
        CRASHDUMP_PRINT(ERR, stderr, "Failed to read %s\n", filename);
        info.isSuccess = false;
    }

    fclose(file);
    return info;
}
cJSON* getSectionObj(cJSON* inputFile, int sectionIndex)
{
    cJSON* sectionsObj =
        cJSON_GetObjectItemCaseSensitive(inputFile, "Sections");
    cJSON* currentSectionObj = cJSON_GetArrayItem(sectionsObj, sectionIndex);
    return currentSectionObj;
}
char* getSectionName(cJSON* inputFile, int sectionIndex)
{
    char* sectionName;
    cJSON* currentSectionObj = getSectionObj(inputFile, sectionIndex);
    if (currentSectionObj != NULL)
    {
        sectionName = currentSectionObj->child->string;
        return sectionName;
    }
    else
    {
        return NULL;
    }
}
