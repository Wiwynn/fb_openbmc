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

#pragma once
#include "./mock/test_crashdump.hpp"

#include <cjson/cJSON.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <filesystem>

#define NTIME(n) for (int i = 0; i < n; i++)

// uncomment to enable debug print out
// #define DEBUG_FLAG
#ifdef DEBUG_FLAG
inline void DEBUG_PRINT(cJSON* root, cJSON* expected)
{
    char* jsonStr = NULL;
    jsonStr = cJSON_Print(root);
    printf("%s\n", jsonStr);
    jsonStr = cJSON_Print(expected);
    printf("%s\n", jsonStr);
};

inline void DEBUG_CC_PRINT(uint8_t cc, uint64_t val)
{
    printf("cc:0x%x val:0x%" PRIx64 "\n", cc, val);
};
#endif
char* readTestFile(char* filename);
char* removeQuotes(char* str);
extern bool enableDebug;
void Logging(cJSON* cjson, char* description);
cJSON* getJsonObjectFromPath(const char* path, cJSON* root);
void debugcJSON(cJSON* root, const char* savePath, bool verbose);
void outputDebugFile(cJSON* root, const char* fileName, bool verbose);
int totalSetBits(uint8_t* array, int length);

// Expectations helpers
void manageGetClientAddrs(TestCrashdump& crashdump, uint8_t noSucces);
void manageGetClientAddrs_noSeq(TestCrashdump& crashdump, uint8_t noSucces);
void manageGetCPUID_1CPU(TestCrashdump& crashdump, CPUModel model);
void manageGetPlatformIDs_1CPU(TestCrashdump& crashdump, uint8_t* platformID);
void manageGetUcodePatchVers_1CPU(TestCrashdump& crashdump, uint8_t* uCodeVer);
void manageGetDieMaskGNR_1CPU(TestCrashdump& crashdump, uint8_t* dieMask);
void manageGetDieMaskGNR_1CPU(TestCrashdump& crashdump, uint8_t* dieMask,
                              uint8_t cc);
void manageGetCoreMaskGNR_1CPU_1Compute(TestCrashdump& crashdump,
                                        uint8_t* coreMaskLow,
                                        uint8_t* coreMaskHigh);
void manageGetCoreMaskSPR_1CPU(TestCrashdump& crashdump, uint8_t* coreMaskLow,
                               uint8_t* coreMaskHigh);
void manageGetChaMaskGNR_1CPU_1Compute(TestCrashdump& crashdump,
                                       uint8_t* chaMask0Low,
                                       uint8_t* chaMask0High);
void manageGetChaMaskSPR_1CPU(TestCrashdump& crashdump, uint8_t* chaMask0Low,
                              uint8_t* chaMask0High);
void managePHY2LOG(TestCrashdump& crashdump, int8_t cpuCount,
                   int8_t computeCount, int8_t coreCount);
void managePHY2LOG_CHA(TestCrashdump& crashdump, int8_t cpuCount,
                       int8_t computeCount, int8_t coreCount);
void manageBusEnum(TestCrashdump& crashdump, int8_t cpuCount,
                   int32_t busValid32, int32_t busPostEnum32);
void manageSavePeciWake_1CPU_1Domain(TestCrashdump& crashdump);
void manageCheckPeciWake_1CPU_1Domain(TestCrashdump& crashdump);
void manageMCA_1Module_withFail(TestCrashdump& crashdump,
                                int core_rdiamsr_nRegs);

bool searchJson(cJSON* cjson, const char* regexStr);
