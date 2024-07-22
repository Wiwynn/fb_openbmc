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
#include "test_utils.hpp"

#include <regex.h>
#include <stdio.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "safe_str_lib.h"

bool enableOutputDebugFile = false;
bool enableDebug = false;
void Logging(cJSON* cjson, char* description)
{
    if (enableDebug)
    {
        char* jsonStr = NULL;
        if (cjson != NULL)
        {
            jsonStr = cJSON_Print(cjson);
            printf("%s%s\n", description, jsonStr);
        }
        else
        {
            printf("%s(NULL)\n", description);
        }
        free(jsonStr);
    }
}

bool searchJson(cJSON* cjson, const char* regexStr)
{
    regex_t regex;
    if (regcomp(&regex, regexStr, REG_EXTENDED))
    {
        printf("Error compiling regex\n");
        return false;
    }

    cJSON* log_item = NULL;
    cJSON_ArrayForEach(log_item, cjson)
    {
        if (cJSON_IsString(log_item))
        {
            if (regexec(&regex, log_item->valuestring, 0, NULL, 0) == 0)
            {
                printf("Match found in %s: %s\n", log_item->string,
                       log_item->valuestring);
                return true;
            }
        }
    }

    regfree(&regex);
    return false;
}

char* readTestFile(char* filename)
{
    char* buffer = NULL;
    FILE* fp = fopen(filename, "r");
    if (fp != NULL)
    {
        uint64_t length = 0;
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        buffer = (char*)calloc(length, sizeof(char));
        if (buffer)
        {
            size_t result = 0;
            result = fread(buffer, 1, length, fp);
            if (result != length)
            {
                printf("Size Error!\n");
            }
        }
        fclose(fp);
    }
    else
    {
        printf("Fail to open %s\n", filename);
    }
    return buffer;
}

char* removeQuotes(char* str)
{
    char* ptr = str;
    ptr++;
    ptr[strnlen_s(ptr, sizeof(ptr)) - 1] = 0;
    return ptr;
}

cJSON* getJsonObjectFromPath(const char* path, cJSON* root)
{
    cJSON* currentObject = root;
    char* currentPath = strdup(path);
    char* token;

    while ((token = strsep(&currentPath, "/")) != NULL)
    {
        int mismatch = 1;
        strcmp_s(token, 256, "", &mismatch);
        if (mismatch == 1)
        {
            continue;
        }

        if (cJSON_IsArray(currentObject))
        {
            int index = atoi(token);
            currentObject = cJSON_GetArrayItem(currentObject, index);
        }
        else
        {
            currentObject = cJSON_GetObjectItem(currentObject, token);
        }

        if (currentObject == NULL)
        {
            break;
        }
    }

    return currentObject;
}

void outputDebugFile(cJSON* root, const char* fileName, bool verbose)
{
    if (enableOutputDebugFile)
    {
        // Output to unique tmp directory via username; Erase "_" at beginning
        // of cmake variable
        std::string userName = std::string(TMP_DIR_POSTFIX).erase(0, 1);
        std::filesystem::path fullPath =
            std::string("/tmp/") + userName + "_" + fileName;

        // Remove old file
        std::filesystem::remove(fullPath);

        // Output
        debugcJSON(root, fullPath.c_str(), verbose);
    }
}

void debugcJSON(cJSON* root, const char* savePath, bool verbose)
{
    char* jsonStr = NULL;
    if (root != NULL)
    {
        jsonStr = cJSON_Print(root);
        if (verbose)
        {
            printf("%s\n", jsonStr);
        }
        if (jsonStr != NULL)
        {
            if (savePath != NULL)
            {
                FILE* fp = fopen(savePath, "w");
                fprintf(fp, "%s", jsonStr);
                fclose(fp);
            }
        }
    }
    else
    {
        printf("cJSON is null!\n");
    }
    free(jsonStr);
}

int countSetBits(uint8_t n)
{
    int count = 0;
    while (n)
    {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

int totalSetBits(uint8_t* array, int length)
{
    int total = 0;
    for (int i = 0; i < length; i++)
    {
        total += countSetBits(array[i]);
    }
    return total;
}

void manageGetClientAddrs(TestCrashdump& crashdump, uint8_t noSucces)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(noSucces)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(MAX_CPUS - noSucces)
        .WillRepeatedly(DoAll(Return(PECI_CC_TIMEOUT)))
        .RetiresOnSaturation();
}

void manageGetClientAddrs_noSeq(TestCrashdump& crashdump, uint8_t noSucces)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(MAX_CPUS - noSucces)
        .WillRepeatedly(DoAll(Return(PECI_CC_TIMEOUT)))
        .RetiresOnSaturation();

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(noSucces)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetCPUID_1CPU(TestCrashdump& crashdump, CPUModel model)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(SetArgPointee<1>(model), SetArgPointee<2>(0x1),
                        SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)));
}

void manageGetDieMaskGNR_1CPU(TestCrashdump& crashdump, uint8_t* dieMask)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetDieMaskGNR_1CPU(TestCrashdump& crashdump, uint8_t* dieMask,
                              uint8_t cc)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                        SetArgPointee<5>(cc), Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetCoreMaskGNR_1CPU_1Compute(TestCrashdump& crashdump,
                                        uint8_t* coreMaskLow,
                                        uint8_t* coreMaskHigh)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetCoreMaskSPR_1CPU(TestCrashdump& crashdump, uint8_t* coreMaskLow,
                               uint8_t* coreMaskHigh)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetChaMaskGNR_1CPU_1Compute(TestCrashdump& crashdump,
                                       uint8_t* chaMask0Low,
                                       uint8_t* chaMask0High)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .WillOnce(DoAll(SetArrayArgument<8>(chaMask0Low, chaMask0Low + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMask0High, chaMask0High + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetChaMaskSPR_1CPU(TestCrashdump& crashdump, uint8_t* chaMask0Low,
                              uint8_t* chaMask0High)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(chaMask0Low, chaMask0Low + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(chaMask0High, chaMask0High + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageSavePeciWake_1CPU_1Domain(TestCrashdump& crashdump)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(0x1),
                        SetArgPointee<6>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageCheckPeciWake_1CPU_1Domain(TestCrashdump& crashdump)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(0x1),
                        SetArgPointee<6>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetPlatformIDs_1CPU(TestCrashdump& crashdump, uint8_t* platformID)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(SetArrayArgument<4>(platformID, platformID + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageGetUcodePatchVers_1CPU(TestCrashdump& crashdump, uint8_t* uCodeVer)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(SetArrayArgument<4>(uCodeVer, uCodeVer + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageMCA_1Module_withFail(TestCrashdump& crashdump,
                                int core_rdiamsr_nRegs)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        .WillOnce(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                        SetArgPointee<5>(0x83), Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();
}

void manageBusEnum(TestCrashdump& crashdump, int8_t cpuCount,
                   int32_t busValid32, int32_t busPostEnum32)
{
    static uint8_t busValid[4];
    for (int i = 0; i < 4; i++)
    {
        busValid[i] = (busValid32 >> (i * 8)) & 0xFF;
    }

    static uint8_t busPostEnum[4];
    for (int i = 0; i < 4; i++)
    {
        busPostEnum[i] = (busPostEnum32 >> (i * 8)) & 0xFF;
    }

    for (int i = 0; i < cpuCount; i++)
    {
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(busValid, busValid + 4),
                            SetArgPointee<9>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(busPostEnum, busPostEnum + 4),
                            SetArgPointee<9>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }
}

void managePHY2LOG(TestCrashdump& crashdump, int8_t cpuCount,
                   int8_t computeCount, int8_t coreCount)
{
    // Loop as a pair
    for (int i = 0; i < (cpuCount * computeCount * coreCount / 4); i++)
    {
        EXPECT_CALL(*crashdump.libPeciMock, peci_WrEndPointConfigMmio_dom)
            .WillOnce(Return(PECI_CC_SUCCESS))
            .RetiresOnSaturation();

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio_dom)
            .WillOnce(DoAll(SetArgPointee<10>(i),
                            SetArgPointee<11>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }
}

void managePHY2LOG_CHA(TestCrashdump& crashdump, int8_t cpuCount,
                       int8_t computeCount, int8_t coreCount)
{
    // Calculate the total number of iterations for 1D array
    int totalIterations = cpuCount * computeCount * coreCount / 4;

    // Create a static vector to hold all the chaResponse arrays
    static std::vector<std::vector<uint8_t>> chaResponses;

    // Pre-allocate the chaResponse arrays
    for (int i = 0; i < totalIterations; i++)
    {
        // CHA will have unique values across all computes
        chaResponses.push_back({static_cast<uint8_t>(i), 0xFF, 0xFF, 0xFF});
    }

    // Loop as a pair
    for (int i = 0; i < totalIterations; i++)
    {
        // Obtain a pointer to the current chaResponse array
        uint8_t* chaResponsePtr = chaResponses[i].data();

        EXPECT_CALL(*crashdump.libPeciMock, peci_WrEndPointConfigMmio_dom)
            .WillOnce(Return(PECI_CC_SUCCESS))
            .RetiresOnSaturation();

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio_dom)
            .WillOnce(
                DoAll(SetArrayArgument<10>(chaResponsePtr, chaResponsePtr + 4),
                      SetArgPointee<11>(PECI_DEV_CC_SUCCESS),
                      Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }
}