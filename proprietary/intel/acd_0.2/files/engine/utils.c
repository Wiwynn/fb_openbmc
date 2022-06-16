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
    return ret;
}

static uint32_t setMask(uint32_t msb, uint32_t lsb)
{
    uint32_t range = (msb - lsb) + 1;
    uint32_t mask = ((1u << range) - 1);
    return mask;
}

void setFields(uint32_t* value, uint32_t msb, uint32_t lsb, uint32_t setVal)
{
    uint32_t mask = setMask(msb, lsb);
    setVal = setVal << lsb;
    *value &= ~(mask << lsb);
    *value |= setVal;
}

uint32_t getFields(uint32_t value, uint32_t msb, uint32_t lsb)
{
    uint32_t mask = setMask(msb, lsb);
    value = value >> lsb;
    return (mask & value);
}

uint32_t bitField(uint32_t offset, uint32_t size, uint32_t val)
{
    uint32_t mask = (1u << size) - 1;
    return (val & mask) << offset;
}

cJSON* readInputFile(const char* filename)
{
    char* buffer = NULL;
    cJSON* jsonBuf = NULL;
    long int length = 0;
    FILE* fp = fopen(filename, "r");
    size_t result = 0;

    if (fp == NULL)
    {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    if (length == -1L)
    {
        fclose(fp);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);
    buffer = (char*)calloc(length, sizeof(char));
    if (buffer)
    {
        result = fread(buffer, 1, length, fp);
        if ((int)result != length)
        {
            fprintf(stderr, "fread read %zu bytes, but length is %ld", result,
                    length);
            fclose(fp);
            FREE(buffer);
            return NULL;
        }
    }

    fclose(fp);
    // Convert and return cJSON object from buffer
    jsonBuf = cJSON_Parse(buffer);
    FREE(buffer);
    return jsonBuf;
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

bool isBigCoreRegVersionMatch(cJSON* root, uint32_t version)
{
    cJSON* child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);

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
                        strcmp_s(versionItem->valuestring,
                                 strnlen_s(jsonItemName, NAME_STR_LEN),
                                 jsonItemName, &mismatch);
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

cJSON* getValidBigCoreMap(cJSON* decodeArray, char* version)
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
                    return mapItem;
                }
            }
        }
    }

    return NULL;
}

cJSON* getCrashDataSectionBigCoreRegList(cJSON* root, char* version)
{
    cJSON* child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);

    if (child != NULL)
    {
        cJSON* decodeObject = cJSON_GetObjectItemCaseSensitive(child, "Decode");

        if (decodeObject != NULL)
        {
            cJSON* mapObject = getValidBigCoreMap(decodeObject, version);
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

uint32_t getCrashDataSectionBigCoreSize(cJSON* root, char* version)
{
    uint32_t size = 0;
    cJSON* child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);

    if (child != NULL)
    {
        cJSON* decodeObject = cJSON_GetObjectItemCaseSensitive(child, "Decode");

        if (decodeObject != NULL)
        {
            cJSON* mapObject = getValidBigCoreMap(decodeObject, version);

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

void storeCrashDataSectionBigCoreSize(cJSON* root, char* version,
                                      uint32_t totalSize)
{
    cJSON* child = getNewCrashDataSection(root, BIG_CORE_SECTION_NAME);

    if (child != NULL)
    {
        char jsonItemName[NAME_STR_LEN] = {0};
        cd_snprintf_s(jsonItemName, NAME_STR_LEN, "0x%x", totalSize);
        cJSON_AddStringToObject(
            getValidBigCoreMap(
                cJSON_GetObjectItemCaseSensitive(child, "Decode"), version),
            "_size", jsonItemName);
    }
}

cJSON* selectAndReadInputFile(Model cpuModel, char** filename, bool isTelemetry)
{
    char cpuStr[CPU_STR_LEN] = {0};
    char nameStr[NAME_STR_LEN] = {0};

    switch (cpuModel)
    {
        case cd_icx:
        case cd_icx2:
        case cd_icxd:
            strcpy_s(cpuStr, sizeof("icx"), "icx");
            break;
        case cd_spr:
            strcpy_s(cpuStr, sizeof("spr"), "spr");
            break;
        case cd_sprhbm:
            strcpy_s(cpuStr, sizeof("sprhbm"), "sprhbm");
            break;
        default:
            CRASHDUMP_PRINT(ERR, stderr,
                            "Error selecting input file (CPUID 0x%x).\n",
                            cpuModel);
            return NULL;
    }

    char* override_file = OVERRIDE_INPUT_FILE;

    if (isTelemetry)
    {
        override_file = OVERRIDE_TELEMETRY_FILE;
    }

    cd_snprintf_s(nameStr, NAME_STR_LEN, override_file, cpuStr);

    if (access(nameStr, F_OK) != -1)
    {
        CRASHDUMP_PRINT(INFO, stderr, "Using override file - %s\n", nameStr);
    }
    else
    {
        char* default_file = DEFAULT_INPUT_FILE;
        if (isTelemetry)
        {
            default_file = DEFAULT_TELEMETRY_FILE;
        }
        cd_snprintf_s(nameStr, NAME_STR_LEN, default_file, cpuStr);
    }
    *filename = (char*)malloc(sizeof(nameStr));
    if (*filename == NULL)
    {
        return NULL;
    }
    strcpy_s(*filename, sizeof(nameStr), nameStr);

    return readInputFile(nameStr);
}

cJSON* getPeciAccessType(cJSON* root)
{
    if (root != NULL)
    {
        cJSON* child = cJSON_GetObjectItem(root, "crash_data");
        if (child != NULL)
        {
            cJSON* accessType = cJSON_GetObjectItem(child, "AccessMethod");
            if (accessType != NULL)
            {
                return accessType;
            }
        }
    }
    return NULL;
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
    cJSON* inputField = getNewCrashDataSectionObjectOneLevel(
        cpuInfo->inputFile.bufferPtr, BIG_CORE_SECTION_NAME,
        "MaxCollectionSec");

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
        cJSON_GetObjectItemCaseSensitive(torSection, "_skip_on_fail");

    if (skipIfFailRead != NULL)
    {
        return cJSON_IsTrue(skipIfFailRead);
    }

    return false;
}

void updateMcaRunTime(cJSON* root, struct timespec* start)
{
    char* key = "_time";
    cJSON* mcaUncoreTime = cJSON_GetObjectItemCaseSensitive(root, key);

    if (mcaUncoreTime != NULL)
    {
        char timeString[64];
        struct timespec finish = {};

        clock_gettime(CLOCK_MONOTONIC, &finish);
        uint64_t mcaCoreRunTimeInNs =
            tsToNanosecond(&finish) - tsToNanosecond(start);

        // Remove unit "s" from logged "_time"
        char* str = mcaUncoreTime->valuestring;
        str[strnlen_s(str, sizeof(str)) - 1] = '\0';

        // Calculate, replace "_time" from uncore MCA,
        // and log total MCA run time
        double macUncoreTimeInSec = atof(mcaUncoreTime->valuestring);
        double totalMcaRunTimeInSec =
            (double)mcaCoreRunTimeInNs / 1e9 + macUncoreTimeInSec;
        cd_snprintf_s(timeString, sizeof(timeString), "%.2fs",
                      totalMcaRunTimeInSec);
        cJSON_DeleteItemFromObjectCaseSensitive(root, key);
        cJSON_AddStringToObject(root, key, timeString);
    }

    clock_gettime(CLOCK_MONOTONIC, start);
}

uint8_t getBus(Model model, uint8_t u8index)
{
    if (model == cd_icx || model == cd_icx2 || model == cd_icxd)
    {
        if (sPciReg[u8index].u8Bus == 30)
        {
            return 13;
        }
        if (sPciReg[u8index].u8Bus == 31)
        {
            return 14;
        }
    }
    return sPciReg[u8index].u8Bus;
}

int getPciRegister(CPUInfo* cpuInfo, SRegRawData* sRegData, uint8_t u8index)
{
    int peci_fd = -1;
    int ret = 0;
    uint16_t u16Offset = 0;
    uint8_t u8Size = 0;
    uint8_t u8Bus = getBus(cpuInfo->model, u8index);
    ret = peci_Lock(&peci_fd, PECI_WAIT_FOREVER);
    if (ret != PECI_CC_SUCCESS)
    {
        sRegData->ret = ret;
        return ACD_FAILURE;
    }
    switch (sPciReg[u8index].u8Size)
    {
        case UT_REG_DWORD:
            ret = peci_RdEndPointConfigPciLocal_seq(
                cpuInfo->clientAddr, sPciReg[u8index].u8Seg, u8Bus,
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
                ret = peci_RdEndPointConfigPciLocal_seq(
                    cpuInfo->clientAddr, sPciReg[u8index].u8Seg, u8Bus,
                    sPciReg[u8index].u8Dev, sPciReg[u8index].u8Func, u16Offset,
                    u8Size, (uint8_t*)&sRegData->uValue.u32[u8Dword], peci_fd,
                    &sRegData->cc);
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

void adjustTotalTime(struct timespec* crashdumpTime)
{
    crashdumpTime->tv_sec = crashdumpTime->tv_sec + 1;
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

cJSON* getNVDSection(cJSON* root, const char* const section, bool* const enable)
{
    *enable = false;
    cJSON* child = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(root, "NVD"), section);

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

cJSON* getCrashlogExcludeList(cJSON* root)
{
    cJSON* crashlogSection = getNewCrashDataSection(root, "crashlog");

    if (crashlogSection != NULL)
    {
        cJSON* excludeAgentsList = cJSON_GetObjectItemCaseSensitive(
            crashlogSection, "ExcludeAgentIDs");
        return excludeAgentsList;
    }

    return NULL;
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
    cJSON* inputField = getNewCrashDataSectionObjectOneLevel(
        cpuInfo->inputFile.bufferPtr, BIG_CORE_SECTION_NAME,
        "MaxCollectionCores");

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
int fillInputFile(CPUInfo* cpuInfo, char* cSectionName, cJSON* pJsonChild,
                  InputFileInfo* inputFileInfo)
{
    cJSON* cpu = NULL;
    char jsonItemName[SI_JSON_STRING_LEN] = {0};

    if (inputFileInfo->unique)
    {
        cJSON_AddStringToObject(pJsonChild, cSectionName,
                                cpuInfo->inputFile.filenamePtr);
    }
    else
    {
        // For now, the CPU number is just the bottom nibble of the
        // PECI client ID
        int cpuNum = cpuInfo->clientAddr & 0xF;
        // Add the CPU number object if it doesn't already exist
        cd_snprintf_s(jsonItemName, SI_JSON_STRING_LEN, SI_JSON_SOCKET_NAME,
                      cpuNum);
        if ((cpu = cJSON_GetObjectItemCaseSensitive(pJsonChild,
                                                    jsonItemName)) == NULL)
        {
            cJSON_AddItemToObject(pJsonChild, jsonItemName,
                                  cpu = cJSON_CreateObject());
        }

        if (cpuInfo->inputFile.filenamePtr == NULL)
        {
            cJSON_AddStringToObject(cpu, cSectionName, MD_NA);
        }
        else
        {
            cJSON_AddStringToObject(cpu, cSectionName,
                                    cpuInfo->inputFile.filenamePtr);
        }
    }

    return ACD_SUCCESS;
}

/******************************************************************************
 *
 *   logResetDetected
 *
 *   This function logs the section which was being captured when reset occurred
 *
 ******************************************************************************/
int logResetDetected(cJSON* metadata, int cpuNum, int sectionName)
{
    char resetSection[SI_JSON_STRING_LEN];

    if (sectionName != DEFAULT_VALUE)
    {
        cd_snprintf_s(resetSection, SI_JSON_STRING_LEN, RESET_DETECTED_NAME,
                      cpuNum, sectionNames[sectionName].name);

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
 *   fillMeVersionJson
 *
 *   This function fills in the me_fw_ver JSON info
 *
 ******************************************************************************/
int fillMeVersion(char* cSectionName, cJSON* pJsonChild)
{
    // Fill in as N/A for now
    cJSON_AddStringToObject(pJsonChild, cSectionName, MD_NA);
    return ACD_SUCCESS;
    // char jsonItemString[SI_JSON_STRING_LEN];

    // cd_snprintf_s(jsonItemString, SI_JSON_STRING_LEN,
    // "%02d.%02x.%02x.%02x%x",
    // sSysInfoRawData->meFwDeviceId.u8FirmwareMajorVersion & 0x7f,
    // sSysInfoRawData->meFwDeviceId.u8B2AuxFwRevInfo & 0x0f,
    // sSysInfoRawData->meFwDeviceId.u8FirmwareMinorVersion,
    // sSysInfoRawData->meFwDeviceId.u8B1AuxFwRevInfo,
    // sSysInfoRawData->meFwDeviceId.u8B2AuxFwRevInfo >> 4);
    // cJSON_AddStringToObject(pJsonChild, cSectionName, jsonItemString);
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
