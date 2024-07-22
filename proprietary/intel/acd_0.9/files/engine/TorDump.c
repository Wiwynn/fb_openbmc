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

#include "TorDump.h"

#include "utils.h"

/******************************************************************************
 *
 *   torDumpJsonSPR
 *
 *   This function formats the TOR dump into a JSON object
 *
 ******************************************************************************/
static void torDumpJsonSPR(uint32_t u32Cha, uint32_t u32TorIndex,
                           uint32_t u32TorSubIndex, uint32_t u32PayloadBytes,
                           uint8_t* pu8TorCrashdumpData, cJSON* pJsonChild,
                           bool bInvalid, uint8_t cc, int ret, bool skipCha,
                           uint8_t chaOffset)
{
    cJSON* channel;
    cJSON* tor;
    char jsonItemName[TD_JSON_STRING_LEN];
    char jsonItemString[TD_JSON_STRING_LEN];
    char jsonErrorString[TD_JSON_STRING_LEN];

    // Add the channel number item to the TOR dump JSON structure only if it
    // doesn't already exist
    cd_snprintf_s(jsonItemName, TD_JSON_STRING_LEN, TD_JSON_CHA_NAME,
                  u32Cha + chaOffset);
    if ((channel = cJSON_GetObjectItemCaseSensitive(pJsonChild,
                                                    jsonItemName)) == NULL)
    {
        cJSON_AddItemToObject(pJsonChild, jsonItemName,
                              channel = cJSON_CreateObject());
    }

    // Add the TOR Index item to the TOR dump JSON structure only if it
    // doesn't already exist
    cd_snprintf_s(jsonItemName, TD_JSON_STRING_LEN, TD_JSON_TOR_NAME,
                  u32TorIndex);
    if ((tor = cJSON_GetObjectItemCaseSensitive(channel, jsonItemName)) == NULL)
    {
        cJSON_AddItemToObject(channel, jsonItemName,
                              tor = cJSON_CreateObject());
    }
    // Add the SubIndex data to the TOR dump JSON structure
    cd_snprintf_s(jsonItemName, TD_JSON_STRING_LEN, TD_JSON_SUBINDEX_NAME,
                  u32TorSubIndex);
    if (skipCha)
    {
        cd_snprintf_s(jsonItemString, TD_JSON_STRING_LEN, TD_NA);
        cJSON_AddStringToObject(tor, jsonItemName, jsonItemString);
        return;
    }
    if (bInvalid)
    {
        cd_snprintf_s(jsonItemString, TD_JSON_STRING_LEN, TD_FIXED_DATA_CC_RC,
                      cc, ret);
        cJSON_AddStringToObject(tor, jsonItemName, jsonItemString);
        return;
    }
    else
    {
        cd_snprintf_s(jsonItemString, sizeof(jsonItemString), "0x0");
        bool leading = true;
        char* ptr = &jsonItemString[2];

        for (int i = u32PayloadBytes - 1; i >= 0; i--)
        {
            // exclude any leading zeros per schema
            if (leading && pu8TorCrashdumpData[i] == 0)
            {
                continue;
            }
            leading = false;

            ptr += cd_snprintf_s(ptr, u32PayloadBytes, "%02x",
                                 pu8TorCrashdumpData[i]);
        }
        if (PECI_CC_UA(cc))
        {
            cd_snprintf_s(jsonErrorString, TD_JSON_STRING_LEN, TD_DATA_CC_RC,
                          cc, ret);
            strcat_s(jsonItemString, TD_JSON_STRING_LEN, jsonErrorString);
            cJSON_AddStringToObject(tor, jsonItemName, jsonItemString);
            return;
        }
    }
    cJSON_AddStringToObject(tor, jsonItemName, jsonItemString);
}

int logTorSectionSPR(CPUInfo* cpuInfo, cJSON* outputNode,
                     uint8_t u8PayloadBytes, uint8_t domainID,
                     uint8_t logicalDieNum, RunTimeInfo runTimeInfo,
                     LoggerStruct* loggerStruct)
{
    (void)domainID;
    (void)logicalDieNum;
    (void)runTimeInfo;
    (void)loggerStruct;
    int ret = 0;
    uint8_t cc = 0;

    bool skipCha = false;
    bool skipFromInputFile = getSkipFromNewInputFile(cpuInfo, "TOR");

    // Crashdump Get Frames
    for (size_t cha = 0; cha < cpuInfo->chaCount; cha++)
    {
        for (uint32_t u32TorIndex = 0; u32TorIndex < TD_TORS_PER_CHA;
             u32TorIndex++)
        {
            for (uint32_t u32TorSubIndex = 0;
                 u32TorSubIndex < TD_SUBINDEX_PER_TOR; u32TorSubIndex++)
            {
                uint8_t* pu8TorCrashdumpData =
                    (uint8_t*)(calloc(u8PayloadBytes, sizeof(uint8_t)));
                bool bInvalid = false;
                if (pu8TorCrashdumpData == NULL)
                {
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "Error allocating memory (size:%u)\n",
                                    u8PayloadBytes);
                    return ACD_ALLOCATE_FAILURE;
                }
                if (!skipCha)
                {
                    ret = peci_CrashDump_GetFrame(
                        cpuInfo->clientAddr, PECI_CRASHDUMP_TOR, cha,
                        (u32TorIndex | (u32TorSubIndex << 8)), u8PayloadBytes,
                        pu8TorCrashdumpData, &cc);

                    if (ret != PECI_CC_SUCCESS)
                    {
                        bInvalid = true;
                        CRASHDUMP_PRINT(ERR, stderr,
                                        "Error (%d) during GetFrame"
                                        "(cha:%d index:%d sub-index:%d)\n",
                                        ret, (int)cha, u32TorIndex,
                                        u32TorSubIndex);
                    }
                }
                torDumpJsonSPR(cha, u32TorIndex, u32TorSubIndex, u8PayloadBytes,
                               pu8TorCrashdumpData, outputNode, bInvalid, cc,
                               ret, skipCha, 0);
                free(pu8TorCrashdumpData);
                if (PECI_CC_UA(cc) && !skipCha)
                {
                    if (skipFromInputFile)
                    {
                        skipCha = true;
                    }
                }
            }
        }
        skipCha = false;
    }
    return ACD_SUCCESS;
}

/******************************************************************************
 *
 *   torDumpDelimitedValues
 *
 *   This function formats the TOR dump into a large string of semicolon
 *   separated values
 *
 ******************************************************************************/
static void torDumpDelimitedValues(LoggerStruct* loggerStruct, char* chaKey,
                                   uint32_t u32PayloadBytes,
                                   uint8_t* pu8TorCrashdumpData, bool bInvalid,
                                   uint8_t cc, int ret, bool skipCha)
{
    char jsonItemString[TD_JSON_STRING_LEN];
    char jsonErrorString[TD_JSON_STRING_LEN];

    if (skipCha)
    {
        cd_snprintf_s(jsonItemString, TD_JSON_STRING_LEN, TD_NA);
        LogDelimitedValue(loggerStruct, chaKey, jsonItemString);
        return;
    }
    if (bInvalid)
    {
        cd_snprintf_s(jsonItemString, TD_JSON_STRING_LEN, TD_FIXED_DATA_CC_RC,
                      cc, ret);
        LogDelimitedValue(loggerStruct, chaKey, jsonItemString);
        return;
    }
    else
    {
        cd_snprintf_s(jsonItemString, sizeof(jsonItemString), "0x0");
        bool leading = true;
        char* ptr = &jsonItemString[2];

        for (int i = u32PayloadBytes - 1; i >= 0; i--)
        {
            // exclude any leading zeros per schema
            if (leading && pu8TorCrashdumpData[i] == 0)
            {
                continue;
            }
            leading = false;

            ptr += cd_snprintf_s(ptr, u32PayloadBytes, "%02x",
                                 pu8TorCrashdumpData[i]);
        }
        if (PECI_CC_UA(cc))
        {
            cd_snprintf_s(jsonErrorString, TD_JSON_STRING_LEN, TD_DATA_CC_RC,
                          cc, ret);
            strcat_s(jsonItemString, TD_JSON_STRING_LEN, jsonErrorString);
            LogDelimitedValue(loggerStruct, chaKey, jsonItemString);
            return;
        }
    }
    LogDelimitedValue(loggerStruct, chaKey, jsonItemString);
}

int logTorSectionGNR(CPUInfo* cpuInfo, cJSON* outputNode,
                     uint8_t u8PayloadBytes, uint8_t domainID,
                     uint8_t logicalDieNum, RunTimeInfo runTimeInfo,
                     LoggerStruct* loggerStruct)
{
    int ret = 0;
    uint8_t cc = 0;
    char jsonChaKey[TD_JSON_STRING_LEN];

    bool skipCha = false;
    bool skipFromInputFile = getSkipFromNewInputFile(cpuInfo, "TOR");

    // Total of 60 instances of CHAs in domain 9 if available
    int domain9CHALow = 0;
    int domain9CHAHigh = 59;

    // Total of 60 instances of CHAs in domain 10 if available
    int domain10CHALow = 0;
    int domain10CHAHigh = 59;

    // Total of 8 instances of CHAs in domain 11 if available
    int domain11CHALow = 0;
    int domain11CHAHigh = 7;
    bool skipDomain = false;

    cJSON* torSection =
        getNewCrashDataSection(cpuInfo->inputFile.bufferPtr, "TOR");
    getRangeValues(torSection, "Domain9CHA", &domain9CHALow, &domain9CHAHigh);
    getRangeValues(torSection, "Domain10CHA", &domain10CHALow,
                   &domain10CHAHigh);
    getRangeValues(torSection, "Domain11CHA", &domain11CHALow,
                   &domain11CHAHigh);
    int domain9CHACount = domain9CHAHigh - domain9CHALow + 1;
    int domain10CHACount = domain10CHAHigh - domain10CHALow + 1;

    uint8_t startInstanceNum = 0;
    uint8_t endInstanceNum = 0;
    uint8_t chaOffset = 0;

    if (domainID == 9)
    {
        startInstanceNum = domain9CHALow;
        endInstanceNum = domain9CHAHigh;
        chaOffset = 0;
        if (cpuInfo->chaCount < domain9CHACount)
        {
            endInstanceNum = cpuInfo->chaCount - 1;
        }
    }

    if (domainID == 10 && cpuInfo->chaCount > domain9CHACount)
    {
        startInstanceNum = domain10CHALow;
        endInstanceNum = domain10CHAHigh;
        chaOffset = domain9CHACount;
        if (cpuInfo->chaCount - domain9CHACount < domain9CHACount)
        {
            endInstanceNum = cpuInfo->chaCount - (domain9CHACount + 1);
        }
    }
    else if (domainID == 10 && cpuInfo->chaCount < domain9CHACount)
    {
        skipDomain = true;
    }

    if (domainID == 11 &&
        cpuInfo->chaCount > (domain9CHACount + domain10CHACount))
    {
        startInstanceNum = domain11CHALow;
        endInstanceNum =
            cpuInfo->chaCount - (domain9CHACount + domain10CHACount + 1);
        chaOffset = domain9CHACount + domain10CHACount;
    }
    else if (domainID == 11 &&
             cpuInfo->chaCount <= (domain9CHACount + domain10CHACount))
    {
        skipDomain = true;
    }

    if (skipDomain)
    {
        return ACD_SUCCESS;
    }

    bool sectionTimeout = false;
    bool globalTimeout = false;
    // Crashdump Get Frames
    for (size_t cha = startInstanceNum; cha <= endInstanceNum; cha++)
    {
        LogConfig(loggerStruct, KEY_DELIMITED_VALUES);
        // Build stringify key for cha
        cd_snprintf_s(jsonChaKey, TD_JSON_STRING_LEN, TD_JSON_CHA_NAME,
                      (int)(cha + chaOffset));

        for (uint32_t u32TorIndex = 0; u32TorIndex < TD_TORS_PER_CHA;
             u32TorIndex++)
        {
            for (uint32_t u32TorSubIndex = 0;
                 u32TorSubIndex < TD_SUBINDEX_PER_TOR; u32TorSubIndex++)
            {
                uint8_t* pu8TorCrashdumpData =
                    (uint8_t*)(calloc(u8PayloadBytes, sizeof(uint8_t)));
                bool bInvalid = false;
                if (pu8TorCrashdumpData == NULL)
                {
                    CRASHDUMP_PRINT(ERR, stderr,
                                    "Error allocating memory (size:%u)\n",
                                    u8PayloadBytes);
                    return ACD_ALLOCATE_FAILURE;
                }
                if (!skipCha)
                {
                    // Check timing
                    double globalRemainingTime = getTimeRemainingFromStart(
                        runTimeInfo.maxGlobalTime, runTimeInfo.globalRunTime);
                    double sectionRemainingTime = getTimeRemainingFromStart(
                        runTimeInfo.maxSectionTime, runTimeInfo.sectionRunTime);
                    if (globalRemainingTime <= 0)
                    {
                        globalTimeout = true;
                        skipCha = true;
                    }
                    if (sectionRemainingTime <= 0)
                    {
                        sectionTimeout = true;
                        skipCha = true;
                    }

                    if (!skipCha)
                    {
                        ret = peci_CrashDump_GetFrame_dom(
                            cpuInfo->clientAddr, domainID, PECI_CRASHDUMP_TOR,
                            cha, (u32TorIndex | (u32TorSubIndex << 8)),
                            u8PayloadBytes, pu8TorCrashdumpData, &cc);

                        if (ret != PECI_CC_SUCCESS)
                        {
                            bInvalid = true;
                            CRASHDUMP_PRINT(ERR, stderr,
                                            "Error (%d) during GetFrame"
                                            "(cha:%d index:%d sub-index:%d)\n",
                                            ret, (int)cha, u32TorIndex,
                                            u32TorSubIndex);
                        }
                    }
                }
                torDumpDelimitedValues(loggerStruct, jsonChaKey, u8PayloadBytes,
                                       pu8TorCrashdumpData, bInvalid, cc, ret,
                                       skipCha);
                free(pu8TorCrashdumpData);
                if (PECI_CC_UA(cc) && !skipCha)
                {
                    if (skipFromInputFile)
                    {
                        skipCha = true;
                    }
                }
            }
        }
        skipCha = false;
        LogDumpDelimitedValues(outputNode, loggerStruct, TD_STRINGIFY_ELEMENTS);
    }

    if (globalTimeout)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Global Timeout while collecting TOR\n");
        return ACD_GLOBAL_TIMEOUT;
    }
    if (sectionTimeout)
    {
        CRASHDUMP_PRINT(ERR, stderr, "Section Timeout while collecting TOR\n");
        return ACD_SECTION_TIMEOUT;
    }

    return ACD_SUCCESS;
}

static const STorSectionVx sTorSectionVx[] = {
    {cd_spr, logTorSectionSPR}, {cd_sprhbm, logTorSectionSPR},
    {cd_gnr, logTorSectionGNR}, {cd_gnrd, logTorSectionGNR},
    {cd_emr, logTorSectionSPR}, {cd_srf, logTorSectionGNR}};

int logTorSection(CPUInfo* cpuInfo, cJSON* outputNode, uint8_t u8PayloadBytes,
                  uint8_t domainID, uint8_t logicalDieNum,
                  RunTimeInfo runTimeInfo, LoggerStruct* loggerStruct)
{
    if (outputNode == NULL)
    {
        return ACD_INVALID_OBJECT;
    }

    for (uint32_t i = 0; i < (sizeof(sTorSectionVx) / sizeof(STorSectionVx));
         i++)
    {
        if (cpuInfo->model == sTorSectionVx[i].cpuModel)
        {
            return sTorSectionVx[i].logTorSectionVx(
                cpuInfo, outputNode, u8PayloadBytes, domainID, logicalDieNum,
                runTimeInfo, loggerStruct);
        }
    }

    CRASHDUMP_PRINT(ERR, stderr, "Cannot find version for %s\n", __FUNCTION__);
    return ACD_FAILURE;
}
