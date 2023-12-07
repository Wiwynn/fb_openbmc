/******************************************************************************
 *
 * INTEL CONFIDENTIAL
 *
 * Copyright 2022 Intel Corporation.
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

#include <peci.h>

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

static int getDieMask(uint8_t dieMaskData[])
{
    int dieMask = 0;

    for (int i = 0; i < 8; i++)
    {
        dieMask += (dieMaskData[i] << 8 * i);
    }

    return dieMask;
}

static int getNoActiveComputes(uint8_t dieMaskData[])
{
    int computesActive = 0;

    int dieMask = (getDieMask(dieMaskData) & 0xF00) >> 8;
    for (uint8_t u8CoreNum = 0; u8CoreNum < MAX_NUM_DIE; u8CoreNum++)
    {
        if (!CHECK_BIT(dieMask, u8CoreNum))
        {
            continue;
        }
        computesActive++;
    }
    return computesActive;
}

static size_t getChaMask(uint8_t chaMaskLow[], uint8_t chaMaskHigh[])
{
    uint32_t chaMaskL = 0;
    uint32_t chaMaskH = 0;
    size_t chaMask = 0;

    for (int i = 0; i < 4; i++)
    {
        chaMaskL += (chaMaskLow[i] << 8 * i);
    }
    for (int i = 0; i < 4; i++)
    {
        chaMaskH += (chaMaskHigh[i] << 8 * (i + 4));
    }

    chaMask = chaMaskH;
    chaMask <<= 32;
    chaMask |= chaMaskL;

    return chaMask;
}

static int getNoActiveCHAs(uint8_t chaMaskLow[], uint8_t chaMaskHigh[])
{
    uint32_t chaMaskL = 0;
    uint32_t chaMaskH = 0;

    for (int i = 0; i < 4; i++)
    {
        chaMaskL += (chaMaskLow[i] << 8 * i);
    }
    for (int i = 0; i < 4; i++)
    {
        chaMaskH += (chaMaskHigh[i] << 8 * (i + 4));
    }

    size_t nChasActive =
        __builtin_popcount(chaMaskL) + __builtin_popcount(chaMaskH);
    return nChasActive;
}

int gnr_nCPUs = 2;
std::string crashdumpContents;
std::filesystem::path gnr_out_file = "max_output.json";

// GetDieMask
// Data[1:] contain the active computes
// 0xF = computes 0-3
uint8_t Data[8] = {0x11, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0, 0x0};
int nActiveComputes = getNoActiveComputes(Data);
int dieMask = getDieMask(Data);

// GetCHAMasks
uint8_t chaMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
// 0xFFFFFFFF = cha 0-31
uint8_t chaMaskLow[4] = {0xff, 0xff, 0xff, 0xff};
int nActiveChas = getNoActiveCHAs(chaMaskLow, chaMaskHigh);

// GetCoreMasks
// 0xFFFFFFFF = core 0-31
uint8_t coreMaskHigh[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t coreMaskLow[4] = {0xff, 0xff, 0xff, 0xff};
int nActiveCores = getNoActiveCHAs(coreMaskLow, coreMaskHigh);
size_t coreMask = getChaMask(coreMaskLow, coreMaskHigh);

// All
uint8_t DataSample[8] = {0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde};

TEST(GNRMaxSizeTest, createCrashdump_GNR)
{
    TestCrashdump crashdump(cd_gnr);
    std::string triggerType = "UnitTest";
    std::string timestamp = crashdump::newTimestamp();
    bool isTelemetry = false;

    for (int i = 0; i < gnr_nCPUs; i++)
    {
        crashdump.cpus[i].clientAddr = 0x0;
        crashdump.cpus[i].cpuidRead.cpuidValid = false;
    }

    ///////////////////////////////////////////////////////////////////////////
    // ----------------------------[Start of vars]-----------------------------
    // METADATA
    uint8_t data0[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    uint8_t data1[4] = {0xbb, 0xbb, 0xbb, 0x00};

    // MCA_UNCORE
    int mca_uncore_nRegs = getNumberOfRepetitions(
        crashdump.cpus[0].inputFile.bufferPtr, "MCA_UNCORE", "RdIAMSR_dom");

    // MCA_CBO
    int mca_cbo_nRegs = getNumberOfRepetitions(
        crashdump.cpus[0].inputFile.bufferPtr, "MCA_CBO", "RdIAMSR_dom");

    // Uncore_PCI
    int pci_nRegs = repsUncorePCI(crashdump.cpus[0].inputFile.bufferPtr);

    // Uncore_RdIAMSR
    int uncore_rdiamsr_nRegs = getNumberOfRepetitions(
        crashdump.cpus[0].inputFile.bufferPtr, "Uncore_RdIAMSR", "RdIAMSR_dom");

    // TOR
    uint8_t u8CrashdumpEnabled = 0;
    uint16_t u16CrashdumpNumAgents = 2;
    uint8_t u64UniqueId[8] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t PayloadExp[8] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // BigCore
    uint8_t firstFrame[8] = {0x01, 0x90, 0x02, 0x04,
                             0xa8, 0x05, 0x00, 0x03}; // 0x030005a804029001;

    // crashlog
    uint8_t telemetry_answer0[1] = {0x01}; // Telemetry supported
    uint8_t telemetry_answer1[1] = {0x08}; // agents
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
    uint8_t returnValue[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    // ----------------------------[End of vars]-------------------------------

    ///////////////////////////////////////////////////////////////////////////
    ////////////////////////////////[Sections]/////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    // Crashlog
    Sequence sCrashlog;
    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_GetCrashlogSample_dom)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(returnValue, returnValue + 8),
                              SetArgPointee<6>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherRd_dom)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                              SetArgPointee<6>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_ConfigWatcherWr_dom)
        .WillRepeatedly(DoAll(SetArrayArgument<5>(returnValue, returnValue + 1),
                              SetArgPointee<6>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    for (int i = 0; i < (gnr_nCPUs * nActiveComputes); i++)
    {
        EXPECT_CALL(*crashdump.libPeciMock, peci_Telemetry_Discovery_dom)
            .InSequence(sCrashlog)
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer0, telemetry_answer0 + 1),
                SetArgPointee<8>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(
                SetArrayArgument<7>(telemetry_answer1, telemetry_answer1 + 1),
                SetArgPointee<8>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails0,
                                                returnCrashlogDetails0 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails1,
                                                returnCrashlogDetails1 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails2,
                                                returnCrashlogDetails2 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails3,
                                                returnCrashlogDetails3 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails4,
                                                returnCrashlogDetails4 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails5,
                                                returnCrashlogDetails5 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails6,
                                                returnCrashlogDetails6 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<7>(returnCrashlogDetails7,
                                                returnCrashlogDetails7 + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)));
    }

    ///////////////////////////////////////////////////////////////////////////
    // Metadata_Cpu_Late
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .Times(nActiveComputes * gnr_nCPUs)
        .WillRepeatedly(DoAll(SetArrayArgument<8>(data1, data1 + 4),
                              Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    /////////////////////////////////////////////////////////////////////////
    // Uncore_RdIAMSR
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        .Times(uncore_rdiamsr_nRegs * nActiveComputes * gnr_nCPUs)
        .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    ///////////////////////////////////////////////////////////////////////////
    // Uncore_PCI

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
        .Times(pci_nRegs * nActiveComputes * gnr_nCPUs)
        .WillRepeatedly(DoAll(SetArrayArgument<8>(DataSample, DataSample + 8),
                              SetArgPointee<9>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    ///////////////////////////////////////////////////////////////////////////
    Sequence sMCA;
    // MCA_UNCORE
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        .Times(mca_uncore_nRegs * nActiveComputes * gnr_nCPUs)
        .InSequence(sMCA)
        .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    ///////////////////////////////////////////////////////////////////////////
    // MCA_CBO
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        .Times(mca_cbo_nRegs * nActiveComputes * gnr_nCPUs * nActiveChas)
        .InSequence(sMCA)
        .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    ///////////////////////////////////////////////////////////////////////////
    // BigCore
    Sequence sBigCore;

    for (int i = 0; i < (gnr_nCPUs * nActiveComputes); i++)
    {
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .InSequence(sBigCore)
            .WillOnce(DoAll(SetArgPointee<7>(u8CrashdumpEnabled),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .InSequence(sBigCore)
            .WillOnce(DoAll(SetArgPointee<7>(u16CrashdumpNumAgents),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .InSequence(sBigCore)
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_Discovery_dom)
            .InSequence(sBigCore)
            .WillOnce(DoAll(SetArrayArgument<7>(PayloadExp, PayloadExp + 8),
                            SetArgPointee<8>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();

        EXPECT_CALL(*crashdump.libPeciMock, peci_CrashDump_GetFrame_dom)
            .InSequence(sBigCore)
            .WillOnce(DoAll(SetArrayArgument<6>(firstFrame, firstFrame + 8),
                            SetArgPointee<7>(PECI_DEV_CC_SUCCESS),
                            Return(PECI_CC_SUCCESS)))
            .WillRepeatedly(
                DoAll(SetArrayArgument<6>(DataSample, DataSample + 8),
                      SetArgPointee<7>(PECI_DEV_CC_SUCCESS),
                      Return(PECI_CC_SUCCESS)));
    }

    // ///////////////////////////////////////////////////////////////////////////
    // TOR
    Sequence sTor;

    for (int i = 0; i < (gnr_nCPUs * nActiveComputes); i++)
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
            .WillOnce(DoAll(SetArrayArgument<7>(u64UniqueId, u64UniqueId + 8),
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
            .WillRepeatedly(DoAll(
                SetArrayArgument<6>(DataSample, DataSample + 8),
                SetArgPointee<7>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    // METADATA
    Sequence sMetadata_Cpu_Early;
    for (int i = 0; i < (gnr_nCPUs * nActiveComputes); i++)
    {
        // PreReqs
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
            .InSequence(sMetadata_Cpu_Early)
            .WillOnce(DoAll(SetArrayArgument<5>(data0, data0 + 8),
                            SetArgPointee<6>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<5>(data1, data1 + 8),
                            SetArgPointee<6>(0x40), Return(PECI_CC_SUCCESS)));

        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .InSequence(sMetadata_Cpu_Early)
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(data0, data0 + 4),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(data0, data0 + 4),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(data0, data0 + 4),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(data0, data0 + 4),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();

        // Comands
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .InSequence(sMetadata_Cpu_Early)
            .WillOnce(DoAll(SetArrayArgument<8>(data1, data1 + 4),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(data1, data1 + 4),
                            Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(SetArrayArgument<8>(data1, data1 + 4),
                            Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig_dom)
            .InSequence(sMetadata_Cpu_Early)
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)))
            .WillOnce(DoAll(Return(PECI_CC_SUCCESS)));
    }

    ///////////////////////////////////////////////////////////////////////////
    ////////////////////////////[CreateCrashdump]//////////////////////////////
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    // CreateCrashdump
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(2 * gnr_nCPUs) // by CPU
        .WillRepeatedly(DoAll(SetArgPointee<4>(0x1),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    Sequence sGetClientAddrs;
    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .Times(gnr_nCPUs)
        .InSequence(sGetClientAddrs)
        .WillRepeatedly(DoAll(Return(PECI_CC_SUCCESS)));

    EXPECT_CALL(*crashdump.libPeciMock, peci_Ping)
        .Times(MAX_CPUS - gnr_nCPUs)
        .InSequence(sGetClientAddrs)
        .WillRepeatedly(DoAll(Return(PECI_CC_TIMEOUT)));

    // getCPUID()
    EXPECT_CALL(*crashdump.libPeciMock, peci_GetCPUID)
        .Times(gnr_nCPUs)
        .WillRepeatedly(DoAll(
            SetArgPointee<1>((CPUModel)GNR_MODEL), SetArgPointee<2>(1),
            SetArgPointee<3>(PECI_DEV_CC_SUCCESS), Return(PECI_CC_SUCCESS)));

    // getDieMask()
    EXPECT_CALL(*crashdump.libPeciMock, peci_RdPkgConfig)
        .Times(gnr_nCPUs) // 1 x CPU
        .WillRepeatedly(DoAll(SetArrayArgument<4>(Data, Data + 8),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)))
        .RetiresOnSaturation();

    Sequence sGetMasks;
    // getCoreMasks()
    for (int i = 0; i < (gnr_nCPUs * nActiveComputes); i++)
    {
        // Cores
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .InSequence(sGetMasks)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskLow, coreMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .InSequence(sGetMasks)
            .WillOnce(DoAll(SetArrayArgument<8>(coreMaskHigh, coreMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }
    // GetChaMasks
    for (int i = 0; i < (gnr_nCPUs * nActiveComputes); i++)
    {
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .InSequence(sGetMasks)
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskLow, chaMaskLow + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
        EXPECT_CALL(*crashdump.libPeciMock, peci_RdEndPointConfigPciLocal_dom)
            .InSequence(sGetMasks)
            .WillOnce(DoAll(SetArrayArgument<8>(chaMaskHigh, chaMaskHigh + 4),
                            SetArgPointee<9>(0x40), Return(PECI_CC_SUCCESS)))
            .RetiresOnSaturation();
    }

    ///////////////////////////////////////////////////////////////////////////
    //***********************************************************************//

    crashdump::createCrashdump(crashdump.cpus, crashdumpContents, triggerType,
                               timestamp, isTelemetry);

    //***********************************************************************//
    ///////////////////////////////////////////////////////////////////////////

    std::fstream fpJson;
    fpJson.open(gnr_out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << crashdumpContents.c_str();
        fpJson.close();
    }
}

TEST(GNRMaxSizeTest, fillNewSection_GNR_MCA_CORE)
{
    TestCrashdump crashdump(cd_gnr);
    struct timespec sectionStart;
    struct timespec globalStart;
    clock_gettime(CLOCK_MONOTONIC, &sectionStart);
    clock_gettime(CLOCK_MONOTONIC, &globalStart);
    PlatformInfo platformInfo;
    RunTimeInfo runTimeInfo;
    runTimeInfo.globalRunTime = globalStart;
    runTimeInfo.sectionRunTime = sectionStart;
    runTimeInfo.maxGlobalTime = 700;

    cJSON* jOut = readInputFile(gnr_out_file.c_str());

    for (int i = 0; i < MAX_CPUS; i++)
    {
        if (i < gnr_nCPUs)
        {
            crashdump.cpus[i].clientAddr = MIN_CLIENT_ADDR + i;
            crashdump.cpus[i].cpuidRead.cpuidValid = true;
            crashdump.cpus[i].chaCountRead.chaCountValid = true;
            crashdump.cpus[i].coreMaskRead.coreMaskValid = true;
            crashdump.cpus[i].cpuidRead.cpuModel =
                static_cast<CPUModel>(GNR_MODEL);
            crashdump.cpus[i].model = cd_gnr;
            crashdump.cpus[i].dieMaskInfo.dieMask = dieMask;
            initComputeDieStructure(&crashdump.cpus[i].dieMaskInfo,
                                    &crashdump.cpus[i].computeDies);

            crashdump.cpus[i].computeDies->coreMask = coreMask;
            crashdump.cpus[i].inputFile.bufferPtr =
                crashdump.cpus[0].inputFile.bufferPtr;
        }

        else
        {
            crashdump.cpus[i].clientAddr = 0;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // MCA_CORE
    int mca_core_nRegs = getNumberOfRepetitions(
        crashdump.cpus[0].inputFile.bufferPtr, "MCA_CORE", "RdIAMSR_dom");
    Sequence sMCA_CORE;

    EXPECT_CALL(*crashdump.libPeciMock, peci_RdIAMSR_dom)
        // Multiplied by 2 for each thread
        .Times(2 * mca_core_nRegs * gnr_nCPUs * nActiveComputes * nActiveCores)
        .WillRepeatedly(DoAll(SetArgPointee<4>(0xDEADBEEFDEADBEEF),
                              SetArgPointee<5>(PECI_DEV_CC_SUCCESS),
                              Return(PECI_CC_SUCCESS)));

    for (int i = 0; i < MAX_CPUS; i++)
    {
        if (i < gnr_nCPUs)
        {
            int dieMaskClean = (dieMask & 0xF00) >> 8;
            for (uint8_t u8CoreNum = 0; u8CoreNum < MAX_NUM_DIE; u8CoreNum++)
            {
                if (!CHECK_BIT(dieMaskClean, u8CoreNum))
                {
                    continue;
                }

                fillNewSection(jOut, &platformInfo, &crashdump.cpus[i], i,
                               u8CoreNum, &runTimeInfo, 8, false);
            }
        }
    }

    char* sOut = cJSON_Print(jOut);

    std::fstream fpJson;
    fpJson.open(gnr_out_file.c_str(), std::ios::out);
    if (fpJson)
    {
        fpJson << sOut;
        fpJson.close();
    }
}
