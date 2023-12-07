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
#include "../mock/test_crashdump.hpp"
#include "../test_utils.hpp"
#include "crashdump.hpp"

#include <fstream>
#include <iostream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

extern "C" {
}

using namespace ::testing;
using namespace crashdump;

// Notes: uncomment to enable debug
//#define DEBUG_FLAG

static void validateCrashlogVersionAndSize(cJSON* output,
                                           std::string expectedVersion,
                                           std::string expectedSize)
{
    cJSON* actual =
        cJSON_GetObjectItemCaseSensitive(output, "crashlog_version");
    ASSERT_NE(actual, nullptr);
    EXPECT_STREQ(actual->valuestring, expectedVersion.c_str());
    actual = cJSON_GetObjectItemCaseSensitive(output, "size");
    ASSERT_NE(actual, nullptr);
    EXPECT_STREQ(actual->valuestring, expectedSize.c_str());
}

static int getNumberOfRepetitions(cJSON* root, char* section, char* command)
{
    cJSON* sectionObj = getNewCrashDataSection(root, section);
    cJSON* peciCmds = cJSON_GetObjectItemCaseSensitive(sectionObj, "Commands");
    cJSON* commandObjs =
        cJSON_GetObjectItemCaseSensitive(peciCmds->child, command);

    int nRegs = cJSON_GetArraySize(commandObjs);
    return nRegs;
}

static int repsUncorePCI(cJSON* root)
{
    cJSON* sectionObj = getNewCrashDataSection(root, "Uncore_PCI");
    cJSON* peciCmds = cJSON_GetObjectItemCaseSensitive(sectionObj, "Commands");
    cJSON* commandObjs = cJSON_GetObjectItemCaseSensitive(
        peciCmds->child, "RdEndpointConfigPCILocal_dom");

    int n_4Byte_regs = 0;
    int n_8Byte_regs = 0;
    cJSON* peci_call = NULL;
    cJSON_ArrayForEach(peci_call, commandObjs)
    {
        unsigned char rx_len =
            cJSON_GetArrayItem(peci_call->child, 7)->valueint;

        switch (rx_len)
        {
            case sizeof(uint8_t):
            case sizeof(uint16_t):
            case sizeof(uint32_t):
                n_4Byte_regs++;
                break;
            case sizeof(uint64_t):
                n_8Byte_regs++;
                break;
        }
    }
    int total_reps = n_4Byte_regs + (2 * n_8Byte_regs);

    return total_reps;
}

TEST(CrashdumpTest, test_isPECIAvailable)
{
    TestCrashdump crashdump(cd_spr);
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

    EXPECT_TRUE(crashdump::isPECIAvailable());
}

TEST(CrashdumpTest, test_isPECIAvailable_NO_CPUS)
{
    TestCrashdump crashdump(cd_spr);
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillRepeatedly(DoAll(Return(PECI_CC_TIMEOUT)));
    EXPECT_FALSE(crashdump::isPECIAvailable());
}

TEST(CrashdumpTest, test_initCPUInfo)
{
    TestCrashdump crashdump(cd_spr);
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    initCPUInfo(cpus);
    EXPECT_EQ(cpus[0].cpuidRead.source, 2);
    EXPECT_EQ(cpus[0].coreMaskRead.source, 2);
    EXPECT_EQ(cpus[0].chaCountRead.source, 2);
    EXPECT_FALSE(cpus[0].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[0].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[0].chaCountRead.chaCountValid);
}

TEST(CrashdumpTest, test_getCPUData_Startup)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)0x000806F0), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArrayArgument<7>(chaMaskHigh, chaMaskHigh + 4), // cha mask 0
            SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(
            DoAll(SetArrayArgument<7>(chaMaskLow, chaMaskLow + 4), // cha mask 1
                  SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)));
    size_t chaExpectedValue = 13;
    uint64_t coreExpectedValue = 0x403020108070605;
    initCPUInfo(cpus);
    getCPUData(cpus, STARTUP);
    EXPECT_TRUE(cpus[0].cpuidRead.cpuidValid);
    EXPECT_EQ(cpus[0].cpuidRead.source, 1);
    EXPECT_EQ(cpus[0].coreMaskRead.source, 1);
    EXPECT_EQ(cpus[0].cpuidRead.cpuModel, spr);
    EXPECT_TRUE(cpus[0].coreMaskRead.coreMaskValid);
    EXPECT_TRUE(cpus[0].chaCountRead.chaCountValid);
    EXPECT_EQ(cpus[0].chaCountRead.source, 1);
    EXPECT_EQ(cpus[0].chaCount, chaExpectedValue);
    EXPECT_EQ(cpus[0].coreMask, coreExpectedValue);
}

TEST(CrashdumpTest, test_getCPUData_Event)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    size_t chaExpectedValue = 13;
    uint64_t coreExpectedValue = 0x403020108070605;
    initCPUInfo(cpus);
    cpus[0].cpuidRead.cpuidValid = true;
    cpus[0].chaCountRead.chaCountValid = true;
    cpus[0].coreMaskRead.coreMaskValid = true;
    cpus[0].cpuidRead.source = STARTUP;
    cpus[0].chaCountRead.source = STARTUP;
    cpus[0].coreMaskRead.source = STARTUP;
    cpus[0].cpuidRead.cpuModel = spr;
    cpus[0].chaCount = chaExpectedValue;
    cpus[0].coreMask = coreExpectedValue;
    getCPUData(cpus, EVENT);
    EXPECT_TRUE(cpus[0].cpuidRead.cpuidValid);
    EXPECT_TRUE(cpus[0].chaCountRead.chaCountValid);
    EXPECT_TRUE(cpus[0].coreMaskRead.coreMaskValid);
    EXPECT_EQ(cpus[0].cpuidRead.source, 1);
    EXPECT_EQ(cpus[0].chaCountRead.source, 1);
    EXPECT_EQ(cpus[0].coreMaskRead.source, 1);
    EXPECT_EQ(cpus[0].cpuidRead.cpuModel, spr);
    EXPECT_EQ(cpus[0].chaCount, chaExpectedValue);
    EXPECT_EQ(cpus[0].coreMask, coreExpectedValue);
}

// TODO: Fix this test
TEST(CrashdumpTest, test_getCPUData_overwrite)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    cpus[0] = {
        .clientAddr = 48,
        .model = (Model)0x0,
        .dieMaskInfo = {},
        .computeDies = {},
        .coreMask = 0x0,
        .crashedCoreMask = 0x0,
        .chaCount = 0,
        .initialPeciWake = ON,
        .inputFile = {},
        .cpuidRead = {},
        .chaCountRead = {},
        .coreMaskRead = {},
        .dimmMask = 0,
    };

    cpus[1] = {.clientAddr = 49,
               .model = (Model)0x0,
               .dieMaskInfo = {},
               .computeDies = {},
               .coreMask = 0x0,
               .crashedCoreMask = 0x0,
               .chaCount = 0,
               .initialPeciWake = ON,
               .inputFile = {},
               .cpuidRead = {},
               .chaCountRead = {},
               .coreMaskRead = {},
               .dimmMask = 0};

    uint8_t coreMaskHigh[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    // uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    // uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(SetArgPointee<1>((CPUModel)0x0), SetArgPointee<2>(1),
                        SetArgPointee<3>(PECI_DEV_CC_NEED_RETRY),
                        Return(PECI_CC_SUCCESS)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(PECI_DEV_CC_NEED_RETRY),
                        Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArrayArgument<7>(chaMaskHigh, chaMaskHigh + 4), // cha mask 0
            SetArgPointee<8>(PECI_DEV_CC_NEED_RETRY), Return(PECI_CC_SUCCESS)));
    size_t chaExpectedValue0 = 13;
    uint64_t coreExpectedValue0 = 0x403020108070605;
    size_t chaExpectedValue1 = 0;
    uint64_t coreExpectedValue1 = 0x0;
    cpus[0].cpuidRead.cpuidValid = true;
    cpus[0].chaCountRead.chaCountValid = true;
    cpus[0].coreMaskRead.coreMaskValid = true;
    cpus[0].model = cd_spr;
    cpus[0].cpuidRead.cpuModel = spr;
    cpus[1].cpuidRead.cpuidValid = false;
    cpus[1].chaCountRead.chaCountValid = false;
    cpus[1].coreMaskRead.coreMaskValid = false;
    cpus[0].chaCount = chaExpectedValue0;
    cpus[0].coreMask = coreExpectedValue0;
    getCPUData(cpus, EVENT);
    EXPECT_TRUE(cpus[1].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[1].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[1].coreMaskRead.coreMaskValid);
    EXPECT_EQ(cpus[1].cpuidRead.source, 3);
    EXPECT_EQ(cpus[1].cpuidRead.cpuModel, spr);
    EXPECT_EQ(cpus[1].model, cd_spr);
    EXPECT_EQ(cpus[1].chaCount, chaExpectedValue1);
    EXPECT_EQ(cpus[1].coreMask, coreExpectedValue1);
}

TEST(CrashdumpTest, test_getCPUData_invalid)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(SetArgPointee<1>((CPUModel)0x0), SetArgPointee<2>(1),
                        SetArgPointee<3>(PECI_DEV_CC_NEED_RETRY),
                        Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArgPointee<1>((CPUModel)0x0), SetArgPointee<2>(1),
                        SetArgPointee<3>(PECI_DEV_CC_NEED_RETRY),
                        Return(PECI_CC_SUCCESS)));

    size_t chaExpectedValue0 = 0;
    uint64_t coreExpectedValue0 = 0x0;
    initCPUInfo(cpus);
    getCPUData(cpus, EVENT);
    EXPECT_TRUE(cpus[0].cpuidRead.cpuidValid);
    EXPECT_TRUE(cpus[1].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[0].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[0].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[1].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[1].coreMaskRead.coreMaskValid);
    EXPECT_EQ(cpus[0].cpuidRead.source, EVENT);
    EXPECT_EQ(cpus[1].cpuidRead.source, EVENT);
    EXPECT_EQ(cpus[0].chaCount, chaExpectedValue0);
    EXPECT_EQ(cpus[0].coreMask, coreExpectedValue0);
    EXPECT_EQ(cpus[1].chaCount, chaExpectedValue0);
    EXPECT_EQ(cpus[1].coreMask, coreExpectedValue0);
}

TEST(CrashdumpTest, test_platformTurnOff)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)0x000806F0), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)0x000806F0), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArrayArgument<7>(chaMaskHigh, chaMaskHigh + 4), // cha mask 0
            SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(
            DoAll(SetArrayArgument<7>(chaMaskLow, chaMaskLow + 4), // cha mask 1
                  SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArrayArgument<7>(chaMaskHigh, chaMaskHigh + 4), // cha mask 0
            SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(
            DoAll(SetArrayArgument<7>(chaMaskLow, chaMaskLow + 4), // cha mask 1
                  SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)));
    initCPUInfo(cpus);
    getCPUData(cpus, STARTUP);
    // create_crashdump
    initCPUInfo(cpus);
    getCPUData(cpus, EVENT);
    EXPECT_TRUE(cpus[0].cpuidRead.cpuidValid);
    EXPECT_TRUE(cpus[1].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[2].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[3].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[4].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[5].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[6].cpuidRead.cpuidValid);
    EXPECT_FALSE(cpus[7].cpuidRead.cpuidValid);
    EXPECT_TRUE(cpus[0].chaCountRead.chaCountValid);
    EXPECT_TRUE(cpus[1].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[2].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[3].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[4].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[5].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[6].chaCountRead.chaCountValid);
    EXPECT_FALSE(cpus[7].chaCountRead.chaCountValid);
    EXPECT_TRUE(cpus[0].coreMaskRead.coreMaskValid);
    EXPECT_TRUE(cpus[1].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[2].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[3].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[4].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[5].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[6].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(cpus[7].coreMaskRead.coreMaskValid);
}

TEST(CrashdumpTest, createCrashdump_SPR_MCA_check)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo* cpus = crashdump.cpus;
    std::string crashdumpContents;
    std::string triggerType = "UnitTest";
    std::string timestamp = newTimestamp();
    bool isTelemetry = false;
    crashdump.libPeciMock->DelegateForCrashdumpSetUp();

    uint8_t addr = MIN_CLIENT_ADDR;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].clientAddr = addr;
        cpus[i].cpuidRead.cpuidValid = true;
        cpus[i].chaCountRead.chaCountValid = true;
        cpus[i].chaCount = 4;
        cpus[i].coreMaskRead.coreMaskValid = true;
        cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(SPR_MODEL);
        cpus[i].model = cd_spr;
        addr++;
    }
    createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                    isTelemetry);

    // Spot check MCA uncore section
    cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
    std::filesystem::path file = "/tmp/out-spr_mca.json";
    std::filesystem::remove(file);
    debugcJSON(output, file.c_str(), false);
#endif

    cJSON* child;
    child = getJsonObjectFromPath(
        "crash_data/PROCESSORS/cpu0/MCA/uncore/MC4/mc4_status", output);
    ASSERT_FALSE(child == NULL);

    // Spot check MCA uncore CBO section
    child = getJsonObjectFromPath(
        "crash_data/PROCESSORS/cpu0/MCA/uncore/CBO0/cbo0_status", output);
    ASSERT_FALSE(child == NULL);

    // Spot check MCA Core section
    child = getJsonObjectFromPath(
        "crash_data/PROCESSORS/cpu0/MCA/core1/thread0/MC0/mc0_status", output);
    ASSERT_FALSE(child == NULL);
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_Uncore_Set_Domain)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_Uncore_Set_Domain");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x0f, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_uncore_set_domain.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonPathStr[NAME_STR_LEN];
        int keysCount = 5;
        char keys[keysCount][NAME_STR_LEN] = {
            "mc4_ctl", "mc4_status", "mc4_addr", "mc4_misc", "mc4_ctl2"};
        cJSON* child = NULL;

        int cpuCount = 1;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            for (int idx = 0; idx < keysCount; idx++)
            {
                cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                              "crash_data/PROCESSORS/cpu%d/io0/MCA/"
                              "uncore/MC4/%s",
                              cpu, keys[idx]);
                child = getJsonObjectFromPath(jsonPathStr, output);
                ASSERT_FALSE(child == NULL) << "idx:" << idx;
            }
            // time key location check
            cd_snprintf_s(
                jsonPathStr, sizeof(jsonPathStr),
                "crash_data/PROCESSORS/cpu%d/_time_MCA_Uncore_Set_Domain", cpu);
            child = getJsonObjectFromPath(jsonPathStr, output);
            ASSERT_FALSE(child == NULL);
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_Uncore_Loop_Domain)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_Uncore_Loop_Domain");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x0f, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_uncore_loop_domain.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonStr[NAME_STR_LEN];
        int keysCount = 5;
        char keys[keysCount][NAME_STR_LEN] = {
            "mc6_ctl", "mc6_status", "mc6_addr", "mc6_misc", "mc6_ctl2"};
        cJSON* child = NULL;

        int cpuCount = 1;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            // computex Check
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MC6/%s",
                                  cpu, compute, keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }

                // time key location check
                cd_snprintf_s(jsonStr, sizeof(jsonStr),
                              "crash_data/PROCESSORS/cpu%d/compute%d/"
                              "_time_MCA_Uncore_Loop_Domain",
                              cpu);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL) << "compute:" << compute;
            }

            // iox Check
            int ioDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.io.effectiveMask);
            for (int io = 0; io < ioDieCount; io++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(
                        jsonStr, sizeof(jsonStr),
                        "crash_data/PROCESSORS/cpu%d/io%d/MCA/uncore/MC6/%s",
                        cpu, io, keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }

                // time key location check
                cd_snprintf_s(jsonStr, sizeof(jsonStr),
                              "crash_data/PROCESSORS/cpu%d/io%d/"
                              "_time_MCA_Uncore_Loop_Domain",
                              cpu);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL) << "io:" << io;
            }
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_UNCORE_IO)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_UNCORE_IO");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;
        const int MAX_NUM_OF_UPI = 6;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x0f, 0x00, 0x00, 0x00};

        uint8_t diemask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(diemask, diemask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        // Spot check MCA uncore section
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_uncore_io.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child =
            getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/io0", output);
        ASSERT_FALSE(child == NULL);
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/_version", output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0x3e02f001");

        // Check number of UPI instances
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/uncore", output);
        int numOfUPI = cJSON_GetArraySize(child);
        ASSERT_TRUE(numOfUPI == MAX_NUM_OF_UPI) << "numOfUPI:" << numOfUPI;

        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/uncore/UPI1/upi1_ctl", output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/uncore/UPI1/upi1_status",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/uncore/UPI1/upi1_addr", output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/uncore/UPI1/upi1_misc", output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/uncore/UPI1/upi1_ctl2", output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/MCA/_time_MCA_UNCORE_IO", output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_UNCORE_COMPUTE)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_UNCORE_COMPUTE");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x0f, 0x00, 0x00, 0x00};

        uint8_t diemask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(diemask, diemask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_uncore_compute.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        int cpuCount = 1;
        int instanceCount = 12;
        char jsonPathStr[NAME_STR_LEN];
        cJSON* child = NULL;

        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                /************************** MSE ***************************/
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MSE%d/mse%d_ctl2",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MSE%d/mse%d_ctl",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MSE%d/mse%d_status",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MSE%d/mse%d_addr",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MSE%d/mse%d_misc",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                /************************** MCHN ***************************/
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MCHN%d/mchn%d_misc2",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MCHN%d/mchn%d_ctl",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MCHN%d/mchn%d_status",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MCHN%d/mchn%d_addr",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/MCHN%d/mchn%d_misc",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                /************************** B2CMI ***************************/
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/B2CMI%d/b2cmi%d_ctl2",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/B2CMI%d/b2cmi%d_ctl",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/B2CMI%d/b2cmi%d_status",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/B2CMI%d/b2cmi%d_addr",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                for (int instance = 0; instance < instanceCount; instance++)
                {
                    cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                                  "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                                  "uncore/B2CMI%d/b2cmi%d_misc",
                                  cpu, compute, instance, instance);
                    child = getJsonObjectFromPath(jsonPathStr, output);
                    ASSERT_FALSE(child == NULL) << "instance:" << instance;
                }
                // time key location check
                cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                              "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                              "_time_MCA_UNCORE_COMPUTE",
                              cpu);
                child = getJsonObjectFromPath(jsonPathStr, output);
                ASSERT_FALSE(child == NULL) << "compute:" << compute;
            }
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_CBO)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_CBO");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x0f, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        // Spot check MCA uncore section
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_cbo.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0", output);
        ASSERT_FALSE(child == NULL);
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_status",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_addr",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_ctl",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_misc",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_misc2",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_misc3",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0/cbo0_misc4",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/_time_MCA_CBO", output);
        ASSERT_FALSE(child == NULL);
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute1/MCA/uncore/CBO0/cbo0_status",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute2/MCA/uncore/CBO0/cbo0_status",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        // Expecting 7 members in CBO0
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/uncore/CBO0", output);
        ASSERT_TRUE(cJSON_GetArraySize(child) == 7);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_LLC)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_LLC");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x0f, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_llc.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonPathStr[NAME_STR_LEN];
        char regStr[NAME_STR_LEN];
        int keysCount = 5;
        char keys[keysCount][NAME_STR_LEN] = {"llc%d_misc", "llc%d_ctl",
                                              "llc%d_status", "llc%d_addr",
                                              "llc%d_misc"};
        cJSON* child = NULL;

        int cpuCount = 1;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                int llcCount =
                    __builtin_popcount(cpus[cpu].computeDies[compute].chaMask);
                for (int llc = 0; llc < llcCount; llc++)
                {
                    for (int idx = 0; idx < keysCount; idx++)
                    {
                        cd_snprintf_s(regStr, sizeof(regStr), keys[idx], llc);
                        cd_snprintf_s(
                            jsonPathStr, sizeof(jsonPathStr),
                            "crash_data/PROCESSORS/cpu%d/compute%d/MCA/"
                            "uncore/LLC%d/%s",
                            cpu, compute, llc, regStr);
                        child = getJsonObjectFromPath(jsonPathStr, output);
                        ASSERT_FALSE(child == NULL) << "idx:" << idx;
                    }
                }
                // time key location check
                cd_snprintf_s(
                    jsonPathStr, sizeof(jsonPathStr),
                    "crash_data/PROCESSORS/cpu%d/compute%d/MCA/_time_MCA_LLC",
                    cpu);
                child = getJsonObjectFromPath(jsonPathStr, output);
                ASSERT_FALSE(child == NULL) << "compute:" << compute;
            }
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_MCA_CORE)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "MCA_CORE");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x01, 0x00, 0x00, 0x00};

        uint8_t Data[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));
        int core_rdiamsr_nRegs = getNumberOfRepetitions(
            cpus[0].inputFile.bufferPtr, "MCA_CORE", "RdIAMSR_dom");

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .Times(2 * core_rdiamsr_nRegs) // Multiplied by 2 for each thread
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        uint8_t addr = MIN_CLIENT_ADDR;
        for (int i = 0; i < MAX_CPUS; i++)
        {
            cpus[i].clientAddr = 0x0;
            cpus[i].cpuidRead.cpuidValid = false;
            addr++;
        }
        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        // Spot check MCA uncore section
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_mca_core.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0", output);
        ASSERT_FALSE(child == NULL);
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/core0/thread0/MC0/mc0_ctl",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread0/MC0/mc0_status",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread0/MC0/mc0_addr",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread0/MC0/mc0_misc",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread0/MC0/mc0_ctl2",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/core0/thread1/MC3/mc3_ctl",
            output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread1/MC3/mc3_status",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread1/MC3/mc3_addr",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread1/MC3/mc3_misc",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/MCA/"
                                      "core0/thread1/MC3/mc3_ctl2",
                                      output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0xdeadbeefdeadbeef");
        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/MCA/_time_MCA_CORE", output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Metadata_General)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "Metadata_General");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillRepeatedly(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                                  SetArgPointee<2>(1),
                                  SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));

        uint8_t addr = MIN_CLIENT_ADDR;
        int cpuCount = 2;
        for (int i = 0; i < cpuCount; i++)
        {
            cpus[i].clientAddr = addr;
            cpus[i].cpuidRead.cpuidValid = true;
            cpus[i].chaCountRead.chaCountValid = true;
            cpus[i].chaCount = 1;
            cpus[i].coreMaskRead.coreMaskValid = true;
            cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(CPUIDs[i]);
            cpus[i].model = cd_gnr;
            cpus[i].dieMaskInfo.dieMask = 0xe11;
            initComputeDieStructure(&cpus[i].dieMaskInfo, &cpus[i].computeDies);
            addr++;
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_metadata_general.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonStr[NAME_STR_LEN];
        const int keysCount = 12;
        char keys[keysCount][NAME_STR_LEN] = {
            "_version",      "bmc_fw_ver",    "bios_id",
            "me_fw_ver",     "timestamp",     "trigger_type",
            "platform_name", "crashdump_ver", "_input_file_ver",
            "_input_file",   "_total_time",   "_reset_detected"};

        cJSON* child = NULL;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            for (int idx = 0; idx < keysCount; idx++)
            {
                cd_snprintf_s(jsonStr, sizeof(jsonStr),
                              "crash_data/METADATA/%s", keys[idx]);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL) << "idx:" << idx;
            }
            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/METADATA/_time_Metadata_General", cpu);
            child = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(child == NULL);
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Metadata_Cpu_General)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "Metadata_Cpu_General");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillRepeatedly(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                                  SetArgPointee<2>(1),
                                  SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));

        uint8_t addr = MIN_CLIENT_ADDR;
        int cpuCount = 2;
        for (int i = 0; i < cpuCount; i++)
        {
            cpus[i].clientAddr = addr;
            cpus[i].cpuidRead.cpuidValid = true;
            cpus[i].chaCountRead.chaCountValid = true;
            cpus[i].chaCount = 1;
            cpus[i].coreMaskRead.coreMaskValid = true;
            cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(CPUIDs[i]);
            cpus[i].model = cd_gnr;
            cpus[i].dieMaskInfo.dieMask = 0xe11;
            initComputeDieStructure(&cpus[i].dieMaskInfo, &cpus[i].computeDies);
            addr++;
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_metadata_cpu_general.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonStr[NAME_STR_LEN];
        const int keysCount = 3;
        char keys[keysCount][NAME_STR_LEN] = {"peci_id", "cpuid",
                                              "_cpuid_source"};
        cJSON* child = NULL;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            for (int idx = 0; idx < keysCount; idx++)
            {
                cd_snprintf_s(jsonStr, sizeof(jsonStr),
                              "crash_data/METADATA/cpu%d/%s", cpu, keys[idx]);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL) << "idx:" << idx;
            }

            cd_snprintf_s(
                jsonStr, sizeof(jsonStr),
                "crash_data/METADATA/cpu%d/_time_Metadata_Cpu_General", cpu);
            child = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(child == NULL);
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Metadata_Cpu_Early)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "Metadata_Cpu_Early");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillRepeatedly(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                                  SetArgPointee<2>(1),
                                  SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));

        uint8_t addr = MIN_CLIENT_ADDR;
        int cpuCount = 2;
        for (int i = 0; i < cpuCount; i++)
        {
            cpus[i].clientAddr = addr;
            cpus[i].cpuidRead.cpuidValid = true;
            cpus[i].chaCountRead.chaCountValid = true;
            cpus[i].chaCount = 1;
            cpus[i].coreMaskRead.coreMaskValid = true;
            cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(CPUIDs[i]);
            cpus[i].model = cd_gnr;
            cpus[i].dieMaskInfo.dieMask = 0xe11;
            initComputeDieStructure(&cpus[i].dieMaskInfo, &cpus[i].computeDies);
            addr++;
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_metadata_cpu_early.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonStr[NAME_STR_LEN];
        int keysCount = 3;
        char keys[keysCount][NAME_STR_LEN] = {"firstierrtsc", "firstmcerrtsc",
                                              "mca_err_src_log"};
        cJSON* child = NULL;

        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            // computex Check
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/compute%d/%s", cpu,
                                  compute, keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }
            }

            // iox Check
            int ioDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.io.effectiveMask);
            for (int io = 0; io < ioDieCount; io++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/io%d/%s", cpu, io,
                                  keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }
            }

            // io0 specific check
            keysCount = 2;
            char io0keys[keysCount][NAME_STR_LEN] = {"ierrloggingreg",
                                                     "mcerrloggingreg"};

            for (int io = 0; io < 1; io++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/io%d/%s", cpu, io,
                                  io0keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "io0 - idx:" << idx;
                }
            }

            // time key check
            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/METADATA/cpu%d/_time_Metadata_Cpu_Early",
                          cpu);
            child = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(child == NULL);
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Metadata_Cpu_Compute_Early)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        char* sectionName = "Metadata_Cpu_Compute_Early";
        TestCrashdump crashdump(testModels[i], sectionName);
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        // Diemask setup - 0xe11 - 3 compute dies, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // Three sets of masks for core & cha
        uint8_t maskHighC0[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t maskLowC0[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t maskHighC1[4] = {0x02, 0x00, 0x00, 0x00};
        uint8_t maskLowC1[4] = {0x02, 0x00, 0x00, 0x00};
        uint8_t maskHighC2[4] = {0x03, 0x00, 0x00, 0x00};
        uint8_t maskLowC2[4] = {0x03, 0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        // Mocking for both getCoreMasks() and getCHACounts()
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC0, maskLowC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC0, maskHighC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC1, maskLowC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC1, maskHighC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC2, maskLowC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC2, maskHighC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC0, maskLowC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC0, maskHighC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC1, maskLowC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC1, maskHighC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC2, maskLowC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC2, maskHighC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file =
            "/tmp/out-gnr_metadata_rdglobalvars_compute_early.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        uint8_t cpuCount = 1;
        char jsonStr[NAME_STR_LEN];
        const int keysCount = 6;
        char keys[keysCount][NAME_STR_LEN] = {
            "_core_mask_source", "_cha_count_source", "core_mask",
            "core_count",        "cha_count",         "cha_mask"};
        cJSON* child = NULL;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/compute%d/%s", cpu,
                                  compute, keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }
                cd_snprintf_s(jsonStr, sizeof(jsonStr),
                              "crash_data/METADATA/cpu%d/compute%d/"
                              "_time_%s",
                              cpu, compute, sectionName);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL) << "compute:" << compute;
            }
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Metadata_Cpu_Compute_Late)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        char* sectionName = "Metadata_Cpu_Compute_Late";
        TestCrashdump crashdump(testModels[i], sectionName);
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        // Diemask setup - 0xe11 - 3 compute dies, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // Three sets of masks for core & cha
        uint8_t maskHighC0[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t maskLowC0[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t maskHighC1[4] = {0x02, 0x00, 0x00, 0x00};
        uint8_t maskLowC1[4] = {0x02, 0x00, 0x00, 0x00};
        uint8_t maskHighC2[4] = {0x03, 0x00, 0x00, 0x00};
        uint8_t maskLowC2[4] = {0x03, 0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        // Mocking for both getCoreMasks() and getCHACounts()
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC0, maskLowC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC0, maskHighC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC1, maskLowC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC1, maskHighC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC2, maskLowC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC2, maskHighC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC0, maskLowC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC0, maskHighC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC1, maskLowC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC1, maskHighC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC2, maskLowC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC2, maskHighC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_metadata_compute_late.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        uint8_t cpuCount = 1;
        char jsonStr[NAME_STR_LEN];
        const int keysCount = 2;
        char keys[keysCount][NAME_STR_LEN] = {"final_crashcore_count",
                                              "final_crashcore_mask"};
        cJSON* child = NULL;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/compute%d/%s", cpu,
                                  compute, keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }
                cd_snprintf_s(jsonStr, sizeof(jsonStr),
                              "crash_data/METADATA/cpu%d/compute%d/"
                              "_time_%s",
                              cpu, compute, sectionName);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL) << "compute:" << compute;
            }
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Metadata_Cpu_Late)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        char* sectionName = "Metadata_Cpu_Late";
        TestCrashdump crashdump(testModels[i], sectionName);
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        // Diemask setup - 0xe11 - 3 compute dies, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // Three sets of masks for core & cha
        uint8_t maskHighC0[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t maskLowC0[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t maskHighC1[4] = {0x02, 0x00, 0x00, 0x00};
        uint8_t maskLowC1[4] = {0x02, 0x00, 0x00, 0x00};
        uint8_t maskHighC2[4] = {0x03, 0x00, 0x00, 0x00};
        uint8_t maskLowC2[4] = {0x03, 0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        // Mocking for both getCoreMasks() and getCHACounts()
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC0, maskLowC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC0, maskHighC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC1, maskLowC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC1, maskHighC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC2, maskLowC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC2, maskHighC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC0, maskLowC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC0, maskHighC0 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC1, maskLowC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC1, maskHighC1 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskLowC2, maskLowC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(maskHighC2, maskHighC2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoAll(SetArgPointee<8>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<9>(0x40),
                                  Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_metadata_cpu_late.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        uint8_t cpuCount = 1;
        char jsonStr[NAME_STR_LEN];
        const int keysCount = 1;
        const int keysCount2 = 2;
        char keys[keysCount][NAME_STR_LEN] = {"final_mca_err_src_log"};
        char keys2[keysCount2][NAME_STR_LEN] = {"final_ierrloggingreg",
                                                "final_mcerrloggingreg"};

        cJSON* child = NULL;
        for (int cpu = 0; cpu < cpuCount; cpu++)
        {
            int computeDieCount =
                __builtin_popcount(cpus[cpu].dieMaskInfo.compute.effectiveMask);
            for (int compute = 0; compute < computeDieCount; compute++)
            {
                for (int idx = 0; idx < keysCount; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/compute%d/%s", cpu,
                                  compute, keys[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }

                cd_snprintf_s(
                    jsonStr, sizeof(jsonStr),
                    "crash_data/METADATA/cpu%d/_time_Metadata_Cpu_Late", cpu);
                child = getJsonObjectFromPath(jsonStr, output);
                ASSERT_FALSE(child == NULL);
            }

            // io0 only
            for (int io = 0; io < 1; io++)
            {
                for (int idx = 0; idx < keysCount2; idx++)
                {
                    cd_snprintf_s(jsonStr, sizeof(jsonStr),
                                  "crash_data/METADATA/cpu%d/io%d/%s", cpu, io,
                                  keys2[idx]);
                    child = getJsonObjectFromPath(jsonStr, output);
                    ASSERT_FALSE(child == NULL) << "idx:" << idx;
                }
            }
        }
    }
}

TEST(CrashdumpTest, GNR_discovery_flow_STARTUP)
{
    TestCrashdump crashdump(cd_gnr);
    CPUInfo cpus[MAX_CPUS];
    crashdump.libPeciMock->DelegateForCrashdumpSetUp();

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)GNR_MODEL), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t coreMaskHigh2[4] = {0x09, 0x0a, 0x0b, 0x0c};
    uint8_t coreMaskLow2[4] = {0x0d, 0x0e, 0x0f, 0xff};

    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh2[4] = {0x09, 0x0a, 0x0b, 0x0c};
    uint8_t chaMaskLow2[4] = {0x0d, 0x0e, 0x0f, 0xff};

    // Example: 2 IO dies and 3 compute dies
    // ICCCI: 16b'0000 1110 0001 0001
    uint8_t Data[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(1)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        // mock coremask
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh2, coreMaskHigh2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow2, coreMaskLow2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        // mock chamask
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh2, chaMaskHigh2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow2, chaMaskLow2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

    initCPUInfo(cpus);
    getCPUData(cpus, STARTUP);

    EXPECT_EQ(cpus[0].cpuidRead.cpuModel, GNR_MODEL);
    EXPECT_EQ(cpus[0].dieMaskInfo.dieMask, (uint32_t)0xe11);
    EXPECT_EQ(cpus[0].computeDies[0].coreMask, (uint64_t)0x0403020108070605);
    EXPECT_EQ(cpus[0].computeDies[1].coreMask, (uint64_t)0xff0f0e0d0c0b0a09);
    EXPECT_EQ(cpus[0].computeDies[2].coreMask, (uint64_t)0x0403020108070605);

    size_t chaCount =
        __builtin_popcount(0x04030201) + __builtin_popcount(0x08070605);
    EXPECT_EQ(cpus[0].computeDies[0].chaCount, chaCount);
    chaCount = __builtin_popcount(0xff0f0e0d) + __builtin_popcount(0x0c0b0a09);
    EXPECT_EQ(cpus[0].computeDies[1].chaCount, chaCount);
    chaCount = __builtin_popcount(0x04030201) + __builtin_popcount(0x08070605);
    EXPECT_EQ(cpus[0].computeDies[2].chaCount, chaCount);
    free(cpus[0].computeDies);
    free(cpus[1].computeDies);
    free(cpus[2].computeDies);
}

TEST(CrashdumpTest, GNR_discovery_flow_STARTUP_diemaskCC0x40_ondemand)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i]);
        CPUInfo cpus[MAX_CPUS];
        crashdump.libPeciMock->DelegateForCrashdumpSetUp();

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
        uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

        uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
        uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

        uint8_t diemask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // mock startup diemask return 0x40
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(diemask, diemask + 8),
                                  SetArgPointee<5>(0x40),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            // mock chamask
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            // mock the reset of the crashdump flow
            .WillRepeatedly(
                DoAll(SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        ///////// mimic crashdump startup ///////////
        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        initCPUInfo(cpus);
        getCPUData(cpus, STARTUP);
        /////////////////////////////////////////////

        EXPECT_EQ(cpus[0].cpuidRead.cpuModel, CPUIDs[i]);
        EXPECT_EQ(cpus[0].dieMaskInfo.dieMask, (uint32_t)0x211);

        // mimic ondemand
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

#ifdef DEBUG_FLAG
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
        std::filesystem::path file =
            "/tmp/out-gnr_crashdump_startup_diemask_cc0x40_ondemand.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        free(cpus[0].computeDies);
    }
}

TEST(CrashdumpTest, GNR_discovery_flow_STARTUP_diemaskCC0x83_ondemand)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i]);
        CPUInfo cpus[MAX_CPUS];
        crashdump.libPeciMock->DelegateForCrashdumpSetUp();

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        // don't care since mocking return CC:0x83
        uint8_t diemask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // mock startup diemask return 0x83
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(diemask, diemask + 8),
                                  SetArgPointee<5>(0x83),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock the reset of the crashdump flow
            .WillRepeatedly(
                DoAll(SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        ///////// mimic crashdump startup ///////////
        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        initCPUInfo(cpus);
        getCPUData(cpus, STARTUP);
        /////////////////////////////////////////////

        EXPECT_EQ(cpus[0].cpuidRead.cpuModel, CPUIDs[i]);
        // New flow; Don't override diemask on STARTUP
        EXPECT_EQ(cpus[0].dieMaskInfo.dieMask, (uint32_t)0);

        // mimic ondemand
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

#ifdef DEBUG_FLAG
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
        std::filesystem::path file =
            "/tmp/out-gnr_crashdump_startup_diemask_cc0x83_ondemand.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        free(cpus[0].computeDies);
    }
}

TEST(CrashdumpTest, GNR_discovery_flow_EVENT_diemaskCC0x40_ondemand)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i]);
        CPUInfo cpus[MAX_CPUS];
        crashdump.libPeciMock->DelegateForCrashdumpSetUp();

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
        uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

        uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
        uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

        uint8_t diemask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // mock startup diemask return 0x40
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(diemask, diemask + 8),
                                  SetArgPointee<5>(0x40),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            // mock chamask
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            // mock the reset of the crashdump flow
            .WillRepeatedly(
                DoAll(SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        ///////// mimic crashdump startup ///////////
        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        initCPUInfo(cpus);
        getCPUData(cpus, EVENT);
        /////////////////////////////////////////////

        EXPECT_EQ(cpus[0].cpuidRead.cpuModel, CPUIDs[i]);
        // Receive valid Diemask
        EXPECT_EQ(cpus[0].dieMaskInfo.dieMask, (uint32_t)0x211);

        // mimic ondemand
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

#ifdef DEBUG_FLAG
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
        std::filesystem::path file =
            "/tmp/out-gnr_crashdump_startup_diemask_cc0x40_ondemand.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        free(cpus[0].computeDies);
    }
}

TEST(CrashdumpTest, GNR_discovery_flow_EVENT_diemaskCC0x83_ondemand)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    uint32_t defaultDieMasks[] = {GNR_MAX_DIEMASK, SRF_MAX_DIEMASK};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i]);
        CPUInfo cpus[MAX_CPUS];
        crashdump.libPeciMock->DelegateForCrashdumpSetUp();

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
        uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

        uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
        uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};

        // don't care since mocking return CC:0x83
        uint8_t diemask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        // mock startup diemask return 0x83
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(diemask, diemask + 8),
                                  SetArgPointee<5>(0x83),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            // mock chamask
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            // mock the reset of the crashdump flow
            .WillRepeatedly(
                DoAll(SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        ///////// mimic crashdump startup ///////////
        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        initCPUInfo(cpus);
        getCPUData(cpus, EVENT);
        /////////////////////////////////////////////

        EXPECT_EQ(cpus[0].cpuidRead.cpuModel, CPUIDs[i]);

        EXPECT_EQ(cpus[0].dieMaskInfo.dieMask, defaultDieMasks[i]);

        // mimic ondemand
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

#ifdef DEBUG_FLAG
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
        std::filesystem::path file =
            "/tmp/out-gnr_crashdump_startup_diemask_cc0x83_ondemand.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        free(cpus[0].computeDies);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Crashlog)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "crashlog");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;
        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x01, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        uint8_t telemetry_answer0[1] = {0x01}; // Telemetry supported
        uint8_t telemetry_answer1[1] = {0x06}; // number of agents
        uint8_t cc = 0x40;
        uint8_t returnCrashlogDetails0[8] = {
            0x66, 0x55, 0x57, 0x39,
            0xd2, 0xdf, 0x02, 0x00}; // size = 0x02,agent_id = 0xdfd23957,
        uint8_t returnCrashlogDetails1[8] = {
            0x66, 0x55, 0xe0, 0x5d,
            0x33, 0xe3, 0x02, 0x00}; // size = 0x02,agent_id = 0xe3335de0,
        uint8_t returnCrashlogDetails2[8] = {
            0x66, 0x55, 0x82, 0x1a,
            0x6b, 0x40, 0x02, 0x00}; // size = 0x02,agent_id = 0x406b1a82,
        uint8_t returnCrashlogDetails3[8] = {
            0x66, 0x55, 0x60, 0x3a,
            0x01, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0x13a60,
        uint8_t returnCrashlogDetails4[8] = {
            0x66, 0x55, 0xf0, 0xa0,
            0x17, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0x17a0f0,
        uint8_t returnCrashlogDetails5[8] = {
            0x66, 0x55, 0xcb, 0x3d,
            0x0f, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0xf3dcb,

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)));

        uint8_t returnValue[8] = {0x00, 0x01, 0x02, 0x03,
                                  0x04, 0x05, 0x06, 0x07};
        cc = 0x40;
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_Telemetry_GetCrashlogSample_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 8),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherRd_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherWr_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherWr_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }
        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_crashlog.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        // Spot check Crashlog Core section

        char jsonStr[NAME_STR_LEN];
        const int count = 3;
        char domains[count][NAME_STR_LEN] = {"compute0", "io0", "io1"};

        for (int idx = 0; idx < count; idx++)
        {
            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xdfd23957/#data_C_DIE_OOBMSM_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent0 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent0 == NULL);
            EXPECT_STREQ(agent0->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xe3335de0/#data_FW_OOBMSM_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent1 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent1 == NULL);
            EXPECT_STREQ(agent1->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x406b1a82/#data_C_DIE_TOR_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent2 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent2 == NULL);
            EXPECT_STREQ(agent2->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x13a60/#data_PUNIT_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent3 = getJsonObjectFromPath(jsonStr, output);
            EXPECT_STREQ(agent3->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x17a0f0/#data_C_DIE_PCODE_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent4 = getJsonObjectFromPath(jsonStr, output);
            EXPECT_STREQ(agent4->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xf3dcb/#data_IO_DIE_PCODE_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent5 = getJsonObjectFromPath(jsonStr, output);
            EXPECT_STREQ(agent5->valuestring, "AAECAwQFBgc=");
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Crashlog_DumpAllWithUnknownAgent)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "crashlog");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;
        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x01, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        uint8_t telemetry_answer0[1] = {0x01}; // Telemetry supported
        uint8_t telemetry_answer1[1] = {0x07}; // number of agents
        uint8_t cc = 0x40;
        uint8_t returnCrashlogDetails0[8] = {
            0x66, 0x55, 0x57, 0x39,
            0xd2, 0xdf, 0x02, 0x00}; // size = 0x02,agent_id = 0xdfd23957,
        uint8_t returnCrashlogDetails1[8] = {
            0x66, 0x55, 0xe0, 0x5d,
            0x33, 0xe3, 0x02, 0x00}; // size = 0x02,agent_id = 0xe3335de0,
        uint8_t returnCrashlogDetails2[8] = {
            0x66, 0x55, 0x82, 0x1a,
            0x6b, 0x40, 0x02, 0x00}; // size = 0x02,agent_id = 0x406b1a82,
        uint8_t returnCrashlogDetails3[8] = {
            0x66, 0x55, 0x60, 0x3a,
            0x01, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0x13a60,
        uint8_t returnCrashlogDetails4[8] = {
            0x66, 0x55, 0xf0, 0xa0,
            0x17, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0x17a0f0,
        uint8_t returnCrashlogDetails5[8] = {
            0x66, 0x55, 0xcb, 0x3d,
            0x0f, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0xf3dcb,
        uint8_t returnCrashlogDetailsUnknown[8] = {
            0x66, 0x55, 0x01, 0x02,
            0x03, 0x40, 0x02, 0x00}; // size = 0x02,agent_id = 0x40030201,

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(
                DoAll(SetArrayArgument<7>(returnCrashlogDetailsUnknown,
                                          returnCrashlogDetailsUnknown + 8),
                      SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(
                DoAll(SetArrayArgument<7>(returnCrashlogDetailsUnknown,
                                          returnCrashlogDetailsUnknown + 8),
                      SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(
                DoAll(SetArrayArgument<7>(returnCrashlogDetailsUnknown,
                                          returnCrashlogDetailsUnknown + 8),
                      SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)));

        uint8_t returnValue[8] = {0x00, 0x01, 0x02, 0x03,
                                  0x04, 0x05, 0x06, 0x07};
        cc = 0x40;
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_Telemetry_GetCrashlogSample_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 8),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherRd_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherWr_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }
        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_crashlog_unknown_agent.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        // Spot check Crashlog Core section

        char jsonStr[NAME_STR_LEN];
        const int count = 3;
        char domains[count][NAME_STR_LEN] = {"compute0", "io0", "io1"};

        for (int idx = 0; idx < count; idx++)
        {
            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xdfd23957/#data_C_DIE_OOBMSM_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent0 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent0 == NULL);
            EXPECT_STREQ(agent0->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xe3335de0/#data_FW_OOBMSM_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent1 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent1 == NULL);
            EXPECT_STREQ(agent1->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x406b1a82/#data_C_DIE_TOR_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent2 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent2 == NULL);
            EXPECT_STREQ(agent2->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x13a60/#data_PUNIT_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent3 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent3 == NULL);
            EXPECT_STREQ(agent3->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x17a0f0/#data_C_DIE_PCODE_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent4 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent4 == NULL);
            EXPECT_STREQ(agent4->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xf3dcb/#data_IO_DIE_PCODE_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent5 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent5 == NULL);
            EXPECT_STREQ(agent5->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x40030201/#data_40030201",
                          domains[idx]);
            cJSON* agentUnknown = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agentUnknown == NULL);
            EXPECT_STREQ(agentUnknown->valuestring, "AAECAwQFBgc=");
        }
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_Crashlog_DumpAllWithUnknownAgentCC90)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(cd_gnr, "crashlog");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;
        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x01, 0x00, 0x00, 0x00};

        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        uint8_t telemetry_answer0[1] = {0x01}; // Telemetry supported
        uint8_t telemetry_answer1[1] = {0x07}; // number of agents
        uint8_t cc = 0x40;
        uint8_t returnCrashlogDetails0[8] = {
            0x66, 0x55, 0x57, 0x39,
            0xd2, 0xdf, 0x02, 0x00}; // size = 0x02,agent_id = 0xdfd23957,
        uint8_t returnCrashlogDetails1[8] = {
            0x66, 0x55, 0xe0, 0x5d,
            0x33, 0xe3, 0x02, 0x00}; // size = 0x02,agent_id = 0xe3335de0,
        uint8_t returnCrashlogDetails2[8] = {
            0x66, 0x55, 0x82, 0x1a,
            0x6b, 0x40, 0x02, 0x00}; // size = 0x02,agent_id = 0x406b1a82,
        uint8_t returnCrashlogDetails3[8] = {
            0x66, 0x55, 0x60, 0x3a,
            0x01, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0x13a60,
        uint8_t returnCrashlogDetails4[8] = {
            0x66, 0x55, 0xf0, 0xa0,
            0x17, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0x17a0f0,
        uint8_t returnCrashlogDetails5[8] = {
            0x66, 0x55, 0xcb, 0x3d,
            0x0f, 0x00, 0x02, 0x00}; // size = 0x02,agent_id = 0xf3dcb,
        uint8_t returnCrashlogDetailsUnknown[8] = {
            0x66, 0x55, 0x01, 0x02,
            0x03, 0x40, 0x02, 0x00}; // size = 0x02,agent_id = 0x40030201,

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(
                DoAll(SetArrayArgument<7>(returnCrashlogDetailsUnknown,
                                          returnCrashlogDetailsUnknown + 8),
                      SetArgPointee<8>(0x90), Return(PECI_CC_INVALID_REQ)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(
                DoAll(SetArrayArgument<7>(returnCrashlogDetailsUnknown,
                                          returnCrashlogDetailsUnknown + 8),
                      SetArgPointee<8>(0x90), Return(PECI_CC_INVALID_REQ)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(
                DoAll(SetArrayArgument<7>(returnCrashlogDetailsUnknown,
                                          returnCrashlogDetailsUnknown + 8),
                      SetArgPointee<8>(0x90), Return(PECI_CC_INVALID_REQ)));

        uint8_t returnValue[8] = {0x00, 0x01, 0x02, 0x03,
                                  0x04, 0x05, 0x06, 0x07};
        cc = 0x40;
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_Telemetry_GetCrashlogSample_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 8),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherRd_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherWr_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                      SetArgPointee<6>(cc), Return(PECI_CC_SUCCESS)));

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }
        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file =
            "/tmp/out-gnr_crashlog_unknown_agent_cc90.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        char jsonStr[NAME_STR_LEN];
        const int count = 3;
        char domains[count][NAME_STR_LEN] = {"compute0", "io0", "io1"};

        for (int idx = 0; idx < count; idx++)
        {
            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xdfd23957/#data_C_DIE_OOBMSM_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent0 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent0 == NULL);
            EXPECT_STREQ(agent0->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xe3335de0/#data_FW_OOBMSM_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent1 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent1 == NULL);
            EXPECT_STREQ(agent1->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x406b1a82/#data_C_DIE_TOR_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent2 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent2 == NULL);
            EXPECT_STREQ(agent2->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x13a60/#data_PUNIT_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent3 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent3 == NULL);
            EXPECT_STREQ(agent3->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x17a0f0/#data_C_DIE_PCODE_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent4 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent4 == NULL);
            EXPECT_STREQ(agent4->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0xf3dcb/#data_IO_DIE_PCODE_CRASH_INDEX",
                          domains[idx]);
            cJSON* agent5 = getJsonObjectFromPath(jsonStr, output);
            ASSERT_FALSE(agent5 == NULL);
            EXPECT_STREQ(agent5->valuestring, "AAECAwQFBgc=");

            cd_snprintf_s(jsonStr, sizeof(jsonStr),
                          "crash_data/PROCESSORS/cpu0/%s/crashlog/"
                          "agent_id_0x40030201/#data_40030201",
                          domains[idx]);
            cJSON* agentUnknown = getJsonObjectFromPath(jsonStr, output);
            ASSERT_TRUE(agentUnknown == NULL) << domains[idx];
        }
    }
}

TEST(CrashdumpTest, createCrashdump_post_reset_flow)
{
    TestCrashdump crashdump(cd_gnr);
    CPUInfo* cpus = crashdump.cpus;
    std::string crashdumpContents;
    std::string triggerType = "PostReset";
    std::string timestamp = newTimestamp();
    bool isTelemetry = false;
    crashdump.libPeciMock->DelegateForCrashdumpSetUp();

    uint8_t addr = MIN_CLIENT_ADDR;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].clientAddr = addr;
        cpus[i].cpuidRead.cpuidValid = true;
        cpus[i].chaCountRead.chaCountValid = true;
        cpus[i].chaCount = 4;
        cpus[i].coreMaskRead.coreMaskValid = true;
        cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(GNR_MODEL);
        cpus[i].model = cd_gnr;
        cpus[i].dieMaskInfo.dieMask = 0xe11;
        initComputeDieStructure(&cpus[i].dieMaskInfo, &cpus[i].computeDies);
        addr++;
    }
    createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                    isTelemetry);

    cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
    std::filesystem::path file = "/tmp/out-gnr_postwarmreset.json";
    std::filesystem::remove(file);
    debugcJSON(output, file.c_str(), false);
#endif

    // MCA Core section SHOULD NOT be present
    cJSON* child = getJsonObjectFromPath(
        // Only io0/4 are responding for now
        "crash_data/PROCESSORS/cpu0/io0/MCA/core1/thread0/MC0/mc0_status",
        output);
    ASSERT_TRUE(child == NULL);

    // Expect METADATA & MCA Uncore data are present when trigger string is
    // "PostReset" or "PostReset" &
    // "PostResetData" : true is in the input file
    child = getJsonObjectFromPath(
        "crash_data/PROCESSORS/cpu0/io0/MCA/uncore/MC4/mc4_status", output);
    ASSERT_FALSE(child == NULL);
}

TEST(CrashdumpTest, GNR_Uncore_PCI)
{
    TestCrashdump crashdump(cd_gnr, "Uncore_PCI");
    CPUInfo* cpus = crashdump.cpus;
    std::string crashdumpContents;
    std::string triggerType = "UnitTest";
    std::string timestamp = newTimestamp();
    bool isTelemetry = false;
    uint8_t cc = 0x40;

    // Diemask setup - 0xE11 - 3 compute die, 2 io dies
    uint8_t dieMask[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)GNR_MODEL), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    /*
        TODO:   RepsUncorePCI will only work with MAX_DIE_MASK.
                Otherwise some commands may be skipped if not in diemask.
    */
    int pci_nRegs = repsUncorePCI(cpus[0].inputFile.bufferPtr);
    uint8_t Data[8] = {0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde};

    // TODO: Check why input file and repsUncorePCI() is off by 12
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .Times(pci_nRegs + 12)
        .WillRepeatedly(DoAll(SetArrayArgument<8>(Data, Data + 8),
                              SetArgPointee<9>(cc), Return(PECI_CC_SUCCESS)));

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                    isTelemetry);

    cJSON* output = cJSON_Parse(crashdumpContents.c_str());

    cJSON* uncore_cpu0_compute0 = getJsonObjectFromPath(
        "crash_data/PROCESSORS/cpu0/compute0/uncore", output);
    ASSERT_TRUE(uncore_cpu0_compute0 != NULL);

    cJSON* uncore_compute0_io0 =
        getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/io0/uncore", output);
    ASSERT_TRUE(uncore_compute0_io0 != NULL);

    cJSON* uncore_compute0_io1 =
        getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/io1/uncore", output);
    ASSERT_TRUE(uncore_compute0_io1 != NULL);

#ifdef DEBUG_FLAG
    std::filesystem::path file = "/tmp/out-uncore-pci.json";
    std::filesystem::remove(file);
    debugcJSON(output, file.c_str(), false);
#endif
}

// TODO: Fix this test
TEST(CrashdumpTest, DISABLED_GNR_Uncore_MMIO)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "Uncore_MMIO");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        uint8_t addr = MIN_CLIENT_ADDR;
        for (int i = 0; i < 1; i++)
        {
            cpus[i].clientAddr = addr;
            cpus[i].cpuidRead.cpuidValid = true;
            cpus[i].chaCountRead.chaCountValid = true;
            cpus[i].chaCount = 1;
            cpus[i].coreMaskRead.coreMaskValid = true;
            cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(CPUIDs[i]);
            cpus[i].model = testModels[i];
            cpus[i].dieMaskInfo.dieMask = 0x411;
            initComputeDieStructure(&cpus[i].dieMaskInfo, &cpus[i].computeDies);
            addr++;
        }

        uint8_t cc = 0x40;
        uint8_t peciRdValue = 0x1;
        int uncore_mmio_nRegs =
            getNumberOfRepetitions(cpus[0].inputFile.bufferPtr, "Uncore_MMIO",
                                   "RdEndpointConfigMMIO_dom");

        uint8_t returnBusValidQuery[4] = {0x3f, 0x0f, 0x00,
                                          0xc0}; // 0xc0000f3f; bit 30 set
        uint8_t returnEnumeratedBus[4] = {0x00, 0x00, 0x30, 0x00}; // 0x300000;
        uint8_t returnEnumeratedBus2[4] = {0x08, 0x09, 0x10,
                                           0x11}; // 0x11100908;
        uint8_t Data[8] = {0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde};

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio_dom)
            .Times(uncore_mmio_nRegs)
            .WillRepeatedly(DoAll(SetArrayArgument<10>(Data, Data + 8),
                                  SetArgPointee<11>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(returnBusValidQuery,
                                                returnBusValidQuery + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(returnEnumeratedBus,
                                                returnEnumeratedBus + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(returnEnumeratedBus2,
                                                returnEnumeratedBus2 + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .Times(4) // 4 times x compute x cpu
            .WillRepeatedly(
                DoAll(SetArgPointee<9>(cc), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(2) // 2 times x cpu
            .WillRepeatedly(DoAll(SetArgPointee<4>(peciRdValue),
                                  SetArgPointee<5>(cc),
                                  Return(PECI_CC_SUCCESS)));

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

        cJSON* uncore_compute0_cpu0 = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute2/uncore", output);
        ASSERT_TRUE(uncore_compute0_cpu0 != NULL);

        cJSON* uncore_pci_time = cJSON_GetObjectItemCaseSensitive(
            uncore_compute0_cpu0, "_time_Uncore_MMIO");
        ASSERT_TRUE(uncore_pci_time != NULL);

        ASSERT_TRUE(cJSON_GetArraySize(uncore_compute0_cpu0) > 2);

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-uncore-mmio.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
    }
}

TEST(CrashdumpTest, GNR_Uncore_RdIAMSR_and_RdIAMSR_CHA)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "Uncore_RdIAMSR");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        uint8_t addr = MIN_CLIENT_ADDR;
        for (int i = 0; i < 1; i++)
        {
            cpus[i].clientAddr = 0x0;
            cpus[i].cpuidRead.cpuidValid = false;
            addr++;
        }

        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x01, 0x00, 0x00, 0x00};
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
        uint8_t peciRdValue = 0x1;

        // Uncore_RdIAMSR has LoopOnDomain enabled
        int uncore_rdiamsr_nRegs = getNumberOfRepetitions(
            cpus[0].inputFile.bufferPtr, "Uncore_RdIAMSR", "RdIAMSR_dom");

        // Die mask is hardcoded at 2 IO, 1 compute so 3 domains will loop
        uncore_rdiamsr_nRegs *= 3;

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
            .Times(uncore_rdiamsr_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(2) // 2 times x cpu
            .WillRepeatedly(DoAll(SetArgPointee<4>(peciRdValue),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 4),
                            SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());
        // We expect 2 IO, 1 Compute
        cJSON* uncore_compute0_cpu0 = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/uncore", output);
        EXPECT_TRUE(uncore_compute0_cpu0 != NULL);
        EXPECT_TRUE(cJSON_GetArraySize(uncore_compute0_cpu0) > 2);

        cJSON* uncore_io0_cpu0 = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io0/uncore", output);
        EXPECT_TRUE(uncore_io0_cpu0 != NULL);
        EXPECT_TRUE(cJSON_GetArraySize(uncore_io0_cpu0) > 2);

        cJSON* uncore_io1_cpu0 = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/io1/uncore", output);
        EXPECT_TRUE(uncore_io1_cpu0 != NULL);
        EXPECT_TRUE(cJSON_GetArraySize(uncore_io1_cpu0) > 2);

        cJSON* uncore_rdiamsr = cJSON_GetObjectItemCaseSensitive(
            uncore_compute0_cpu0, "_time_Uncore_RdIAMSR");
        EXPECT_TRUE(uncore_rdiamsr != NULL);

        EXPECT_TRUE(cJSON_GetArraySize(uncore_compute0_cpu0) > 2);

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-uncore-rdiamsr.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_TOR)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "TOR");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;
        int nCPUs = 1;

        for (int i = 0; i < nCPUs; i++)
        {
            cpus[i].clientAddr = 0x0;
            crashdump.cpus[i].cpuidRead.cpuidValid = false;
        }

        // GetCHAMasks
        // 0x1 = cha 0
        int nActiveChas =
            1; // Please change this according with the below value
        uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t chaMaskLow[4] = {0x01, 0x00, 0x00, 0x00};

        // GetCoreMasks
        // 0x1 = core 0
        uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskLow[4] = {0x01, 0x00, 0x00, 0x00};

        // GetDieMask
        // Data[1:] -> 0x2 = compute 1 active
        int nComputes = 1; // Please change this according to the below value
        uint8_t DataDieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t DataSample[8] = {0xef, 0xbe, 0xad, 0xde,
                                 0xef, 0xbe, 0xad, 0xde};

        Sequence sTor;
        for (int i = 0; i < (nCPUs * nComputes); i++)
        {
            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
                .InSequence(sTor)
                .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                                SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                                Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
                .InSequence(sTor)
                .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                                SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                                Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
                .InSequence(sTor)
                .WillOnce(
                    DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                          SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                          Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
                .InSequence(sTor)
                .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                                SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                                Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();

            EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
                .Times(nActiveChas * 32 * 8)
                .InSequence(sTor)
                .WillRepeatedly(
                    DoAll(SetArrayArgument<6>(DataSample, DataSample + 8),
                          SetArgPointee<7>(PECI_DEV_CC_SUCCESS),
                          Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
        }

        ///////////////////////////////////////////////////////////////////////////
        ////////////////////////////[CreateCrashdump]//////////////////////////////
        ///////////////////////////////////////////////////////////////////////////

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(2 * nCPUs) // by CPU
            .WillRepeatedly(DoAll(SetArgPointee<4>(0x1),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        Sequence sGetClientAddrs;
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .Times(nCPUs)
            .InSequence(sGetClientAddrs)
            .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .Times(MAX_CPUS - nCPUs)
            .InSequence(sGetClientAddrs)
            .WillRepeatedly(DoAll(Return(PECI_CC_TIMEOUT)));

        // getCPUID()
        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .Times(nCPUs)
            .WillRepeatedly(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                                  SetArgPointee<2>(1),
                                  SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        // getDieMask()
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(nCPUs) // 1 x CPU
            .WillRepeatedly(DoAll(
                SetArrayArgument<4>(DataDieMask, DataDieMask + 8),
                SetArgPointee<5>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();

        Sequence sGetMasks;
        // getCoreMasks()
        for (int i = 0; i < (nCPUs * nComputes); i++)
        {
            // Cores
            EXPECT_CALL(*crashdump.libPeciMock,
                        peci_RdEndPointConfigPciLocal_dom)
                .InSequence(sGetMasks)
                .WillOnce(
                    DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                          SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
            EXPECT_CALL(*crashdump.libPeciMock,
                        peci_RdEndPointConfigPciLocal_dom)
                .InSequence(sGetMasks)
                .WillOnce(
                    DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                          SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
        }
        // GetChaMasks
        for (int i = 0; i < (nCPUs * nComputes); i++)
        {
            EXPECT_CALL(*crashdump.libPeciMock,
                        peci_RdEndPointConfigPciLocal_dom)
                .InSequence(sGetMasks)
                .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                                SetArgPointee<9>(0x40),
                                Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
            EXPECT_CALL(*crashdump.libPeciMock,
                        peci_RdEndPointConfigPciLocal_dom)
                .InSequence(sGetMasks)
                .WillOnce(
                    DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                          SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
                .RetiresOnSaturation();
        }

        ///////////////////////////////////////////////////////////////////////////
        //***********************************************************************//
        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        //***********************************************************************//
        ///////////////////////////////////////////////////////////////////////////

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr-tor.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* torCompute0Cpu0 = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/TOR", output);
        ASSERT_TRUE(torCompute0Cpu0 != NULL);

        cJSON* torCompute0Cpu0_time =
            cJSON_GetObjectItemCaseSensitive(torCompute0Cpu0, "_time_TOR");
        ASSERT_TRUE(torCompute0Cpu0_time != NULL);

        cJSON* cha0 = cJSON_GetObjectItemCaseSensitive(torCompute0Cpu0, "cha0");
        ASSERT_TRUE(cha0 != NULL);

        cJSON* index0 = cJSON_GetObjectItemCaseSensitive(cha0, "index0");
        ASSERT_TRUE(index0 != NULL);

        cJSON* subindex0 =
            cJSON_GetObjectItemCaseSensitive(index0, "subindex0");
        ASSERT_TRUE(subindex0 != NULL);

        EXPECT_STREQ(subindex0->valuestring, "0xdeadbeefdeadbeef");
    }
}

/*
 * TODO: This is a place holder test.
 * Mock level: hard (need to mock timing to test various timeout scenarios)
 */
TEST(CrashdumpTest, DISABLED_createCrashdump_GNR_BigCore_isIerrPresent)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        // Thread 0 non-sq with checksum
        uint8_t firstFrame[8] = {0x02, 0x54, 0x03, 0x44,
                                 0xb8, 0x05, 0x00, 0x00}; // 0x05b844035402;
        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x00;
        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81,
                                  0x00,     0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file =
            "/tmp/out-gnr_bigcore_is_ierr_present.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/thread0",
            output);
        ASSERT_NE(child, nullptr);
        validateCrashlogVersionAndSize(child, "0x44035402", "0x05b8");

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread0/LBR31_INFO",
                                      output);
        ASSERT_FALSE(child == NULL);

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread0/CHECKSUM",
                                      output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_BigCore_Thread0_check)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        // Thread 0 non-sq with checksum
        uint8_t firstFrame[8] = {0x02, 0x54, 0x03, 0x44,
                                 0xb8, 0x05, 0x00, 0x00}; // 0x05b844035402;
        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x00;
        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81,
                                  0x00,     0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_bigcore_t0.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/thread0",
            output);
        ASSERT_NE(child, nullptr);
        validateCrashlogVersionAndSize(child, "0x44035402", "0x05b8");

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread0/LBR31_INFO",
                                      output);
        ASSERT_FALSE(child == NULL);

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread0/CHECKSUM",
                                      output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_BigCore_Thread1_with_SQ)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        uint8_t firstFrame[8] = {0x02, 0x54, 0x03, 0x44,
                                 0xb0, 0x05, 0x08, 0x04}; // 0x040805b044035402;

        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x01;
        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81,
                                  0x00,     0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_bigcore_t1_sq.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/thread1",
            output);
        ASSERT_FALSE(child == NULL);
        validateCrashlogVersionAndSize(child, "0x44035402", "0x040805b0");

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread1/CHECKSUM",
                                      output);

        // thread1 decode should not have CHECKSUM
        ASSERT_TRUE(child == NULL);

        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/SQS/entry64",
            output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_BigCore_Invalid_Version_Raw_Dump)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        // Invalid version
        uint8_t firstFrame[8] = {0x02, 0x54, 0x03, 0xFF,
                                 0xb0, 0x05, 0x08, 0x04}; // 0x040805b0FF035402;

        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x01;
        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81,
                                  0x00,     0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file =
            "/tmp/out-gnr_bigcore_t1_invalid_version_raw.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child =
            getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                  "big_core/core0/thread1/raw_0x0",
                                  output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0x040805b0ff035402");
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_BigCore_Invalid_Size_Raw_Dump)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        // Invalid size
        uint8_t sizeByteOne = 0x01;
        uint8_t firstFrame[8] = {
            0x02, 0x54,        0x03, 0x44,
            0xb0, sizeByteOne, 0x08, 0x04}; // 0x040801b044035402;
        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x01;
        uint8_t multiThread = 0x00; // 0x07 multi-thread

        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81, multiThread,
                                  0x00,     0x00, 0x00}; // 0x0781800000;

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file =
            "/tmp/out-gnr_bigcore_t1_invalid_size_raw.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child =
            getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                  "big_core/core0/thread1/raw_0x0",
                                  output);
        ASSERT_FALSE(child == NULL);
        EXPECT_STREQ(child->valuestring, "0x040801b044035402");
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_BigCore_Thread0_without_LBRs)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>((CPUModel)CPUIDs[i]),
                            SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        uint8_t firstFrame[8] = {0x02, 0x54, 0x03, 0x44,
                                 0x90, 0x02, 0x00, 0x00}; // 0x029044035402;
        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x00;
        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81,
                                  0x00,     0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_bigcore_t0_wo_lbr.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/thread0",
            output);
        validateCrashlogVersionAndSize(child, "0x44035402", "0x0290");

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread0/CHECKSUM",
                                      output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, createCrashdump_GNR_BigCore_Thread1_without_LBRs_with_SQ)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "BigCore");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "UnitTest";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;

        // Diemask setup - 0x211 - 1 compute die, 2 io dies
        uint8_t dieMask[8] = {0x11, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};

        EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
            .WillOnce(DoAll(SetArgPointee<1>(CPUIDs[i]), SetArgPointee<2>(1),
                            SetArgPointee<3>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
            .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                                  SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                                  Return(PECI_CC_SUCCESS)));

        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t mca_erc_src[8] = {0x00, 0x00, 0x08, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t coreMaskHigh[4] = {0x0, 0x0, 0x0, 0x0};
        uint8_t coreMaskLow[4] = {0x1, 0x0, 0x0, 0x0};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            // mock coremask
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                      SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(0x94), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_RdEndPointConfigPciLocal_seq_dom)
            .WillOnce(DoAll(SetArrayArgument<8>(mca_erc_src, mca_erc_src + 8),
                            SetArgPointee<10>(0x94), Return(PECI_CC_SUCCESS)));

        // Testing firstframe
        uint8_t firstFrame[8] = {0x02, 0x54, 0x03, 0x44,
                                 0x88, 0x02, 0x08, 0x04}; // 0x028844035402;
        uint8_t secondFrame[8] = {0x03, 0xca, 0x63, 0xf6,
                                  0x97, 0x03, 0x00, 0x00}; // 0x0397f663ca03;
        uint8_t thirdFrame[8] = {0x11, 0x00, 0x00, 0x88,
                                 0x03, 0x00, 0x00, 0x00}; // 0x0388000011;
        uint8_t threadID = 0x01;

        uint8_t fourthFrame[8] = {threadID, 0x00, 0x80, 0x81,
                                  0x00,     0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(secondFrame, secondFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(thirdFrame, thirdFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(fourthFrame, fourthFrame + 8),
                            SetArgPointee<7>(0x40), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(DoDefault());

        for (int i = 0; i < MAX_CPUS; i++)
        {
            memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        }

        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);
        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_bigcore_t1_wo_lbr_sq.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif

        cJSON* child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/thread1",
            output);
        validateCrashlogVersionAndSize(child, "0x44035402", "0x04080288");

        child = getJsonObjectFromPath("crash_data/PROCESSORS/cpu0/compute0/"
                                      "big_core/core0/thread1/CHECKSUM",
                                      output);

        // CHECKSUM should NOT be in thread1
        ASSERT_TRUE(child == NULL);

        child = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/big_core/core0/SQS/entry64",
            output);

        // Should have "entry64" in SQS as CHECKSUM
        ASSERT_FALSE(child == NULL);
    }
}

// TODO: Fix this test
TEST(CrashdumpTest,
     DISABLED_createCrashdump_GNR_crashlog_fail_crashdump_status_fail)
{
    Model testModels[] = {cd_gnr, cd_srf};
    CPUModel CPUIDs[] = {(CPUModel)GNR_MODEL, (CPUModel)SRF_MODEL};
    for (long unsigned int i = 0; i < (sizeof(testModels) / sizeof(Model)); i++)
    {
        TestCrashdump crashdump(testModels[i], "crashlog");
        CPUInfo* cpus = crashdump.cpus;
        std::string crashdumpContents;
        std::string triggerType = "IERR";
        std::string timestamp = newTimestamp();
        bool isTelemetry = false;
        uint8_t telemetry_answer0[1] = {0x01}; // Telemetry supported
        uint8_t telemetry_answer1[1] = {0x08}; // agents
        uint8_t cc = 0x40;
        uint8_t returnCrashlogDetails0[8] = {
            0x66, 0x55, 0x36, 0x0b,
            0xba, 0x84, 0x02, 0x00}; // size = 0x02,agent_id = 0x84ba0b36,
        uint8_t returnCrashlogDetails1[8] = {
            0x66, 0x55, 0x3f, 0xf4,
            0x56, 0x99, 0x02, 0x00}; // size = 0x02,agent_id = 0x9956f43f,
        uint8_t returnCrashlogDetails2[8] = {
            0x66, 0x55, 0x09, 0x3b,
            0x32, 0xd5, 0x02, 0x00}; // size = 0x02,agent_id = 0xd5323b09,
        uint8_t returnCrashlogDetails3[8] = {
            0x66, 0x55, 0x78, 0x14,
            0x6d, 0xf2, 0x02, 0x00}; // size = 0x02,agent_id = 0xf26d1478,
        uint8_t returnCrashlogDetails4[8] = {
            0x66, 0x55, 0x90, 0x3c,
            0x8e, 0x25, 0x02, 0x00}; // size = 0x02,agent_id = 0x258e3c90,
        uint8_t returnCrashlogDetails5[8] = {
            0x66, 0x55, 0x88, 0x99,
            0x06, 0x9f, 0x02, 0x00}; // size = 0x02,agent_id = 0x8899069f,
        uint8_t returnCrashlogDetails6[8] = {
            0x66, 0x55, 0x1f, 0x9c,
            0x4b, 0x28, 0x02, 0x00}; // size = 0x02,agent_id = 0x284b9c1f,
        uint8_t returnCrashlogDetails7[8] = {
            0x66, 0x55, 0x20, 0xb7,
            0xc9, 0x57, 0x02, 0x00}; // size = 0x02,agent_id = 0x57c9bf20,

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails6,
                                                returnCrashlogDetails6 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails7,
                                                returnCrashlogDetails7 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails6,
                                                returnCrashlogDetails6 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails7,
                                                returnCrashlogDetails7 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)));

        uint8_t returnValue[8] = {0x00, 0x01, 0x02, 0x03,
                                  0x04, 0x05, 0x06, 0x07};
        cc = 0x40;
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_Telemetry_GetCrashlogSample_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 8),
                      SetArgPointee<6>(cc), Return(PECI_CC_TIMEOUT)));
        cpus[0].clientAddr = MIN_CLIENT_ADDR;
        cpus[0].cpuidRead.cpuidValid = true;
        cpus[0].chaCountRead.chaCountValid = true;
        cpus[0].chaCount = 4;
        cpus[0].coreMaskRead.coreMaskValid = true;
        cpus[0].cpuidRead.cpuModel = static_cast<CPUModel>(CPUIDs[i]);
        cpus[0].model = testModels[i];
        cpus[0].dieMaskInfo.dieMask = 0x611;
        initComputeDieStructure(&cpus[0].dieMaskInfo, &cpus[0].computeDies);
        createCrashdump(cpus, crashdumpContents, triggerType, timestamp,
                        isTelemetry);

        ASSERT_TRUE(
            isCrashDumpFail(crashdump::sectionFailCount, triggerType.c_str()));

        cJSON* output = cJSON_Parse(crashdumpContents.c_str());

#ifdef DEBUG_FLAG
        std::filesystem::path file = "/tmp/out-gnr_crashlog_fail.json";
        std::filesystem::remove(file);
        debugcJSON(output, file.c_str(), false);
#endif
        // Spot check Crashlog Core section
        cJSON* agent0 = getJsonObjectFromPath(
            "crash_data/PROCESSORS/cpu0/compute0/crashlog/"
            "agent_id_0x84ba0b36/#data_PCODE_CRASH_INDEX",
            output);
        ASSERT_TRUE(agent0 == NULL);
        initComputeDieStructure(&cpus[0].dieMaskInfo, &cpus[0].computeDies);

        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails6,
                                                returnCrashlogDetails6 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails7,
                                                returnCrashlogDetails7 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails6,
                                                returnCrashlogDetails6 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails7,
                                                returnCrashlogDetails7 + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)));
        EXPECT_CALL(*crashdump.libPeciMock,
                    peci_Telemetry_GetCrashlogSample_dom)
            .WillRepeatedly(
                DoAll(SetArrayArgument<5>(returnValue, returnValue + 8),
                      SetArgPointee<6>(cc), Return(PECI_CC_TIMEOUT)));

        createCrashdump(cpus, crashdumpContents, "PostReset", timestamp,
                        isTelemetry);

        ASSERT_FALSE(isCrashDumpFail(crashdump::sectionFailCount, "PostReset"));
    }
}

TEST(CrashdumpTest, test_emr_startup)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpusInfo[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpusInfo[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)0x000C06f0), SetArgPointee<2>(0),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArrayArgument<7>(chaMaskHigh, chaMaskHigh + 4), // cha mask 0
            SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(
            DoAll(SetArrayArgument<7>(chaMaskLow, chaMaskLow + 4), // cha mask 1
                  SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)));
    initCPUInfo(cpusInfo);
    getCPUData(cpusInfo, STARTUP);
    // create_crashdump
    initCPUInfo(cpusInfo);
    getCPUData(cpusInfo, EVENT);
    EXPECT_TRUE(cpusInfo[0].cpuidRead.cpuidValid);
    EXPECT_EQ(cpusInfo[0].cpuidRead.source, STARTUP);
    EXPECT_EQ(cpusInfo[0].cpuidRead.cpuModel, (CPUModel)0x000C06f0); // EMR
}

TEST(CrashdumpTest, test_emr_event)
{
    TestCrashdump crashdump(cd_spr);
    CPUInfo cpusInfo[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpusInfo[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)0x000C06f0), SetArgPointee<2>(0),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<7>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(
            SetArrayArgument<7>(chaMaskHigh, chaMaskHigh + 4), // cha mask 0
            SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(
            DoAll(SetArrayArgument<7>(chaMaskLow, chaMaskLow + 4), // cha mask 1
                  SetArgPointee<8>(0x40), Return(PECI_CC_SUCCESS)));
    initCPUInfo(cpusInfo);
    getCPUData(cpusInfo, STARTUP);
    // create_crashdump
    initCPUInfo(cpusInfo);
    getCPUData(cpusInfo, EVENT);
    EXPECT_TRUE(cpusInfo[0].cpuidRead.cpuidValid);
    EXPECT_EQ(cpusInfo[0].cpuidRead.source, EVENT);
    EXPECT_EQ(cpusInfo[0].cpuidRead.cpuModel, (CPUModel)0x000C06f0); // EMR
}

TEST(CrashdumpTest, SRF_discovery_flow_startup)
{
    TestCrashdump crashdump(cd_srf);
    CPUInfo cpus[MAX_CPUS];
    crashdump.libPeciMock->DelegateForCrashdumpSetUp();

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)SRF_MODEL), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));

    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t coreMaskHigh2[4] = {0x09, 0x0a, 0x0b, 0x0c};
    uint8_t coreMaskLow2[4] = {0x0d, 0x0e, 0x0f, 0xff};

    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh2[4] = {0x09, 0x0a, 0x0b, 0x0c};
    uint8_t chaMaskLow2[4] = {0x0d, 0x0e, 0x0f, 0xff};

    // Example: 2 IO dies and 3 compute dies
    // ICCCI: 16b'0000 1110 0001 0001
    uint8_t Data[8] = {0x11, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(1)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        // mock coremask
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh2, coreMaskHigh2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow2, coreMaskLow2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        // mock chamask
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh2, chaMaskHigh2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow2, chaMaskLow2 + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                        SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)));

    initCPUInfo(cpus);
    getCPUData(cpus, STARTUP);

    EXPECT_EQ(cpus[0].cpuidRead.cpuModel, (CPUModel)SRF_MODEL);
    EXPECT_EQ(cpus[0].dieMaskInfo.dieMask, (uint32_t)0xe11);
    EXPECT_EQ(cpus[0].computeDies[0].coreMask, (uint64_t)0x0403020108070605);
    EXPECT_EQ(cpus[0].computeDies[1].coreMask, (uint64_t)0xff0f0e0d0c0b0a09);
    EXPECT_EQ(cpus[0].computeDies[2].coreMask, (uint64_t)0x0403020108070605);

    size_t chaCount =
        __builtin_popcount(0x04030201) + __builtin_popcount(0x08070605);
    EXPECT_EQ(cpus[0].computeDies[0].chaCount, chaCount);
    chaCount = __builtin_popcount(0xff0f0e0d) + __builtin_popcount(0x0c0b0a09);
    EXPECT_EQ(cpus[0].computeDies[1].chaCount, chaCount);
    chaCount = __builtin_popcount(0x04030201) + __builtin_popcount(0x08070605);
    EXPECT_EQ(cpus[0].computeDies[2].chaCount, chaCount);
    free(cpus[0].computeDies);
    free(cpus[1].computeDies);
    free(cpus[2].computeDies);
}

TEST(CrashdumpTest, GNR_UpdateGnrPcuDeviceNum_test)
{
    int dev;
    DieMaskInfo dieMaskInfo = {};
    dieMaskInfo.dieMask = 0xe11; // b1110 0001 0001
    initDieReadStructure(&dieMaskInfo, DieMaskRange.GNR.compute,
                         DieMaskRange.GNR.io);

    // 2 io dies and 3 compute dies
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 0);
    ASSERT_EQ(dev, 5);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 4);
    ASSERT_EQ(dev, 9);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 9);
    ASSERT_EQ(dev, 6);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 10);
    ASSERT_EQ(dev, 7);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 11);
    ASSERT_EQ(dev, 8);

    // 2 io dies and 3 compute dies with different die mask
    dieMaskInfo.dieMask = 0x2a12; // b0010 1010 0001 0010
    initDieReadStructure(&dieMaskInfo, DieMaskRange.GNR.compute,
                         DieMaskRange.GNR.io);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 1);
    ASSERT_EQ(dev, 5);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 4);
    ASSERT_EQ(dev, 9);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 9);
    ASSERT_EQ(dev, 6);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 11);
    ASSERT_EQ(dev, 7);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 13);
    ASSERT_EQ(dev, 8);

    // 2 io dies and 1 compute dies with different die mask
    dieMaskInfo.dieMask = 0x211; // b0010 0001 0001
    initDieReadStructure(&dieMaskInfo, DieMaskRange.GNR.compute,
                         DieMaskRange.GNR.io);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 0);
    ASSERT_EQ(dev, 5);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 4);
    ASSERT_EQ(dev, 9);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 9);
    ASSERT_EQ(dev, 6);
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 11);
    ASSERT_EQ(dev, 5); // default to 5 for invalid diemask case
    dev = UpdateGnrPcuDeviceNum(&dieMaskInfo, 13);
    ASSERT_EQ(dev, 5); // default to 5 for invalid diemask case
}

TEST(CrashdumpTest, GNR_ParseExplicitDomain_test)
{
    char* IO_DOMAIN = "io";
    char* COMPUTE_DOMAIN = "compute";
    char* valuestring;
    uint32_t domain;
    acdStatus result;

    // Proper usecase low end
    valuestring = "io1";
    domain = 0;
    result = parseExplicitDomain(IO_DOMAIN, valuestring, &domain);
    EXPECT_EQ(result, ACD_SUCCESS);
    EXPECT_EQ(domain, 1u);

    valuestring = "compute1";
    domain = 0;
    result = parseExplicitDomain(COMPUTE_DOMAIN, valuestring, &domain);
    EXPECT_EQ(result, ACD_SUCCESS);
    EXPECT_EQ(domain, 1u);

    // Proper usecase high end
    valuestring = "io7";
    domain = 0;
    result = parseExplicitDomain(IO_DOMAIN, valuestring, &domain);
    EXPECT_EQ(result, ACD_SUCCESS);
    EXPECT_EQ(domain, 7u);

    valuestring = "compute23";
    domain = 0;
    result = parseExplicitDomain(COMPUTE_DOMAIN, valuestring, &domain);
    EXPECT_EQ(result, ACD_SUCCESS);
    EXPECT_EQ(domain, 23u);

    /* Handle different letter cases
    // Note: this requires memory location rather than hardcoded string (not
    const)
    //
    // This shouldn't be an issue within then engine as it's input is
    dynamically allocated.
    */
    char valuestring5[5] = "Io0";
    domain = 0;
    result = parseExplicitDomain(IO_DOMAIN, valuestring5, &domain);
    EXPECT_EQ(result, ACD_SUCCESS);
    EXPECT_EQ(domain, 0u);

    // Input 3 digits
    valuestring = "compute100";
    domain = 0;
    result = parseExplicitDomain(COMPUTE_DOMAIN, valuestring, &domain);
    EXPECT_EQ(result, ACD_SUCCESS);
    EXPECT_EQ(domain, 100u);

    // Invalid domain name
    valuestring = "domain1";
    domain = 0;
    result = parseExplicitDomain(COMPUTE_DOMAIN, valuestring, &domain);
    EXPECT_EQ(result, ACD_FAILURE);
}

TEST(CrashdumpTest, GNR_MapDieNumToBit_test)
{
    uint32_t dieMask;
    uint32_t domainPos;
    uint32_t mappedDie;

    // GNR ranges
    uint32_t ioRange = 0xFF;
    uint32_t computeRange = 0xFFFFFF00;

    // Invalid DieMask
    dieMask = 0x00000000; // no io/compute
    domainPos = 1;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, (uint32_t)DIE_MAP_ERROR_VALUE);

    // Invalid Range
    dieMask = 0x00000088; // io0:3 io1:7
    domainPos = 1;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, 0x00); // 0x00 is invalid
    EXPECT_EQ(mappedDie, (uint32_t)DIE_MAP_ERROR_VALUE);

    // Position not found
    dieMask = 0x00000088; // io0:3 io1:7
    domainPos = 3;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, (uint32_t)DIE_MAP_ERROR_VALUE);

    // Normal usecase IO
    dieMask = 0x10000081; // io0:0 io1:7 compute0:20
    // Request io0
    domainPos = 0;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, 0u);
    // Request io1
    domainPos = 1;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, 7u);

    // Normal usecase Compute
    dieMask = 0x80000188; // io0:3 io1:7 compute0:8 compute1:31
    // Request compute0
    domainPos = 0;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, computeRange);
    EXPECT_EQ(mappedDie, 8u);
    // Request compute1
    domainPos = 1;
    mappedDie = mapDieNumToBitGNR(dieMask, domainPos, computeRange);
    EXPECT_EQ(mappedDie, 31u);
}
