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
#include <regex>

#include "../../engine/BigCore.h"
#include "../../engine/TorDump.h"
#include "../../engine/crashdump.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

extern "C" {
}

using namespace ::testing;
using namespace crashdump;

// Notes: uncomment to enable debug
// #define DEBUG_FLAG

void setupRdPkgConfigOneCpuThreeCDies(TestCrashdump& crashdump)
{
    static uint8_t dieMask[8] = {0x11, 0x0e};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)));
}

void setupRdPkgConfigOneCpuTwoCDies(TestCrashdump& crashdump)
{
    static uint8_t dieMask[8] = {0x11, 0x06};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)));
}

void setupRdPkgConfigOneCpuOneCDie(TestCrashdump& crashdump)
{
    static uint8_t dieMask[8] = {0x11, 0x02};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)));
}

void setupRdPkgConfigOneCpu(TestCrashdump& crashdump)
{
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
}

bool crashdumpFileWithModifierExists(const std::string& modifier)
{
    std::string pattern = "crashdump_.*_" + modifier + "\\.json";
    std::regex r(pattern);

    for (const auto& entry :
         std::filesystem::directory_iterator("/tmp/crashdump/output"))
    {
        if (std::regex_match(entry.path().filename().string(), r))
        {
            return true;
        }
    }
    return false;
}

void checkLLCRegisters(cJSON* output, int expectedSize)
{
    char jsonPathStr[NAME_STR_LEN];
    char regStr[NAME_STR_LEN];
    int keysCount = 5;
    char keys[keysCount][NAME_STR_LEN] = {
        "llc%d_status", "llc%d_misc2", "llc%d_ctl", "llc%d_addr", "llc%d_misc"};

    cJSON* child;
    int cpuCount = 1;
    for (int cpu = 0; cpu < cpuCount; cpu++)
    {
        for (int llc = 0; llc < expectedSize; llc++)
        {
            for (int idx = 0; idx < keysCount; idx++)
            {
                cd_snprintf_s(regStr, sizeof(regStr), keys[idx], llc);
                cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                              "crash_data/PROCESSORS/cpu%d/soc/MCA/LLC%d/%s",
                              cpu, llc, regStr);
                child = getJsonObjectFromPath(jsonPathStr, output);
                ASSERT_FALSE(child == NULL) << "idx:" << idx;
            }
        }
        // time key location check
        cd_snprintf_s(jsonPathStr, sizeof(jsonPathStr),
                      "crash_data/PROCESSORS/cpu%d/soc/MCA/_time_MCA_LLC", cpu);
        child = getJsonObjectFromPath(jsonPathStr, output);
        ASSERT_FALSE(child == NULL);
    }
}

TEST(CrashdumpTest, test_getClientAddrs_all_cpus_on)
{
    TestCrashdump crashdump(cd_gnr);

    manageGetClientAddrs_noSeq(crashdump, MAX_CPUS);

    uint8_t validAddrs = getClientAddrs(crashdump.cpus);
    EXPECT_EQ(validAddrs, MAX_CPUS);
}

TEST(CrashdumpTest, test_getClientAddrs_all_cpus_off)
{
    TestCrashdump crashdump(cd_gnr);

    manageGetClientAddrs(crashdump, 0);

    uint8_t validAddrs = getClientAddrs(crashdump.cpus);
    EXPECT_EQ(validAddrs, 0);
}

TEST(CrashdumpTest, test_initCPUInfo_cpus_empty_all_cpus_on)
{
    TestCrashdump crashdump(cd_gnr);

    manageGetClientAddrs_noSeq(crashdump, MAX_CPUS);

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&crashdump.cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    acdStatus initStatus = initCPUInfo(crashdump.cpus);

    EXPECT_EQ(initStatus, ACD_SUCCESS);
    EXPECT_EQ(crashdump.cpus[0].cpuidRead.source, 2);
    EXPECT_EQ(crashdump.cpus[0].coreMaskRead.source, 2);
    EXPECT_EQ(crashdump.cpus[0].chaCountRead.source, 2);
    EXPECT_FALSE(crashdump.cpus[0].cpuidRead.cpuidValid);
    EXPECT_FALSE(crashdump.cpus[0].coreMaskRead.coreMaskValid);
    EXPECT_FALSE(crashdump.cpus[0].chaCountRead.chaCountValid);
}

TEST(CrashdumpTest, test_initCPUInfo_cpus_empty_all_cpus_off)
{
    TestCrashdump crashdump(cd_gnr);

    manageGetClientAddrs(crashdump, 0);

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&crashdump.cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    acdStatus initStatus = initCPUInfo(crashdump.cpus);
    EXPECT_EQ(initStatus, ACD_PING_ERROR);
}

TEST(CrashdumpTest, test_initCPUInfo_cpus_not_empty)
{
    TestCrashdump crashdump(cd_gnr);

    acdStatus initStatus = initCPUInfo(crashdump.cpus);
    EXPECT_EQ(initStatus, ACD_FAILURE);
}

TEST(CrashdumpTest, test_isPECIAvailable_cpus_empty_all_cpus_on)
{
    TestCrashdump crashdump(cd_gnr);

    manageGetClientAddrs_noSeq(crashdump, MAX_CPUS);

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&crashdump.cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    EXPECT_TRUE(crashdump::isPECIAvailable());
}

TEST(CrashdumpTest, test_isPECIAvailable_cpus_empty_all_cpus_off)
{
    TestCrashdump crashdump(cd_gnr);

    manageGetClientAddrs(crashdump, 0);

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&crashdump.cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    EXPECT_TRUE(crashdump::isPECIAvailable());
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
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
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
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)));
    ;
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
        .initialPeciWake = ON_PC0,
        .inputFile = {},
        .cpuidRead = {},
        .chaCountRead = {},
        .coreMaskRead = {},
        .dimmMask = 0,
        .platformID = 0x0,
        .ucodePatch = 0x0,
        .coreType = ACD_BIG_CORE,
        .crashdumpDiscoveryMask = 0x0,
    };

    cpus[1] = {
        .clientAddr = 49,
        .model = (Model)0x0,
        .dieMaskInfo = {},
        .computeDies = {},
        .coreMask = 0x0,
        .crashedCoreMask = 0x0,
        .chaCount = 0,
        .initialPeciWake = ON_PC0,
        .inputFile = {},
        .cpuidRead = {},
        .chaCountRead = {},
        .coreMaskRead = {},
        .dimmMask = 0,
        .platformID = 0x0,
        .ucodePatch = 0x0,
        .coreType = ACD_BIG_CORE,
        .crashdumpDiscoveryMask = 0x0,
    };

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

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

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
    EXPECT_EQ(cpus[0].cpuidRead.source, OVERWRITTEN);
    EXPECT_EQ(cpus[1].cpuidRead.source, OVERWRITTEN);
    // Notes: Default model is GNR
    EXPECT_EQ(cpus[0].model, cd_gnr);
    EXPECT_EQ(cpus[1].model, cd_gnr);
    EXPECT_EQ(cpus[0].cpuidRead.cpuModel, GNR_MODEL);
    EXPECT_EQ(cpus[1].cpuidRead.cpuModel, GNR_MODEL);
    EXPECT_EQ(cpus[0].chaCount, chaExpectedValue0);
    EXPECT_EQ(cpus[0].coreMask, coreExpectedValue0);
    EXPECT_EQ(cpus[1].chaCount, chaExpectedValue0);
    EXPECT_EQ(cpus[1].coreMask, coreExpectedValue0);
}

TEST(CrashdumpTest, test_overwriteCPUInfo_gnr)
{
    TestCrashdump crashdump(cd_gnr);
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
        .initialPeciWake = ON_PC0,
        .inputFile = {},
        .cpuidRead = {},
        .chaCountRead = {},
        .coreMaskRead = {},
        .dimmMask = 0,
        .platformID = 0x0,
        .ucodePatch = 0x0,
        .coreType = ACD_BIG_CORE,
        .crashdumpDiscoveryMask = 0x0,
    };
    cpus[1] = {
        .clientAddr = 49,
        .model = (Model)0x0,
        .dieMaskInfo = {},
        .computeDies = {},
        .coreMask = 0x0,
        .crashedCoreMask = 0x0,
        .chaCount = 0,
        .initialPeciWake = ON_PC0,
        .inputFile = {},
        .cpuidRead = {},
        .chaCountRead = {},
        .coreMaskRead = {},
        .dimmMask = 0,
        .platformID = 0x0,
        .ucodePatch = 0x0,
        .coreType = ACD_BIG_CORE,
        .crashdumpDiscoveryMask = 0x0,
    };

    // CPU 0 is invalid
    cpus[0].cpuidRead.cpuidValid = false;
    cpus[0].chaCountRead.chaCountValid = false;
    cpus[0].coreMaskRead.coreMaskValid = false;
    // CPU 1 is valid
    cpus[1].cpuidRead.cpuidValid = true;
    cpus[1].cpuidRead.source = STARTUP;
    cpus[1].chaCountRead.chaCountValid = true;
    cpus[1].coreMaskRead.coreMaskValid = true;
    cpus[1].model = cd_gnr;
    cpus[1].cpuidRead.cpuModel = gnr;

    overwriteCPUInfo(cpus);

    EXPECT_TRUE(cpus[0].cpuidRead.cpuidValid);
    EXPECT_TRUE(cpus[1].cpuidRead.cpuidValid);
    EXPECT_EQ(cpus[0].cpuidRead.source, OVERWRITTEN);
    EXPECT_EQ(cpus[1].cpuidRead.source, STARTUP);
    EXPECT_EQ(cpus[0].cpuidRead.cpuModel, gnr);
    EXPECT_EQ(cpus[1].cpuidRead.cpuModel, gnr);
    EXPECT_EQ(cpus[0].model, cd_gnr);
    EXPECT_EQ(cpus[1].model, cd_gnr);
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
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
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

TEST(CrashdumpTest, test_emr_startup)
{
    TestCrashdump crashdump(cd_emr);
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

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

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
    TestCrashdump crashdump(cd_emr);
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
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
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

    uint8_t dieMask[8] = {0x11, 0x0e};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_TIMEOUT)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
        .WillOnce(DoAll(SetArrayArgument<4>(dieMask, dieMask + 8),
                        SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                        Return(PECI_CC_SUCCESS)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .WillOnce(DoAll(
            SetArgPointee<1>((CPUModel)SRF_MODEL), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));

    uint8_t coreMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t coreMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t coreMaskHigh2[4] = {0x09, 0x0a, 0x0b, 0x0c};
    uint8_t coreMaskLow2[4] = {0x0d, 0x0e, 0x0f, 0xff};

    uint8_t chaMaskHigh[4] = {0x05, 0x06, 0x07, 0x08};
    uint8_t chaMaskLow[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chaMaskHigh2[4] = {0x09, 0x0a, 0x0b, 0x0c};
    uint8_t chaMaskLow2[4] = {0x0d, 0x0e, 0x0f, 0xff};

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
    mappedDie = mapDieNumToBit(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, (uint32_t)DIE_MAP_ERROR_VALUE);

    // Invalid Range
    dieMask = 0x00000088; // io0:3 io1:7
    domainPos = 1;
    mappedDie = mapDieNumToBit(dieMask, domainPos, 0x00); // 0x00 is invalid
    EXPECT_EQ(mappedDie, (uint32_t)DIE_MAP_ERROR_VALUE);

    // Position not found
    dieMask = 0x00000088; // io0:3 io1:7
    domainPos = 3;
    mappedDie = mapDieNumToBit(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, (uint32_t)DIE_MAP_ERROR_VALUE);

    // Normal usecase IO
    dieMask = 0x10000081; // io0:0 io1:7 compute0:20
    // Request io0
    domainPos = 0;
    mappedDie = mapDieNumToBit(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, 0u);
    // Request io1
    domainPos = 1;
    mappedDie = mapDieNumToBit(dieMask, domainPos, ioRange);
    EXPECT_EQ(mappedDie, 7u);

    // Normal usecase Compute
    dieMask = 0x80000188; // io0:3 io1:7 compute0:8 compute1:31
    // Request compute0
    domainPos = 0;
    mappedDie = mapDieNumToBit(dieMask, domainPos, computeRange);
    EXPECT_EQ(mappedDie, 8u);
    // Request compute1
    domainPos = 1;
    mappedDie = mapDieNumToBit(dieMask, domainPos, computeRange);
    EXPECT_EQ(mappedDie, 31u);
}

TEST(CrashdumpTest, getUcodePatchVers_invalid_test)
{
    TestCrashdump crashdump(cd_gnr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    uint8_t Data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x7, 0x8};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_INVALID_REQ),
                              Return(PECI_CC_INVALID_REQ)));

    cpus[0].clientAddr = MIN_CLIENT_ADDR;
    getUcodePatchVers(cpus);
    EXPECT_EQ(cpus[0].ucodePatch, (uint32_t)0x0);
}

TEST(CrashdumpTest, getUcodePatchVers_valid_test)
{
    TestCrashdump crashdump(cd_gnr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    uint8_t Data[8] = {0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x0, 0x0};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    cpus[0].clientAddr = MIN_CLIENT_ADDR;
    getUcodePatchVers(cpus);
    EXPECT_EQ(cpus[0].ucodePatch, (uint32_t)0x04030201);
}

TEST(CrashdumpTest, getPlatformIDs_invalid_test)
{
    TestCrashdump crashdump(cd_gnr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    uint8_t Data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x7, 0x8};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_INVALID_REQ),
                              Return(PECI_CC_INVALID_REQ)));

    cpus[0].clientAddr = MIN_CLIENT_ADDR;
    getPlatformIDs(cpus);
    EXPECT_EQ(cpus[0].platformID, (uint32_t)0x0);
}

TEST(CrashdumpTest, getPlatformIDs_valid_test)
{
    TestCrashdump crashdump(cd_gnr);
    CPUInfo cpus[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    uint8_t Data[8] = {0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x0, 0x0};
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    cpus[0].clientAddr = MIN_CLIENT_ADDR;
    getPlatformIDs(cpus);
    EXPECT_EQ(cpus[0].platformID, (uint32_t)0x04030201);
}

TEST(CrashdumpTest, loadInputFiles_fullFlow)
{
    // TODO: Fix reading input file
    CPUInfo cpus[MAX_CPUS];
    uint8_t addr = MIN_CLIENT_ADDR;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].clientAddr = addr;
        cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(GNR_MODEL);
        cpus[i].model = cd_gnr;
        addr++;
    }

    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    bool isTelemetry = false;

    acdStatus ret = loadInputFiles(cpus, &inputFileInfo, isTelemetry);
    EXPECT_EQ(ret, ACD_SUCCESS);
    EXPECT_FALSE(cpus[0].inputFile.filenamePtr == NULL);
}
TEST(CrashdumpTest, loadInputFiles_bufferNULL)
{
    // TODO: Fix reading input file
    CPUInfo cpus[MAX_CPUS];
    uint8_t addr = MIN_CLIENT_ADDR;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].clientAddr = addr;
        // Obtain NULL buffer via selectAndReadInputFile of invalid model
        // 0x000606A0 == ICX CPUMODEL
        cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(0x000606A0);
        cpus[i].model = cd_icx;
        addr++;
    }

    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    bool isTelemetry = false;

    acdStatus ret = loadInputFiles(cpus, &inputFileInfo, isTelemetry);
    EXPECT_EQ(ret, ACD_INPUT_FILE_ERROR);
}
TEST(CrashdumpTest, loadInputFiles_telemetry_fullFlow)
{
    // TODO: Fix reading input file
    CPUInfo cpus[MAX_CPUS];
    uint8_t addr = MIN_CLIENT_ADDR;
    for (int i = 0; i < MAX_CPUS; i++)
    {
        cpus[i].clientAddr = addr;
        cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(GNR_MODEL);
        cpus[i].model = cd_gnr;
        addr++;
    }

    InputFileInfo inputFileInfo = {
        .unique = true, .filenames = {NULL}, .buffers = {NULL}};

    bool isTelemetry = true;

    acdStatus ret = loadInputFiles(cpus, &inputFileInfo, isTelemetry);
    EXPECT_EQ(ret, ACD_SUCCESS);
    EXPECT_FALSE(cpus[0].inputFile.filenamePtr == NULL);
}

TEST(CrashdumpTest, loadInputFiles_override)
{
    CPUInfo cpus[MAX_CPUS];
    uint8_t addr = MIN_CLIENT_ADDR;

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
        cpus[i].clientAddr = addr;
        cpus[i].cpuidRead.cpuModel = static_cast<CPUModel>(SPR_MODEL);
        cpus[i].model = cd_spr;
        addr++;
    }

    InputFileInfo inputFileInfo = {
        .unique = true,
        .filenames = {NULL},
        .buffers = {NULL},
    };
    memset_s(inputFileInfo.overridePath, MAX_PATH_LEN, 0, MAX_PATH_LEN);

    bool isTelemetry = false;
    std::string sourceDir = SOURCE_DIR;
    std::string inputFilePath = sourceDir + "/";

    strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN, inputFilePath.c_str());

    acdStatus ret = loadInputFiles(cpus, &inputFileInfo, isTelemetry);

    // Make sure input file is read correctly & all pointers are cleanup.
    EXPECT_EQ(ret, ACD_SUCCESS);
    cJSON* cpu =
        cJSON_GetObjectItemCaseSensitive(cpus[0].inputFile.bufferPtr, "cpu");
    ASSERT_FALSE(cpu == NULL);

    EXPECT_STREQ(cpu->valuestring, "spr");
    EXPECT_FALSE(cpus[0].inputFile.filenamePtr == NULL);
    cleanupInputFiles(cpus, &inputFileInfo);
    for (int i = 0; i < MAX_CPUS; i++)
    {
        EXPECT_TRUE(cpus[i].inputFile.filenamePtr == NULL);
    }
    for (int i = 0; i < cd_numberOfModels; i++)
    {
        EXPECT_TRUE(inputFileInfo.filenames[i] == NULL);
    }

    // Negative Test
    inputFilePath = "/invalid/path/";
    strcpy_s(inputFileInfo.overridePath, MAX_PATH_LEN, inputFilePath.c_str());
    ret = loadInputFiles(cpus, &inputFileInfo, isTelemetry);
    EXPECT_EQ(ret, ACD_INPUT_FILE_ERROR);
}

TEST(CrashdumpTest, setPECIAddressAndDeviceName)
{
    acdStatus status;
    CPUInfo cpus[MAX_CPUS];

    for (int i = 0; i < MAX_CPUS; i++)
    {
        memset_s(&cpus[i], sizeof(CPUInfo), 0, sizeof(CPUInfo));
    }

    int cpu0Addr = 0x30;
    int cpu1Addr = 0x31;
    int cpu2Addr = 0x32;
    int cpu3Addr = 0x33;
    int cpu4Addr = 0x34;
    int cpu5Addr = 0x35;
    int cpu6Addr = 0x36;
    int cpu7Addr = 0x37;

    setPECIAddressAndDeviceName(&cpus[0], cpu0Addr, "/dev/peci-0");
    setPECIAddressAndDeviceName(&cpus[1], cpu1Addr, "/dev/peci-1");
    setPECIAddressAndDeviceName(&cpus[2], cpu2Addr, "/dev/peci-2");
    setPECIAddressAndDeviceName(&cpus[3], cpu3Addr, "/dev/peci-3");
    setPECIAddressAndDeviceName(&cpus[4], cpu4Addr, "/dev/peci-4");
    setPECIAddressAndDeviceName(&cpus[5], cpu5Addr, "/dev/peci-5");
    setPECIAddressAndDeviceName(&cpus[6], cpu6Addr, "/dev/peci-6");
    status = setPECIAddressAndDeviceName(&cpus[7], cpu7Addr, "/dev/peci-7");
    EXPECT_EQ(status, ACD_SUCCESS);

    EXPECT_EQ(cpus[0].clientAddr, cpu0Addr);
    EXPECT_STREQ(cpus[0].devName, "/dev/peci-0");
    EXPECT_EQ(cpus[1].clientAddr, cpu1Addr);
    EXPECT_STREQ(cpus[1].devName, "/dev/peci-1");
    EXPECT_EQ(cpus[2].clientAddr, cpu2Addr);
    EXPECT_STREQ(cpus[2].devName, "/dev/peci-2");
    EXPECT_EQ(cpus[3].clientAddr, cpu3Addr);
    EXPECT_STREQ(cpus[3].devName, "/dev/peci-3");
    EXPECT_EQ(cpus[4].clientAddr, cpu4Addr);
    EXPECT_STREQ(cpus[4].devName, "/dev/peci-4");
    EXPECT_EQ(cpus[5].clientAddr, cpu5Addr);
    EXPECT_STREQ(cpus[5].devName, "/dev/peci-5");
    EXPECT_EQ(cpus[6].clientAddr, cpu6Addr);
    EXPECT_STREQ(cpus[6].devName, "/dev/peci-6");
    EXPECT_EQ(cpus[7].clientAddr, cpu7Addr);
    EXPECT_STREQ(cpus[7].devName, "/dev/peci-7");

    // Negative Test Cases
    status = setPECIAddressAndDeviceName(&cpus[0], cpu0Addr, NULL);
    EXPECT_EQ(status, ACD_DEVICE_NAME_ERROR);
    status = setPECIAddressAndDeviceName(NULL, cpu0Addr, "/dev/peci-0");
    EXPECT_EQ(status, ACD_DEVICE_NAME_ERROR);
}

TEST(CrashdumpTest, setPeciWakeOfDomain)
{
    TestCrashdump crashdump(cd_gnr);

    uint8_t clientAddr = 0x30;
    uint8_t domainId = 0;
    uint8_t writeValue = ON_PC0;

    InSequence s;
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_INVALID_REQ)));

    setPeciWakeOfDomain(clientAddr, domainId, writeValue);
    setPeciWakeOfDomain(clientAddr, domainId, writeValue);
}

TEST(CrashdumpTest, checkPeciWakeOfDomain)
{
    TestCrashdump crashdump(cd_gnr);

    uint8_t clientAddr = 0x30;
    uint8_t domainId = 0;

    InSequence s;
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_INVALID_REQ)));

    checkPeciWakeOfDomain(clientAddr, domainId);
    checkPeciWakeOfDomain(clientAddr, domainId);
}

TEST(CrashdumpTest, savePeciWakeOfDomain)
{
    TestCrashdump crashdump(cd_gnr);

    uint8_t clientAddr = 0x30;
    uint8_t domainId = 0;
    uint8_t cc = 0x40;
    pwState peciWakeState;

    InSequence s;

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC0), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC2), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(OFF), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));

    peciWakeState = savePeciWakeOfDomain(clientAddr, domainId);
    EXPECT_EQ(peciWakeState, ON_PC0);
    peciWakeState = savePeciWakeOfDomain(clientAddr, domainId);
    EXPECT_EQ(peciWakeState, ON_PC2);
    peciWakeState = savePeciWakeOfDomain(clientAddr, domainId);
    EXPECT_EQ(peciWakeState, OFF);
}

TEST(CrashdumpTest, savePeciWake_dieMaskSupported)
{
    TestCrashdump crashdump(cd_gnr);

    int numOfCPU = 1;
    for (int i = 0; i < numOfCPU; i++)
    {
        crashdump.cpus[i].cpuidRead.cpuidValid = true;
        initDieReadStructure(&crashdump.cpus[i].dieMaskInfo,
                             DieMaskRange.GNR.compute, DieMaskRange.GNR.io);
        initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                &crashdump.cpus[i].computeDies);
        crashdump.cpus[i].computeDies[0].coreMask = 0x1;
        crashdump.cpus[i].dieMaskInfo.dieMaskSupported = true;
    }

    uint8_t cc = 0x40;

    InSequence s;
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC0), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC2), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC0), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));

    savePeciWake(crashdump.cpus);
    EXPECT_EQ(crashdump.cpus[0].dieMaskInfo.compute.initialPeciWake[0], ON_PC0);
    EXPECT_EQ(crashdump.cpus[0].dieMaskInfo.io.initialPeciWake[0], ON_PC2);
    EXPECT_EQ(crashdump.cpus[0].dieMaskInfo.io.initialPeciWake[1], ON_PC0);
}

TEST(CrashdumpTest, checkPeciWake_dieMaskSupported)
{
    TestCrashdump crashdump(cd_gnr);

    int numOfCPU = 1;
    for (int i = 0; i < numOfCPU; i++)
    {
        crashdump.cpus[i].cpuidRead.cpuidValid = true;
        initDieReadStructure(&crashdump.cpus[i].dieMaskInfo,
                             DieMaskRange.GNR.compute, DieMaskRange.GNR.io);
        initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                &crashdump.cpus[i].computeDies);
        crashdump.cpus[i].computeDies[0].coreMask = 0x1;
        crashdump.cpus[i].dieMaskInfo.dieMaskSupported = true;
    }

    uint8_t cc = 0x40;

    InSequence s;
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC0), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC0), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
        .WillOnce(DoAll(SetArgPointee<5>(ON_PC0), SetArgPointee<6>(cc),
                        Return(PECI_CC_SUCCESS)));

    int ret;
    ret = checkPeciWake(crashdump.cpus);
    EXPECT_EQ(ret, 1);
}

TEST(CrashdumpTest, setPeciWake_dieMaskSupported)
{
    TestCrashdump crashdump(cd_gnr);

    int numOfCPU = 1;
    for (int i = 0; i < numOfCPU; i++)
    {
        crashdump.cpus[i].cpuidRead.cpuidValid = true;
        initDieReadStructure(&crashdump.cpus[i].dieMaskInfo,
                             DieMaskRange.GNR.compute, DieMaskRange.GNR.io);
        initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                &crashdump.cpus[i].computeDies);
        crashdump.cpus[i].computeDies[0].coreMask = 0x1;
        crashdump.cpus[i].dieMaskInfo.dieMaskSupported = true;
    }

    uint8_t cc = 0x40;

    InSequence s;
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

    int ret;
    ret = setPeciWake(crashdump.cpus, ON_PC0);
    EXPECT_EQ(ret, 1);
}

TEST(CrashdumpTest, setPeciWakeInitial_dieMaskSupported)
{
    TestCrashdump crashdump(cd_gnr);

    int numOfCPU = 1;
    for (int i = 0; i < numOfCPU; i++)
    {
        crashdump.cpus[i].cpuidRead.cpuidValid = true;
        initDieReadStructure(&crashdump.cpus[i].dieMaskInfo,
                             DieMaskRange.GNR.compute, DieMaskRange.GNR.io);
        initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                &crashdump.cpus[i].computeDies);
        crashdump.cpus[i].computeDies[0].coreMask = 0x1;
        crashdump.cpus[i].dieMaskInfo.dieMaskSupported = true;
    }

    crashdump.cpus[0].dieMaskInfo.compute.initialPeciWake[0] = ON_PC0;
    crashdump.cpus[0].dieMaskInfo.io.initialPeciWake[0] = ON_PC2;
    crashdump.cpus[0].dieMaskInfo.io.initialPeciWake[1] = OFF;

    uint8_t cc = 0x40;

    InSequence s;
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    EXPECT_CALL(*crashdump.libPeciMock, peci_WrPkgConfig_dom)
        .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

    setPeciWakeInitial(crashdump.cpus);
}
