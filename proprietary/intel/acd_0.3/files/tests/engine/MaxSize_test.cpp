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

#include "../mock/test_crashdump.hpp"
#include "../test_utils.hpp"
#include "crashdump.hpp"

extern "C" {
#include <search.h>

#include "engine/cmdprocessor.h"
#include "engine/crashdump.h"
#include "engine/flow.h"
#include "engine/inputparser.h"
#include "engine/logger.h"
#include "engine/utils.h"
}

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using std::filesystem::current_path;
using ::testing::Return;
#include <fstream>

cJSON* outRoot = cJSON_CreateObject();
size_t nCha = 60;
int nCPUs = 2;
int globalCC = PECI_DEV_CC_SUCCESS;

std::filesystem::path out_file = "max_output.json";

static int getNumberOfRepetitions(cJSON* sections, char* section, char* command)
{
    cJSON* sectionObj =
        cJSON_GetObjectItemCaseSensitive(sections->child, section);
    cJSON* peciCmds = cJSON_GetObjectItemCaseSensitive(sectionObj, "Commands");
    cJSON* commandObjs =
        cJSON_GetObjectItemCaseSensitive(peciCmds->child, command);

    int nRegs = cJSON_GetArraySize(commandObjs);
    return nRegs;
}

static int getNumberOfRepetitionsUncorePCI(cJSON* sections)
{
    cJSON* sectionObj =
        cJSON_GetObjectItemCaseSensitive(sections->child, "Uncore_PCI");
    cJSON* peciCmds = cJSON_GetObjectItemCaseSensitive(sectionObj, "Commands");
    cJSON* commandObjs = cJSON_GetObjectItemCaseSensitive(
        peciCmds->child, "RdEndpointConfigPCILocal");

    int n_rx_4 = 0; // 1 peci call for each
    int n_rx_8 = 0; // 1 peci call for each
    cJSON* peci_call = NULL;
    cJSON_ArrayForEach(peci_call, commandObjs)
    {
        unsigned char rx_len =
            cJSON_GetArrayItem(peci_call->child, 6)->valueint;

        switch (rx_len)
        {
            case sizeof(uint8_t):
            case sizeof(uint16_t):
            case sizeof(uint32_t):
                n_rx_4++;
                break;
            case sizeof(uint64_t):
                n_rx_8++;
                break;
        }
    }
    int total_reps = n_rx_4 + (2 * n_rx_8);

    return total_reps;
}

static int getNumberOfRepetitionsPM_pll(cJSON* sections)
{
    cJSON* sectionObj = cJSON_GetObjectItemCaseSensitive(sections->child,
                                                         "PowerManagement_Pll");
    cJSON* peciCmds = cJSON_GetObjectItemCaseSensitive(sectionObj, "Commands");
    cJSON* commandObjs =
        cJSON_GetObjectItemCaseSensitive(peciCmds->child, "RdPkgConfig");

    int totalReps = 0;
    cJSON* peci_call = NULL;
    cJSON_ArrayForEach(peci_call, commandObjs)
    {
        if (cJSON_HasObjectItem(peci_call, "Repeat"))
        {
            cJSON* repeat =
                cJSON_GetObjectItemCaseSensitive(peci_call, "Repeat");
            totalReps += repeat->valueint;
        }
        else
        {
            totalReps++;
        }
    }

    return totalReps;
}

static int getNoActiveCores(CPUInfo& cpu)
{
    int coresActive = 0;

    for (uint8_t u8CoreNum = 0; u8CoreNum < MAX_CORE_MASK; u8CoreNum++)
    {
        if (!CHECK_BIT(cpu.coreMask, u8CoreNum))
        {
            continue;
        }
        if (CHECK_BIT(cpu.crashedCoreMask, u8CoreNum))
        {
            continue;
        }
        coresActive++;
    }
    return coresActive;
}

class MaxSizeTestFixture : public ::testing::Test
{
  protected:
    static cJSON* inRoot;
    static std::filesystem::path UTFile;
    static void readSampleFile()
    {
        UTFile = std::filesystem::current_path();
        UTFile = UTFile.parent_path();
        UTFile /= "crashdump_input_spr.json";
        inRoot = readInputFile(UTFile.c_str());
        if (inRoot == NULL)
        {
            EXPECT_TRUE(inRoot != NULL);
        }
    }
    static void SetUpTestSuite()
    {
        readSampleFile();
    }

    static void TearDownTestSuite()
    {
        cJSON_Delete(inRoot);
    }

    void SetUp() override
    {
        // Build a list of cpus
        //        CPUInfo cpuInfo = {.clientAddr = 48,
        //                           .model = cd_spr,
        //                           .coreMask = 0x1fffffffffffffff,
        //                           .crashedCoreMask = 0,
        //                           .chaCount = nCha,
        //                           .initialPeciWake = ON,
        //                           .inputFile = {},
        //                           .cpuidRead = {},
        //                           .chaCountRead = {},
        //                           .coreMaskRead = {},
        //                           .dimmMask = 0};
        //
        //        for (int i = 0; i < nCPUs; i++)
        //        {
        //            cpus.push_back(cpuInfo);
        //        }
        for (int i = 0; i < MAX_CPUS; i++)
        {
            cpus[i].clientAddr = 0;
        }

        // Initialize cpuInfo
        cpus[0].clientAddr = 0x30;
        cpus[0].model = cd_spr;
        cpus[0].coreMask = 0x1fffffffffffffff;
        cpus[0].chaCount = nCha;
    }

    void TearDown() override
    {
        FREE(jsonStr);
    }

  public:
    char* jsonStr = NULL;
    uint8_t cc = 0;
    bool enable = false;
    acdStatus status;
    CPUInfo cpus[MAX_CPUS] = {};
};

std::filesystem::path MaxSizeTestFixture::UTFile;
cJSON* MaxSizeTestFixture::inRoot;

TEST_F(MaxSizeTestFixture, metadata)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_metadata.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    InSequence s;

    // METADATA
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        // PreReqs
        uint8_t data0[8] = {0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00};
        uint8_t data1[8] = {0xbb, 0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillOnce(DoAll(SetArrayArgument<4>(data0, data0 + 8),
                            SetArgPointee<5>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<4>(data1, data1 + 8),
                            SetArgPointee<5>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
        // Comands
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    platformInfo.input_file =
        "/usr/share/crashdump/input/crashdump_input_spr.json";
    platformInfo.inputfile_ver = "release";
    platformInfo.crashdump_ver = "BMC_EGS_2.0";
    platformInfo.platform_name = "b220d785-43a4-43c3-9d70-af1f1fb2f1a7";
    platformInfo.me_fw_ver = "N/A";
    platformInfo.time_stamp = "2022-05-30T18:04:35Z";
    platformInfo.trigger_type = "On-Demand";
    platformInfo.bios_id = "EGSDCRB1.SYS.0073.D14.2201260629";
    platformInfo.bmc_fw_ver = "egs-0.91-463-g723c7e-e524c0d";
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    // FillNewSection needs to be executed over two sections
    for (int section = 0; section < 2; section++)
    {
        platformInfo.loopOverOneCpu = false;
        for (int cpu = 0; cpu < MAX_CPUS; cpu++)
        {
            if (!cpus[cpu].clientAddr)
                continue;
            status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0,
                                    0, &runTimeInfo, section, false);
        }
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, MCA_UNCORE)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_mca_uncore.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");
    // MCA_UNCORE
    int mca_uncore_nRegs =
        getNumberOfRepetitions(sections, "MCA_UNCORE", "RdIAMSR");

    InSequence s;

    // MCA_UNCORE
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
            .Times(mca_uncore_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<3>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<4>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, MCA_CBO)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_mca_cbo.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // MCA_CBO
    int mca_cbo_nRegs = getNumberOfRepetitions(sections, "MCA_CBO", "RdIAMSR");

    InSequence s;

    // MCA_CBO
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
            .Times(nCha * mca_cbo_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<3>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<4>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, MCA_CORE)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_mca_core.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // MCA_CORE
    int mca_core_nRegs =
        getNumberOfRepetitions(sections, "MCA_CORE", "RdIAMSR");

    InSequence s;

    // MCA_CORE
    for (CPUInfo& cpu : crashdump.cpus)
    {
        if (!cpu.clientAddr)
            continue;
        int activeCores = getNoActiveCores(cpu);

        uint8_t threadsPerCore = 2;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
            .Times(activeCores * threadsPerCore * mca_core_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<3>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<4>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, Uncore_PCI)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_uncore_pci.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // Uncore_PCI
    int pci_nRegs = getNumberOfRepetitionsUncorePCI(sections);

    InSequence s;

    // Uncore_PCI
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
            .Times(pci_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<7>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<8>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }

    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, Uncore_MMIO)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_mmio.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // Uncore_MMIO
    int uncore_mmio_nRegs =
        getNumberOfRepetitions(sections, "Uncore_MMIO", "RdEndpointConfigMMIO");

    InSequence s;

    // Uncore_MMIO
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        uint8_t returnBusValidQuery[4] = {0x3f, 0x0f, 0x00,
                                          0xc0}; // 0xc0000f3f; bit 30 set
        uint8_t returnEnumeratedBus[4] = {0x00, 0x00, 0xab, 0x00}; // 0xab0000;
        uint8_t returnEnumeratedBus2[4] = {0x89, 0xab, 0xcd,
                                           0xef}; // 0xefcdab89;
        uint8_t cc = 0x40;
        // PreReqs
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal)
            .WillOnce(DoAll(SetArrayArgument<7>(returnBusValidQuery,
                                                returnBusValidQuery + 4),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnEnumeratedBus,
                                                returnEnumeratedBus + 4),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnEnumeratedBus2,
                                                returnEnumeratedBus2 + 4),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        // Commands
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigMmio)
            .Times(uncore_mmio_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<9>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<10>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, Uncore_RdIAMSR)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_rdiamsr.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // Uncore_RdIAMSR
    int uncore_rdiamsr_nRegs =
        getNumberOfRepetitions(sections, "Uncore_RdIAMSR", "RdIAMSR");

    InSequence s;

    // Uncore_RdIAMSR
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
            .Times(uncore_rdiamsr_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<3>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<4>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }

    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, Uncore_RdIAMSR_CHA)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_rdiamsr_cha.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // Uncore_RdIAMSR_CHA
    int uncore_rdiamsr_cha_nRegs =
        getNumberOfRepetitions(sections, "Uncore_RdIAMSR_CHA", "RdIAMSR");

    InSequence s;
    // Uncore_RdIAMSR_CHA
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR)
            .Times(nCha * uncore_rdiamsr_cha_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<3>(0xDEADBEEFDEADBEEF),
                                  SetArgPointee<4>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }

    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, Tor)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_tor.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    InSequence s;

    // TOR
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        uint8_t cc = 0x40;
        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint8_t Data[8] = {0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery)
            .WillOnce(DoAll(SetArgPointee<6>(u8CrashdumpEnabled),
                            SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<6>(u16CrashdumpNumAgents),
                            SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<6>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame)
            .Times(nCha * 32 * 8)
            .WillRepeatedly(DoAll(SetArrayArgument<5>(Data, Data + 8),
                                  SetArgPointee<6>(globalCC),
                                  Return(PECI_CC_SUCCESS)));
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    platformInfo.loopOverOneCpu = false;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        sleep(1);
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }

    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, PM_PCI)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_pm_pci.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // PowerManagement_PCI
    int pm_pci_nRegs =
        getNumberOfRepetitions(sections, "PowerManagement_PCI", "RdPkgConfig");

    InSequence s;

    // PowerManagement_PCI
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        uint8_t Data[8] = {0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde};
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(pm_pci_nRegs)
            .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                                  SetArgPointee<5>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    platformInfo.loopOverOneCpu = false;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, PM_PCI_core)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_pm_pci_core.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // PowerManagement_PCI_core
    int pm_pci_core_nRegs = getNumberOfRepetitions(
        sections, "PowerManagement_PCI_core", "RdPkgConfigCore");

    InSequence s;

    // PowerManagement_PCI_core
    for (CPUInfo& cpu : crashdump.cpus)
    {
        if (!cpu.clientAddr)
            continue;
        int activeCores = getNoActiveCores(cpu);
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(activeCores * pm_pci_core_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEAB),
                                  SetArgPointee<5>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    platformInfo.loopOverOneCpu = false;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, PM_PLL)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_pm_pll.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // PowerManagement_Pll
    int pm_pll_nRegs = getNumberOfRepetitionsPM_pll(sections);

    InSequence s;

    // PowerManagement_Pll
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(pm_pll_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEAB),
                                  SetArgPointee<5>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    platformInfo.loopOverOneCpu = false;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, PM_Dispatcher)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_pm_dispatcher.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    cJSON* sections = cJSON_GetObjectItemCaseSensitive(inRoot, "Sections");

    // PowerManagement_Dispatcher
    int pm_dispatcher_nRegs = getNumberOfRepetitions(
        sections, "PowerManagement_Dispatcher", "RdPkgConfig");

    InSequence s;

    // PowerManagement_Dispatcher
    for (CPUInfo& cpu : crashdump.cpus)
    {
        (void)cpu;
        if (!cpu.clientAddr)
            continue;
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
            .Times(pm_dispatcher_nRegs)
            .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEAB),
                                  SetArgPointee<5>(globalCC),
                                  Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    platformInfo.loopOverOneCpu = false;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }
    char* sOut = cJSON_Print(outRoot);

    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}

TEST_F(MaxSizeTestFixture, Big_core)
{
    TestCrashdump crashdump(cpus);
    static std::filesystem::path UTFile;
    UTFile = std::filesystem::current_path();
    UTFile = UTFile.parent_path();
    UTFile /= "tests/UnitTestFiles/ut_bigcore.json";
    inRoot = readInputFile(UTFile.c_str());
    cpus[0].inputFile.bufferPtr = inRoot;
    RunTimeInfo runTimeInfo;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        cpus[cpu].inputFile.bufferPtr = inRoot;
    }

    InSequence s;

    // BigCore
    for (CPUInfo& cpu : crashdump.cpus)
    {
        if (!cpu.clientAddr)
            continue;
        int coresActive = 0;

        for (uint8_t u8CoreNum = 0; u8CoreNum < MAX_CORE_MASK; u8CoreNum++)
        {
            if (!CHECK_BIT(cpu.coreMask, u8CoreNum))
            {
                continue;
            }
            if (CHECK_BIT(cpu.crashedCoreMask, u8CoreNum))
            {
                continue;
            }
            coresActive++;
        }

        uint8_t cc = 0x40;
        uint8_t u8CrashdumpEnabled = 0;
        uint16_t u16CrashdumpNumAgents = 2;
        uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00};
        uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(cc), Return(PECI_CC_SUCCESS)));
        uint8_t firstFrame[8] = {0x01, 0x90, 0x02, 0x04,
                                 0xa8, 0x05, 0x00, 0x03}; // 0x030005a804029001;
        uint8_t allOtherFrames[8] = {0xab, 0xfa, 0xed, 0xfe, 0xef,
                                     0xbe, 0xad, 0xde}; // 0xdeadbeeffeedfaab;

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<6>(allOtherFrames, allOtherFrames + 8),
                      SetArgPointee<7>(cc), Return(PECI_CC_SUCCESS)));
    }

    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;
    platformInfo.loopOverOneCpu = false;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        sleep(1);
        status = fillNewSection(outRoot, &platformInfo, &cpus[cpu], cpu, 0, 0,
                                &runTimeInfo, 0, false);
    }

    char* sOut = cJSON_Print(outRoot);
    std::fstream fpJson;
    fpJson.open(out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}
