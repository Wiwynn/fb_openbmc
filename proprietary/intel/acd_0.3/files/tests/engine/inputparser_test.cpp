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

extern "C" {
#include "engine/crashdump.h"
#include "engine/inputparser.h"
#include "engine/utils.h"
}

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace ::testing;
using ::testing::Return;

class InputParserTestFixture : public ::testing::Test
{
    void SetUp() override
    {
        for (int i = 0; i < MAX_CPUS; i++)
        {
            cpus[i].clientAddr = 0;
        }
        cpus[0].clientAddr = 0x30;
        cpus[0].model = cd_spr;
        cpus[1].clientAddr = 0x31;
        cpus[1].model = cd_spr;
    }

    void TearDown() override
    {
        cJSON_Delete(params);
    }

  public:
    cJSON* params = NULL;
    InputParserErrInfo errInfo = {};
    acdStatus status;
    CPUInfo cpus[MAX_CPUS] = {};
};

TEST_F(InputParserTestFixture, InputParserUpdateParams)
{
    CmdInOut cmdInOut = {};
    InputParserErrInfo errInfo = {};
    LoggerStruct loggerStruct;

    cmdInOut.internalVarsTracker = cJSON_Parse(R"({"PostEnumBus": "0x7e"})");
    cmdInOut.internalVarName = "PostEnumBus";
    cmdInOut.in.params = cJSON_Parse(
        R"(["Target", 0, 0, "PostEnumBus", 26, 0, "0x201A4", 6, 4])");

    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);

    cmdInOut.in.params = cJSON_Parse(
        R"(["Target", 0, 0, "PostEnumBusInvalid", 26, 0, "0x201A4", 6, 4])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);
}

TEST_F(InputParserTestFixture, InputParserSingle)
{
    params = cJSON_Parse(R"(["Target", 0, 0, "0x10"])");
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct;
    cmdInOut.paramsTracker = cJSON_CreateObject();
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);

    // "Target" should be updated and matching clientAddr
    if (cJSON_GetArraySize(cmdInOut.paramsTracker) != 0)
    {
        int newVal = cJSON_GetArrayItem(params, 0)->valueint;
        EXPECT_EQ(newVal, cpus[0].clientAddr);
    }
    cJSON_Delete(cmdInOut.paramsTracker);
}

TEST_F(InputParserTestFixture, InputParserMulti)
{
    params = cJSON_Parse(R"(["Target", 0, 0, "0x10"])");
    LoggerStruct loggerStruct;
    for (int cpu = 0; cpu < MAX_CPUS; cpu++)
    {
        if (!cpus[cpu].clientAddr)
            continue;
        CmdInOut cmdInOut = {};
        cmdInOut.paramsTracker = cJSON_CreateObject();
        status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
        EXPECT_EQ(ACD_SUCCESS, status);

        if (cJSON_GetArraySize(cmdInOut.paramsTracker) != 0)
        {
            int newVal = cJSON_GetArrayItem(params, 0)->valueint;
            EXPECT_EQ(newVal, cpus[cpu].clientAddr);
        }

        // Reset params for next cpuInfo
        status = ResetParams(params, cmdInOut.paramsTracker);
        EXPECT_EQ(ACD_SUCCESS, status);
        cJSON_Delete(cmdInOut.paramsTracker);
    }
}

TEST_F(InputParserTestFixture, InputParserSingleInvalidTarget)
{
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct;
    cmdInOut.in.params = cJSON_Parse(R"(["TargetInvalid", 0, 0, "0x10"])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);
}

TEST_F(InputParserTestFixture, InputParserSingleInvalidHexString)
{
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct;
    cmdInOut.in.params = cJSON_Parse(R"(["Target", 0, 0, "0x10Invalid"])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);
}

TEST_F(InputParserTestFixture, InputParserDomainID)
{
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct;
    cmdInOut.in.params = cJSON_Parse(R"(["Target", "DomainID", 0, "0x10"])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);
}

TEST_F(InputParserTestFixture, InputParserPunit)
{
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct;
    cmdInOut.in.params = cJSON_Parse(R"(["Target", "PCU-D[9-5]", 0, "0x10"])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);
}

TEST_F(InputParserTestFixture, InputParserExplicitDomain)
{
    CmdInOut cmdInOut = {};
    LoggerStruct loggerStruct;

    // 3 computes and 2 io
    cpus[0].dieMaskInfo.dieMask = GNR_MAX_DIEMASK;
    initDieReadStructure(&cpus[0].dieMaskInfo, DieMaskRange.GNR.compute,
                         DieMaskRange.GNR.io);

    // valid compute
    cmdInOut.in.params =
        cJSON_Parse(R"(["Target", "compute1", 0, 30, 8, 0, "0x504", 8])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);

    // Valid io
    cmdInOut.in.params =
        cJSON_Parse(R"(["Target", "io1", 0, 30, 8, 0, "0x504", 8])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_SUCCESS, status);

    // Attempt to find non existent compute 5
    cmdInOut.in.params =
        cJSON_Parse(R"(["Target", "compute5", 0, 30, 8, 0, "0x504", 8])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_FAILURE_UPDATE_PARAMS, status);

    // Attempt to find non existent io 3
    cmdInOut.in.params =
        cJSON_Parse(R"(["Target", "io3", 0, 30, 8, 0, "0x504", 8])");
    status = UpdateParams(&cpus[0], &cmdInOut, &loggerStruct, &errInfo);
    EXPECT_EQ(ACD_FAILURE_UPDATE_PARAMS, status);
}
